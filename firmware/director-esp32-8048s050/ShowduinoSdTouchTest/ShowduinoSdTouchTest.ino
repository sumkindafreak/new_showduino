/*
  Showduino SD + Touch hardware test
  Board: ESP32-8048S043 / ESP32-8048S050 (800x480 RGB + GT911)

  CRITICAL — Arduino IDE Tools (set again for THIS sketch folder):
  - Board: ESP32S3 Dev Module
  - USB CDC On Boot: Enabled
  - Flash Size: 16MB
  - Flash Mode: QIO 80MHz
  - PSRAM: OPI PSRAM          <--- required (RGB needs ~768KB in PSRAM)
  - CPU Frequency: 240MHz

  Libraries: Arduino_GFX_Library, TAMC_GT911
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TAMC_GT911.h>
#include <esp_heap_caps.h>

// ---- Panel ----
#define SCREEN_W 800
#define SCREEN_H 480
#define TFT_BL   2

#define RGB_DE    40
#define RGB_VSYNC 41
#define RGB_HSYNC 39
#define RGB_PCLK  42
#define RGB_R0 45
#define RGB_R1 48
#define RGB_R2 47
#define RGB_R3 21
#define RGB_R4 14
#define RGB_G0 5
#define RGB_G1 6
#define RGB_G2 7
#define RGB_G3 15
#define RGB_G4 16
#define RGB_G5 4
#define RGB_B0 8
#define RGB_B1 3
#define RGB_B2 46
#define RGB_B3 9
#define RGB_B4 1

// ---- GT911 ----
#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_INT 18
#define TOUCH_RST 38

#define TOUCH_CAL_X_LEFT  790
#define TOUCH_CAL_X_RIGHT  18
#define TOUCH_CAL_Y_TOP   465
#define TOUCH_CAL_Y_BOT    25

// ---- SD SPI ----
#define SD_CS   10
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12
#define SD_SPI_HZ 10000000
#define SD_TEST_PATH "/showduino_test.txt"

// Match working Director panel constructor (framebuffer in PSRAM).
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
  RGB_DE, RGB_VSYNC, RGB_HSYNC, RGB_PCLK,
  RGB_R0, RGB_R1, RGB_R2, RGB_R3, RGB_R4,
  RGB_G0, RGB_G1, RGB_G2, RGB_G3, RGB_G4, RGB_G5,
  RGB_B0, RGB_B1, RGB_B2, RGB_B3, RGB_B4,
  0, 8, 4, 8,
  0, 8, 4, 8,
  1, 16000000, false,
  0, 0, 0
);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
  SCREEN_W, SCREEN_H, rgbpanel, 0, true
);

TAMC_GT911 touch(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, SCREEN_W, SCREEN_H);

bool touchOk = false;
bool sdOk = false;
bool displayOk = false;
String sdMessage = "SD: not tested";
String touchMessage = "Touch: not tested";
uint16_t lastX = 0;
uint16_t lastY = 0;
bool hasTouchPoint = false;
unsigned long touchHits = 0;
unsigned long lastDrawMs = 0;

void haltWithMessage(const char *msg) {
  Serial.println(msg);
  while (true) {
    delay(1000);
    Serial.println(msg);
  }
}

void drawStatusFrame() {
  if (!displayOk) return;
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(20, 20);
  gfx->println("SHOWDUINO SD + TOUCH TEST");

  gfx->setCursor(20, 60);
  gfx->setTextColor(touchOk ? RGB565_GREEN : RGB565_RED);
  gfx->println(touchMessage);

  gfx->setCursor(20, 100);
  gfx->setTextColor(sdOk ? RGB565_GREEN : RGB565_ORANGE);
  gfx->println(sdMessage);

  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(20, 150);
  gfx->println("Touch the screen. Crosshair should track.");
  gfx->setCursor(20, 180);
  gfx->println("Serial Monitor @ 115200 for details.");
}

void softRestoreI2c() {
  // Never Wire.end() — heap corruption with TAMC_GT911 on these boards.
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setTimeOut(100);
  Wire.setClock(400000);
  Serial.println("Touch: I2C soft-restore after SD");
}

bool probeGt911(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(highByte(GT911_PRODUCT_ID));
  Wire.write(lowByte(GT911_PRODUCT_ID));
  if (Wire.endTransmission() != 0) return false;

  uint8_t n = Wire.requestFrom(addr, (uint8_t)4);
  if (n < 4) return false;

  char id[5] = {0};
  for (uint8_t i = 0; i < 4 && Wire.available(); i++) id[i] = (char)Wire.read();
  Serial.printf("Touch: product ID='%s' at 0x%02X\n", id, addr);
  return id[0] == '9' && id[1] == '1' && id[2] == '1';
}

bool initTouch() {
  Serial.println("Touch: probing GT911...");
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setTimeOut(100);
  Wire.setClock(400000);

  uint8_t addr = GT911_ADDR1;  // 0x5D
  touch.begin(addr);
  if (!probeGt911(addr)) {
    addr = GT911_ADDR2;  // 0x14
    touch.begin(addr);
    if (!probeGt911(addr)) {
      touchMessage = "Touch: GT911 NOT FOUND";
      Serial.println(touchMessage);
      return false;
    }
  }

  touch.setRotation(ROTATION_NORMAL);
  touchMessage = String("Touch: OK  I2C 0x") + String(addr, HEX);
  Serial.println(touchMessage);
  return true;
}

bool initSdCard() {
  Serial.println("SD: starting SPI...");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, SPI, SD_SPI_HZ)) {
    sdMessage = "SD: mount FAILED (no card?)";
    Serial.println(sdMessage);
    return false;
  }

  uint8_t type = SD.cardType();
  if (type == CARD_NONE) {
    sdMessage = "SD: no card attached";
    Serial.println(sdMessage);
    return false;
  }

  const char *typeName = "UNKNOWN";
  if (type == CARD_MMC) typeName = "MMC";
  else if (type == CARD_SD) typeName = "SDSC";
  else if (type == CARD_SDHC) typeName = "SDHC";

  uint64_t sizeMb = SD.cardSize() / (1024ULL * 1024ULL);
  Serial.printf("SD: type=%s size=%llu MB\n", typeName, (unsigned long long)sizeMb);

  File w = SD.open(SD_TEST_PATH, FILE_WRITE);
  if (!w) {
    sdMessage = "SD: open write FAILED";
    Serial.println(sdMessage);
    return false;
  }
  String payload = String("showduino ok ") + String(millis());
  w.println(payload);
  w.flush();
  w.close();

  File r = SD.open(SD_TEST_PATH, FILE_READ);
  if (!r) {
    sdMessage = "SD: open read FAILED";
    Serial.println(sdMessage);
    return false;
  }
  String got = r.readStringUntil('\n');
  got.trim();
  r.close();

  if (got != payload) {
    sdMessage = "SD: readback MISMATCH";
    Serial.printf("SD: wrote '%s' read '%s'\n", payload.c_str(), got.c_str());
    return false;
  }

  sdMessage = String("SD: OK  ") + typeName + "  " + String((uint32_t)sizeMb) + "MB  RW pass";
  Serial.println(sdMessage);
  return true;
}

bool readTouch(uint16_t &x, uint16_t &y) {
  if (!touchOk) return false;
  touch.read();
  if (!touch.isTouched) return false;

  int rawX = touch.points[0].x;
  int rawY = touch.points[0].y;
  int mx = map(rawX, TOUCH_CAL_X_LEFT, TOUCH_CAL_X_RIGHT, 0, SCREEN_W - 1);
  int my = map(rawY, TOUCH_CAL_Y_TOP, TOUCH_CAL_Y_BOT, 0, SCREEN_H - 1);
  mx = constrain(mx, 0, SCREEN_W - 1);
  my = constrain(my, 0, SCREEN_H - 1);
  x = (uint16_t)mx;
  y = (uint16_t)my;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== Showduino SD + Touch Test ===");

  Serial.printf("Heap free: %u\n", ESP.getFreeHeap());
  Serial.printf("PSRAM size: %u\n", ESP.getPsramSize());
  Serial.printf("PSRAM free: %u\n", ESP.getFreePsram());

  // RGB 800x480 needs a framebuffer in PSRAM. Without OPI PSRAM this aborts.
  if (ESP.getPsramSize() == 0) {
    haltWithMessage(
      "FATAL: PSRAM=0. In Arduino IDE Tools set PSRAM to 'OPI PSRAM', then re-upload."
    );
  }

  size_t largestPsram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  Serial.printf("Largest PSRAM block: %u (need ~768000)\n", (unsigned)largestPsram);
  if (largestPsram < 800000) {
    haltWithMessage(
      "FATAL: Not enough free PSRAM for RGB framebuffer. Enable OPI PSRAM and re-upload."
    );
  }

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // 1) Touch first
  touchOk = initTouch();

  // 2) Display (needs PSRAM)
  Serial.println("Display: starting...");
  if (!gfx->begin()) {
    haltWithMessage("FATAL: gfx->begin() failed");
  }
  displayOk = true;
  Serial.println("Display: OK");
  gfx->fillScreen(RGB565_BLACK);

  // 3) SD then soft-restore I2C
  sdOk = initSdCard();
  softRestoreI2c();

  if (touchOk) {
    if (!probeGt911(GT911_ADDR1) && !probeGt911(GT911_ADDR2)) {
      touchOk = false;
      touchMessage = "Touch: LOST after SD";
      Serial.println(touchMessage);
    } else {
      touchMessage += "  (ok after SD)";
      Serial.println("Touch: still OK after SD");
    }
  }

  drawStatusFrame();
  Serial.println("Setup done. Touch the panel.");
}

void loop() {
  if (!displayOk) {
    delay(100);
    return;
  }

  uint16_t x = 0, y = 0;
  if (readTouch(x, y)) {
    if (!hasTouchPoint || x != lastX || y != lastY) {
      if (hasTouchPoint) {
        gfx->drawFastHLine(0, lastY, SCREEN_W, RGB565_BLACK);
        gfx->drawFastVLine(lastX, 220, SCREEN_H - 220, RGB565_BLACK);
      }
      lastX = x;
      lastY = y;
      hasTouchPoint = true;
      touchHits++;

      gfx->drawFastHLine(0, y, SCREEN_W, RGB565_YELLOW);
      gfx->drawFastVLine(x, 220, SCREEN_H - 220, RGB565_YELLOW);
      gfx->fillCircle(x, y, 8, RGB565_RED);

      gfx->fillRect(20, 220, 400, 40, RGB565_BLACK);
      gfx->setTextColor(RGB565_WHITE);
      gfx->setTextSize(2);
      gfx->setCursor(20, 230);
      gfx->printf("XY %d,%d  hits %lu", x, y, touchHits);
      Serial.printf("Touch: %d,%d\n", x, y);
    }
  }

  if (millis() - lastDrawMs > 3000) {
    lastDrawMs = millis();
    gfx->fillRect(20, 60, 760, 80, RGB565_BLACK);
    gfx->setTextSize(2);
    gfx->setCursor(20, 60);
    gfx->setTextColor(touchOk ? RGB565_GREEN : RGB565_RED);
    gfx->println(touchMessage);
    gfx->setCursor(20, 100);
    gfx->setTextColor(sdOk ? RGB565_GREEN : RGB565_ORANGE);
    gfx->println(sdMessage);
  }

  delay(10);
}
