#include "ProtocolBridge.h"
#include "CommsUart.h"
#include "EspNowTransport.h"
#include "../BoardConfig.h"

#include <string.h>

static bool sPingPending = false;
static bool sLastPingOk = false;
static uint32_t sPingDeadline = 0;

static bool isLocalDiag(const char *line) {
  return line && (!strcmp(line, "DIAG:PING") || !strcmp(line, "DIAG:PONG"));
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

void protocolBridgeBegin() {
  commsUartBegin();
  espNowTransportSetCommandHandler(onDirectorCommand);
}

void protocolBridgeLoop() {
  char line[SHOWDUINO_COMMS_LINE_MAX + 1];
  while (commsUartReadLine(line, sizeof(line))) {
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
