#include "ProtocolBridge.h"
#include "CommsUart.h"
#include "EspNowTransport.h"
#include "web/CommsWebTunnel.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_show_runtime.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_pixel_node.h"

#include <stdlib.h>
#include <string.h>

static bool sEmergencyActive = false;
static bool sEmergencyWasActive = false;

static bool sPingPending = false;
static bool sLastPingOk = false;
static uint32_t sPingDeadline = 0;

static bool sHadDirector = false;
static bool sHadP4 = false;
static bool sHadAudio = false;
static bool sHadLamp = false;
static bool sHadPixel = false;
static uint32_t sLastAudioSeenMs = 0;
static uint32_t sLastLampSeenMs = 0;
static uint32_t sLastPixelSeenMs = 0;
static char sLastAudioAnnounce[96] = "";
static char sLastLampAnnounce[96] = "";
static char sLastPixelAnnounce[96] = "";

static bool isLocalDiag(const char *line) {
  return line && (!strcmp(line, "DIAG:PING") || !strcmp(line, "DIAG:PONG"));
}

static bool isQuietDeskCmd(const char *command) {
  if (!command) return true;
  if (!strcmp(command, "HEARTBEAT") || !strcmp(command, "OK:HEARTBEAT") ||
      !strcmp(command, "HELLO") || !strcmp(command, "DIAG:PING") ||
      !strcmp(command, "DIAG:PONG")) {
    return true;
  }
  if (!strncmp(command, "TIME:", 5) && strcmp(command, "TIME:REQUEST") != 0) return true;
  if (!strncmp(command, "STATE:GATEWAY:", 14)) return true;
  if (!strncmp(command, "STATE:UPDATE:", 13)) return true;
  if (!strncmp(command, "STATE:", 6) || !strncmp(command, "SNAPSHOT:", 9)) return true;
  return false;
}

static bool isRoutineNodeCmd(const char *command) {
  if (!command) return true;
  if (!strncmp(command, "ANNOUNCE:", 9)) return true;
  if (!strncmp(command, "SOUND:STATUS", 12)) return true;
  if (!strncmp(command, "AUDIO:OWNED:", 12)) return true;
  if (!strncmp(command, "AUDIO:OWNER:", 12)) return true;
  if (!strncmp(command, "AUDIO:CAPS:", 11)) return true;
  if (!strncmp(command, "AUDIO:META:", 11)) return true;
  if (!strncmp(command, "AUDIO:INVENTORY:", 16)) return true;
  if (!strncmp(command, "LAMP:OWNED:", 11)) return true;
  if (!strncmp(command, "PIXEL:OWNED:", 12)) return true;
  if (!strncmp(command, "OWNED:", 6)) return true;
  if (!strncmp(command, "STATUS:", 7)) return true;
  return false;
}

static void parseAnnounceMeta(const char *command, char *mac, size_t macLen,
                              char *fw, size_t fwLen) {
  if (mac && macLen) {
    strncpy(mac, "-", macLen - 1);
    mac[macLen - 1] = '\0';
  }
  if (fw && fwLen) {
    strncpy(fw, "-", fwLen - 1);
    fw[fwLen - 1] = '\0';
  }
  if (!command || strncmp(command, "ANNOUNCE:", 9) != 0) return;
  const char *p = command + 9;
  if (strlen(p) >= 17 && mac && macLen > 17) {
    memcpy(mac, p, 17);
    mac[17] = '\0';
    p += 17;
    if (*p == ':') p++;
    const char *colon = strchr(p, ':');
    size_t n = colon ? (size_t)(colon - p) : strlen(p);
    if (fw && fwLen) {
      if (n >= fwLen) n = fwLen - 1;
      memcpy(fw, p, n);
      fw[n] = '\0';
    }
  }
}

