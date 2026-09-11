#include "EspNowTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_pixel_node.h"

#include <stdio.h>
#include <string.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>

static bool sReady = false;
static bool sHaveDirector = false;
static bool sHaveAudioNode = false;
static bool sHaveLampNode = false;
static uint8_t sDirectorMac[6] = {0};
static uint8_t sAudioNodeMac[6] = {0};
static uint8_t sLampNodeMac[6] = {0};

struct CommsPixelPeer {
  bool have;
  uint8_t mac[6];
  char id[SHOWDUINO_PIXEL_ID_MAX + 1];
  uint32_t lastMs;
};
static CommsPixelPeer sPixel[SHOWDUINO_PIXEL_NODE_MAX_NODES];
static uint16_t sTxSequence = 1;
static uint32_t sRxCount = 0;
static uint32_t sTxCount = 0;
static uint32_t sRejected = 0;
static uint32_t sLastDirectorMs = 0;
static uint32_t sLastAudioNodeMs = 0;
static uint32_t sLastLampNodeMs = 0;
static uint32_t sRejectLogMs = 0;
static ShowduinoDeskCommandFn sHandler = nullptr;
static ShowduinoNodeCommandFn sNodeHandler = nullptr;

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

static wifi_interface_t commsEspNowIf() {
  wifi_mode_t mode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&mode);
  if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) return WIFI_IF_AP;
  return WIFI_IF_STA;
}

static bool addPeer(const uint8_t *mac) {
  if (!mac) return false;
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  /* Channel 0 = current radio channel. Hard-coding channel 1 while SoftAP is
   * up on AP+STA is a known ESP-NOW failure mode that can drop the Director. */
  peer.channel = 0;
  peer.encrypt = false;
  peer.ifidx = commsEspNowIf();
  esp_err_t err = esp_now_add_peer(&peer);
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
    peer.ifidx = (peer.ifidx == WIFI_IF_AP) ? WIFI_IF_STA : WIFI_IF_AP;
    err = esp_now_add_peer(&peer);
  }
  if (err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) return true;
  Serial.printf("[ESPNOW] addPeer failed err=%d ifidx=%d\n", (int)err, (int)peer.ifidx);
  return false;
}

