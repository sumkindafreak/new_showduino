/*
  Showduino C3 Emergency Node

  Specialist ESP-NOW station. May ASSERT the P4 global emergency latch.
  Must never CLEAR it.

  Momentary pushbutton (GPIO4, INPUT_PULLUP, active LOW) is sampled before
  Serial, SoftAP, OLED, or WebUI start. OLED failure never blocks assert.
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/EmergencyInput.h"
#include "src/EmergencyIndicate.h"
#include "src/EmergencyIdentity.h"
#include "src/EmergencyProtocol.h"
#include "src/EmergencyDisplay.h"
#include "src/EspNowEmergencyTransport.h"
#include "src/NodeDiagnostics.h"
#include "../../shared-node/NodeConfig.h"
#include "../../../protocol/showduino_emergency_node.h"
#include "../../../protocol/showduino_log.h"

static String sUsbLine;
static uint8_t sLastLatch = 0;
static uint8_t sLastPressed = 0;
static uint8_t sLastRadio = 0xFF;

static void onEspNowCommand(const char *command, uint32_t sequence) {
  emergencyProtocolApply(command, sequence);
  emergencyDisplayForce();
}

static void handleUsbLine(const String &line) {
  if (!line.length()) return;
  if (showduino_log_handle_command(line.c_str())) return;
  if (nodeDiagHandleLine(line.c_str())) return;
  emergencyProtocolApply(line.c_str(), 0);
  emergencyDisplayForce();
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
  /* Sample momentary button before Serial, SoftAP, OLED, or WebUI. */
  emergencyInputBegin();
  showduino_emergency_machine_init(&gEmergencyMachine, emergencyInputPressed());
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

  const bool oledOk = emergencyDisplayBegin();

  if (gEmergencyMachine.latched) {
    Serial.println("[ESTOP] BOOT BUTTON PRESSED — LOCAL EMERGENCY LATCH");
  } else {
    Serial.println("[ESTOP] BOOT BUTTON RELEASED — READY");
  }
  if (!oledOk) {
    Serial.println("[ESTOP] OLED unavailable — assert path unaffected");
  }

  sLastLatch = gEmergencyMachine.latched;
  sLastPressed = (uint8_t)emergencyInputPressed();
  sLastRadio = emergencyEspNowHaveComms() ? 1 : 0;

  nodeDiagPrintBootBanner();
  emergencyProtocolAnnounce();
  emergencyDisplayForce();
}

void loop() {
  const uint32_t t0 = micros();
  pollUsb();
  emergencyProtocolService();
  emergencyIndicateService(&gEmergencyMachine, emergencyEspNowHaveComms());

  const uint8_t latch = gEmergencyMachine.latched;
  const uint8_t pressed = (uint8_t)emergencyInputPressed();
  const uint8_t radio = emergencyEspNowHaveComms() ? 1 : 0;
  if (latch != sLastLatch || pressed != sLastPressed || radio != sLastRadio) {
    sLastLatch = latch;
    sLastPressed = pressed;
    sLastRadio = radio;
    emergencyDisplayForce();
  } else {
    emergencyDisplayService();
  }

  nodeDiagMarkLoop(micros() - t0);
}
