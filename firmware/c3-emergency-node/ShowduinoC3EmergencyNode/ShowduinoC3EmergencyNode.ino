/*
  Showduino C3 Emergency + Pixel Node

  Specialist ESP-NOW station. May ASSERT the P4 global emergency latch.
  Must never CLEAR it.

  Same physical peer ESTOP-xx also provides full Showduino Pixel Controller
  on GPIO2 (WS2812). Pixel is a capability — not a fake LED-xx identity.

  Boot priority:
    1. Emergency GPIO4 sample
    2. Emergency state machine
    3. Pixel GPIO2 safe (LOW)
    4. Serial / radio
    5. Pixel engine persist/init
    6. OLED / WebUI
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/EmergencyInput.h"
#include "src/EmergencyIndicate.h"
#include "src/EmergencyIdentity.h"
#include "src/EmergencyProtocol.h"
#include "src/EmergencyDisplay.h"
#include "src/EstopPixelEngine.h"
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
  /* 1–2: Emergency input before anything that can delay assert. */
  emergencyInputBegin();
  showduino_emergency_machine_init(&gEmergencyMachine, emergencyInputPressed());
  emergencyIndicateBegin();

  /* 3: Pixel data line safe (no garbage WS2812) before radio/OLED. */
  pixelEngineSafeGpio();

  Serial.begin(115200);

  nodeDiagBegin();
  nodeConfigBegin("sdestop");

  /* 4: radio */
  const bool radioOk = emergencyEspNowBegin();
  emergencyEspNowSetHandler(onEspNowCommand);

  char mac[24];
  emergencyEspNowMacString(mac, sizeof(mac));
  emergencyIdentityBegin(mac);
  emergencyProtocolBegin();
  showduino_emergency_machine_set_radio(&gEmergencyMachine,
                                        radioOk && emergencyEspNowReady());

  /* 5: Pixel engine allocation (after Emergency + radio). */
  Serial.printf("[ESTOP] Free heap before Pixel INIT path: %u\n",
                (unsigned)ESP.getFreeHeap());
  pixelEngineApplyPersisted();
  Serial.printf("[ESTOP] Free heap after Pixel persist/init: %u (cfg=%u)\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)pixelEngineConfiguredCount());

  /* 6: OLED last — failure never blocks assert. */
  const bool oledOk = emergencyDisplayBegin();

  if (gEmergencyMachine.latched) {
    Serial.println("[ESTOP] BOOT BUTTON PRESSED — LOCAL EMERGENCY LATCH");
  } else {
    Serial.println("[ESTOP] BOOT BUTTON RELEASED — READY");
  }
  if (!oledOk) {
    Serial.println("[ESTOP] OLED unavailable — assert path unaffected");
  }
  Serial.printf("[ESTOP] Show Pixel Line GPIO%d (recommend %u ohm series)\n",
                SHOWDUINO_ESTOP_PIXEL_DATA_PIN,
                (unsigned)SHOWDUINO_PIXEL_DATA_RESISTOR_OHMS);

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
