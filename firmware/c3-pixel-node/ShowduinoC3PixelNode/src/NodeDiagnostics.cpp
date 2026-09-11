#include "NodeDiagnostics.h"
#include "PixelEngine.h"
#include "PixelNodeState.h"
#include "PixelIdentity.h"
#include "PixelDisplay.h"
#include "PixelProtocol.h"
#include "EspNowPixelTransport.h"
#include "LocalControls.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_pixel_node.h"
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
  pixelEspNowMacString(mac, sizeof(mac));
  Serial.println("================================================");
  Serial.printf(" SHOWDUINO %s\n", SHOWDUINO_PLATFORM_VERSION);
  Serial.println(" COMPONENT: PIXEL NODE");
  Serial.println("================================================");
  Serial.printf("FW: %s\n", SHOWDUINO_PIXEL_NODE_FW);
  Serial.printf("PROTOCOL: %s\n", SHOWDUINO_PIXEL_PROTOCOL);
  Serial.printf("NODE ID: %s\n", pixelIdentityId());
  Serial.printf("NAME: %s\n", pixelIdentityName());
  Serial.printf("MAC: %s\n", mac);
  Serial.printf("OLED SDA: %d\n", SHOWDUINO_PIXEL_OLED_SDA);
  Serial.printf("OLED SCL: %d\n", SHOWDUINO_PIXEL_OLED_SCL);
  Serial.printf("OLED: 0x%02X %s\n", (unsigned)SHOWDUINO_PIXEL_OLED_ADDR,
                pixelDisplayReady() ? "OK" : "FAULT");
  Serial.printf("PIXEL GPIO: %d\n", SHOWDUINO_PIXEL_DATA_PIN);
  Serial.printf("PIXELS: %u\n", (unsigned)pixelEngineConfiguredCount());
  Serial.printf("INITIALISED: %s\n", pixelEngineReady() ? "YES" : "NO");
  Serial.printf("SEGMENTS: %u\n", (unsigned)pixelEngineActiveSegments());
  Serial.printf("ESP-NOW: %s\n", pixelEspNowReady() ? "ready" : "FAULT");
  Serial.printf("CHANNEL: %u\n", (unsigned)pixelEspNowChannel());
  Serial.printf("Board: %s\n", SHOWDUINO_PIXEL_NODE_BOARD);
  Serial.println("Waiting for Showduino.");
}

void nodeDiagPrintHelp() {
  Serial.println("[CONSOLE] C3 Pixel Node commissioning commands:");
  Serial.println("  HELP | STATUS | MAC");
  Serial.println("  PIXEL:COUNT:<1-512>   (save, do not light)");
  Serial.println("  PIXEL:INIT");
  Serial.println("  PIXEL:STATUS | PIXEL:TEST | PIXEL:BLACKOUT | PIXEL:LOCATE");
  Serial.println("  PIXEL:ID:<LED-01> | PIXEL:NAME:<friendly>");
  Serial.println("  PIXEL:SEGMENT:<0-15>:RANGE:<start>:<count>");
  Serial.println("  PIXEL:SEGMENT:<id>:FX:<name> | START | STOP");
  Serial.println("  OLED:TEST | ESPNOW:STATUS | KEYS:STATUS");
}

void nodeDiagPrintPins() {
  Serial.println("[PINS] HUNT ESP32-C3 Super Mini OLED");
  Serial.printf("  OLED SDA GPIO%d  SCL GPIO%d  addr=0x%02X  vis L%d R%d T%d H%d\n",
                SHOWDUINO_PIXEL_OLED_SDA, SHOWDUINO_PIXEL_OLED_SCL,
                (unsigned)SHOWDUINO_PIXEL_OLED_ADDR,
                OLED_PANEL_MARGIN_LEFT, OLED_PANEL_MARGIN_RIGHT,
                OLED_PANEL_MARGIN_TOP, OLED_PANEL_VISIBLE_H);
  Serial.printf("  PIXEL DATA GPIO%d  max=%u  %uR series\n",
                SHOWDUINO_PIXEL_DATA_PIN,
                (unsigned)SHOWDUINO_PIXEL_NODE_MAX_PIXELS,
                (unsigned)SHOWDUINO_PIXEL_DATA_RESISTOR_OHMS);
  Serial.printf("  BTN A GPIO%d  BTN B GPIO%d\n",
                SHOWDUINO_PIXEL_BTN_A, SHOWDUINO_PIXEL_BTN_B);
  Serial.println("  Do not drive HUNT GPIO3/4/7/8/10. Do not power the strip from the C3.");
}

void nodeDiagPrintStatus() {
  char mac[24];
  pixelEspNowMacString(mac, sizeof(mac));
  Serial.printf("STATE %s\n", pixelNodeStateName());
  Serial.printf("OWNER %s\n", pixelOwnerModeName());
  Serial.printf("ID %s NAME %s\n", pixelIdentityId(), pixelIdentityName());
  Serial.printf("EMERGENCY %s LOCATE %s INIT %s\n",
                pixelEngineEmergency() ? "ACTIVE" : "CLEAR",
                pixelEngineLocateActive() ? "YES" : "NO",
                pixelEngineReady() ? "YES" : "NO");
  Serial.printf("MAC %s\n", mac);
  Serial.printf("UPTIME %lu ms heap=%lu min=%lu loop_us=%lu max=%lu\n",
                (unsigned long)millis(),
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)sMinHeap,
                (unsigned long)sLoopUs,
                (unsigned long)sLoopMaxUs);
  pixelEnginePrintStatus();
  nodeDiagPrintPins();
}

void nodeDiagPrintEspNow() {
  Serial.printf("ESPNOW %s comms=%s ch=%u rssi=%d rx=%lu tx=%lu rej=%lu lastRx=%lu\n",
                pixelEspNowReady() ? "OK" : "FAULT",
                pixelEspNowHaveComms() ? "YES" : "NO",
                (unsigned)pixelEspNowChannel(),
                (int)pixelEspNowRssi(),
                (unsigned long)pixelEspNowRxCount(),
                (unsigned long)pixelEspNowTxCount(),
                (unsigned long)pixelEspNowRejected(),
                (unsigned long)pixelEspNowLastRxMs());
}

void nodeDiagPrintOled() {
  Serial.printf("OLED %s 0x%02X %ux%u ready=%s page=%u\n",
                SHOWDUINO_PIXEL_OLED_CONTROLLER,
                (unsigned)SHOWDUINO_PIXEL_OLED_ADDR,
                (unsigned)SHOWDUINO_PIXEL_OLED_WIDTH,
                (unsigned)SHOWDUINO_PIXEL_OLED_HEIGHT,
                pixelDisplayReady() ? "YES" : "NO",
                (unsigned)pixelDisplayPage());
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
    pixelEspNowMacString(mac, sizeof(mac));
    Serial.println(mac);
    return true;
  }
  if (!strcmp(line, "OLED:TEST")) {
    pixelDisplayOledTest();
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
  if (!strcmp(line, "KEYS:STATUS") || !strcmp(line, "BUTTONS:STATUS")) {
    pixelLocalPrintStatus();
    return true;
  }
  return false;
}