static void notePixelPeer(const uint8_t *mac, const char *command) {
  if (!mac) return;
  char id[SHOWDUINO_PIXEL_ID_MAX + 1] = "";
  if (command && !strncmp(command, "ANNOUNCE:", 9)) {
    ShowduinoPixelAnnounce an{};
    if (showduino_pixel_parse_announce(command, &an) && an.id[0]) {
      strncpy(id, an.id, sizeof(id) - 1);
    }
  }
  CommsPixelPeer *slot = nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sPixel[i].have && memcmp(sPixel[i].mac, mac, 6) == 0) {
      slot = &sPixel[i];
      break;
    }
  }
  if (!slot && id[0]) {
    for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
      if (sPixel[i].have && showduino_pixel_id_equal(sPixel[i].id, id)) {
        slot = &sPixel[i];
        break;
      }
    }
  }
  if (!slot) {
    for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
      if (!sPixel[i].have) {
        slot = &sPixel[i];
        break;
      }
    }
  }
  if (slot) {
    memcpy(slot->mac, mac, 6);
    slot->have = true;
    slot->lastMs = millis();
    if (id[0]) strncpy(slot->id, id, sizeof(slot->id) - 1);
  }
  addPeer(mac);
}

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void onEspNowReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
#else
static void onEspNowReceive(const uint8_t *macAddr, const uint8_t *incomingData, int len) {
  (void)macAddr;
#endif
  if ((size_t)len == SHOWDUINO_NODE_PACKET_SIZE_EXPECTED) {
    ShowduinoValidateResult nvr = showduino_validate_node_rx(incomingData, (size_t)len);
    if (nvr != SHOWDUINO_VALID) {
      sRejected++;
      if (showduino_log_rate_ok(&sRejectLogMs, millis(), SHOWDUINO_LOG_WARN_INTERVAL_MS)) {
        SD_LOGW("COMMS", "ESP-NOW node rejected (%d) len=%d", (int)nvr, len);
      } else {
        SD_LOGT("COMMS", "ESP-NOW node rejected (%d) len=%d", (int)nvr, len);
      }
      return;
    }
    ShowduinoNodePacket np = {};
    memcpy(&np, incomingData, sizeof(np));
    np.nodeType[SHOWDUINO_NODE_TYPE_MAX - 1] = '\0';
    np.command[SHOWDUINO_NODE_COMMAND_MAX - 1] = '\0';
    sRxCount++;
#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (recvInfo && recvInfo->src_addr) {
      if (strcmp(np.nodeType, SHOWDUINO_LEGACY_NODETYPE_AUDIO) == 0) {
        memcpy(sAudioNodeMac, recvInfo->src_addr, 6);
        sHaveAudioNode = true;
        sLastAudioNodeMs = millis();
        addPeer(sAudioNodeMac);
      } else if (strcmp(np.nodeType, SHOWDUINO_LEGACY_NODETYPE_LAMP) == 0) {
        memcpy(sLampNodeMac, recvInfo->src_addr, 6);
        sHaveLampNode = true;
        sLastLampNodeMs = millis();
        addPeer(sLampNodeMac);
      } else if (strcmp(np.nodeType, SHOWDUINO_LEGACY_NODETYPE_PIXEL) == 0) {
        notePixelPeer(recvInfo->src_addr, np.command);
      }
    }
#else
    if (strcmp(np.nodeType, SHOWDUINO_LEGACY_NODETYPE_AUDIO) == 0) {
      sLastAudioNodeMs = millis();
    } else if (strcmp(np.nodeType, SHOWDUINO_LEGACY_NODETYPE_LAMP) == 0) {
      sLastLampNodeMs = millis();
    } else if (strcmp(np.nodeType, SHOWDUINO_LEGACY_NODETYPE_PIXEL) == 0) {
      notePixelPeer(macAddr, np.command);
    }
#endif
    SD_LOGT("COMMS", "RX <- Node %s seq=%lu cmd=%s",
            np.nodeType, (unsigned long)np.sequence, np.command);
    if (sNodeHandler) sNodeHandler(np.nodeType, np.command, np.sequence);
    return;
  }

  ShowduinoValidateResult vr = showduino_validate_desk_rx(incomingData, (size_t)len);
  if (vr != SHOWDUINO_VALID) {
    sRejected++;
    if (showduino_log_rate_ok(&sRejectLogMs, millis(), SHOWDUINO_LOG_WARN_INTERVAL_MS)) {
      SD_LOGW("COMMS", "ESP-NOW desk rejected (%d) len=%d", (int)vr, len);
    } else {
      SD_LOGT("COMMS", "ESP-NOW desk rejected (%d) len=%d", (int)vr, len);
    }
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
    addPeer(sDirectorMac);
  }
#endif

  SD_LOGT("COMMS", "RX <- Director seq=%u cmd=%s",
          (unsigned)packet.sequence, packet.command);

  if (sHandler) sHandler(packet.command);
}

