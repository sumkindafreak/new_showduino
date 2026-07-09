/*
  Showduino Director 5" - ESP32-8048S050 / ESP32-S3 RGB display

  Clean Showduino/GoreFX base firmware.

  Required Arduino libraries:
  - lvgl 9.x
  - Arduino_GFX_Library
  - TAMC_GT911
  - ESP32 Arduino core libraries: SPI, SD, FS, Wire

  Recommended Arduino IDE settings:
  - Board: ESP32S3 Dev Module
  - USB CDC On Boot: Enabled
  - CPU Frequency: 240MHz
  - Flash Size: 16MB
  - Flash Mode: QIO 80MHz
  - PSRAM: OPI PSRAM / Enabled
  - Serial Monitor: 115200 baud
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Wire.h>
#include <lvgl.h>
#include <TAMC_GT911.h>
#include <esp_heap_caps.h>

#include "BoardConfig.h"
#include "TouchDriver.h"
#include "ShowduinoUi.h"

// =========================================================
// Display objects using the manufacturer's RGB pin map
// =========================================================
Arduino_ESP32RGBPanel *rgbBus = new Arduino_ESP32RGBPanel(
  GFX_NOT_DEFINED, GFX_NOT_DEFINED, GFX_NOT_DEFINED,
  RGB_DE_PIN, RGB_VSYNC_PIN, RGB_HSYNC_PIN, RGB_PCLK_PIN,
  RGB_R0_PIN, RGB_R1_PIN, RGB_R2_PIN, RGB_R3_PIN, RGB_R4_PIN,
  RGB_G0_PIN, RGB_G1_PIN, RGB_G2_PIN, RGB_G3_PIN, RGB_G4_PIN, RGB_G5_PIN,
  RGB_B0_PIN, RGB_B1_PIN, RGB_B2_PIN, RGB_B3_PIN, RGB_B4_PIN
);

Arduino_RPi_DPI_RGBPanel *gfx = new Arduino_RPi_DPI_RGBPanel(
  rgbBus,
  SCREEN_WIDTH, RGB_HSYNC_POLARITY, RGB_HSYNC_FRONT, RGB_HSYNC_PULSE, RGB_HSYNC_BACK,
  SCREEN_HEIGHT, RGB_VSYNC_POLARITY, RGB_VSYNC_FRONT, RGB_VSYNC_PULSE, RGB_VSYNC_BACK,
  RGB_PCLK_ACTIVE_NEG, RGB_PREFER_SPEED, true
);

// =========================================================
// Global objects and variables
// =========================================================
static lv_display_t *displayHandle = nullptr;
static lv_indev_t *touchInputHandle = nullptr;
static lv_color_t *drawBuffer = nullptr;

ShowduinoTouchDriver touchDriver;
ShowduinoUi ui;

String usbInputBuffer;
String stageInputBuffer;

unsigned long lastHeartbeatMs = 0;
unsigned long lastHelloMs = 0;
unsigned long lastUiRefreshMs = 0;
unsigned long lastLvglTickMs = 0;
unsigned long bootMs = 0;

bool stageReady = false;
bool emergencyLocked = false;
uint32_t txCount = 0;
uint32_t rxCount = 0;

// =========================================================
// Backlight helper
// =========================================================
void setBacklight(bool on) {
  digitalWrite(TFT_BL_PIN, on ? TFT_BL_ON : TFT_BL_OFF);
  Serial.println(on ? "Backlight: ON" : "Backlight: OFF");
}

// =========================================================
// SD card helper
// =========================================================
void initSdCard() {
  Serial.println("SD: starting SPI bus...");
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, SPI, 10000000)) {
    Serial.println("SD: no card mounted or mount failed.");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("SD: no card attached.");
    return;
  }

  Serial.print("SD: card type = ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");

  uint64_t cardSizeMb = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD: card size = %llu MB\n", cardSizeMb);
}

// =========================================================
// LVGL display flush callback
// Sends LVGL's rendered pixels to the RGB panel.
// =========================================================
void lvglFlush(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap) {
  uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
  uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)pixelMap, width, height);
  lv_display_flush_ready(display);
}

// =========================================================
// LVGL touch read callback
// Converts GT911 touch into LVGL pointer events.
// =========================================================
void lvglTouchRead(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t x = 0;
  uint16_t y = 0;

  if (touchDriver.read(x, y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// =========================================================
// Stage Engine serial command sender
// =========================================================
void sendToStage(const String &command) {
  Serial1.println(command);
  txCount++;

  if (command == "EMERGENCY:STOP") emergencyLocked = true;
  if (command == "EMERGENCY:CLEAR") emergencyLocked = false;

  ui.setTraffic(txCount, rxCount);
  ui.setEmergencyLocked(emergencyLocked);
  ui.appendLog("TX -> Stage: " + command);
}

void handleUiCommand(const String &command) {
  sendToStage(command);
}

// =========================================================
// Stage Engine response parser
// =========================================================
void handleStageLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  rxCount++;
  ui.appendLog("RX <- Stage: " + line);

  if (line == "READY") stageReady = true;
  if (line == "STATUS:READY") stageReady = true;
  if (line == "STATUS:EMERGENCY_LOCKED") emergencyLocked = true;
  if (line == "STATUS:EMERGENCY_CLEARED") emergencyLocked = false;

  ui.setStageReady(stageReady);
  ui.setEmergencyLocked(emergencyLocked);
  ui.setTraffic(txCount, rxCount);
}

void readStageSerial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\n' || c == '\r') {
      if (stageInputBuffer.length() > 0) {
        handleStageLine(stageInputBuffer);
        stageInputBuffer = "";
      }
    } else {
      stageInputBuffer += c;
      if (stageInputBuffer.length() > 220) {
        stageInputBuffer = "";
        ui.appendLog("Stage response too long; buffer cleared.");
      }
    }
  }
}

// =========================================================
// USB Serial command bridge for bench testing
// =========================================================
void handleUsbLine(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command == "HELP") {
    ui.appendLog("USB commands: HELLO, STATUS:REQUEST, SHOW:START, SHOW:STOP, RELAY:1:ON, RELAY:1:OFF, EMERGENCY:STOP, EMERGENCY:CLEAR");
    return;
  }

  sendToStage(command);
}

void readUsbSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (usbInputBuffer.length() > 0) {
        handleUsbLine(usbInputBuffer);
        usbInputBuffer = "";
      }
    } else {
      usbInputBuffer += c;
      if (usbInputBuffer.length() > 180) {
        usbInputBuffer = "";
        ui.appendLog("USB command too long; buffer cleared.");
      }
    }
  }
}

// =========================================================
// Automatic Stage Engine heartbeat / hello
// =========================================================
void sendHeartbeatIfDue() {
  unsigned long now = millis();
  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    sendToStage("HEARTBEAT");
  }
}

void sendHelloIfNeeded() {
  if (stageReady) return;

  unsigned long now = millis();
  if (now - lastHelloMs >= HELLO_RETRY_INTERVAL_MS) {
    lastHelloMs = now;
    sendToStage("HELLO");
  }
}

// =========================================================
// Setup
// =========================================================
void setup() {
  Serial.begin(USB_DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("Showduino Director 5in - ESP32-8048S050 starting...");

  pinMode(TFT_BL_PIN, OUTPUT);
  setBacklight(true);

  Serial.println("Display: starting RGB panel...");
  if (!gfx->begin()) Serial.println("Display: gfx->begin() failed.");
  else Serial.println("Display: RGB panel ready.");

  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(20, 20);
  gfx->println("Showduino 5in booting...");

  initSdCard();
  touchDriver.begin();

  Serial.println("LVGL: starting...");
  lv_init();

  size_t bufferPixels = SCREEN_WIDTH * LVGL_BUFFER_LINES;
  drawBuffer = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * bufferPixels, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (drawBuffer == nullptr) {
    Serial.println("LVGL: PSRAM buffer failed, trying internal RAM...");
    drawBuffer = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * bufferPixels, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }

  if (drawBuffer == nullptr) {
    Serial.println("LVGL: draw buffer allocation failed. Halting.");
    while (true) delay(1000);
  }

  displayHandle = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(displayHandle, lvglFlush);
  lv_display_set_buffers(displayHandle, drawBuffer, nullptr, sizeof(lv_color_t) * bufferPixels, LV_DISPLAY_RENDER_MODE_PARTIAL);

  touchInputHandle = lv_indev_create();
  lv_indev_set_type(touchInputHandle, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touchInputHandle, lvglTouchRead);

  bootMs = millis();
  ui.setBootTime(bootMs);
  ui.begin(handleUiCommand);
  ui.appendLog("Showduino OS online on ESP32-8048S050.");

  Serial1.begin(STAGE_ENGINE_BAUD, SERIAL_8N1, STAGE_ENGINE_RX_PIN, STAGE_ENGINE_TX_PIN);
  Serial.printf("Stage UART: RX=%d TX=%d baud=%d\n", STAGE_ENGINE_RX_PIN, STAGE_ENGINE_TX_PIN, STAGE_ENGINE_BAUD);

  lastHeartbeatMs = millis();
  lastHelloMs = 0;
  lastUiRefreshMs = millis();
  lastLvglTickMs = millis();

  sendToStage("HELLO");
  Serial.println("Setup complete. Type HELP in Serial Monitor for bench commands.");
}

// =========================================================
// Main loop
// =========================================================
void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastLvglTickMs;
  lastLvglTickMs = now;

  lv_tick_inc(elapsed);

  readUsbSerial();
  readStageSerial();
  sendHeartbeatIfDue();
  sendHelloIfNeeded();

  if (now - lastUiRefreshMs >= UI_REFRESH_INTERVAL_MS) {
    lastUiRefreshMs = now;
    ui.setStageReady(stageReady);
    ui.setEmergencyLocked(emergencyLocked);
    ui.setTraffic(txCount, rxCount);
    ui.updateStatusWidgets();
  }

  lv_timer_handler();
  delay(5);
}
