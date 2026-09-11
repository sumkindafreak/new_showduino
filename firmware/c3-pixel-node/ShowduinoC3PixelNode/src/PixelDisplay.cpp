#include "PixelDisplay.h"
#include "PixelEngine.h"
#include "PixelNodeState.h"
#include "PixelIdentity.h"
#include "EspNowPixelTransport.h"
#include "../BoardConfig.h"

#include <string.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 sDisp(SHOWDUINO_PIXEL_OLED_WIDTH, SHOWDUINO_PIXEL_OLED_HEIGHT, &Wire, -1);
static bool sReady = false;
static int16_t sVisX = SHOWDUINO_PIXEL_OLED_VISIBLE_X;
static int16_t sVisY = SHOWDUINO_PIXEL_OLED_VISIBLE_Y;
static uint8_t sPage = 0;
static uint32_t sLastPaint = 0;
static char sSig[96] = "";

static void lineAt(int16_t x, int16_t y, const char *text) {
  sDisp.setTextSize(1);
  sDisp.setTextColor(SSD1306_WHITE);
  sDisp.setCursor(sVisX + x, sVisY + y);
  sDisp.print(text ? text : "");
}

static void paint() {
  if (!sReady) return;
  sDisp.clearDisplay();

  const ShowduinoPixelNodeState st = pixelNodeStateDisplay();
  char l1[20], l2[20], l3[20], l4[20];

  if (sPage == 1) {
    lineAt(0, 0, "C3 PIXEL PINS");
    snprintf(l2, sizeof(l2), "OLED %d/%d", SHOWDUINO_PIXEL_OLED_SDA, SHOWDUINO_PIXEL_OLED_SCL);
    snprintf(l3, sizeof(l3), "DATA GPIO%d", SHOWDUINO_PIXEL_DATA_PIN);
    snprintf(l4, sizeof(l4), "CH %u %s", (unsigned)pixelEspNowChannel(),
             pixelEspNowHaveComms() ? "LINK" : "SCAN");
    lineAt(0, 9, l2);
    lineAt(0, 18, l3);
    lineAt(0, 27, l4);
    sDisp.display();
    return;
  }

  if (st == SHOWDUINO_PIXEL_ST_EMERGENCY || pixelEngineEmergency()) {
    lineAt(0, 0, "SHOWDUINO");
    lineAt(0, 9, "!!! EMERGENCY !!!");
    lineAt(0, 18, "ALL LINE WHITE");
    snprintf(l4, sizeof(l4), "%s", pixelIdentityId());
    lineAt(0, 27, l4);
  } else if (st == SHOWDUINO_PIXEL_ST_LOCATE) {
    lineAt(0, 0, "SHOWDUINO");
    lineAt(0, 9, "LOCATE");
    lineAt(0, 18, pixelIdentityId());
    lineAt(0, 27, pixelIdentityName());
  } else if (st == SHOWDUINO_PIXEL_ST_FAULT) {
    lineAt(0, 0, "PIXEL NODE");
    snprintf(l2, sizeof(l2), "FAULT %s", pixelNodeStateFault());
    lineAt(0, 9, l2);
    lineAt(0, 18, pixelIdentityId());
    lineAt(0, 27, " ");
  } else if (st == SHOWDUINO_PIXEL_ST_UNINIT || !pixelEngineReady()) {
    lineAt(0, 0, "PIXEL NODE");
    snprintf(l2, sizeof(l2), "%.12s", pixelIdentityId());
    lineAt(0, 9, l2);
    lineAt(0, 18, "NOT INITIALISED");
    snprintf(l4, sizeof(l4), "PIX %u %s",
             (unsigned)pixelEngineConfiguredCount(),
             pixelEspNowHaveComms() ? "LINK" : "SCAN");
    lineAt(0, 27, l4);
  } else if (st == SHOWDUINO_PIXEL_ST_SEARCHING || st == SHOWDUINO_PIXEL_ST_BOOTING) {
    lineAt(0, 0, "PIXEL NODE");
    lineAt(0, 9, pixelIdentityId());
    lineAt(0, 18, "SEARCHING");
    lineAt(0, 27, "FOR COMMS...");
  } else {
    snprintf(l1, sizeof(l1), "%.12s", pixelIdentityId());
    snprintf(l2, sizeof(l2), "%.12s", pixelIdentityName());
    snprintf(l3, sizeof(l3), "PIX %u S%u",
             (unsigned)pixelEngineCount(),
             (unsigned)pixelEngineActiveSegments());
    snprintf(l4, sizeof(l4), "%s CH%u",
             pixelEspNowHaveComms() ? "LINK OK" : "LOST",
             (unsigned)pixelEspNowChannel());
    lineAt(0, 0, l1);
    lineAt(0, 9, l2);
    lineAt(0, 18, l3);
    lineAt(0, 27, l4);
  }
  sDisp.display();
}