static void noteAudioDiscovery(const char *command) {
  if (!command || strncmp(command, "ANNOUNCE:", 9) != 0) return;
  if (!showduino_log_changed(sLastAudioAnnounce, sizeof(sLastAudioAnnounce), command)) {
    return;
  }
  char mac[24];
  char fw[16];
  parseAnnounceMeta(command, mac, sizeof(mac), fw, sizeof(fw));
  SD_LOGI("COMMS", "Audio Node discovered");
  SD_LOGI("COMMS", "  FW: %s", fw);
  SD_LOGI("COMMS", "  MAC: %s", mac);
}

static void noteLampDiscovery(const char *command) {
  if (!command || strncmp(command, "ANNOUNCE:", 9) != 0) return;
  if (!showduino_log_changed(sLastLampAnnounce, sizeof(sLastLampAnnounce), command)) {
    return;
  }
  char mac[24];
  char fw[16];
  parseAnnounceMeta(command, mac, sizeof(mac), fw, sizeof(fw));
  SD_LOGI("COMMS", "Lamp Node discovered");
  SD_LOGI("COMMS", "  FW: %s", fw);
  SD_LOGI("COMMS", "  MAC: %s", mac);
}

static void notePixelDiscovery(const char *command) {
  if (!command || strncmp(command, "ANNOUNCE:", 9) != 0) return;
  if (!showduino_log_changed(sLastPixelAnnounce, sizeof(sLastPixelAnnounce), command)) {
    return;
  }
  ShowduinoPixelAnnounce an{};
  if (showduino_pixel_parse_announce(command, &an)) {
    SD_LOGI("COMMS", "Pixel Node discovered ID=%s", an.id[0] ? an.id : "-");
    SD_LOGI("COMMS", "  FW: %s", an.firmware[0] ? an.firmware : "-");
    SD_LOGI("COMMS", "  MAC: %s", an.mac[0] ? an.mac : "-");
  }
}

static void forwardToAudioNode(const char *command, uint32_t sequence) {
  if (!command || !command[0]) return;
  if (espNowTransportSendToAudioNode(command, sequence)) {
    SD_LOGT("COMMS", "TX -> Audio seq=%lu cmd=%s", (unsigned long)sequence, command);
    if (!isRoutineNodeCmd(command) &&
        strncmp(command, "AUDIO:NODE:STATUS", 17) != 0 &&
        strncmp(command, "AUDIO:NODE:OWN:GRANT", 20) != 0) {
      SD_LOGD("COMMS", "P4 -> Audio: %s", command);
    }
  } else {
    static uint32_t sHoldMs = 0;
    if (showduino_log_rate_ok(&sHoldMs, millis(), 5000UL)) {
      SD_LOGW("COMMS", "Audio Node route held — no peer yet");
    }
  }
}

static void forwardToLampNode(const char *command, uint32_t sequence) {
  if (!command || !command[0]) return;
  if (espNowTransportSendToLampNode(command, sequence)) {
    SD_LOGT("COMMS", "TX -> Lamp seq=%lu cmd=%s", (unsigned long)sequence, command);
    if (!isRoutineNodeCmd(command) &&
        strncmp(command, "LAMP:STATUS", 11) != 0 &&
        strncmp(command, "LAMP:NODE:OWN:GRANT", 19) != 0 &&
        strncmp(command, "LAMP:OWN:GRANT", 14) != 0) {
      SD_LOGD("COMMS", "P4 -> Lamp: %s", command);
    }
  } else {
    static uint32_t sHoldMs = 0;
    if (showduino_log_rate_ok(&sHoldMs, millis(), 5000UL)) {
      SD_LOGW("COMMS", "Lamp Node route held — no peer yet");
    }
  }
}

static void forwardToPixelNode(const char *id, const char *command, uint32_t sequence) {
  if (!id || !command || !command[0]) return;
  if (espNowTransportSendToPixelNode(id, command, sequence)) {
    SD_LOGT("COMMS", "TX -> Pixel %s seq=%lu cmd=%s", id, (unsigned long)sequence, command);
    if (!isRoutineNodeCmd(command) &&
        strncmp(command, "PIXEL:STATUS", 12) != 0 &&
        strncmp(command, "PIXEL:OWN:GRANT", 15) != 0) {
      SD_LOGD("COMMS", "P4 -> Pixel %s: %s", id, command);
    }
  } else {
    static uint32_t sHoldMs = 0;
    if (showduino_log_rate_ok(&sHoldMs, millis(), 5000UL)) {
      SD_LOGW("COMMS", "Pixel Node %s route held — no peer yet", id);
    }
  }
}

