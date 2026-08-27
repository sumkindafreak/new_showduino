#include "EspNowTransport.h"
#include "../BoardConfig.h"

#include <stdio.h>
#include <string.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>

static bool sReady = false;
static bool sHaveDirector = false;
static uint8_t sDirectorMac[6] = {0};
static uint16_t sTxSequence = 1;
static uint32_t sRxCount = 0;
static uint32_t sTxCount = 0;
static uint32_t sRejected = 0;
static uint32_t sLastDirectorMs = 0;
static ShowduinoDeskCommandFn sHandler = nullptr;

void espNowTransportPrintMac(const uint8_t *mac) {
  if (!mac) {
    Serial.print("(null)");
    return;
  }
  for (uint8_t i = 0; i < 6; i++) {
    if (i > 0) Serial.print(":");
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
  }
}

bool espNowTransportReadStaMac(uint8_t out[6]) {
  if (!out) return false;
  memset(out, 0, 6);
  if (esp_read_mac(out, ESP_MAC_WIFI_STA) == ESP_OK) {
    for (int i = 0; i < 6; i++) {
      if (out[i] != 0) return true;
    }
  }
  String s = WiFi.macAddress();
  unsigned a[6] = {0};
  if (sscanf(s.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
             &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 6) {
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)a[i];
    return true;
  }
  return false;
}

static bool addDirectorPeer(const uint8_t *mac) {
  if (!mac) return false;
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = SHOWDUINO_ESPNOW_CHANNEL;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  esp_err_t err = esp_now_add_peer(&peer);
  return err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST;
}

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void onEspNowReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
#else
static void onEspNowReceive(const uint8_t *macAddr, const uint8_t *incomingData, int len) {
  (void)macAddr;
#endif
  ShowduinoValidateResult vr = showduino_validate_desk_rx(incomingData, (size_t)len);
  if (vr != SHOWDUINO_VALID) {
    sRejected++;
    Serial.printf("[COMMS] ESP-NOW rejected packet (%d) len=%d\n", (int)vr, len);
    return;
  }

  ShowduinoDeskPacket packet = {};
  memcpy(&packet, incomingData, sizeof(packet));
  packet.command[SHOWDUINO_DESK_COMMAND_MAX - 1] = '\0';

  sRxCount++;
  sLastDirectorMs = millis();

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  if (recvInfo && recvInfo->src_addr) {
    memcpy(sDirectorMac, recvInfo->src_addr, 6);
    sHaveDirector = true;
    addDirectorPeer(sDirectorMac);
  }
#endif

  Serial.print("[COMMS] RX <- Director ");
#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  if (recvInfo && recvInfo->src_addr) espNowTransportPrintMac(recvInfo->src_addr);
#else
  Serial.print("(peer)");
#endif
  Serial.print(" seq=");
  Serial.print(packet.sequence);
  Serial.print(" cmd=");
  Serial.println(packet.command);

  if (sHandler) sHandler(packet.command);
}

bool espNowTransportBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  /* USB 5 V often cannot hold default TX power. ESP-NOW still works at 8.5 dBm
   * on the bench. Do not call esp_wifi_start() again — WiFi.mode() already did. */
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.disconnect(false, false);
  delay(150);
  if (esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("[COMMS] ESP-NOW channel set failed");
    Serial.flush();
  }
  delay(50);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[COMMS] ESP-NOW init failed");
    Serial.flush();
    sReady = false;
    return false;
  }

  esp_now_register_recv_cb(onEspNowReceive);
  sReady = true;
  return true;
}

bool espNowTransportReady() {
  return sReady;
}

void espNowTransportSetCommandHandler(ShowduinoDeskCommandFn fn) {
  sHandler = fn;
}

bool espNowTransportSendToDirector(const char *command) {
  if (!sReady || !sHaveDirector || !command || !command[0]) return false;

  ShowduinoDeskPacket packet = {};
  showduino_desk_packet_init(&packet, sTxSequence++, millis());
  if (showduino_desk_set_command(&packet, command) != 0) {
    sRejected++;
    Serial.println("[COMMS] P4 line too long for ESP-NOW desk packet; dropped");
    return false;
  }

  if (!addDirectorPeer(sDirectorMac)) return false;
  esp_err_t err = esp_now_send(sDirectorMac, (uint8_t *)&packet, sizeof(packet));
  if (err != ESP_OK) return false;
  sTxCount++;
  return true;
}

bool espNowTransportHaveDirector() {
  return sHaveDirector;
}

void espNowTransportDirectorMac(uint8_t out[6]) {
  if (!out) return;
  memcpy(out, sDirectorMac, 6);
}

uint32_t espNowTransportRxCount() { return sRxCount; }
uint32_t espNowTransportTxCount() { return sTxCount; }
uint32_t espNowTransportRejectedCount() { return sRejected; }
uint32_t espNowTransportLastDirectorMs() { return sLastDirectorMs; }