static void makeSig(char *out, size_t n) {
  snprintf(out, n, "%u|%u|%d|%d|%d|%u|%s|%s",
           (unsigned)pixelNodeStateDisplay(),
           (unsigned)sPage,
           pixelEngineReady() ? 1 : 0,
           pixelEngineEmergency() ? 1 : 0,
           pixelEngineLocateActive() ? 1 : 0,
           (unsigned)pixelEngineConfiguredCount(),
           pixelIdentityId(),
           pixelEspNowHaveComms() ? "1" : "0");
}

bool pixelDisplayBegin() {
  Wire.begin(SHOWDUINO_PIXEL_OLED_SDA, SHOWDUINO_PIXEL_OLED_SCL);
  Wire.setClock(SHOWDUINO_PIXEL_OLED_I2C_HZ);
  sReady = sDisp.begin(SSD1306_SWITCHCAPVCC, SHOWDUINO_PIXEL_OLED_ADDR, true, false);
  if (!sReady) return false;

#if SHOWDUINO_PIXEL_OLED_ROTATION_180
  sDisp.ssd1306_command(SSD1306_SEGREMAP);
  sDisp.ssd1306_command(SSD1306_COMSCANINC);
  sVisY = (int16_t)(sDisp.height() - SHOWDUINO_PIXEL_OLED_VISIBLE_Y -
                    SHOWDUINO_PIXEL_OLED_VISIBLE_H);
#else
  sVisY = SHOWDUINO_PIXEL_OLED_VISIBLE_Y;
#endif
  sVisX = SHOWDUINO_PIXEL_OLED_VISIBLE_X;
  sDisp.setTextWrap(false);
  sPage = 0;
  sSig[0] = 0;
  paint();
  return true;
}

bool pixelDisplayReady() { return sReady; }

void pixelDisplayService() {
  if (!sReady) return;
  char sig[96];
  makeSig(sig, sizeof(sig));
  const uint32_t now = millis();
  if (!strcmp(sig, sSig) && (now - sLastPaint) < SHOWDUINO_PIXEL_OLED_REFRESH_MS) return;
  strncpy(sSig, sig, sizeof(sSig) - 1);
  sLastPaint = now;
  paint();
}

void pixelDisplayForce() {
  sSig[0] = 0;
  pixelDisplayService();
}

void pixelDisplayNextPage() {
  sPage = (uint8_t)((sPage + 1) % 2);
  pixelDisplayForce();
}

uint8_t pixelDisplayPage() { return sPage; }

void pixelDisplayOledTest() {
  if (!sReady) return;
  sDisp.clearDisplay();
  lineAt(0, 0, "OLED TEST");
  lineAt(0, 9, "PIXEL NODE");
  lineAt(0, 18, "SSD1306 0x3C");
  lineAt(0, 27, "VIEWPORT OK");
  sDisp.display();
  delay(250);
  pixelDisplayForce();
}

const char *pixelDisplayController() { return SHOWDUINO_PIXEL_OLED_CONTROLLER; }
