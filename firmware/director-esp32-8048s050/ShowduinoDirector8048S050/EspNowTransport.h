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
#include "../../../protocol/showduino_radio_follow.h"

// =========================================================
// Showduino ESP-NOW transport
// This touchscreen Director <-> standalone ESP32-S3 Comms Controller
// Desk packet: protocol/showduino_desk_packet.h (wire-compatible)
// =========================================================

class ShowduinoEspNowTransport {
public:
  bool begin() {
    Serial.println("ESP-NOW: starting portable Director transport...");

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(200);

    // Critical on RGB S3 boards: modem sleep kills ESP-NOW within seconds.
    esp_wifi_set_ps(WIFI_PS_NONE);
    follow_.begin(SHOWDUINO_RADIO_HOME_CHANNEL);
    follow_.lock(SHOWDUINO_RADIO_HOME_CHANNEL);
    delay(50);

    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary, &second);
    Serial.printf("ESP-NOW: Wi-Fi channel = %u (PS=NONE)\n", primary);

    Serial.print("ESP-NOW: Director MAC = ");
    Serial.println(WiFi.macAddress());

    esp_err_t initErr = esp_now_init();
    if (initErr != ESP_OK && initErr != ESP_ERR_ESPNOW_EXIST) {
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

  // Soft recover - keep a live peer (del/add churn drops RX).
  // reinit: operator Retry - tear down and start ESP-NOW again.
  bool recover(bool reinit = false) {
    if (reinit) {
      if (online) {
        esp_now_deinit();
        online = false;
      }
      return begin();
    }
    if (!online) {
      return begin();
    }
    esp_wifi_set_ps(WIFI_PS_NONE);
    follow_.lock(follow_.channel());
    if (!esp_now_is_peer_exist(stageBridgeMac)) {
      return addBridgePeer();
    }
    return true;
  }

  void service(bool haveLink) {
    follow_.tick(haveLink);
  }

  uint8_t radioChannel() const { return follow_.channel(); }

  bool sendCommand(const String &command) {
    if (!online) return false;
    if (sendBusy) return false;

    ShowduinoEspNowPacket packet = {};
    packet.magic = SHOWDUINO_ESPNOW_MAGIC;
    packet.version = SHOWDUINO_ESPNOW_VERSION;
    packet.sequence = nextSequence++;
    packet.sentMillis = millis();
    command.substring(0, SHOWDUINO_ESPNOW_COMMAND_MAX - 1).toCharArray(packet.command, SHOWDUINO_ESPNOW_COMMAND_MAX);

    lastCommand = command;
    lastSequence = packet.sequence;

    sendBusy = true;
    lastCallbackOk = false;
    callbackSeen = false;

    esp_err_t result = esp_now_send(stageBridgeMac, (uint8_t *)&packet, sizeof(packet));
    if (result != ESP_OK) {
      sendBusy = false;
      lastSendOk = false;
      return false;
    }

    lastSendOk = true;
    return true;
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
  static const uint8_t RX_QUEUE_DEPTH = 32;

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
    SHOWDUINO_COMMS_MAC_0,
    SHOWDUINO_COMMS_MAC_1,
    SHOWDUINO_COMMS_MAC_2,
    SHOWDUINO_COMMS_MAC_3,
    SHOWDUINO_COMMS_MAC_4,
    SHOWDUINO_COMMS_MAC_5
  };
  ShowduinoRadioFollow follow_{};

  bool addBridgePeer() {
    if (esp_now_is_peer_exist(stageBridgeMac)) {
      // Already present - leave it alone (del/add churn breaks RX).
      return true;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, stageBridgeMac, 6);
    peerInfo.channel = 0;
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