static void onNodeCommand(const char *nodeType, const char *command, uint32_t sequence) {
  if (!nodeType || !command) return;
  if (!strcmp(nodeType, SHOWDUINO_LEGACY_NODETYPE_AUDIO)) {
    sLastAudioSeenMs = millis();
    if (!sHadAudio) sHadAudio = true;
    noteAudioDiscovery(command);
  } else if (!strcmp(nodeType, SHOWDUINO_LEGACY_NODETYPE_LAMP)) {
    sLastLampSeenMs = millis();
    if (!sHadLamp) sHadLamp = true;
    noteLampDiscovery(command);
  } else if (!strcmp(nodeType, SHOWDUINO_LEGACY_NODETYPE_PIXEL)) {
    sLastPixelSeenMs = millis();
    if (!sHadPixel) sHadPixel = true;
    notePixelDiscovery(command);
  }
  char line[SHOWDUINO_COMMS_LINE_MAX + 1];
  snprintf(line, sizeof(line), "NODE:%s:%s", nodeType, command);
  commsUartWriteLine(line);
  SD_LOGT("COMMS", "TX -> P4: %s seq=%lu", line, (unsigned long)sequence);
}

static bool handleP4Route(const char *line) {
  if (!line) return false;

  if (!strncmp(line, SHOWDUINO_LEGACY_ROUTE_AUDIO,
               strlen(SHOWDUINO_LEGACY_ROUTE_AUDIO))) {
    const char *rest = line + strlen(SHOWDUINO_LEGACY_ROUTE_AUDIO);
    uint32_t seq = 0;
    const char *cmd = rest;
    if (rest[0] >= '0' && rest[0] <= '9') {
      char *end = nullptr;
      seq = (uint32_t)strtoul(rest, &end, 10);
      if (end && *end == ':') cmd = end + 1;
    }
    forwardToAudioNode(cmd, seq);
    return true;
  }

  if (!strncmp(line, SHOWDUINO_LEGACY_ROUTE_LAMP,
               strlen(SHOWDUINO_LEGACY_ROUTE_LAMP))) {
    const char *rest = line + strlen(SHOWDUINO_LEGACY_ROUTE_LAMP);
    uint32_t seq = 0;
    const char *cmd = rest;
    if (rest[0] >= '0' && rest[0] <= '9') {
      char *end = nullptr;
      seq = (uint32_t)strtoul(rest, &end, 10);
      if (end && *end == ':') cmd = end + 1;
    }
    forwardToLampNode(cmd, seq);
    return true;
  }

  if (!strncmp(line, SHOWDUINO_LEGACY_ROUTE_PIXEL,
               strlen(SHOWDUINO_LEGACY_ROUTE_PIXEL))) {
    ShowduinoPixelRoute rt{};
    if (showduino_pixel_parse_route(line, &rt)) {
      forwardToPixelNode(rt.id, rt.command, rt.sequence);
      return true;
    }
  }

  return false;
}

