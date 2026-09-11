/*
  Showduino C3 Pixel Node — HUNT ESP32-C3 Super Mini OLED board

  Remote equivalent of the P4 GPIO23 Show Pixel Line.
  Same model: PIXEL LINE → SEGMENTS → EFFECTS → PARAMETERS.
  The P4 remains authoritative. This node renders locally.
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/PixelEngine.h"
#include "src/PixelNodeState.h"
#include "src/PixelIdentity.h"
#include "src/PixelDisplay.h"
#include "src/PixelProtocol.h"
#include "src/EspNowPixelTransport.h"
#include "src/LocalControls.h"
#include "src/NodeDiagnostics.h"
#include "../../shared-node/NodeConfig.h"
#include "../../../protocol/showduino_pixel_node.h"
#include "../../../protocol/showduino_log.h"

static String sUsbLine;

static void onEspNowCommand(const char *command, uint32_t sequence) {
  SD_LOGT("ESPNOW", "RX seq=%lu cmd=%s", (unsigned long)sequence,
          command ? command : "");
  pixelProtocolApply(command, sequence, SHOWDUINO_CMD_ORIGIN_SHOW);
}

static void handleUsbLine(const String &line) {
  if (!line.length()) return;
  if (showduino_log_handle_command(line.c_str())) return;
  if (nodeDiagHandleLine(line.c_str())) return;
  pixelProtocolApply(line.c_str(), 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
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

static void pollButtons() {
  const PixelLocalEvent ev = pixelLocalPoll();
  if (ev.btn == PIXEL_BTN_NONE) return;
  if (ev.btn == PIXEL_BTN_A) {
    Serial.printf("[BUTTON] A %s\n", ev.longPress ? "LONG" : "SHORT");
    if (ev.longPress) nodeDiagPrintStatus();
    else pixelProtocolLocalTest();
  } else if (ev.btn == PIXEL_BTN_B) {
    Serial.printf("[BUTTON] B %s\n", ev.longPress ? "LONG" : "SHORT");
    if (ev.longPress) nodeDiagPrintStatus();
    else pixelDisplayNextPage();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  /* GPIO SAFE first — data line held low before any radio or OLED work. */
  pixelEngineSafeGpio();
  nodeDiagBegin();
  nodeConfigBegin("sdpixel");
  pixelNodeStateBegin(SHOWDUINO_PIXEL_ST_BOOTING);

  const bool oledOk = pixelDisplayBegin();
  pixelLocalBegin();
  const bool radioOk = pixelEspNowBegin();
  pixelEspNowSetHandler(onEspNowCommand);

  char mac[24];
  pixelEspNowMacString(mac, sizeof(mac));
  pixelIdentityBegin(mac);
  pixelEngineApplyPersisted();
  pixelProtocolBegin();

  if (!radioOk) {
    pixelNodeStateSetFault("ESPNOW");
  } else if (!oledOk) {
    pixelNodeStateSetFault("OLED");
  } else {
    pixelNodeStateSet(SHOWDUINO_PIXEL_ST_SEARCHING);
  }

  nodeDiagPrintBootBanner();
  pixelProtocolAnnounce();
}

void loop() {
  const uint32_t t0 = micros();
  pollUsb();
  pollButtons();
  pixelEngineService();
  pixelProtocolService();
  pixelDisplayService();
  nodeDiagMarkLoop(micros() - t0);
}
