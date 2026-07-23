#ifndef SHOWDUINO_ESPNOW_TRANSPORT_H
#define SHOWDUINO_ESPNOW_TRANSPORT_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/portmacro.h>
#include "BoardConfig.h"
#include "../../../protocol/showduino_desk_packet.h"
#include "../../../protocol/showduino_validation.h"

// =========================================================
// Showduino ESP-NOW transport
// Director S3 <-> C3 SuperMini bridge
// Desk packet: protocol/showduino_desk_packet.h (wire-compatible)
// =========================================================

class ShowduinoEspNowTransport {
public:
  bool begin() {
    Serial.println("ESP-NOW: starting portable Director transport...");
    Serial.printf("ESP-NOW: free heap before Wi-Fi = %u\n", (unsigned)ESP.getFreeHeap());

    if (!bringUpWifiSta()) {
      online = false;
      return false;
    }

    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary, &second);
    Serial.printf("ESP-NOW: Wi-Fi channel = %u (PS=NONE)\n", primary);

    String macStr = WiFi.macAddress();
    Serial.print("ESP-NOW: Director MAC = ");
    Serial.println(macStr);

    if (primary == 0 || macStr == "00:00:00:00:00:00" || macStr.length() < 11) {
      Serial.println("ESP-NOW: Wi-Fi radio not up (channel/MAC invalid) — abort");
      online = false;
      return false;
    }

    if (esp_now_init() != ESP_OK) {
      Serial.println("ESP-NOW: init failed.");
      online = false;
      return false;
    }

    esp_now_register_send_cb(onSentStatic);
    esp_now_register_recv_cb(onRecvStatic);

    if (!addBridgePeer()) {
      online = false;
      return false;
    }

    online = true;
    sendBusy = false;
    portENTER_CRITICAL(&rxMux);
    rxHead = 0;
    rxTail = 0;
    portEXIT_CRITICAL(&rxMux);

    Serial.print("ESP-NOW: bridge peer = ");
    printMac(stageBridgeMac);
    Serial.println();
    return true;
  }

  // Soft recover — channel drift / SoftAP side-effects. Avoids per-packet churn.
  bool recover() {
    if (!bringUpWifiSta()) {
      online = false;
      return false;
    }
    if (esp_now_is_peer_exist(stageBridgeMac)) {
      esp_now_del_peer(stageBridgeMac);
    }
    if (!online) {
      /* First bring-up failed earlier — complete ESP-NOW init now if possible. */
      if (esp_now_init() != ESP_OK) {
        /* May already be initialised from a partial begin(). */
      }
      esp_now_register_send_cb(onSentStatic);
      esp_now_register_recv_cb(onRecvStatic);
    }
    bool ok = addBridgePeer();
    online = ok;
    return ok;
  }

  bool sendCommand(const String &command) {
    if (!online) return false;

    unsigned long startWait = millis();
    while (sendBusy && (millis() - startWait) < 30) {
      delay(1);
    }

    ShowduinoEspNowPacket packet = {};
    packet.magic = SHOWDUINO_ESPNOW_MAGIC;
    packet.version = SHOWDUINO_ESPNOW_VERSION;
    packet.sequence = nextSequence++;
    packet.sentMillis = millis();
    command.substring(0, SHOWDUINO_ESPNOW_COMMAND_MAX - 1).toCharArray(packet.command, SHOWDUINO_ESPNOW_COMMAND_MAX);

    lastCommand = command;
    lastSequence = packet.sequence;

    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary, &second);
    if (primary != (uint8_t)SHOWDUINO_ESPNOW_CHANNEL) {
      recover();
    }

    sendBusy = true;
    lastCallbackOk = false;
    callbackSeen = false;

    esp_err_t result = esp_now_send(stageBridgeMac, (uint8_t *)&packet, sizeof(packet));
    if (result == ESP_ERR_ESPNOW_CHAN) {
      Serial.println("ESP-NOW: CHAN mismatch → recover");
      recover();
      sendBusy = true;
      lastCallbackOk = false;
      callbackSeen = false;
      result = esp_now_send(stageBridgeMac, (uint8_t *)&packet, sizeof(packet));
    }
    if (result != ESP_OK) {
      sendBusy = false;
      lastSendOk = false;
      return false;
    }

    unsigned long t0 = millis();
    while (!callbackSeen && (millis() - t0) < 80) {
      delay(1);
    }

    lastSendOk = (callbackSeen && lastCallbackOk);
    return lastSendOk;
  }

  bool popReply(String &outLine) {
    portENTER_CRITICAL(&rxMux);
    if (rxHead == rxTail) {
      portEXIT_CRITICAL(&rxMux);
      return false;
    }
    char local[SHOWDUINO_ESPNOW_COMMAND_MAX];
    memcpy(local, rxQueue[rxTail], SHOWDUINO_ESPNOW_COMMAND_MAX);
    rxTail = (uint8_t)((rxTail + 1) % RX_QUEUE_DEPTH);
    portEXIT_CRITICAL(&rxMux);

    outLine = String(local);
    return true;
  }

  bool isOnline() const { return online; }
  bool wasLastSendOk() const { return lastSendOk; }

