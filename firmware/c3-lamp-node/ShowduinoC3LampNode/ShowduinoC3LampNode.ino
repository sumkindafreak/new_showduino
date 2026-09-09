/*
  Showduino C3 Lamp Node — HUNT ESP32-C3 Super Mini OLED board

  Specialist lamp output node. Receives commands over ESP-NOW from the
  Communications S3, executes carbide-lamp FX locally, and reports to the P4.
  The P4 remains authoritative. This node never starts a production.
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/LampEngine.h"
#include "src/LampNodeState.h"
#include "src/LampDisplay.h"
#include "src/LampProtocol.h"
#include "src/EspNowLampTransport.h"
#include "src/LocalControls.h"
#include "src/NodeDiagnostics.h"
#include "../../../protocol/showduino_lamp_node.h"
#include "../../../protocol/showduino_log.h"

static String sUsbLine;

static void onEspNowCommand(const char *command, uint32_t sequence) {
  SD_LOGT("ESPNOW", "RX seq=%lu cmd=%s", (unsigned long)sequence,
          command ? command : "");
  lampProtocolApply(command, sequence, true);
}

static void handleUsbLine(const String &line) {
  if (!line.length()) return;
  if (showduino_log_handle_command(line.c_str())) return;
  if (nodeDiagHandleLine(line.c_str())) return;
  lampProtocolApply(line.c_str(), 0, false);
}

static void pollUsb() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      sUsbLine.trim();
      if (sUsbLine.length()) handleUsbLine(sUsbLine);
      sUsbLine = "";
    } else if (sUsbLine.length() < 120) {
      sUsbLine += c;
    }
  }
}

static void pollButtons() {
  const LampLocalEvent ev = lampLocalPoll();
  if (ev.btn == LAMP_BTN_NONE) return;

  if (ev.btn == LAMP_BTN_A) {
    Serial.printf("[BUTTON] A %s\n", ev.longPress ? "LONG" : "SHORT");
    if (ev.longPress) nodeDiagPrintStatus();
    else lampProtocolLocalTest();
  } else if (ev.btn == LAMP_BTN_B) {
    Serial.printf("[BUTTON] B %s\n", ev.longPress ? "LONG" : "SHORT");
    if (ev.longPress) nodeDiagPrintStatus();
    else lampDisplayNextPage();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  nodeDiagBegin();
  lampNodeStateBegin(SHOWDUINO_LAMP_ST_BOOTING);

  const bool oledOk = lampDisplayBegin();
  const bool lampOk = lampEngineBegin();
  lampLocalBegin();
  const bool radioOk = lampEspNowBegin();
  lampEspNowSetHandler(onEspNowCommand);
  lampProtocolBegin();

  if (!lampOk) {
    lampNodeStateSetFault("LAMP");
  } else if (!radioOk) {
    lampNodeStateSetFault("ESPNOW");
  } else if (!oledOk) {
    lampNodeStateSetFault("OLED");
  } else {
    lampNodeStateSet(SHOWDUINO_LAMP_ST_SEARCHING);
  }

  nodeDiagPrintBootBanner();
  lampProtocolAnnounce();
}

void loop() {
  const uint32_t t0 = micros();
  pollUsb();
  pollButtons();
  lampEngineService();
  lampProtocolService();
  lampDisplayService();
  nodeDiagMarkLoop(micros() - t0);
}
