#include "EmergencyDisplay.h"
#include "EmergencyIdentity.h"
#include "EmergencyProtocol.h"
#include "EmergencyInput.h"
#include "EspNowEmergencyTransport.h"
#include "../BoardConfig.h"

#include <string.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 sDisp(SHOWDUINO_ESTOP_OLED_WIDTH, SHOWDUINO_ESTOP_OLED_HEIGHT,
                              &Wire, -1);
static bool sReady = false;
static int16_t sVisX = SHOWDUINO_ESTOP_OLED_VISIBLE_X;
static int16_t sVisY = SHOWDUINO_ESTOP_OLED_VISIBLE_Y;
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

  const ShowduinoEmergencyNodeState st = gEmergencyMachine.state;
  const int latched = gEmergencyMachine.latched ? 1 : 0;
  const int pressed = emergencyInputPressed();
  const int linked = emergencyEspNowHaveComms() ? 1 : 0;
  char l1[20], l2[20], l3[20], l4[20];

  if (st == SHOWDUINO_ESTOP_ST_FAULT) {
    lineAt(0, 0, "ESTOP NODE");
    lineAt(0, 9, "FAULT");
    snprintf(l3, sizeof(l3), "%.12s", emergencyIdentityId());
    lineAt(0, 18, l3);
    lineAt(0, 27, "CHECK SYSTEM");
  } else if (latched || pressed) {
    lineAt(0, 0, "EMERGENCY");
    if (pressed) {
      lineAt(0, 9, "BUTTON HELD");
      snprintf(l3, sizeof(l3), "%.12s", emergencyIdentityId());
      lineAt(0, 18, l3);
      lineAt(0, 27, linked ? "ASSERTED" : "LINK LOST");
    } else if (!linked) {
      snprintf(l2, sizeof(l2), "%.12s", emergencyIdentityId());
      lineAt(0, 9, l2);
      lineAt(0, 18, "LATCHED");
      lineAt(0, 27, "LINK LOST");
    } else {
      snprintf(l2, sizeof(l2), "%.12s", emergencyIdentityId());
      lineAt(0, 9, l2);
      lineAt(0, 18, "ASSERTED");
      lineAt(0, 27, "LINK OK");
    }
  } else if (!linked || st == SHOWDUINO_ESTOP_ST_SEARCHING ||
             st == SHOWDUINO_ESTOP_ST_BOOTING) {
    snprintf(l1, sizeof(l1), "%.12s", emergencyIdentityId());
    lineAt(0, 0, l1);
    lineAt(0, 9, "SEARCHING");
    lineAt(0, 18, "FOR COMMS...");
    lineAt(0, 27, "READY");
  } else {
    snprintf(l1, sizeof(l1), "%.12s", emergencyIdentityId());
    snprintf(l2, sizeof(l2), "%.12s", emergencyIdentityName());
    lineAt(0, 0, l1);
    lineAt(0, 9, l2);
    lineAt(0, 18, "READY");
    lineAt(0, 27, "LINK OK");
  }

  (void)l4;
  sDisp.display();
}

static void makeSig(char *out, size_t n) {
  snprintf(out, n, "%u|%u|%d|%d|%d|%s|%s",
           (unsigned)gEmergencyMachine.state,
           (unsigned)gEmergencyMachine.latched,
           emergencyInputPressed(),
           emergencyEspNowHaveComms() ? 1 : 0,
           (int)gEmergencyMachine.pending_assert,
           emergencyIdentityId(),
           emergencyIdentityName());
}

bool emergencyDisplayBegin() {
  Wire.begin(SHOWDUINO_ESTOP_OLED_SDA, SHOWDUINO_ESTOP_OLED_SCL);
  Wire.setClock(SHOWDUINO_ESTOP_OLED_I2C_HZ);
  sReady = sDisp.begin(SSD1306_SWITCHCAPVCC, SHOWDUINO_ESTOP_OLED_ADDR, true, false);
  if (!sReady) {
    Serial.println("[ESTOP-OLED] init FAIL — emergency continues without display");
    return false;
  }

#if SHOWDUINO_ESTOP_OLED_ROTATION_180
  sDisp.ssd1306_command(SSD1306_SEGREMAP);
  sDisp.ssd1306_command(SSD1306_COMSCANINC);
  sVisY = (int16_t)(sDisp.height() - SHOWDUINO_ESTOP_OLED_VISIBLE_Y -
                    SHOWDUINO_ESTOP_OLED_VISIBLE_H);
#else
  sVisY = SHOWDUINO_ESTOP_OLED_VISIBLE_Y;
#endif
  sVisX = SHOWDUINO_ESTOP_OLED_VISIBLE_X;
  sDisp.setTextWrap(false);
  sSig[0] = 0;
  paint();
  Serial.printf("[ESTOP-OLED] SSD1306 0x%02X SDA=%d SCL=%d READY\n",
                (unsigned)SHOWDUINO_ESTOP_OLED_ADDR,
                SHOWDUINO_ESTOP_OLED_SDA, SHOWDUINO_ESTOP_OLED_SCL);
  return true;
}

bool emergencyDisplayReady() { return sReady; }

void emergencyDisplayService() {
  if (!sReady) return;
  char sig[96];
  makeSig(sig, sizeof(sig));
  const uint32_t now = millis();
  if (!strcmp(sig, sSig) &&
      (now - sLastPaint) < SHOWDUINO_ESTOP_NODE_OLED_REFRESH_MS) {
    return;
  }
  strncpy(sSig, sig, sizeof(sSig) - 1);
  sSig[sizeof(sSig) - 1] = 0;
  sLastPaint = now;
  paint();
}

void emergencyDisplayForce() {
  sSig[0] = 0;
  emergencyDisplayService();
}

void emergencyDisplayOledTest() {
  if (!sReady) return;
  sDisp.clearDisplay();
  lineAt(0, 0, "OLED TEST");
  lineAt(0, 9, "ESTOP NODE");
  lineAt(0, 18, "SSD1306 0x3C");
  lineAt(0, 27, "VIEWPORT OK");
  sDisp.display();
  delay(250);
  emergencyDisplayForce();
}

const char *emergencyDisplayController() { return SHOWDUINO_ESTOP_OLED_CONTROLLER; }
