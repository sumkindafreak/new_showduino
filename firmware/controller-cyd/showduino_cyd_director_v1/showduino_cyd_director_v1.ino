/*
  =========================================================
  Showduino v1 - CYD Director Test Firmware
  =========================================================

  Target board:
    ESP32-2432S028R / CYD 2.8 inch touchscreen

  Purpose:
    Tonight-ready controller firmware.
    This CYD does not drive show hardware directly.
    It sends serial commands to the Arduino Mega Stage Engine.

  Mega firmware expected:
    firmware/executor-mega/showduino_mega_v1/showduino_mega_v1.ino

  Required libraries:
    - TFT_eSPI
    - XPT2046_Bitbang

  Wiring:
    CYD TX pin 1  -> Mega RX1 pin 19
    CYD RX pin 3  -> Mega TX1 pin 18
    CYD GND       -> Mega GND

  Serial:
    115200 baud
*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "XPT2046_Bitbang.h"

// =========================================================
// Serial pins to Mega
// =========================================================

#define MEGA_RX_PIN 3
#define MEGA_TX_PIN 1
#define MEGA_BAUD_RATE 115200
#define USB_BAUD_RATE 115200

// =========================================================
// Touchscreen pins
// =========================================================

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// =========================================================
// CYD pins
// =========================================================

#define BACKLIGHT_PIN 21
#define CYD_LED_BLUE 17
#define CYD_LED_RED 4
#define CYD_LED_GREEN 16

// =========================================================
// Colours
// =========================================================

#define COL_BG TFT_BLACK
#define COL_HEADER 0x2104
#define COL_PANEL 0x1082
#define COL_TEXT TFT_WHITE
#define COL_DIM 0x8410
#define COL_BUTTON 0x3186
#define COL_ACCENT TFT_CYAN
#define COL_WARN TFT_ORANGE
#define COL_ERROR TFT_RED
#define COL_OK TFT_GREEN

// =========================================================
// Display/touch objects
// =========================================================

TFT_eSPI tft = TFT_eSPI();
XPT2046_Bitbang touch(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS);

// =========================================================
// Button structure
// =========================================================

struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char* label;
  const char* command;
  uint16_t colour;
};

Button buttons[] = {
  {  10,  46, 145, 38, "HEARTBEAT", "HEARTBEAT", COL_BUTTON },
  { 165,  46, 145, 38, "STATUS", "STATUS:REQUEST", COL_BUTTON },

  {  10,  92, 145, 42, "SCENE TEST", "SCENE:TEST", COL_ACCENT },
  { 165,  92, 145, 42, "SCENE STOP", "SCENE:STOP", COL_BUTTON },

  {  10, 142, 145, 38, "PIXEL BLACKOUT", "PIXEL:ALL:BLACKOUT", COL_BUTTON },
  { 165, 142, 145, 38, "AUDIO 001", "AUDIO:PLAY:001", COL_BUTTON },

  {  10, 188,  68, 38, "P1", "PIXEL:1:EFFECT:PULSE", COL_BUTTON },
  {  86, 188,  68, 38, "P2", "PIXEL:2:EFFECT:FIRE", COL_BUTTON },
  { 162, 188,  68, 38, "P3", "PIXEL:3:EFFECT:STROBE", COL_BUTTON },
  { 238, 188,  72, 38, "P4", "PIXEL:4:COLOR:0,0,255", COL_BUTTON },

  {  10, 236, 300, 58, "EMERGENCY STOP", "EMERGENCY:STOP", COL_ERROR }
};

const uint8_t BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0]);

// =========================================================
// State
// =========================================================

String megaBuffer = "";
String lastMegaMessage = "No Mega message yet";
String lastCommandSent = "None";
unsigned long lastHeartbeatMs = 0;
unsigned long lastTouchMs = 0;
bool megaAlive = false;

// =========================================================
// CYD RGB LED helper. Most CYD boards use active LOW LEDs.
// =========================================================

void setCydLed(bool redOn, bool greenOn, bool blueOn) {
  digitalWrite(CYD_LED_RED, redOn ? LOW : HIGH);
  digitalWrite(CYD_LED_GREEN, greenOn ? LOW : HIGH);
  digitalWrite(CYD_LED_BLUE, blueOn ? LOW : HIGH);
}

// =========================================================
// Draw helpers
// =========================================================

void drawHeader() {
  tft.fillRect(0, 0, 320, 36, COL_HEADER);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_TEXT, COL_HEADER);
  tft.drawString("SHOWDUINO v1 DIRECTOR", 8, 18, 2);

  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(megaAlive ? COL_OK : COL_WARN, COL_HEADER);
  tft.drawString(megaAlive ? "MEGA OK" : "MEGA ?", 312, 18, 2);
}

void drawButton(const Button& b) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, b.colour);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, b.colour);
  tft.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2, 2);
}

void drawStatusPanel() {
  tft.fillRect(0, 302, 320, 18, COL_PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("TX:", 4, 311, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  String tx = lastCommandSent;
  if (tx.length() > 30) tx = tx.substring(0, 30);
  tft.drawString(tx, 24, 311, 1);

  tft.fillRect(0, 320 - 18, 320, 18, COL_BG);
}

void drawMegaMessage() {
  tft.fillRect(0, 296, 320, 24, COL_PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("RX:", 4, 308, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  String rx = lastMegaMessage;
  if (rx.length() > 34) rx = rx.substring(0, 34);
  tft.drawString(rx, 24, 308, 1);
}

void drawScreen() {
  tft.fillScreen(COL_BG);
  drawHeader();

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    drawButton(buttons[i]);
  }

  drawMegaMessage();
}

// =========================================================
// Serial commands
// =========================================================

void sendToMega(const String& command) {
  Serial2.println(command);
  Serial.print("[CYD -> MEGA] ");
  Serial.println(command);

  lastCommandSent = command;
  setCydLed(false, false, true);
  delay(20);
  setCydLed(false, false, false);

  drawHeader();
  drawMegaMessage();
}

void handleMegaLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print("[MEGA -> CYD] ");
  Serial.println(line);

  lastMegaMessage = line;

  if (line == "STATUS:ALIVE" || line == "STATUS:READY") {
    megaAlive = true;
    setCydLed(false, true, false);
  }

  if (line == "STATUS:EMERGENCY_ACTIVE") {
    megaAlive = true;
    setCydLed(true, false, false);
  }

  drawHeader();
  drawMegaMessage();
}

void readMegaSerial() {
  while (Serial2.available() > 0) {
    char c = Serial2.read();

    if (c == '\r') continue;

    if (c == '\n') {
      handleMegaLine(megaBuffer);
      megaBuffer = "";
    } else {
      if (megaBuffer.length() < 120) {
        megaBuffer += c;
      } else {
        megaBuffer = "";
      }
    }
  }
}

// =========================================================
// Touch handling
// =========================================================

bool getTouchPoint(int16_t& x, int16_t& y) {
  if (!touch.touched()) return false;

  TS_Point p = touch.getPoint();

  // CYD mapping for landscape rotation can vary by panel.
  // This is a safe starting point. If reversed, swap/invert here.
  x = map(p.x, 200, 3900, 0, 320);
  y = map(p.y, 200, 3900, 0, 240);

  x = constrain(x, 0, 319);
  y = constrain(y, 0, 239);

  return true;
}

void handleTouch() {
  if (millis() - lastTouchMs < 250) return;

  int16_t tx, ty;
  if (!getTouchPoint(tx, ty)) return;

  lastTouchMs = millis();

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    Button& b = buttons[i];

    if (tx >= b.x && tx <= b.x + b.w && ty >= b.y && ty <= b.y + b.h) {
      drawButton(b);
      sendToMega(String(b.command));
      return;
    }
  }
}

// =========================================================
// Periodic heartbeat
// =========================================================

void updateHeartbeat() {
  if (millis() - lastHeartbeatMs >= 5000) {
    lastHeartbeatMs = millis();
    sendToMega("HEARTBEAT");
  }
}

// =========================================================
// Arduino setup/loop
// =========================================================

void setup() {
  Serial.begin(USB_BAUD_RATE);
  Serial2.begin(MEGA_BAUD_RATE, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  pinMode(CYD_LED_RED, OUTPUT);
  pinMode(CYD_LED_GREEN, OUTPUT);
  pinMode(CYD_LED_BLUE, OUTPUT);
  setCydLed(false, false, false);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COL_BG);

  touch.begin();

  Serial.println();
  Serial.println("==============================================");
  Serial.println("Showduino v1 CYD Director Test Firmware");
  Serial.println("==============================================");

  drawScreen();
  sendToMega("HEARTBEAT");
}

void loop() {
  readMegaSerial();
  handleTouch();
  updateHeartbeat();
}
