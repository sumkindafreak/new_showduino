#include "EspNowNodeTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_node_packet.h"
#include "../../../protocol/showduino_validation.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_radio_follow.h"
#include "../../shared-node/NodeSoftAp.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <string.h>

static bool sReady = false;
static bool sHaveComms = false;
static bool sLoggedComms = false;
static uint8_t sCommsMac[6] = {0};
static uint8_t sSelfMac[6] = {0};
static uint32_t sRx = 0;
static uint32_t sTx = 0;
static uint32_t sRej = 0;
static uint32_t sLastRx = 0;
static uint32_t sSendFail = 0;
static uint32_t sLastService = 0;
static AudioNodeRxFn sHandler = nullptr;
static ShowduinoRadioFollow sFollow;
static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool addPeer(const uint8_t *mac) {
  if (!mac) return false;
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  wifi_mode_t mode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&mode);
  peer.ifidx = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) ? WIFI_IF_AP : WIFI_IF_STA;
  const esp_err_t err = esp_now_add_peer(&peer);
  if (err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) return true;
  peer.ifidx = (peer.ifidx == WIFI_IF_AP) ? WIFI_IF_STA : WIFI_IF_AP;
  const esp_err_t err2 = esp_now_add_peer(&peer);
  return err2 == ESP_OK || err2 == ESP_ERR_ESPNOW_EXIST;
}

static void lockChannel() {
  sFollow.lock(sFollow.channel());
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
    const bool first = !sHaveComms;
    memcpy(sCommsMac, src, 6);
    sHaveComms = true;
    addPeer(sCommsMac);
    if (first || !sLoggedComms) {
      sLoggedComms = true;
      SD_LOGI("AUDIO", "Comms connected");
    }
  }
  sRx++;
  sLastRx = millis();
  SD_LOGT("AUDIO", "RX seq=%lu cmd=%s", (unsigned long)pkt.sequence, pkt.command);
  if (sHandler) sHandler(pkt.command, pkt.sequence);
}

static bool initEspNow() {
  if (esp_now_init() != ESP_OK) {
    SD_LOGE("ESPNOW", "init failed");
    sReady = false;
    return false;
  }
  esp_now_register_recv_cb(onRx);
  addPeer(kBroadcast);
  if (sHaveComms) addPeer(sCommsMac);
  sReady = true;
  return true;
}

void audioEspNowReassert() {
  lockChannel();
  if (!sReady) return;
  addPeer(kBroadcast);
  if (sHaveComms) addPeer(sCommsMac);
}

void audioEspNowRecover() {
  /* Do not deinit ESP-NOW. Tearing down the radio on the node does not repair
   * Comms, and SoftAP bring-up used to call this hook on every Audio Node
   * power-on. Reassert channel and peers only. */
  audioEspNowReassert();
}

void audioEspNowService() {
  const uint32_t now = millis();
  if ((now - sLastService) < 400UL) return;
  sLastService = now;
  const bool haveLink = sHaveComms && sLastRx &&
      (now - sLastRx) < SHOWDUINO_RADIO_LINK_FRESH_MS;
  sFollow.tick(haveLink);
  if (nodeSoftApStarted()) nodeSoftApFollowChannel(sFollow.channel());
}

bool audioEspNowBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  delay(80);
  sFollow.begin(SHOWDUINO_RADIO_HOME_CHANNEL);
  sFollow.lock(SHOWDUINO_RADIO_HOME_CHANNEL);
  memset(sSelfMac, 0, 6);
  esp_read_mac(sSelfMac, ESP_MAC_WIFI_STA);
  if (!initEspNow()) return false;
  SD_LOGI("ESPNOW", "ready (STA, channel locked)");
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
  if (esp_now_send(dest, (const uint8_t *)&pkt, sizeof(pkt)) != ESP_OK) {
    sSendFail++;
    addPeer(kBroadcast);
    if (esp_now_send(kBroadcast, (const uint8_t *)&pkt, sizeof(pkt)) != ESP_OK) {
      if (sSendFail >= 8) {
        sSendFail = 0;
        audioEspNowReassert();
      }
      return false;
    }
  }
  sSendFail = 0;
  sTx++;
  return true;
}

void audioEspNowMacString(char *out, size_t n) {
  if (!out || n < 18) return;
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
           sSelfMac[0], sSelfMac[1], sSelfMac[2], sSelfMac[3], sSelfMac[4], sSelfMac[5]);
}

void audioEspNowMacBytes(uint8_t out[6]) {
  if (!out) return;
  memcpy(out, sSelfMac, 6);
}

uint32_t audioEspNowRxCount() { return sRx; }
uint32_t audioEspNowTxCount() { return sTx; }
uint32_t audioEspNowRejected() { return sRej; }
uint32_t audioEspNowLastRxMs() { return sLastRx; }
bool audioEspNowHaveComms() { return sHaveComms; }
