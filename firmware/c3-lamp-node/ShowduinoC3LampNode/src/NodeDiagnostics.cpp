#include "NodeDiagnostics.h"
#include "LampEngine.h"
#include "LampNodeState.h"
#include "LampDisplay.h"
#include "LampProtocol.h"
#include "EspNowLampTransport.h"
#include "LocalControls.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_lamp_node.h"
#include "../../../protocol/showduino_log.h"

static uint32_t sLoopUs = 0;
static uint32_t sLoopMaxUs = 0;
static uint32_t sMinHeap = 0;
static uint32_t sPixelTestUntil = 0;
static uint8_t sPixelTestPhase = 0;

void nodeDiagBegin() {
  sMinHeap = ESP.getFreeHeap();
}

void nodeDiagMarkLoop(uint32_t elapsedUs) {
  sLoopUs = elapsedUs;
  if (elapsedUs > sLoopMaxUs) sLoopMaxUs = elapsedUs;
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < sMinHeap) sMinHeap = heap;

  if (sPixelTestUntil && (int32_t)(millis() - sPixelTestUntil) < 0) {
    if (lampEngineEmergency()) {
      sPixelTestUntil = 0;
      return;
    }
    const uint32_t phase = (millis() / 180UL) % 4UL;
    if (phase != sPixelTestPhase) {
      sPixelTestPhase = (uint8_t)phase;
      if (phase == 0) lampEngineFill(80, 0, 0);
      else if (phase == 1) lampEngineFill(0, 80, 0);
      else if (phase == 2) lampEngineFill(0, 0, 80);
      else lampEngineFill(80, 80, 80);
    }
  } else if (sPixelTestUntil) {
    sPixelTestUntil = 0;
    if (!lampEngineEmergency() && !lampEngineActive()) lampEngineOff();
  }
}

void nodeDiagPrintBootBanner() {
  char mac[24];
  lampEspNowMacString(mac, sizeof(mac));
  Serial.println("================================================");
  Serial.println(" SHOWDUINO C3 LAMP NODE");
  Serial.println("================================================");
  Serial.printf("Name: %s\n", SHOWDUINO_LAMP_NODE_NAME);
  Serial.printf("Board: %s\n", SHOWDUINO_LAMP_NODE_BOARD);
  Serial.printf("Firmware: %s  Protocol: %s\n", SHOWDUINO_LAMP_NODE_FW,
                SHOWDUINO_LAMP_PROTOCOL);
  Serial.printf("ESP32: %s rev %u\n", ESP.getChipModel(),
                (unsigned)ESP.getChipRevision());
  Serial.printf("Flash: %lu MB  PSRAM: %s\n",
                (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)),
                ESP.getPsramSize() ? "present" : "none");
  Serial.printf("MAC: %s\n", mac);
  Serial.printf("OLED: %s addr=0x%02X %ux%u SDA=%d SCL=%d %s\n",
                SHOWDUINO_LAMP_OLED_CONTROLLER,
                (unsigned)SHOWDUINO_LAMP_OLED_ADDR,
                (unsigned)SHOWDUINO_LAMP_OLED_WIDTH,
                (unsigned)SHOWDUINO_LAMP_OLED_HEIGHT,
                SHOWDUINO_LAMP_OLED_SDA, SHOWDUINO_LAMP_OLED_SCL,
                lampDisplayReady() ? "OK" : "FAULT");
  Serial.printf("ESP-NOW: %s ch=%u comms=%s\n",
                lampEspNowReady() ? "ready" : "FAULT",
                (unsigned)SHOWDUINO_ESPNOW_CHANNEL,
                lampEspNowHaveComms() ? "ONLINE" : "SEARCHING");
  Serial.printf("Lamp: WS2812 x%u GPIO%d order=GRB 330R series (bench)\n",
                (unsigned)lampEngineCount(), lampEnginePin());
  Serial.printf("Status pixel: not fitted (OLED is primary)\n");
  Serial.printf("State: %s  emergency=%s  fault=%s\n",
                lampNodeStateName(),
                lampEngineEmergency() ? "ACTIVE" : "CLEAR",
                lampNodeStateFault());
  Serial.println("Pin map: OLED 5/6  BTN 9/0  LAMP GPIO2  unused HUNT 3,4,7,8,10");
  Serial.println("Waiting for Showduino.");
}

