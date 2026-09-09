#include "LampDisplay.h"
#include "LampEngine.h"
#include "LampNodeState.h"
#include "EspNowLampTransport.h"
#include "../BoardConfig.h"

#include <string.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 sDisp(SHOWDUINO_LAMP_OLED_WIDTH, SHOWDUINO_LAMP_OLED_HEIGHT, &Wire, -1);
static bool sReady = false;
static int16_t sVisX = SHOWDUINO_LAMP_OLED_VISIBLE_X;
static int16_t sVisY = SHOWDUINO_LAMP_OLED_VISIBLE_Y;
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

  const ShowduinoLampNodeState st = lampNodeState();
  char l2[20], l3[20], l4[20];

  if (sPage == 1) {
    lineAt(0, 0, "C3 LAMP PINS");
    snprintf(l2, sizeof(l2), "OLED %d/%d", SHOWDUINO_LAMP_OLED_SDA, SHOWDUINO_LAMP_OLED_SCL);
    snprintf(l3, sizeof(l3), "PX GPIO%d x%u", lampEnginePin(), (unsigned)lampEngineCount());
    snprintf(l4, sizeof(l4), "BTN A%d B%d", SHOWDUINO_LAMP_BTN_A, SHOWDUINO_LAMP_BTN_B);
    lineAt(0, 9, l2);
    lineAt(0, 18, l3);
    lineAt(0, 27, l4);
    sDisp.display();
    return;
  }

  lineAt(0, 0, "SHOWDUINO");
  lineAt(0, 9, "C3 LAMP");

  if (st == SHOWDUINO_LAMP_ST_EMERGENCY || lampEngineEmergency()) {
    lineAt(0, 18, "!!! EMERGENCY !!!");
    lineAt(0, 27, "WHITE / IDLE WAIT");
  } else if (st == SHOWDUINO_LAMP_ST_FAULT) {
    snprintf(l3, sizeof(l3), "FAULT %s", lampNodeStateFault());
    lineAt(0, 18, l3);
    lineAt(0, 27, " ");
  } else if (st == SHOWDUINO_LAMP_ST_SEARCHING || st == SHOWDUINO_LAMP_ST_BOOTING) {
    lineAt(0, 18, "SEARCHING");
    lineAt(0, 27, "FOR COMMS...");
  } else if (lampEngineActive()) {
    snprintf(l3, sizeof(l3), "FX: %.12s", lampEngineFxDisplay());
    snprintf(l4, sizeof(l4), "BRI: %u%%", (unsigned)lampEngineBrightness());
    lineAt(0, 18, l3);
    lineAt(0, 27, l4);
  } else {
    snprintf(l3, sizeof(l3), "LINK: %s", lampEspNowHaveComms() ? "ONLINE" : "WAIT");
    snprintf(l4, sizeof(l4), "STATE: %s", showduino_lamp_state_name(st));
    lineAt(0, 18, l3);
    lineAt(0, 27, l4);
  }
  sDisp.display();
}

static void makeSig(char *out, size_t n) {
  snprintf(out, n, "%u|%u|%d|%d|%s|%u|%u",
           (unsigned)lampNodeState(),
           (unsigned)sPage,
           lampEngineActive() ? 1 : 0,
           lampEngineEmergency() ? 1 : 0,
           lampEngineFxToken(),
           (unsigned)lampEngineBrightness(),
           lampEspNowHaveComms() ? 1 : 0);
}

bool lampDisplayBegin() {
  Wire.begin(SHOWDUINO_LAMP_OLED_SDA, SHOWDUINO_LAMP_OLED_SCL);
  Wire.setClock(SHOWDUINO_LAMP_OLED_I2C_HZ);
  sReady = sDisp.begin(SSD1306_SWITCHCAPVCC, SHOWDUINO_LAMP_OLED_ADDR, true, false);
  if (!sReady) return false;

#if SHOWDUINO_LAMP_OLED_ROTATION_180
  sDisp.ssd1306_command(SSD1306_SEGREMAP);
  sDisp.ssd1306_command(SSD1306_COMSCANINC);
  sVisY = (int16_t)(sDisp.height() - SHOWDUINO_LAMP_OLED_VISIBLE_Y -
                    SHOWDUINO_LAMP_OLED_VISIBLE_H);
#else
  sVisY = SHOWDUINO_LAMP_OLED_VISIBLE_Y;
#endif
  sVisX = SHOWDUINO_LAMP_OLED_VISIBLE_X;
  sDisp.setTextWrap(false);
  sPage = 0;
  sSig[0] = 0;
  paint();
  return true;
}

bool lampDisplayReady() { return sReady; }

void lampDisplayService() {
  if (!sReady) return;
  char sig[96];
  makeSig(sig, sizeof(sig));
  const uint32_t now = millis();
  if (!strcmp(sig, sSig) && (now - sLastPaint) < SHOWDUINO_LAMP_OLED_REFRESH_MS) return;
  strncpy(sSig, sig, sizeof(sSig) - 1);
  sLastPaint = now;
  paint();
}

void lampDisplayForce() {
  sSig[0] = 0;
  lampDisplayService();
}

void lampDisplayNextPage() {
  sPage = (uint8_t)((sPage + 1) % 2);
  lampDisplayForce();
}

uint8_t lampDisplayPage() { return sPage; }

void lampDisplayOledTest() {
  if (!sReady) return;
  sDisp.clearDisplay();
  lineAt(0, 0, "OLED TEST");
  lineAt(0, 9, "C3 LAMP");
  lineAt(0, 18, "SSD1306 0x3C");
  lineAt(0, 27, "VIEWPORT OK");
  sDisp.display();
  delay(250);
  lampDisplayForce();
}

const char *lampDisplayController() { return SHOWDUINO_LAMP_OLED_CONTROLLER; }
