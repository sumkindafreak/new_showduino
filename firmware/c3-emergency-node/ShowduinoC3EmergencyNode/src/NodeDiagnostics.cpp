#include "NodeDiagnostics.h"
#include "EmergencyIdentity.h"
#include "EmergencyProtocol.h"
#include "EmergencyInput.h"
#include "EspNowEmergencyTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_emergency_node.h"
#include "../../../protocol/showduino_version.h"
#include "../../../protocol/showduino_log.h"

static uint32_t sLoopUs = 0;
static uint32_t sLoopMaxUs = 0;
static uint32_t sMinHeap = 0;

void nodeDiagBegin() {
  sMinHeap = ESP.getFreeHeap();
}

void nodeDiagMarkLoop(uint32_t elapsedUs) {
  sLoopUs = elapsedUs;
  if (elapsedUs > sLoopMaxUs) sLoopMaxUs = elapsedUs;
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < sMinHeap) sMinHeap = heap;
}

void nodeDiagPrintBootBanner() {
  char mac[24];
  emergencyEspNowMacString(mac, sizeof(mac));
  Serial.println("================================================");
  Serial.printf(" SHOWDUINO %s\n", SHOWDUINO_PLATFORM_VERSION);
  Serial.println(" COMPONENT: EMERGENCY NODE");
  Serial.println("================================================");
  Serial.printf("FW: %s\n", SHOWDUINO_EMERGENCY_NODE_FW);
  Serial.printf("PROTOCOL: %s\n", SHOWDUINO_EMERGENCY_PROTOCOL);
  Serial.printf("NODE ID: %s\n", emergencyIdentityId());
  Serial.printf("NAME: %s\n", emergencyIdentityName());
  Serial.printf("MAC: %s\n", mac);
  Serial.printf("NC GPIO: %d  OPEN_LEVEL=%s  VERIFIED=%s\n",
                SHOWDUINO_ESTOP_NODE_GPIO,
                SHOWDUINO_ESTOP_NODE_OPEN_LEVEL == HIGH ? "HIGH" : "LOW",
                SHOWDUINO_EMERGENCY_NODE_GPIO_VERIFIED ? "YES" : "NO");
  Serial.printf("INPUT: %s  LATCH: %s\n",
                gEmergencyMachine.input_open ? "OPEN" : "CLOSED",
                gEmergencyMachine.latched ? "LATCHED" : "CLEAR");
  Serial.printf("ESP-NOW: %s  CH: %u\n",
                emergencyEspNowReady() ? "ready" : "FAULT",
                (unsigned)emergencyEspNowChannel());
  Serial.println("RULE: ASSERT ONLY — NEVER CLEAR");
  Serial.println("Waiting for Showduino.");
}

void nodeDiagPrintHelp() {
  Serial.println("[CONSOLE] Emergency Node commissioning commands:");
  Serial.println("  HELP | STATUS | MAC");
  Serial.println("  ESTOP:STATUS | ESTOP:REARM");
  Serial.println("  ESTOP:ID:ESTOP-01 | ESTOP:NAME:ENTRANCE");
  Serial.println("  ESTOP:CLEAR is rejected. This node cannot clear P4 emergency.");
}

void nodeDiagPrintStatus() {
  char mac[24];
  emergencyEspNowMacString(mac, sizeof(mac));
  Serial.printf("[ESTOP] id=%s name=%s in=%s latch=%u ack=%u radio=%u ch=%u rssi=%d mac=%s heap=%u loop=%luus\n",
                emergencyIdentityId(), emergencyIdentityName(),
                gEmergencyMachine.input_open ? "OPEN" : "CLOSED",
                (unsigned)gEmergencyMachine.latched,
                (unsigned)gEmergencyMachine.acked,
                emergencyEspNowHaveComms() ? 1U : 0U,
                (unsigned)emergencyEspNowChannel(),
                (int)emergencyEspNowRssi(),
                mac,
                (unsigned)ESP.getFreeHeap(),
                (unsigned long)sLoopUs);
}

bool nodeDiagHandleLine(const char *line) {
  if (!line || !line[0]) return false;
  if (!strcmp(line, "HELP")) { nodeDiagPrintHelp(); return true; }
  if (!strcmp(line, "STATUS") || !strcmp(line, "ESTOP:STATUS")) {
    nodeDiagPrintStatus();
    return true;
  }
  if (!strcmp(line, "MAC")) {
    char mac[24];
    emergencyEspNowMacString(mac, sizeof(mac));
    Serial.println(mac);
    return true;
  }
  return false;
}