void nodeDiagPrintHelp() {
  Serial.println("[CONSOLE] C3 Lamp Node commissioning commands:");
  Serial.println("  HELP");
  Serial.println("  STATUS");
  Serial.println("  MAC");
  Serial.println("  OLED:TEST");
  Serial.println("  OLED:STATUS");
  Serial.println("  LAMP:STATUS");
  Serial.println("  LAMP:TEST");
  Serial.println("  LAMP:OFF | LAMP:STOP");
  Serial.println("  LAMP:LIST");
  Serial.println("  LAMP:SOLID:<r>,<g>,<b>");
  Serial.println("  LAMP:FX:<TOKEN>[:BRI=n][:SPD=n][:INT=n]");
  Serial.println("  LAMP:BRIGHTNESS:<0-100>");
  Serial.println("  ESPNOW:STATUS");
  Serial.println("  PIXEL:TEST");
  Serial.println("  RUN:TEST");
  Serial.println("  KEYS:STATUS");
  Serial.println("  LOG:LEVEL | LOG:LEVEL:ERROR|WARN|INFO|DEBUG|TRACE");
  Serial.println("RUN:TEST is silent/non-destructive. PIXEL:TEST is a brief RGB lamp check.");
}

void nodeDiagPrintPins() {
  Serial.println("[PINS] HUNT ESP32-C3 Super Mini OLED");
  Serial.printf("  OLED SDA GPIO%d  SCL GPIO%d  addr=0x%02X  %s 128x64 vis 28,24,38\n",
                SHOWDUINO_LAMP_OLED_SDA, SHOWDUINO_LAMP_OLED_SCL,
                (unsigned)SHOWDUINO_LAMP_OLED_ADDR, SHOWDUINO_LAMP_OLED_CONTROLLER);
  Serial.printf("  LAMP DIN GPIO%d x%u GRB + 330R (carbide-proven, HUNT-unused)\n",
                SHOWDUINO_LAMP_PIXEL_PIN, (unsigned)SHOWDUINO_LAMP_PIXEL_COUNT);
  Serial.printf("  BTN A GPIO%d  BTN B GPIO%d  (active LOW pull-up)\n",
                SHOWDUINO_LAMP_BTN_A, SHOWDUINO_LAMP_BTN_B);
  Serial.println("  Do not drive GPIO3/7 RGB, GPIO8 heartbeat, GPIO4 rumble, GPIO10 buzzer");
  Serial.println("  No spare GPIO taken for a diagnostic WS2812");
}

void nodeDiagPrintStatus() {
  char mac[24];
  lampEspNowMacString(mac, sizeof(mac));
  Serial.printf("STATE %s\n", lampNodeStateName());
  Serial.printf("FAULT %s\n", lampNodeStateFault());
  Serial.printf("SHOW_CONTROLLED %s\n", lampNodeStateShowControlled() ? "YES" : "NO");
  Serial.printf("EMERGENCY %s\n", lampEngineEmergency() ? "ACTIVE" : "CLEAR");
  Serial.printf("FX %s (%s) active=%s\n", lampEngineFxToken(), lampEngineFxDisplay(),
                lampEngineActive() ? "YES" : "NO");
  Serial.printf("BRIGHTNESS %u\n", (unsigned)lampEngineBrightness());
  Serial.printf("MAC %s\n", mac);
  Serial.printf("LAST_CMD %s\n", lampNodeStateLastCommand());
  Serial.printf("LAST_RESULT %s\n", lampNodeStateLastResult());
  Serial.printf("UPTIME %lu ms heap=%lu min=%lu loop_us=%lu max=%lu\n",
                (unsigned long)millis(),
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)sMinHeap,
                (unsigned long)sLoopUs,
                (unsigned long)sLoopMaxUs);
  nodeDiagPrintPins();
}

void nodeDiagPrintEspNow() {
  Serial.printf("ESPNOW %s comms=%s rx=%lu tx=%lu rej=%lu lastRx=%lu\n",
                lampEspNowReady() ? "OK" : "FAULT",
                lampEspNowHaveComms() ? "YES" : "NO",
                (unsigned long)lampEspNowRxCount(),
                (unsigned long)lampEspNowTxCount(),
                (unsigned long)lampEspNowRejected(),
                (unsigned long)lampEspNowLastRxMs());
}

