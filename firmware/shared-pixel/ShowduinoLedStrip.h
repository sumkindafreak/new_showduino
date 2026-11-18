#ifndef SHOWDUINO_LED_STRIP_BACKEND_H
#define SHOWDUINO_LED_STRIP_BACKEND_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <new>

// Driver selection is per firmware build. Legacy remains the default.
#ifndef SHOWDUINO_LED_USE_FASTLED
#define SHOWDUINO_LED_USE_FASTLED 0
#endif
#if SHOWDUINO_LED_USE_FASTLED != 0 && SHOWDUINO_LED_USE_FASTLED != 1
#error SHOWDUINO_LED_USE_FASTLED must be 0 or 1
#endif

#if SHOWDUINO_LED_USE_FASTLED
#ifndef SHOWDUINO_LED_GPIO
#error Define the fixed SHOWDUINO_LED_GPIO before including ShowduinoLedStrip.h
#endif
#include <FastLED.h>

// One FastLED controller per fixed GPIO. FastLED owns its registered controller;
// this object owns only the RGB buffer and detaches it before freeing it.
template<int DATA_GPIO>
class ShowduinoFastLedStrip {
 public:
  ShowduinoFastLedStrip(uint16_t count, int16_t pin, neoPixelType type)
      : count_(count), pin_(pin), type_(type) {}
  ShowduinoFastLedStrip(const ShowduinoFastLedStrip&) = delete;
  ShowduinoFastLedStrip& operator=(const ShowduinoFastLedStrip&) = delete;
  ~ShowduinoFastLedStrip() {
    if (owner() == this) {
      controller()->setLeds(nullptr, 0);
      owner() = nullptr;
    }
    delete[] pixels_;
  }

  bool begin() {
    if (ready()) return true;
    // This first backend implements the fitted WS2812 GRB/800kHz lines only.
    // Reject unsupported geometry rather than silently changing colour order.
    if (DATA_GPIO < 0 || pin_ != DATA_GPIO || !count_ ||
        type_ != (NEO_GRB + NEO_KHZ800) || (owner() && owner() != this)) return false;
    CRGB *buffer = new (std::nothrow) CRGB[count_];
    if (!buffer) return false;
    for (uint16_t i = 0; i < count_; ++i) buffer[i] = CRGB(0, 0, 0);
    if (!controller()) {
      controller() = &FastLED.addLeds<WS2812, DATA_GPIO, GRB>(buffer, count_);
      controller()->setCorrection(UncorrectedColor);
      controller()->setTemperature(UncorrectedTemperature);
      controller()->setDither(0);
    } else {
      controller()->setLeds(buffer, count_);
    }
    pixels_ = buffer;
    owner() = this;
    return true;
  }

  // Ready means an owned buffer/controller binding, not measured wire output.
  bool ready() const { return pixels_ && controller() && owner() == this; }
  uint16_t numPixels() const { return count_; }
  void setBrightness(uint8_t value) { brightness_ = value; }
  uint8_t getBrightness() const { return brightness_; }
  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
  }
  void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (ready() && index < count_) pixels_[index] = CRGB(r, g, b);
  }
  void setPixelColor(uint16_t index, uint32_t rgb) {
    setPixelColor(index, uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb));
  }
  uint32_t getPixelColor(uint16_t index) const {
    if (!ready() || index >= count_) return 0;
    return Color(pixels_[index].r, pixels_[index].g, pixels_[index].b);
  }
  void clear() {
    if (ready()) for (uint16_t i = 0; i < count_; ++i) pixels_[i] = CRGB(0, 0, 0);
  }
  void show() {
    // Never FastLED.show(): another line's refresh must not flush this line.
    if (ready()) controller()->showLeds(brightness_);
  }

 private:
  static CLEDController*& controller() { static CLEDController *value = nullptr; return value; }
  static ShowduinoFastLedStrip*& owner() { static ShowduinoFastLedStrip *value = nullptr; return value; }
  uint16_t count_;
  int16_t pin_;
  neoPixelType type_;
  uint8_t brightness_ = 255;
  CRGB *pixels_ = nullptr;
};

using ShowduinoLedStrip = ShowduinoFastLedStrip<SHOWDUINO_LED_GPIO>;
inline const char *showduinoLedStripBackendName() { return "FastLED"; }
#else
class ShowduinoLedStrip : public Adafruit_NeoPixel {
 public:
  using Adafruit_NeoPixel::Adafruit_NeoPixel;
  ShowduinoLedStrip(const ShowduinoLedStrip&) = delete;
  ShowduinoLedStrip& operator=(const ShowduinoLedStrip&) = delete;
  bool begin() {
    ready_ = Adafruit_NeoPixel::begin() && numPixels() > 0;
    return ready_;
  }
  bool ready() const { return ready_ && numPixels() > 0; }
 private:
  bool ready_ = false;
};
inline const char *showduinoLedStripBackendName() { return "Adafruit NeoPixel"; }
#endif

#endif
