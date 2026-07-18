/*
  Showduino Touch Probe v2 — ESP32-8048S043 / 8048S050

  Standalone. Shows diagnostics ON SCREEN (Serial optional).
  Tries:
    - I2C Wire + Wire1 on SDA=19 SCL=20 (GT911 capacitive)
    - Raw SPI XPT2046 on CS=38 (resistive) — no extra library needed

  Arduino IDE:
    Board: ESP32S3 Dev Module
    USB CDC On Boot: Enabled
    PSRAM: OPI PSRAM
    Flash Size: 16MB
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>

static const int SCREEN_W = 800;
static const int SCREEN_H = 480;
static const int TFT_BL = 2;

static const int TOUCH_SDA = 19;
static const int TOUCH_SCL = 20;
static const int TOUCH_RST = 38;
static const int TOUCH_INT = 18;  // often NC (R17 empty)

static const int SD_CS = 10;
static const int SPI_MOSI = 11;
static const int SPI_SCK = 12;
static const int SPI_MISO = 13;
static const int XPT_CS = 38;

static const uint8_t GT_A = 0x5D;
static const uint8_t GT_B = 0x14;

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
  40, 41, 39, 42,
  45, 48, 47, 21, 14,
  5, 6, 7, 15, 16, 4,
  8, 3, 46, 9, 1,
  0, 8, 4, 8,
  0, 8, 4, 8,
  1, 16000000, false,
  0, 0, 0
);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(SCREEN_W, SCREEN_H, rgbpanel, 0, true);

enum class Mode : uint8_t { None, Gt911, XptRaw };
Mode mode = Mode::None;

TwoWire *gtBus = nullptr;
uint8_t gtAddr = 0;
String diagLines[12];
int diagCount = 0;

uint16_t lastX = 0, lastY = 0;
bool wasTouched = false;

void logLine(const String &s) {
  Serial.println(s);
  if (diagCount < 12) diagLines[diagCount++] = s;
}

void setBacklight(bool on) {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, on ? HIGH : LOW);
}

void drawDiagScreen(const char *title) {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(12, 8);
  gfx->println(title);

  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(1);
  int y = 44;
  for (int i = 0; i < diagCount; i++) {
    gfx->setCursor(12, y);
    gfx->println(diagLines[i]);
    y += 16;
  }
}

void drawTouchDot(uint16_t x, uint16_t y) {
  static uint16_t px = 0xFFFF, py = 0xFFFF;
  if (px != 0xFFFF) gfx->fillCircle(px, py, 16, RGB565_BLACK);
  gfx->fillCircle(x, y, 16, RGB565_RED);
  gfx->fillRect(0, 0, SCREEN_W, 36, RGB565_BLACK);
  gfx->setTextColor(RGB565_YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(12, 10);
  gfx->printf("TOUCH x=%u y=%u", x, y);
  px = x;
  py = y;
}

void holdSpiQuiet() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(XPT_CS, OUTPUT);
  digitalWrite(XPT_CS, HIGH);
}

// ---- GT911 via raw I2C (no library required for detect/read) ----
void gtResetForAddr(uint8_t wantAddr) {
  // Even if INT is NC on the MCU side, RST must leave the chip out of reset.
  pinMode(TOUCH_RST, OUTPUT);
  pinMode(TOUCH_INT, OUTPUT);

  digitalWrite(TOUCH_INT, LOW);
  digitalWrite(TOUCH_RST, LOW);
  delay(50);

  // 0x5D = INT low during RST release; 0x14 = INT high
  digitalWrite(TOUCH_INT, (wantAddr == GT_B) ? HIGH : LOW);
  delay(5);
  digitalWrite(TOUCH_RST, HIGH);
  delay(80);
  pinMode(TOUCH_INT, INPUT);
  delay(80);
}

void gtRstOnly() {
  // Some FPC boards hard-tie GT911 INT to GND — only pulse RST.
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(50);
  digitalWrite(TOUCH_RST, HIGH);
  delay(120);
}

bool busProbe(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

uint8_t busScan(TwoWire &bus, const char *name) {
  logLine(String("I2C scan ") + name + " SDA19/SCL20");
  uint8_t n = 0;
  String found = "  found:";
  for (uint8_t a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      found += " 0x" + String(a, HEX);
      n++;
    }
  }
  if (n == 0) logLine("  (none)");
  else logLine(found);
  return n;
}

bool gtReadReg(TwoWire &bus, uint8_t addr, uint16_t reg, uint8_t *buf, uint8_t len) {
  bus.beginTransmission(addr);
  bus.write((uint8_t)(reg >> 8));
  bus.write((uint8_t)(reg & 0xFF));
  if (bus.endTransmission(false) != 0) return false;
  uint8_t got = bus.requestFrom(addr, len);
  if (got != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = bus.read();
  return true;
}

bool gtWriteReg(TwoWire &bus, uint8_t addr, uint16_t reg, uint8_t val) {
  bus.beginTransmission(addr);
  bus.write((uint8_t)(reg >> 8));
  bus.write((uint8_t)(reg & 0xFF));
  bus.write(val);
  return bus.endTransmission() == 0;
}

bool tryGtOnBus(TwoWire &bus, const char *name) {
  bus.begin(TOUCH_SDA, TOUCH_SCL);
  bus.setClock(100000);
  bus.setTimeOut(200);

  // Strategy A: RST-only (INT hard-wired on many Suntoms)
  gtRstOnly();
  busScan(bus, String(name) + " after RST");

  uint8_t addr = 0;
  if (busProbe(bus, GT_A)) addr = GT_A;
  else if (busProbe(bus, GT_B)) addr = GT_B;

  // Strategy B: address latch via GPIO18 (only if R17 populated)
  if (addr == 0) {
    gtResetForAddr(GT_A);
    busScan(bus, String(name) + " latch 0x5D");
    if (busProbe(bus, GT_A)) addr = GT_A;
    else if (busProbe(bus, GT_B)) addr = GT_B;
  }
  if (addr == 0) {
    gtResetForAddr(GT_B);
    busScan(bus, String(name) + " latch 0x14");
    if (busProbe(bus, GT_A)) addr = GT_A;
    else if (busProbe(bus, GT_B)) addr = GT_B;
  }

  if (addr == 0) return false;

  // Confirm product ID @ 0x8140
  uint8_t id[4] = {0};
  if (gtReadReg(bus, addr, 0x8140, id, 4)) {
    char pid[5] = {(char)id[0], (char)id[1], (char)id[2], (char)id[3], 0};
    logLine(String("GT911 ID '") + pid + "' @" + name + " 0x" + String(addr, HEX));
  } else {
    logLine(String("GT911 ACK @") + name + " 0x" + String(addr, HEX) + " (no ID)");
  }

  // Clear status register
  gtWriteReg(bus, addr, 0x814E, 0);

  gtBus = &bus;
  gtAddr = addr;
  mode = Mode::Gt911;
  return true;
}

bool tryGt911() {
  holdSpiQuiet();
  logLine("Probing GT911 capacitive...");

  // Keep RST high before bus start
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, HIGH);
  delay(20);

  if (tryGtOnBus(Wire, "Wire0")) return true;
  if (tryGtOnBus(Wire1, "Wire1")) return true;

  logLine("GT911: not found on Wire0/Wire1");
  return false;
}

bool readGt911(uint16_t &x, uint16_t &y) {
  if (gtBus == nullptr || gtAddr == 0) return false;

  uint8_t status = 0;
  if (!gtReadReg(*gtBus, gtAddr, 0x814E, &status, 1)) return false;

  uint8_t touches = status & 0x0F;
  bool bufferReady = (status & 0x80) != 0;
  if (!bufferReady || touches == 0) {
    if (bufferReady) gtWriteReg(*gtBus, gtAddr, 0x814E, 0);
    return false;
  }

  uint8_t pt[7] = {0};
  if (!gtReadReg(*gtBus, gtAddr, 0x814F, pt, 7)) return false;
  gtWriteReg(*gtBus, gtAddr, 0x814E, 0);

  uint16_t rawX = (uint16_t)pt[1] | ((uint16_t)pt[2] << 8);
  uint16_t rawY = (uint16_t)pt[3] | ((uint16_t)pt[4] << 8);

  // Try common orientations — pick one that stays in range first
  x = constrain((int)rawX, 0, SCREEN_W - 1);
  y = constrain((int)rawY, 0, SCREEN_H - 1);

  // If raw looks mirrored for this panel, flip
  if (rawX > SCREEN_W || rawY > SCREEN_H) {
    x = constrain((int)(SCREEN_W - 1 - rawX), 0, SCREEN_W - 1);
    y = constrain((int)(SCREEN_H - 1 - rawY), 0, SCREEN_H - 1);
  }
  return true;
}

// ---- Raw XPT2046 (no library) ----
uint16_t xptTransfer(uint8_t cmd) {
  digitalWrite(XPT_CS, LOW);
  SPI.transfer(cmd);
  uint16_t data = ((uint16_t)SPI.transfer(0x00) << 8) | SPI.transfer(0x00);
  digitalWrite(XPT_CS, HIGH);
  return data >> 3;  // 12-bit left-justified in 16
}

bool tryXptRaw() {
  logLine("Probing XPT2046 resistive (raw SPI)...");
  holdSpiQuiet();

  pinMode(XPT_CS, OUTPUT);
  digitalWrite(XPT_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, XPT_CS);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  // Take a few samples — a live XPT returns non-stuck values
  uint16_t z1 = xptTransfer(0xB1);
  uint16_t z2 = xptTransfer(0xC1);
  uint16_t rx = xptTransfer(0xD1);
  uint16_t ry = xptTransfer(0x91);
  delay(5);
  uint16_t z1b = xptTransfer(0xB1);
  uint16_t rxb = xptTransfer(0xD1);

  SPI.endTransaction();

  logLine("  XPT z1=" + String(z1) + " z2=" + String(z2) + " x=" + String(rx) + " y=" + String(ry));

  // Dead bus usually reads 0 or 4095 constantly
  bool looksAlive = !((z1 == 0 && z1b == 0 && rx == 0 && rxb == 0) ||
                      (z1 >= 4090 && z1b >= 4090 && rx >= 4090 && rxb >= 4090));

  // Also accept if values change between samples (noise/activity)
  if (z1 != z1b || rx != rxb) looksAlive = true;

  if (!looksAlive) {
    logLine("XPT2046: bus looks dead");
    return false;
  }

  mode = Mode::XptRaw;
  logLine("XPT2046 READY — press firmly");
  return true;
}

bool readXptRaw(uint16_t &x, uint16_t &y) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  uint16_t z1 = xptTransfer(0xB1);
  uint16_t z2 = xptTransfer(0xC1);
  uint16_t rx = xptTransfer(0xD0);
  uint16_t ry = xptTransfer(0x90);
  SPI.endTransaction();

  uint16_t z = (z1 > z2) ? (z1 - z2) : 0;
  // Pressure threshold — tune if needed
  if (z < 80) return false;

  x = constrain(map((int)rx, 200, 3900, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
  y = constrain(map((int)ry, 200, 3900, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
  return true;
}

bool readTouch(uint16_t &x, uint16_t &y) {
  if (mode == Mode::Gt911) return readGt911(x, y);
  if (mode == Mode::XptRaw) return readXptRaw(x, y);
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== Touch Probe v2 ===");

  setBacklight(true);
  holdSpiQuiet();

  if (!gfx->begin()) Serial.println("Display begin failed");
  else Serial.println("Display OK");

  diagCount = 0;
  logLine("Display OK — probing touch");
  drawDiagScreen("Touch Probe v2");

  bool ok = tryGt911();
  if (!ok) ok = tryXptRaw();

  drawDiagScreen(ok ? "TOUCH FOUND — press glass" : "NO TOUCH FOUND");

  if (!ok) {
    logLine("Tips: C board needs GT911 on I2C 19/20");
    logLine("R board needs XPT2046 on SPI CS38");
    logLine("Paste Serial log if possible");
    drawDiagScreen("NO TOUCH FOUND — see lines");
  }

  Serial.println("Setup done.");
}

void loop() {
  if (mode == Mode::None) {
    delay(200);
    return;
  }

  uint16_t x = 0, y = 0;
  if (readTouch(x, y)) {
    if (!wasTouched || x != lastX || y != lastY) {
      Serial.printf("TOUCH x=%u y=%u\n", x, y);
      drawTouchDot(x, y);
      lastX = x;
      lastY = y;
    }
    wasTouched = true;
  } else if (wasTouched) {
    Serial.println("RELEASE");
    wasTouched = false;
  }
  delay(20);
}