void nodeDiagPrintLamp() {
  Serial.printf("LAMP WS2812 GPIO%d count=%u bri=%u fx=%s emergency=%s\n",
                lampEnginePin(), (unsigned)lampEngineCount(),
                (unsigned)lampEngineBrightness(), lampEngineFxToken(),
                lampEngineEmergency() ? "YES" : "NO");
}

void nodeDiagPrintOled() {
  Serial.printf("OLED %s 0x%02X %ux%u ready=%s page=%u\n",
                SHOWDUINO_LAMP_OLED_CONTROLLER,
                (unsigned)SHOWDUINO_LAMP_OLED_ADDR,
                (unsigned)SHOWDUINO_LAMP_OLED_WIDTH,
                (unsigned)SHOWDUINO_LAMP_OLED_HEIGHT,
                lampDisplayReady() ? "YES" : "NO",
                (unsigned)lampDisplayPage());
}

void nodeDiagPrintRunTest() {
  Serial.println("[TEST] C3 Lamp Node commissioning (non-destructive)");
  Serial.printf("  Board %s\n", SHOWDUINO_LAMP_NODE_BOARD);
  Serial.printf("  OLED %s\n", lampDisplayReady() ? "PASS" : "FAIL");
  Serial.printf("  ESP-NOW %s comms=%s\n",
                lampEspNowReady() ? "PASS" : "FAIL",
                lampEspNowHaveComms() ? "YES" : "NO");
  Serial.printf("  Lamp engine GPIO%d x%u — not driven by RUN:TEST\n",
                lampEnginePin(), (unsigned)lampEngineCount());
  lampLocalPrintStatus();
  Serial.println("  PIXEL:TEST is the safe RGB lamp check. LAMP:TEST runs Carbide Flame.");
}

void nodeDiagPixelTest() {
  if (lampEngineEmergency()) {
    Serial.println("[PIXEL] blocked — emergency");
    return;
  }
  if (lampNodeStateShowControlled() || lampEngineActive()) {
    Serial.println("[PIXEL] blocked — lamp already in use");
    return;
  }
  sPixelTestUntil = millis() + 1500UL;
  sPixelTestPhase = 255;
  Serial.println("[PIXEL] RGBW lamp test 1.5 s");
}

bool nodeDiagHandleLine(const char *line) {
  if (!line || !line[0]) return false;
  if (showduino_log_handle_command(line)) return true;
  if (!strcmp(line, "HELP")) {
    nodeDiagPrintHelp();
    return true;
  }
  if (!strcmp(line, "STATUS")) {
    nodeDiagPrintStatus();
    nodeDiagPrintEspNow();
    nodeDiagPrintOled();
    return true;
  }
  if (!strcmp(line, "MAC")) {
    char mac[24];
    lampEspNowMacString(mac, sizeof(mac));
    Serial.println(mac);
    return true;
  }
  if (!strcmp(line, "OLED:TEST")) {
    lampDisplayOledTest();
    Serial.println("[OLED] test pattern");
    return true;
  }
  if (!strcmp(line, "OLED:STATUS")) {
    nodeDiagPrintOled();
    return true;
  }
  if (!strcmp(line, "ESPNOW:STATUS")) {
    nodeDiagPrintEspNow();
    return true;
  }
  if (!strcmp(line, "LAMP:STATUS")) {
    nodeDiagPrintLamp();
    char st[96];
    lampProtocolFormatStatus(st, sizeof(st));
    Serial.println(st);
    return true;
  }
  if (!strcmp(line, "PIXEL:TEST") || !strcmp(line, "LED:TEST")) {
    nodeDiagPixelTest();
    return true;
  }
  if (!strcmp(line, "RUN:TEST")) {
    nodeDiagPrintRunTest();
    return true;
  }
  if (!strcmp(line, "KEYS:STATUS") || !strcmp(line, "BUTTONS:STATUS")) {
    lampLocalPrintStatus();
    return true;
  }
  return false;
}
