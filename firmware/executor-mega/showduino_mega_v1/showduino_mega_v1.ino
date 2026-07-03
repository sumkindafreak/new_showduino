/*
  =========================================================
  Showduino v1 - Mega Executor Weekend Hardware Stack
  =========================================================

  Target board:
    Arduino Mega 2560

  v1 hardware connected to Mega:
    - 4 NeoPixel pixel lines
    - SD card reader
    - RTC module over I2C
    - Audio control abstraction for local SD/PCM-style audio hardware

  Controller:
    ESP32 CYD touchscreen talks to this Mega over Serial1.

  Serial protocol examples:
    HEARTBEAT
    STATUS:REQUEST
    SD:STATUS
    RTC:STATUS
    PIXEL:ALL:BLACKOUT
    PIXEL:1:COLOR:255,0,0
    PIXEL:1:EFFECT:PULSE
    PIXEL:1:EFFECT:FIRE
    PIXEL:1:EFFECT:STROBE
    AUDIO:PLAY:001
    AUDIO:STOP
    AUDIO:VOLUME:80
    SCENE:TEST
    SCENE:STOP
    EMERGENCY:STOP
    EMERGENCY:CLEAR

  Notes:
    - This sketch is intentionally non-blocking.
    - Audio is currently an abstraction/log layer until the exact audio board is chosen.
    - A bare PCM5102A normally needs I2S audio data; Mega does not provide ESP32-style native I2S.
      So this firmware gives us the command structure ready to map onto the final audio hardware.
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>

// =========================================================
// Serial configuration
// =========================================================

#define USB_BAUD_RATE 115200
#define CYD_BAUD_RATE 115200

// Mega Serial1 pins:
// RX1 = 19, TX1 = 18
// Connect CYD TX -> Mega RX1(19)
// Connect CYD RX -> Mega TX1(18)
// GND must be common.

// =========================================================
// SD card configuration
// =========================================================

#define SD_CS_PIN 53

// Mega hardware SPI pins:
// MISO = 50
// MOSI = 51
// SCK  = 52
// CS   = 53 by default here

// =========================================================
// RTC configuration
// =========================================================

// Mega I2C pins:
// SDA = 20
// SCL = 21

RTC_DS3231 rtc;
bool rtcPresent = false;
bool sdPresent = false;

// =========================================================
// Pixel configuration
// =========================================================

#define PIXEL_LINE_COUNT 4
#define PIXELS_PER_LINE 60

#define PIXEL_LINE_1_PIN 4
#define PIXEL_LINE_2_PIN 5
#define PIXEL_LINE_3_PIN 15
#define PIXEL_LINE_4_PIN 16

#define DEFAULT_BRIGHTNESS 120
#define PIXEL_UPDATE_INTERVAL_MS 30

Adafruit_NeoPixel pixels1(PIXELS_PER_LINE, PIXEL_LINE_1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels2(PIXELS_PER_LINE, PIXEL_LINE_2_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels3(PIXELS_PER_LINE, PIXEL_LINE_3_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels4(PIXELS_PER_LINE, PIXEL_LINE_4_PIN, NEO_GRB + NEO_KHZ800);

Adafruit_NeoPixel* pixelLines[PIXEL_LINE_COUNT] = {
  &pixels1,
  &pixels2,
  &pixels3,
  &pixels4
};

const uint8_t pixelPins[PIXEL_LINE_COUNT] = {
  PIXEL_LINE_1_PIN,
  PIXEL_LINE_2_PIN,
  PIXEL_LINE_3_PIN,
  PIXEL_LINE_4_PIN
};

enum PixelEffectType {
  FX_OFF,
  FX_SOLID,
  FX_PULSE,
  FX_FIRE,
  FX_STROBE
};

struct PixelLineState {
  PixelEffectType effect;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  uint8_t speed;
  unsigned long effectStartMs;
  unsigned long durationMs;
  bool active;
  bool strobeOn;
};

PixelLineState pixelState[PIXEL_LINE_COUNT];
unsigned long lastPixelUpdateMs = 0;

// =========================================================
// Scene test state
// =========================================================

bool sceneRunning = false;
unsigned long sceneStartMs = 0;
bool sceneCueAudioStarted = false;
bool sceneCueBuildStarted = false;
bool sceneCueStrobeStarted = false;
bool sceneCueBlackoutDone = false;

// =========================================================
// Emergency state
// =========================================================

bool emergencyActive = false;

// =========================================================
// Audio abstraction state
// =========================================================

bool audioPlaying = false;
uint8_t audioVolume = 80;
char currentTrack[8] = "000";

// =========================================================
// Serial command buffers
// =========================================================

String cydCommandBuffer = "";
String usbCommandBuffer = "";

// =========================================================
// Helper: send response to CYD and USB debug
// =========================================================

void sendResponse(const String& message) {
  Serial1.println(message);
  Serial.print(F("[OUT] "));
  Serial.println(message);
}

void debugPrint(const String& message) {
  Serial.print(F("[MEGA] "));
  Serial.println(message);
}

// =========================================================
// Helper: colour packing for NeoPixels
// =========================================================

uint32_t makeColor(Adafruit_NeoPixel* strip, uint8_t r, uint8_t g, uint8_t b) {
  return strip->Color(r, g, b);
}

// =========================================================
// Pixel functions
// =========================================================

void setPixelLineOff(uint8_t lineIndex) {
  if (lineIndex >= PIXEL_LINE_COUNT) return;

  pixelState[lineIndex].effect = FX_OFF;
  pixelState[lineIndex].active = false;

  Adafruit_NeoPixel* strip = pixelLines[lineIndex];
  strip->clear();
  strip->show();
}

void blackoutAllPixels() {
  for (uint8_t i = 0; i < PIXEL_LINE_COUNT; i++) {
    setPixelLineOff(i);
  }
  debugPrint(F("All pixel lines blacked out"));
}

void setPixelSolid(uint8_t lineIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  if (lineIndex >= PIXEL_LINE_COUNT) return;

  pixelState[lineIndex].effect = FX_SOLID;
  pixelState[lineIndex].red = r;
  pixelState[lineIndex].green = g;
  pixelState[lineIndex].blue = b;
  pixelState[lineIndex].brightness = brightness;
  pixelState[lineIndex].speed = 50;
  pixelState[lineIndex].durationMs = 0;
  pixelState[lineIndex].effectStartMs = millis();
  pixelState[lineIndex].active = true;

  Adafruit_NeoPixel* strip = pixelLines[lineIndex];
  strip->setBrightness(brightness);
  uint32_t c = makeColor(strip, r, g, b);

  for (uint16_t i = 0; i < strip->numPixels(); i++) {
    strip->setPixelColor(i, c);
  }
  strip->show();
}

void startPixelEffect(uint8_t lineIndex, PixelEffectType effect, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, uint8_t speed, unsigned long durationMs) {
  if (lineIndex >= PIXEL_LINE_COUNT) return;

  pixelState[lineIndex].effect = effect;
  pixelState[lineIndex].red = r;
  pixelState[lineIndex].green = g;
  pixelState[lineIndex].blue = b;
  pixelState[lineIndex].brightness = brightness;
  pixelState[lineIndex].speed = speed;
  pixelState[lineIndex].durationMs = durationMs;
  pixelState[lineIndex].effectStartMs = millis();
  pixelState[lineIndex].active = true;
  pixelState[lineIndex].strobeOn = false;

  pixelLines[lineIndex]->setBrightness(brightness);
}

void updatePixelPulse(uint8_t lineIndex, unsigned long now) {
  Adafruit_NeoPixel* strip = pixelLines[lineIndex];
  PixelLineState& s = pixelState[lineIndex];

  unsigned long elapsed = now - s.effectStartMs;
  uint16_t wave = (elapsed / max<uint8_t>(1, s.speed)) % 512;
  uint8_t level = wave < 256 ? wave : 511 - wave;

  uint8_t r = (uint16_t)s.red * level / 255;
  uint8_t g = (uint16_t)s.green * level / 255;
  uint8_t b = (uint16_t)s.blue * level / 255;

  uint32_t c = makeColor(strip, r, g, b);
  for (uint16_t i = 0; i < strip->numPixels(); i++) {
    strip->setPixelColor(i, c);
  }
  strip->show();
}

void updatePixelFire(uint8_t lineIndex, unsigned long now) {
  Adafruit_NeoPixel* strip = pixelLines[lineIndex];
  PixelLineState& s = pixelState[lineIndex];

  randomSeed(analogRead(A0) + now);

  for (uint16_t i = 0; i < strip->numPixels(); i++) {
    uint8_t flicker = random(80, 255);
    uint8_t r = (uint16_t)s.red * flicker / 255;
    uint8_t g = (uint16_t)s.green * random(20, flicker) / 255;
    uint8_t b = (uint16_t)s.blue * random(0, 80) / 255;
    strip->setPixelColor(i, makeColor(strip, r, g, b));
  }

  strip->show();
}

void updatePixelStrobe(uint8_t lineIndex, unsigned long now) {
  Adafruit_NeoPixel* strip = pixelLines[lineIndex];
  PixelLineState& s = pixelState[lineIndex];

  uint16_t interval = max<uint8_t>(20, 255 - s.speed);
  bool shouldBeOn = ((now - s.effectStartMs) / interval) % 2 == 0;

  if (shouldBeOn != s.strobeOn) {
    s.strobeOn = shouldBeOn;
    uint32_t c = shouldBeOn ? makeColor(strip, s.red, s.green, s.blue) : makeColor(strip, 0, 0, 0);

    for (uint16_t i = 0; i < strip->numPixels(); i++) {
      strip->setPixelColor(i, c);
    }
    strip->show();
  }
}

void updatePixels() {
  unsigned long now = millis();

  if (now - lastPixelUpdateMs < PIXEL_UPDATE_INTERVAL_MS) {
    return;
  }
  lastPixelUpdateMs = now;

  for (uint8_t i = 0; i < PIXEL_LINE_COUNT; i++) {
    PixelLineState& s = pixelState[i];

    if (!s.active) continue;

    if (s.durationMs > 0 && now - s.effectStartMs >= s.durationMs) {
      setPixelLineOff(i);
      continue;
    }

    switch (s.effect) {
      case FX_PULSE:
        updatePixelPulse(i, now);
        break;

      case FX_FIRE:
        updatePixelFire(i, now);
        break;

      case FX_STROBE:
        updatePixelStrobe(i, now);
        break;

      case FX_SOLID:
      case FX_OFF:
      default:
        break;
    }
  }
}

// =========================================================
// Audio abstraction functions
// =========================================================

void audioPlay(const String& track) {
  audioPlaying = true;
  track.toCharArray(currentTrack, sizeof(currentTrack));

  debugPrint(String(F("Audio PLAY requested for track ")) + track);
  debugPrint(F("Audio hardware mapping still needs final board interface"));

  sendResponse(String(F("OK:AUDIO:PLAY:")) + track);
}

void audioStop() {
  audioPlaying = false;
  strcpy(currentTrack, "000");

  debugPrint(F("Audio STOP requested"));
  sendResponse(F("OK:AUDIO:STOP"));
}

void audioSetVolume(uint8_t volume) {
  audioVolume = constrain(volume, 0, 100);
  debugPrint(String(F("Audio volume set to ")) + audioVolume);
  sendResponse(String(F("OK:AUDIO:VOLUME:")) + audioVolume);
}

void reportAudioStatus() {
  String status = F("STATUS:AUDIO:");
  status += audioPlaying ? F("PLAYING:") : F("STOPPED:");
  status += currentTrack;
  status += F(":VOL:");
  status += audioVolume;
  sendResponse(status);
}

// =========================================================
// SD and RTC status functions
// =========================================================

void reportSDStatus() {
  if (sdPresent) {
    sendResponse(F("STATUS:SD:OK"));
  } else {
    sendResponse(F("STATUS:SD:FAIL"));
  }
}

void reportRTCStatus() {
  if (!rtcPresent) {
    sendResponse(F("STATUS:RTC:FAIL"));
    return;
  }

  DateTime now = rtc.now();
  char buffer[40];
  snprintf(buffer, sizeof(buffer), "STATUS:RTC:%04d-%02d-%02d:%02d:%02d:%02d",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  sendResponse(String(buffer));
}

void reportFullStatus() {
  sendResponse(F("STATUS:ALIVE"));
  reportSDStatus();
  reportRTCStatus();
  reportAudioStatus();
}

// =========================================================
// Scene test
// =========================================================

void startTestScene() {
  if (emergencyActive) {
    sendResponse(F("ERR:SCENE:EMERGENCY_ACTIVE"));
    return;
  }

  sceneRunning = true;
  sceneStartMs = millis();
  sceneCueAudioStarted = false;
  sceneCueBuildStarted = false;
  sceneCueStrobeStarted = false;
  sceneCueBlackoutDone = false;

  blackoutAllPixels();
  sendResponse(F("OK:SCENE:TEST:START"));
}

void stopScene() {
  sceneRunning = false;
  blackoutAllPixels();
  audioStop();
  sendResponse(F("OK:SCENE:STOP"));
}

void updateScene() {
  if (!sceneRunning || emergencyActive) return;

  unsigned long t = millis() - sceneStartMs;

  if (!sceneCueAudioStarted && t >= 0) {
    sceneCueAudioStarted = true;
    audioPlay(F("001"));
    startPixelEffect(0, FX_PULSE, 255, 0, 0, 150, 8, 4000);
    startPixelEffect(1, FX_FIRE, 255, 80, 0, 180, 40, 4000);
    sendResponse(F("CUE:SCENE:TEST:0"));
  }

  if (!sceneCueBuildStarted && t >= 4000) {
    sceneCueBuildStarted = true;
    startPixelEffect(2, FX_PULSE, 0, 120, 255, 180, 5, 3000);
    sendResponse(F("CUE:SCENE:TEST:4000"));
  }

  if (!sceneCueStrobeStarted && t >= 7000) {
    sceneCueStrobeStarted = true;
    startPixelEffect(3, FX_STROBE, 255, 255, 255, 255, 220, 1500);
    sendResponse(F("CUE:SCENE:TEST:7000"));
  }

  if (!sceneCueBlackoutDone && t >= 9000) {
    sceneCueBlackoutDone = true;
    blackoutAllPixels();
    audioStop();
    sceneRunning = false;
    sendResponse(F("OK:SCENE:TEST:END"));
  }
}

// =========================================================
// Emergency handling
// =========================================================

void emergencyStop() {
  emergencyActive = true;
  sceneRunning = false;
  blackoutAllPixels();
  audioStop();
  sendResponse(F("STATUS:EMERGENCY_ACTIVE"));
}

void emergencyClear() {
  emergencyActive = false;
  blackoutAllPixels();
  sendResponse(F("STATUS:READY"));
}

// =========================================================
// Command parsing helpers
// =========================================================

uint8_t parseLineNumber(const String& command) {
  int firstColon = command.indexOf(':');
  int secondColon = command.indexOf(':', firstColon + 1);

  if (firstColon < 0 || secondColon < 0) return 255;

  String lineText = command.substring(firstColon + 1, secondColon);
  lineText.trim();

  if (lineText == "ALL") return 254;

  int lineNumber = lineText.toInt();
  if (lineNumber < 1 || lineNumber > PIXEL_LINE_COUNT) return 255;

  return lineNumber - 1;
}

bool parseRGB(const String& rgbText, uint8_t& r, uint8_t& g, uint8_t& b) {
  int comma1 = rgbText.indexOf(',');
  int comma2 = rgbText.indexOf(',', comma1 + 1);

  if (comma1 < 0 || comma2 < 0) return false;

  int rv = rgbText.substring(0, comma1).toInt();
  int gv = rgbText.substring(comma1 + 1, comma2).toInt();
  int bv = rgbText.substring(comma2 + 1).toInt();

  if (rv < 0 || rv > 255 || gv < 0 || gv > 255 || bv < 0 || bv > 255) return false;

  r = rv;
  g = gv;
  b = bv;
  return true;
}

// =========================================================
// Command handlers
// =========================================================

void handlePixelCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == F("PIXEL:ALL:BLACKOUT") || command == F("PIXELS:ALL:BLACKOUT")) {
    blackoutAllPixels();
    sendResponse(F("OK:PIXEL:ALL:BLACKOUT"));
    return;
  }

  uint8_t lineIndex = parseLineNumber(command);
  if (lineIndex == 255 || lineIndex == 254) {
    sendResponse(F("ERR:PIXEL:INVALID_LINE"));
    return;
  }

  if (command.indexOf(F(":COLOR:")) >= 0) {
    int pos = command.indexOf(F(":COLOR:"));
    String rgbText = command.substring(pos + 7);
    uint8_t r, g, b;

    if (!parseRGB(rgbText, r, g, b)) {
      sendResponse(F("ERR:PIXEL:BAD_COLOR"));
      return;
    }

    setPixelSolid(lineIndex, r, g, b, DEFAULT_BRIGHTNESS);
    sendResponse(String(F("OK:PIXEL:")) + String(lineIndex + 1) + F(":COLOR"));
    return;
  }

  if (command.indexOf(F(":EFFECT:PULSE")) >= 0) {
    startPixelEffect(lineIndex, FX_PULSE, 255, 0, 0, DEFAULT_BRIGHTNESS, 8, 0);
    sendResponse(String(F("OK:PIXEL:")) + String(lineIndex + 1) + F(":EFFECT:PULSE"));
    return;
  }

  if (command.indexOf(F(":EFFECT:FIRE")) >= 0) {
    startPixelEffect(lineIndex, FX_FIRE, 255, 80, 0, 180, 40, 0);
    sendResponse(String(F("OK:PIXEL:")) + String(lineIndex + 1) + F(":EFFECT:FIRE"));
    return;
  }

  if (command.indexOf(F(":EFFECT:STROBE")) >= 0) {
    startPixelEffect(lineIndex, FX_STROBE, 255, 255, 255, 255, 220, 0);
    sendResponse(String(F("OK:PIXEL:")) + String(lineIndex + 1) + F(":EFFECT:STROBE"));
    return;
  }

  if (command.endsWith(F(":OFF"))) {
    setPixelLineOff(lineIndex);
    sendResponse(String(F("OK:PIXEL:")) + String(lineIndex + 1) + F(":OFF"));
    return;
  }

  sendResponse(F("ERR:PIXEL:UNKNOWN"));
}

void handleAudioCommand(String command) {
  command.trim();

  String upper = command;
  upper.toUpperCase();

  if (upper.startsWith(F("AUDIO:PLAY:"))) {
    String track = command.substring(11);
    track.trim();
    audioPlay(track);
    return;
  }

  if (upper == F("AUDIO:STOP")) {
    audioStop();
    return;
  }

  if (upper.startsWith(F("AUDIO:VOLUME:"))) {
    uint8_t volume = upper.substring(13).toInt();
    audioSetVolume(volume);
    return;
  }

  if (upper == F("AUDIO:STATUS")) {
    reportAudioStatus();
    return;
  }

  sendResponse(F("ERR:AUDIO:UNKNOWN"));
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  Serial.print(F("[IN] "));
  Serial.println(command);

  String upper = command;
  upper.toUpperCase();

  if (upper == F("EMERGENCY:STOP")) {
    emergencyStop();
    return;
  }

  if (upper == F("EMERGENCY:CLEAR")) {
    emergencyClear();
    return;
  }

  if (emergencyActive) {
    sendResponse(F("ERR:EMERGENCY_ACTIVE"));
    return;
  }

  if (upper == F("HEARTBEAT")) {
    sendResponse(F("STATUS:ALIVE"));
    return;
  }

  if (upper == F("STATUS:REQUEST")) {
    reportFullStatus();
    return;
  }

  if (upper == F("SD:STATUS")) {
    reportSDStatus();
    return;
  }

  if (upper == F("RTC:STATUS")) {
    reportRTCStatus();
    return;
  }

  if (upper.startsWith(F("PIXEL:")) || upper.startsWith(F("PIXELS:"))) {
    handlePixelCommand(command);
    return;
  }

  if (upper.startsWith(F("AUDIO:"))) {
    handleAudioCommand(command);
    return;
  }

  if (upper == F("SCENE:TEST")) {
    startTestScene();
    return;
  }

  if (upper == F("SCENE:STOP")) {
    stopScene();
    return;
  }

  sendResponse(F("ERR:UNKNOWN_COMMAND"));
}

// =========================================================
// Serial reading
// =========================================================

void readSerialPort(Stream& port, String& buffer) {
  while (port.available() > 0) {
    char c = port.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      handleCommand(buffer);
      buffer = "";
    } else {
      if (buffer.length() < 120) {
        buffer += c;
      } else {
        buffer = "";
        sendResponse(F("ERR:COMMAND_TOO_LONG"));
      }
    }
  }
}

// =========================================================
// Setup functions
// =========================================================

void initialisePixels() {
  for (uint8_t i = 0; i < PIXEL_LINE_COUNT; i++) {
    pixelLines[i]->begin();
    pixelLines[i]->setBrightness(DEFAULT_BRIGHTNESS);
    pixelLines[i]->clear();
    pixelLines[i]->show();

    pixelState[i].effect = FX_OFF;
    pixelState[i].red = 0;
    pixelState[i].green = 0;
    pixelState[i].blue = 0;
    pixelState[i].brightness = DEFAULT_BRIGHTNESS;
    pixelState[i].speed = 50;
    pixelState[i].effectStartMs = 0;
    pixelState[i].durationMs = 0;
    pixelState[i].active = false;
    pixelState[i].strobeOn = false;
  }

  debugPrint(F("4 pixel lines initialised"));
}

void initialiseSD() {
  pinMode(SD_CS_PIN, OUTPUT);
  sdPresent = SD.begin(SD_CS_PIN);

  if (sdPresent) {
    debugPrint(F("SD card OK"));
  } else {
    debugPrint(F("SD card FAILED or missing"));
  }
}

void initialiseRTC() {
  Wire.begin();
  rtcPresent = rtc.begin();

  if (rtcPresent) {
    debugPrint(F("RTC OK"));

    if (rtc.lostPower()) {
      debugPrint(F("RTC lost power. Setting RTC to compile time."));
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    debugPrint(F("RTC FAILED or missing"));
  }
}

void bootPixelTest() {
  setPixelSolid(0, 255, 0, 0, 80);
  setPixelSolid(1, 0, 255, 0, 80);
  setPixelSolid(2, 0, 0, 255, 80);
  setPixelSolid(3, 255, 255, 255, 80);
  delay(300);
  blackoutAllPixels();
}

// =========================================================
// Arduino setup and loop
// =========================================================

void setup() {
  Serial.begin(USB_BAUD_RATE);
  Serial1.begin(CYD_BAUD_RATE);

  delay(300);

  Serial.println();
  Serial.println(F("==============================================="));
  Serial.println(F("Showduino v1 - Mega Executor Weekend Stack"));
  Serial.println(F("Hardware: 4 Pixels + SD + RTC + Audio Abstraction"));
  Serial.println(F("==============================================="));

  initialisePixels();
  initialiseSD();
  initialiseRTC();
  bootPixelTest();

  sendResponse(F("STATUS:READY"));
  reportFullStatus();
}

void loop() {
  readSerialPort(Serial1, cydCommandBuffer);
  readSerialPort(Serial, usbCommandBuffer);

  updatePixels();
  updateScene();
}
