#include "EspNowNodeTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_node_packet.h"
#include "../../../protocol/showduino_validation.h"
#include "../../../protocol/showduino_legacy_strings.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>

static bool sReady = false;
static bool sHaveComms = false;
static uint8_t sCommsMac[6] = {0};
static uint8_t sSelfMac[6] = {0};
static uint32_t sRx = 0;
static uint32_t sTx = 0;
static uint32_t sRej = 0;
static uint32_t sLastRx = 0;
static AudioNodeRxFn sHandler = nullptr;
static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool addPeer(const uint8_t *mac) {
  if (!mac) return false;
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = SHOWDUINO_ESPNOW_CHANNEL;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  const esp_err_t err = esp_now_add_peer(&peer);
  return err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST;
}

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void onRx(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
static void onRx(const uint8_t *macAddr, const uint8_t *data, int len) {
  const uint8_t *src = macAddr;
#endif
  if (showduino_validate_node_rx(data, (size_t)len) != SHOWDUINO_VALID) {
    sRej++;
    return;
  }
  ShowduinoNodePacket pkt = {};
  memcpy(&pkt, data, sizeof(pkt));
  pkt.nodeType[SHOWDUINO_NODE_TYPE_MAX - 1] = '\0';
  pkt.command[SHOWDUINO_NODE_COMMAND_MAX - 1] = '\0';
  if (strcmp(pkt.nodeType, SHOWDUINO_LEGACY_NODETYPE_AUDIO) != 0) {
    sRej++;
    return;
  }

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  const uint8_t *src = (info && info->src_addr) ? info->src_addr : nullptr;
#endif
  if (src) {
    memcpy(sCommsMac, src, 6);
    sHaveComms = true;
    addPeer(sCommsMac);
  }
  sRx++;
  sLastRx = millis();
  if (sHandler) sHandler(pkt.command, pkt.sequence);
}

bool audioEspNowBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  delay(80);
  if (esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("[ESPNOW] channel set failed");
  }
  memset(sSelfMac, 0, 6);
  esp_read_mac(sSelfMac, ESP_MAC_WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESPNOW] init failed");
    sReady = false;
    return false;
  }
  addPeer(kBroadcast);
  esp_now_register_recv_cb(onRx);
  sReady = true;
  Serial.println("[ESPNOW] ready");
  return true;
}

bool audioEspNowReady() { return sReady; }
void audioEspNowSetHandler(AudioNodeRxFn fn) { sHandler = fn; }

bool audioEspNowSend(const char *command, uint32_t sequence) {
  if (!sReady || !command || !command[0]) return false;
  ShowduinoNodePacket pkt = {};
  showduino_node_packet_init(&pkt, SHOWDUINO_LEGACY_NODETYPE_AUDIO, sequence);
  if (showduino_node_set_command(&pkt, command) != 0) {
    sRej++;
    return false;
  }
  const uint8_t *dest = sHaveComms ? sCommsMac : kBroadcast;
  if (!addPeer(dest)) return false;
  if (esp_now_send(dest, (const uint8_t *)&pkt, sizeof(pkt)) != ESP_OK) return false;
  sTx++;
  return true;
}

void audioEspNowMacString(char *out, size_t n) {
  if (!out || n < 18) return;
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
           sSelfMac[0], sSelfMac[1], sSelfMac[2], sSelfMac[3], sSelfMac[4], sSelfMac[5]);
}

uint32_t audioEspNowRxCount() { return sRx; }
uint32_t audioEspNowTxCount() { return sTx; }
uint32_t audioEspNowRejected() { return sRej; }
uint32_t audioEspNowLastRxMs() { return sLastRx; }
bool audioEspNowHaveComms() { return sHaveComms; }