static void fanoutEmergency() {
  if (sEmergencyActive == sEmergencyWasActive) return;
  sEmergencyWasActive = sEmergencyActive;
  SD_LOG_EMERGENCY("COMMS", sEmergencyActive);
  forwardToAudioNode(sEmergencyActive ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR", 0);
  forwardToLampNode(sEmergencyActive ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR", 0);
  espNowTransportSendToAllPixelNodes(sEmergencyActive ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR", 0);
}

static void onDirectorCommand(const char *command) {
  if (!command || !command[0]) return;
  if (isLocalDiag(command)) {
    SD_LOGD("COMMS", "DIAG from Director ignored (local UART only)");
    return;
  }
  if (!sHadDirector) {
    sHadDirector = true;
    SD_LOGI("COMMS", "Director online");
  }
  commsUartWriteLine(command);
  if (isQuietDeskCmd(command)) {
    SD_LOGT("COMMS", "TX -> P4: %s", command);
  } else {
    SD_LOGD("COMMS", "Director -> P4: %s", command);
    SD_LOGT("COMMS", "TX -> P4: %s", command);
  }
}

static void observeP4Emergency(const char *line) {
  if (!line || !line[0]) return;

  const ShowduinoEmergencyWire em = showduino_parse_state_emergency(line);
  if (em == SHOWDUINO_EMERGENCY_WIRE_ACTIVE) {
    sEmergencyActive = true;
    return;
  }
  if (em == SHOWDUINO_EMERGENCY_WIRE_CLEAR) {
    sEmergencyActive = false;
    return;
  }

  if (!strncmp(line, SHOW_STATE_WIRE_PREFIX, strlen(SHOW_STATE_WIRE_PREFIX))) {
    const char *name = line + strlen(SHOW_STATE_WIRE_PREFIX);
    if (!strcmp(name, "EMERGENCY_STOP") || !strcmp(name, "EMERGENCY")) {
      sEmergencyActive = true;
    }
    return;
  }

  const ShowduinoShowRuntimeWire show = showduino_parse_state_show(line);
  if (show == SHOWDUINO_SHOW_WIRE_EMERGENCY) {
    sEmergencyActive = true;
    return;
  }

  if (!strcmp(line, SHOWDUINO_LEGACY_STATUS_ELOCKED)) {
    sEmergencyActive = true;
    return;
  }
  if (!strcmp(line, SHOWDUINO_LEGACY_STATUS_ECLEARED) ||
      !strcmp(line, SHOWDUINO_LEGACY_EMERGENCY_CLEAR_OK)) {
    sEmergencyActive = false;
  }
}

void protocolBridgeBegin() {
  commsUartBegin();
  espNowTransportSetCommandHandler(onDirectorCommand);
  espNowTransportSetNodeHandler(onNodeCommand);
  sEmergencyActive = false;
  sEmergencyWasActive = false;
  sHadDirector = false;
  sHadP4 = false;
  sHadAudio = false;
  sHadLamp = false;
  sLastAudioSeenMs = 0;
  sLastLampSeenMs = 0;
  sLastAudioAnnounce[0] = '\0';
  sLastLampAnnounce[0] = '\0';
}

static void consumeWebTunnelBody() {
  while (commsUartAvailable() > 0 && commsWebTunnelConsumingBytes()) {
    const int c = commsUartRead();
    if (c < 0) break;
    commsWebTunnelOnByte((char)c);
  }
}

static void servicePresence() {
  const uint32_t now = millis();
  const bool p4 = protocolBridgeP4Alive();
  if (p4 && !sHadP4) {
    sHadP4 = true;
    SD_LOGI("COMMS", "P4 connected");
  } else if (!p4 && sHadP4) {
    sHadP4 = false;
    SD_LOGW("COMMS", "P4 lost");
  }

  const bool director = protocolBridgeDirectorOnline();
  if (director && !sHadDirector) {
    sHadDirector = true;
    SD_LOGI("COMMS", "Director online");
  } else if (!director && sHadDirector && espNowTransportHaveDirector()) {
    /* Keep "seen" peer; only warn on stale after timeout via Online check */
    static uint32_t sDirWarnMs = 0;
    if (showduino_log_rate_ok(&sDirWarnMs, now, 10000UL)) {
      SD_LOGW("COMMS", "Director offline");
    }
    sHadDirector = false;
  }

  if (sHadAudio && sLastAudioSeenMs &&
      (now - sLastAudioSeenMs) > SHOWDUINO_COMMS_LINK_TIMEOUT_MS) {
    sHadAudio = false;
    sLastAudioAnnounce[0] = '\0';
    SD_LOGW("COMMS", "Audio Node lost");
  }
  if (sHadLamp && sLastLampSeenMs &&
      (now - sLastLampSeenMs) > SHOWDUINO_COMMS_LINK_TIMEOUT_MS) {
    sHadLamp = false;
    sLastLampAnnounce[0] = '\0';
    SD_LOGW("COMMS", "Lamp Node lost");
  }
  if (sHadPixel && sLastPixelSeenMs &&
      (now - sLastPixelSeenMs) > SHOWDUINO_COMMS_LINK_TIMEOUT_MS) {
    sHadPixel = false;
    sLastPixelAnnounce[0] = '\0';
    SD_LOGW("COMMS", "Pixel Node lost");
  }
}

void protocolBridgeLoop() {
  servicePresence();

  if (commsWebTunnelConsumingBytes()) {
    consumeWebTunnelBody();
    return;
  }

  char line[SHOWDUINO_COMMS_LINE_MAX + 1];
  while (!commsWebTunnelConsumingBytes() && commsUartReadLine(line, sizeof(line))) {
    if (commsWebTunnelOnLine(line)) {
      consumeWebTunnelBody();
      break;
    }
    if (!strncmp(line, SHOWDUINO_WEB_TUNNEL_RESP_PREFIX,
                 strlen(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX))) {
      continue;
    }
    if (!strcmp(line, "DIAG:PING")) {
      SD_LOGT("COMMS", "UART RX: DIAG:PING");
      commsUartWriteLine("DIAG:PONG");
      SD_LOGT("COMMS", "UART TX: DIAG:PONG");
      continue;
    }
    if (!strcmp(line, "DIAG:PONG")) {
      SD_LOGT("COMMS", "UART RX: DIAG:PONG");
      if (sPingPending) {
        sPingPending = false;
        sLastPingOk = true;
        SD_LOGI("COMMS", "PING:P4 OK");
      }
      continue;
    }

    SD_LOGT("COMMS", "UART RX: %s", line);
    observeP4Emergency(line);
    fanoutEmergency();
    if (handleP4Route(line)) {
      continue;
    }
    if (espNowTransportHaveDirector()) {
      if (espNowTransportSendToDirector(line)) {
        if (isQuietDeskCmd(line)) {
          SD_LOGT("COMMS", "TX -> Director: %s", line);
        } else {
          SD_LOGT("COMMS", "TX -> Director: %s", line);
        }
      }
    } else {
      static uint32_t sHoldDirMs = 0;
      if (showduino_log_rate_ok(&sHoldDirMs, millis(), 5000UL)) {
        SD_LOGW("COMMS", "P4 line held — no Director peer yet");
      }
    }
  }

  if (sPingPending && (int32_t)(millis() - sPingDeadline) >= 0) {
    sPingPending = false;
    sLastPingOk = false;
    SD_LOGW("COMMS", "PING:P4 timeout — no DIAG:PONG");
  }
}

bool protocolBridgePingP4() {
  if (!commsUartReady()) {
    SD_LOGE("COMMS", "PING:P4 failed — UART not ready");
    return false;
  }
  sPingPending = true;
  sLastPingOk = false;
  sPingDeadline = millis() + SHOWDUINO_COMMS_PING_TIMEOUT_MS;
  commsUartWriteLine("DIAG:PING");
  SD_LOGT("COMMS", "UART TX: DIAG:PING");
  return true;
}

bool protocolBridgePingPending() { return sPingPending; }
bool protocolBridgeLastPingOk() { return sLastPingOk; }

bool protocolBridgeP4Alive() {
  if (!commsUartEverRx()) return false;
  return (millis() - commsUartLastRxMs()) < SHOWDUINO_COMMS_LINK_TIMEOUT_MS;
}

bool protocolBridgeDirectorOnline() {
  if (!espNowTransportHaveDirector()) return false;
  uint32_t last = espNowTransportLastDirectorMs();
  if (last == 0) return false;
  return (millis() - last) < SHOWDUINO_COMMS_LINK_TIMEOUT_MS;
}

bool protocolBridgeEmergencyActive() {
  if (!protocolBridgeP4Alive()) return false;
  return sEmergencyActive;
}
