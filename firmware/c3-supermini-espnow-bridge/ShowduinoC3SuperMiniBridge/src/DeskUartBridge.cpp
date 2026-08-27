#include "DeskUartBridge.h"
#include "../BoardConfig.h"
#include "DeviceManager.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "../../../protocol/showduino_desk_packet.h"
#include "../../../protocol/showduino_validation.h"
#include "../../../protocol/showduino_web_tunnel.h"

static bool sReady = false;
static bool sHaveDirector = false;
static uint8_t sDirectorMac[6] = {};
static uint16_t sTxSeq = 1;

static void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (i) Serial.print(':');
    if (mac[i] < 16) Serial.print('0');
    Serial.print(mac[i], HEX);
  }
}

static bool addPeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = SHOWDUINO_ESPNOW_CHANNEL;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  esp_err_t err = esp_now_add_peer(&peer);
  return err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST;
}

void deskUartBridgeSendToP4(const char *command) {
  if (!command || !command[0]) return;
  Serial1.println(command);
  Serial.print("[SUE] UART → P4: ");
  Serial.println(command);
}

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *src = (info && info->src_addr) ? info->src_addr : nullptr;
#else
static void onRecv(const uint8_t *src, const uint8_t *data, int len) {
#endif
  if (showduino_validate_desk_rx(data, (size_t)len) != SHOWDUINO_VALID) return;

  ShowduinoDeskPacket packet = {};
  memcpy(&packet, data, sizeof(packet));
  packet.command[SHOWDUINO_DESK_COMMAND_MAX - 1] = '\0';

  if (src) {
    memcpy(sDirectorMac, src, 6);
    sHaveDirector = true;
    addPeer(src);
  }

  Serial.print("[SUE] ESP-NOW ← Director cmd=");
  Serial.println(packet.command);
  deskUartBridgeSendToP4(packet.command);
}

void deskUartBridgeBegin() {
  /* SoftAP already owns channel 1. Calling esp_wifi_set_channel() again after
   * WiFi.softAP() stops AP beacons on ESP32-C3 AP_STA, so phones never see
   * Showduino-Studio. Keep PS off only. */
  esp_wifi_set_ps(WIFI_PS_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[SUE] ESP-NOW init failed — Director desk link unavailable");
    sReady = false;
    return;
  }

  esp_now_register_recv_cb(onRecv);
  sReady = true;
  Serial.print("[SUE] ESP-NOW desk bridge ready  MAC=");
  Serial.println(WiFi.macAddress());
}

void deskUartBridgeOnP4Line(const String &line) {
  if (line.length() == 0) return;

  /*
   * A real UART response is also the Stage Controller heartbeat for Web Studio.
   * Register it as the canonical IAN destination so Show/Scene commands can be
   * routed to the StageRuntimeBridge without inventing a second control path.
   */
  gDeviceManager.noteUartSighting(
      "ian", "Stage Controller", "ian",
      "SceneRuntime,Scheduler,RelayOutput,Lighting,AudioPlayback,PixelOutput,Temperature,Humidity,Logging");

  if (!sReady || !sHaveDirector) return;
  if (line.startsWith(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX)) return;

  ShowduinoDeskPacket packet = {};
  showduino_desk_packet_init(&packet, sTxSeq++, millis());
  if (showduino_desk_set_command(&packet, line.c_str()) != 0) return;
  if (!addPeer(sDirectorMac)) return;
  (void)esp_now_send(sDirectorMac, (uint8_t *)&packet, sizeof(packet));
}
