/*
  Showduino C3 Emergency Node

  Specialist ESP-NOW station. May ASSERT the P4 global emergency latch.
  Must never CLEAR it.

  Safety input is sampled before Serial, SoftAP, or WebUI start.
  GPIO map in BoardConfig.h is UNCONFIRMED — do not flash until commissioned.
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/EmergencyInput.h"
#include "src/EmergencyIndicate.h"
#include "src/EmergencyIdentity.h"
#include "src/EmergencyProtocol.h"
#include "src/EspNowEmergencyTransport.h"
#include "src/NodeDiagnostics.h"
#include "../../shared-node/NodeConfig.h"
#include "../../../protocol/showduino_emergency_node.h"
#include "../../../protocol/showduino_log.h"

static String sUsbLine;

static void onEspNowCommand(const char *command, uint32_t sequence) {
  emergencyProtocolApply(command, sequence);
}

static void handleUsbLine(const String &line) {
  if (!line.length()) return;
  if (showduino_log_handle_command(line.c_str())) return;
  if (nodeDiagHandleLine(line.c_str())) return;
  emergencyProtocolApply(line.c_str(), 0);
}

static void pollUsb() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      sUsbLine.trim();
      if (sUsbLine.length()) handleUsbLine(sUsbLine);
      sUsbLine = "";
    } else if (sUsbLine.length() < 160) {
      sUsbLine += c;
    }
  }
}

void setup() {
  /* Read the NC input before any Serial delay, SoftAP, or WebUI. */
  emergencyInputBegin();
  showduino_emergency_machine_init(&gEmergencyMachine, emergencyInputOpen());
  emergencyIndicateBegin();

  Serial.begin(115200);

  nodeDiagBegin();
  nodeConfigBegin("sdestop");

  const bool radioOk = emergencyEspNowBegin();
  emergencyEspNowSetHandler(onEspNowCommand);

  char mac[24];
  emergencyEspNowMacString(mac, sizeof(mac));
  emergencyIdentityBegin(mac);
  emergencyProtocolBegin();
  showduino_emergency_machine_set_radio(&gEmergencyMachine,
                                        radioOk && emergencyEspNowReady());

  if (gEmergencyMachine.latched) {
    Serial.println("[ESTOP] BOOT NC OPEN — LOCAL EMERGENCY LATCH");
  }

  nodeDiagPrintBootBanner();
  emergencyProtocolAnnounce();
}

void loop() {
  const uint32_t t0 = micros();
  pollUsb();
  emergencyProtocolService();
  emergencyIndicateService(&gEmergencyMachine, emergencyEspNowHaveComms());
  nodeDiagMarkLoop(micros() - t0);
}
