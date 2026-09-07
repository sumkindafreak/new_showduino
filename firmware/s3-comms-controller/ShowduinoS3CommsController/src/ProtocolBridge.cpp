#include "ProtocolBridge.h"
#include "CommsUart.h"
#include "EspNowTransport.h"
#include "web/CommsWebTunnel.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_show_runtime.h"
#include "../../../protocol/showduino_legacy_strings.h"

#include <stdlib.h>
#include <string.h>

static bool sEmergencyActive = false;
static bool sEmergencyWasActive = false;

static bool sPingPending = false;
static bool sLastPingOk = false;
static uint32_t sPingDeadline = 0;

static bool isLocalDiag(const char *line) {
  return line && (!strcmp(line, "DIAG:PING") || !strcmp(line, "DIAG:PONG"));
}

static void forwardToAudioNode(const char *command, uint32_t sequence) {
  if (!command || !command[0]) return;
  if (espNowTransportSendToAudioNode(command, sequence)) {
    Serial.printf("[COMMS] TX -> Audio Node seq=%lu cmd=%s\n",
                  (unsigned long)sequence, command);
  } else {
    Serial.println("[COMMS] Audio Node route held — no peer yet");
  }
}

static void onNodeCommand(const char *nodeType, const char *command, uint32_t sequence) {
  if (!nodeType || !command) return;
  char line[SHOWDUINO_COMMS_LINE_MAX + 1];
  snprintf(line, sizeof(line), "NODE:%s:%s", nodeType, command);
  commsUartWriteLine(line);
  Serial.printf("[COMMS] TX -> P4: %s seq=%lu\n", line, (unsigned long)sequence);
}

static bool handleP4Route(const char *line) {
  if (!line || strncmp(line, SHOWDUINO_LEGACY_ROUTE_AUDIO,
                       strlen(SHOWDUINO_LEGACY_ROUTE_AUDIO)) != 0) {
    return false;
  }
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

static void fanoutEmergency() {
  if (sEmergencyActive == sEmergencyWasActive) return;
  sEmergencyWasActive = sEmergencyActive;
  forwardToAudioNode(sEmergencyActive ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR", 0);
}

static void onDirectorCommand(const char *command) {
  if (!command || !command[0]) return;
  if (isLocalDiag(command)) {
    Serial.println("[COMMS] DIAG from Director ignored (local UART only)");
    return;
  }
  commsUartWriteLine(command);
  Serial.print("[COMMS] TX -> P4: ");
  Serial.println(command);
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
}

static void consumeWebTunnelBody() {
  while (commsUartAvailable() > 0 && commsWebTunnelConsumingBytes()) {
    const int c = commsUartRead();
    if (c < 0) break;
    commsWebTunnelOnByte((char)c);
  }
}

void protocolBridgeLoop() {
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
      Serial.println("[COMMS] UART RX: DIAG:PING");
      commsUartWriteLine("DIAG:PONG");
      Serial.println("[COMMS] UART TX: DIAG:PONG");
      continue;
    }
    if (!strcmp(line, "DIAG:PONG")) {
      Serial.println("[COMMS] UART RX: DIAG:PONG");
      if (sPingPending) {
        sPingPending = false;
        sLastPingOk = true;
        Serial.println("[COMMS] PING:P4 OK");
      }
      continue;
    }

    Serial.print("[COMMS] UART RX: ");
    Serial.println(line);
    observeP4Emergency(line);
    fanoutEmergency();
    if (handleP4Route(line)) {
      continue;
    }
    if (espNowTransportHaveDirector()) {
      if (espNowTransportSendToDirector(line)) {
        Serial.print("[COMMS] TX -> Director: ");
        Serial.println(line);
      }
    } else {
      Serial.println("[COMMS] P4 line held — no Director peer yet");
    }
  }

  if (sPingPending && (int32_t)(millis() - sPingDeadline) >= 0) {
    sPingPending = false;
    sLastPingOk = false;
    Serial.println("[COMMS] PING:P4 timeout — no DIAG:PONG");
  }
}

bool protocolBridgePingP4() {
  if (!commsUartReady()) {
    Serial.println("[COMMS] PING:P4 failed — UART not ready");
    return false;
  }
  sPingPending = true;
  sLastPingOk = false;
  sPingDeadline = millis() + SHOWDUINO_COMMS_PING_TIMEOUT_MS;
  commsUartWriteLine("DIAG:PING");
  Serial.println("[COMMS] UART TX: DIAG:PING");
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