bool espNowTransportBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  /* USB 5 V often cannot hold default TX power. ESP-NOW still works at 8.5 dBm
   * on the bench. Do not call esp_wifi_start() again — WiFi.mode() already did. */
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.disconnect(false, false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  delay(150);
  if (esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    SD_LOGE("COMMS", "ESP-NOW channel set failed");
    Serial.flush();
  }
  delay(50);

  if (esp_now_init() != ESP_OK) {
    SD_LOGE("COMMS", "ESP-NOW init failed");
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

void espNowTransportSetNodeHandler(ShowduinoNodeCommandFn fn) {
  sNodeHandler = fn;
}

bool espNowTransportSendToDirector(const char *command) {
  if (!sReady || !sHaveDirector || !command || !command[0]) return false;

  ShowduinoDeskPacket packet = {};
  showduino_desk_packet_init(&packet, sTxSequence++, millis());
  if (showduino_desk_set_command(&packet, command) != 0) {
    sRejected++;
    SD_LOGW("COMMS", "P4 line too long for ESP-NOW desk packet; dropped");
    return false;
  }

  if (!addPeer(sDirectorMac)) return false;
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

bool espNowTransportSendToAudioNode(const char *command, uint32_t sequence) {
  if (!sReady || !sHaveAudioNode || !command || !command[0]) return false;
  ShowduinoNodePacket packet = {};
  showduino_node_packet_init(&packet, SHOWDUINO_LEGACY_NODETYPE_AUDIO, sequence);
  if (showduino_node_set_command(&packet, command) != 0) {
    sRejected++;
    return false;
  }
  if (!addPeer(sAudioNodeMac)) return false;
  if (esp_now_send(sAudioNodeMac, (uint8_t *)&packet, sizeof(packet)) != ESP_OK) return false;
  sTxCount++;
  return true;
}

bool espNowTransportHaveAudioNode() { return sHaveAudioNode; }

void espNowTransportAudioNodeMac(uint8_t out[6]) {
  if (!out) return;
  memcpy(out, sAudioNodeMac, 6);
}

bool espNowTransportSendToLampNode(const char *command, uint32_t sequence) {
  if (!sReady || !sHaveLampNode || !command || !command[0]) return false;
  ShowduinoNodePacket packet = {};
  showduino_node_packet_init(&packet, SHOWDUINO_LEGACY_NODETYPE_LAMP, sequence);
  if (showduino_node_set_command(&packet, command) != 0) {
    sRejected++;
    return false;
  }
  if (!addPeer(sLampNodeMac)) return false;
  if (esp_now_send(sLampNodeMac, (uint8_t *)&packet, sizeof(packet)) != ESP_OK) return false;
  sTxCount++;
  return true;
}

bool espNowTransportHaveLampNode() { return sHaveLampNode; }

void espNowTransportLampNodeMac(uint8_t out[6]) {
  if (!out) return;
  memcpy(out, sLampNodeMac, 6);
}

bool espNowTransportSendToPixelNode(const char *id, const char *command, uint32_t sequence) {
  if (!sReady || !id || !id[0] || !command || !command[0]) return false;
  CommsPixelPeer *slot = nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sPixel[i].have && showduino_pixel_id_equal(sPixel[i].id, id)) {
      slot = &sPixel[i];
      break;
    }
  }
  if (!slot) return false;
  ShowduinoNodePacket packet = {};
  showduino_node_packet_init(&packet, SHOWDUINO_LEGACY_NODETYPE_PIXEL, sequence);
  if (showduino_node_set_command(&packet, command) != 0) {
    sRejected++;
    return false;
  }
  if (!addPeer(slot->mac)) return false;
  if (esp_now_send(slot->mac, (uint8_t *)&packet, sizeof(packet)) != ESP_OK) return false;
  sTxCount++;
  return true;
}

void espNowTransportSendToAllPixelNodes(const char *command, uint32_t sequence) {
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (!sPixel[i].have) continue;
    if (sPixel[i].id[0]) {
      (void)espNowTransportSendToPixelNode(sPixel[i].id, command, sequence);
    } else {
      ShowduinoNodePacket packet = {};
      showduino_node_packet_init(&packet, SHOWDUINO_LEGACY_NODETYPE_PIXEL, sequence);
      if (showduino_node_set_command(&packet, command) != 0) continue;
      if (!addPeer(sPixel[i].mac)) continue;
      if (esp_now_send(sPixel[i].mac, (uint8_t *)&packet, sizeof(packet)) == ESP_OK) sTxCount++;
    }
  }
}

bool espNowTransportHavePixelNode() {
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sPixel[i].have) return true;
  }
  return false;
}

uint8_t espNowTransportPixelNodeCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sPixel[i].have) n++;
  }
  return n;
}

uint32_t espNowTransportRxCount() { return sRxCount; }
uint32_t espNowTransportTxCount() { return sTxCount; }
uint32_t espNowTransportRejectedCount() { return sRejected; }
uint32_t espNowTransportLastDirectorMs() { return sLastDirectorMs; }
uint32_t espNowTransportLastAudioNodeMs() { return sLastAudioNodeMs; }
uint32_t espNowTransportLastLampNodeMs() { return sLastLampNodeMs; }
