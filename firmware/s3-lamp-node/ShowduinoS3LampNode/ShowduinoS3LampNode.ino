/*
  Showduino S3 Lamp Node — ESP32-S3 interactive carbide-lamp simulator.

  Specialist practical-prop controller. Not a Pixel Node. Not an Audio Node.
  The P4 remains authoritative. This node renders flame, blow-out, and
  local Fermion effect audio. GPIOs are unconfirmed — do not flash until
  Toby traces the jewel, button, sensors, and UART.
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/LampEngine.h"
#include "src/LampNodeState.h"
#include "src/LampConfig.h"
#include "src/LampProtocol.h"
#include "src/LampSensors.h"
#include "src/LampAudio.h"
#include "src/EspNowLampTransport.h"
#include "src/LocalControls.h"
#include "src/NodeDiagnostics.h"
#include "../../shared-node/NodeConfig.h"
#include "../../../protocol/showduino_lamp_node.h"
#include "../../../protocol/showduino_log.h"

static String sUsbLine;

static void onEspNowCommand(const char *command, uint32_t sequence) {
  SD_LOGT("ESPNOW", "RX seq=%lu cmd=%s", (unsigned long)sequence,
          command ? command : "");
  lampProtocolApply(command, sequence, SHOWDUINO_CMD_ORIGIN_SHOW);
}

static void handleUsbLine(const String &line) {
  if (!line.length()) return;
  if (showduino_log_handle_command(line.c_str())) return;
  if (nodeDiagHandleLine(line.c_str())) return;
  lampProtocolApply(line.c_str(), 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
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

static void pollButton() {
  const LampLocalEvent ev = lampLocalPoll();
  if (ev.btn == LAMP_BTN_NONE) return;
  Serial.printf("[BUTTON] striker %s\n", ev.longPress ? "LONG" : "SHORT");
  if (ev.longPress) nodeDiagPrintStatus();
  else lampProtocolLocalIgnite();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  nodeDiagBegin();
  lampConfigBegin();
  lampNodeStateBegin(SHOWDUINO_LAMP_ST_BOOTING);
  lampEngineBegin();
  lampSensorsBegin();
  lampAudioBegin();
  lampLocalBegin();

  const bool radioOk = lampEspNowBegin();
  lampEspNowSetHandler(onEspNowCommand);
  lampProtocolBegin();

  if (!radioOk) {
    lampNodeStateSetFault("ESPNOW");
  } else {
    lampNodeStateSet(SHOWDUINO_LAMP_ST_SEARCHING);
  }

  nodeDiagPrintBootBanner();
  lampProtocolAnnounce();
}

void loop() {
  const uint32_t t0 = micros();
  pollUsb();
  pollButton();
  lampSensorsService();
  lampAudioService();
  lampEngineService();
  lampProtocolService();
  nodeDiagMarkLoop(micros() - t0);
}
