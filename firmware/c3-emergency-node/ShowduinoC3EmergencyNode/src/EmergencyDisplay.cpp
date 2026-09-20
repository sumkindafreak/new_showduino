#include "EmergencyDisplay.h"
#include "EmergencyIdentity.h"
#include "EspNowEmergencyTransport.h"
#include "../BoardConfig.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 sDisp(SHOWDUINO_ESTOP_OLED_WIDTH, SHOWDUINO_ESTOP_OLED_HEIGHT, &Wire, -1);
static bool sReady = false;
static int16_t sVisX = SHOWDUINO_ESTOP_OLED_MARGIN_LEFT;
static int16_t sVisY = SHOWDUINO_ESTOP_OLED_MARGIN_TOP;
static uint32_t sLastPaint = 0;

static void lineAt(int16_t y, const char *text) {
  sDisp.setTextSize(1);
  sDisp.setTextColor(SSD1306_WHITE);
  sDisp.setCursor(sVisX, sVisY + y);
  sDisp.print(text ? text : "");
}

bool emergencyDisplayBegin() {
  Wire.begin(SHOWDUINO_ESTOP_OLED_SDA, SHOWDUINO_ESTOP_OLED_SCL);
  Wire.setClock(SHOWDUINO_ESTOP_OLED_I2C_HZ);
  sReady = sDisp.begin(SSD1306_SWITCHCAPVCC, SHOWDUINO_ESTOP_OLED_ADDR, true, false);
  if (!sReady) return false;
#if SHOWDUINO_ESTOP_OLED_ROTATION_180
  sDisp.ssd1306_command(SSD1306_SEGREMAP);
  sDisp.ssd1306_command(SSD1306_COMSCANINC);
  sVisY = (int16_t)(sDisp.height() - SHOWDUINO_ESTOP_OLED_MARGIN_TOP -
                    SHOWDUINO_ESTOP_OLED_VISIBLE_H);
#endif
  sDisp.setTextWrap(false);
  return true;
}

void emergencyDisplayService(const ShowduinoEmergencyMachine *machine) {
  if (!sReady || !machine) return;
  const uint32_t now = millis();
  if (sLastPaint && (now - sLastPaint) < 250UL) return;
  sLastPaint = now;

  sDisp.clearDisplay();
  char line[22];

  lineAt(0, "EMERGENCY NODE");
  snprintf(line, sizeof(line), "%.12s", emergencyIdentityId());
  lineAt(9, line);

  if (machine->latched) {
    lineAt(18, "!! EMERGENCY !!");
  } else if (machine->input_open) {
    lineAt(18, "BUTTON ACTIVE");
  } else {
    lineAt(18, "READY");
  }

  snprintf(line, sizeof(line), "%s CH%u",
           emergencyEspNowHaveComms() ? "LINK OK" : "SEARCH",
           (unsigned)emergencyEspNowChannel());
  lineAt(27, line);
  sDisp.display();
}