private:
  static const uint8_t RX_QUEUE_DEPTH = 16;

  bool online = false;
  bool lastSendOk = false;
  uint16_t nextSequence = 1;
  uint16_t lastSequence = 0;
  String lastCommand;

  static portMUX_TYPE rxMux;
  static volatile bool sendBusy;
  static volatile bool callbackSeen;
  static volatile bool lastCallbackOk;
  static volatile uint8_t rxHead;
  static volatile uint8_t rxTail;
  static char rxQueue[RX_QUEUE_DEPTH][SHOWDUINO_ESPNOW_COMMAND_MAX];

  uint8_t stageBridgeMac[6] = {
    SHOWDUINO_P4_C6_MAC_0,
    SHOWDUINO_P4_C6_MAC_1,
    SHOWDUINO_P4_C6_MAC_2,
    SHOWDUINO_P4_C6_MAC_3,
    SHOWDUINO_P4_C6_MAC_4,
    SHOWDUINO_P4_C6_MAC_5
  };

  bool bringUpWifiSta() {
    WiFi.persistent(false);

    /* Arduino WiFi.mode() may log ESP_ERR_WIFI_NOT_INIT(0x3001) during a
       failed re-init when internal heap is exhausted — call this early. */
    if (!WiFi.mode(WIFI_STA)) {
      Serial.println("ESP-NOW: WiFi.mode(WIFI_STA) failed");
      return false;
    }
    WiFi.disconnect(false, false);
    delay(100);

    esp_err_t startErr = esp_wifi_start();
    if (startErr != ESP_OK) {
      Serial.printf("ESP-NOW: esp_wifi_start -> %s (%d)\n",
                    esp_err_to_name(startErr), (int)startErr);
    }

    /* Critical on RGB S3 boards: modem sleep kills ESP-NOW within seconds. */
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_err_t chErr = esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (chErr != ESP_OK) {
      Serial.printf("ESP-NOW: set_channel(%u) failed: %s\n",
                    (unsigned)SHOWDUINO_ESPNOW_CHANNEL, esp_err_to_name(chErr));
      return false;
    }
    delay(50);

    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary, &second);
    return primary == (uint8_t)SHOWDUINO_ESPNOW_CHANNEL;
  }

  bool addBridgePeer() {
    if (esp_now_is_peer_exist(stageBridgeMac)) {
      // Already present — leave it alone (del/add churn breaks RX).
      return true;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, stageBridgeMac, 6);
    peerInfo.channel = 0; /* current home channel — SoftAP-safe */
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    esp_err_t peerErr = esp_now_add_peer(&peerInfo);
    if (peerErr != ESP_OK && peerErr != ESP_ERR_ESPNOW_EXIST) {
      Serial.printf("ESP-NOW: failed to add bridge peer (%d).\n", (int)peerErr);
      return false;
    }
    return true;
  }

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
  static void onSentStatic(const esp_now_send_info_t *txInfo, esp_now_send_status_t status) {
    (void)txInfo;
#else
  static void onSentStatic(const uint8_t *macAddr, esp_now_send_status_t status) {
    (void)macAddr;
#endif
    lastCallbackOk = (status == ESP_NOW_SEND_SUCCESS);
    callbackSeen = true;
    sendBusy = false;
  }

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  static void onRecvStatic(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
    (void)recvInfo;
#else
  static void onRecvStatic(const uint8_t *macAddr, const uint8_t *incomingData, int len) {
    (void)macAddr;
#endif
    ShowduinoValidateResult vr = showduino_validate_desk_rx(incomingData, (size_t)len);
    if (vr != SHOWDUINO_VALID) return;

    ShowduinoEspNowPacket packet = {};
    memcpy(&packet, incomingData, sizeof(packet));

    portENTER_CRITICAL(&rxMux);
    uint8_t next = (uint8_t)((rxHead + 1) % RX_QUEUE_DEPTH);
    if (next == rxTail) {
      rxTail = (uint8_t)((rxTail + 1) % RX_QUEUE_DEPTH);
    }
    memcpy(rxQueue[rxHead], packet.command, SHOWDUINO_ESPNOW_COMMAND_MAX);
    rxHead = next;
    portEXIT_CRITICAL(&rxMux);
  }

  void printMac(const uint8_t *mac) {
    for (uint8_t i = 0; i < 6; i++) {
      if (i > 0) Serial.print(":");
      if (mac[i] < 16) Serial.print("0");
      Serial.print(mac[i], HEX);
    }
  }
};

portMUX_TYPE ShowduinoEspNowTransport::rxMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool ShowduinoEspNowTransport::sendBusy = false;
volatile bool ShowduinoEspNowTransport::callbackSeen = false;
volatile bool ShowduinoEspNowTransport::lastCallbackOk = false;
volatile uint8_t ShowduinoEspNowTransport::rxHead = 0;
volatile uint8_t ShowduinoEspNowTransport::rxTail = 0;
char ShowduinoEspNowTransport::rxQueue[ShowduinoEspNowTransport::RX_QUEUE_DEPTH][SHOWDUINO_ESPNOW_COMMAND_MAX];

#endif
