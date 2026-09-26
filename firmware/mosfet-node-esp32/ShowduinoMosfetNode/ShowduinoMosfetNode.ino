/*
  Showduino MOSFET Node 0.1.1 — ESP32_MOS_X4 / 303E32NMOS4

  Four low-voltage powered outputs with PWM / pulse / fade.
  Local 4× NeoPixel identifiers on GPIO25 (NOT a theatrical Pixel Line).
  Fail-safe: ALL OFF on boot, authority loss, emergency, show stop.
  GPIO map is SOFTWARE DEFINED — physical verification required.
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/MosfetOutputEngine.h"
#include "src/MosfetIdentifierPixels.h"
#include "src/MosfetNodeState.h"
#include "src/MosfetIdentity.h"
#include "src/MosfetProtocol.h"
#include "src/MosfetStatusLed.h"
#include "src/MosfetWeb.h"
#include "src/EspNowMosfetTransport.h"
#include "../../shared-node/NodeConfig.h"
#include "../../../protocol/showduino_mosfet_node.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_ownership.h"

static String sUsbLine;

static void onEspNowCommand(const char *command, uint32_t sequence) {
  mosfetProtocolApply(command, sequence, SHOWDUINO_CMD_ORIGIN_SHOW);
}

static void handleUsbLine(const String &line) {
  if (!line.length()) return;
  if (showduino_log_handle_command(line.c_str())) return;
  mosfetProtocolApply(line.c_str(), 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
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
  /* BOOT ORDER: MOSFET outputs LOW first, then identifier data line LOW. */
  mosfetOutputEngineSafeGpioFirst();
  mosfetIdentifierPixelsSafeGpio();
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("[MOSFET] === Showduino MOSFET Node " SHOWDUINO_MOSFET_NODE_FW " ===");
  Serial.println("[MOSFET] Board: " SHOWDUINO_MOSFET_BOARD);
  Serial.println("[MOSFET] BOOT: GPIO outputs driven LOW before radio/services");
#if !SHOWDUINO_MOSFET_GPIO_VERIFIED
  Serial.println("[MOSFET] GPIO MAP: SOFTWARE DEFINED / HARDWARE UNVERIFIED");
  Serial.printf("[MOSFET] OUT1=GPIO%u OUT2=GPIO%u OUT3=GPIO%u OUT4=GPIO%u LED=GPIO%u IDENT=GPIO%u\n",
                (unsigned)SHOWDUINO_MOSFET_OUT1_GPIO,
                (unsigned)SHOWDUINO_MOSFET_OUT2_GPIO,
                (unsigned)SHOWDUINO_MOSFET_OUT3_GPIO,
                (unsigned)SHOWDUINO_MOSFET_OUT4_GPIO,
                (unsigned)SHOWDUINO_MOSFET_STATUS_LED_GPIO,
                (unsigned)SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO);
#endif

  mosfetOutputEngineBegin();
  mosfetIdentifierPixelsBegin();
  mosfetStatusLedBegin();
  nodeConfigBegin("sdmosfet");
  mosfetNodeStateBegin();

  const bool radioOk = mosfetEspNowBegin();
  mosfetEspNowSetHandler(onEspNowCommand);

  char mac[24];
  mosfetEspNowMacString(mac, sizeof(mac));
  mosfetIdentityBegin(mac);
  mosfetProtocolBegin();
  mosfetWebBegin();

  if (!radioOk) {
    mosfetNodeStateSetFault("ESPNOW");
  }

  Serial.printf("[MOSFET] Identity %s / %s MAC=%s\n",
                mosfetIdentityId(), mosfetIdentityName(), mac);
  Serial.println("[MOSFET] PCB-marked input range: 5–60 V DC (not characterised)");
  Serial.println("[MOSFET] Commission with benign low-voltage test loads only");
  mosfetProtocolAnnounce();
}

void loop() {
  pollUsb();
  mosfetOutputEngineLoop();
  mosfetNodeStateLoop();
  mosfetProtocolLoop();
  mosfetEspNowService();
  mosfetWebLoop();
  mosfetStatusLedLoop();
  mosfetIdentifierPixelsService();
}
