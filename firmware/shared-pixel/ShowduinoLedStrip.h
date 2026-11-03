#ifndef SHOWDUINO_LED_STRIP_BACKEND_H
#define SHOWDUINO_LED_STRIP_BACKEND_H

/*
 * Showduino LED Strip Backend — common API for existing node pixel engines.
 *
 * All original effect loops, brightness values and emergency priorities remain
 * in their owning engines. This class only changes the physical output driver.
 *
 * Before including this file define SHOWDUINO_LED_GPIO (board's fixed LED GPIO)
 * and optionally SHOWDUINO_LED_USE_FASTLED (0/1). Legacy is the default.
 *
 * Requires the existing Adafruit_NeoPixel library to keep compatibility with
 * NEO_GRB / NEO_KHZ800 board configuration values. When FastLED is enabled,
 * the FastLED library must also be installed.
 */
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifndef SHOWDUINO_LED_USE_FASTLED
#define SHOWDUINO_LED_USE_FASTLED 0
#endif

#if SHOWDUINO_LED_USE_FASTLED
#ifndef SHOWDUINO_LED_GPIO
#error SHOWDUINO_LED_GPIO must be defined before ShowduinoLedStrip.h
#endif
#include <FastLED.h>
#include <new>

template<int DATA_GPIO>
class ShowduinoFastLedStrip {
 public:
  ShowduinoFastLedStrip(uint16_t count, int16_t pin, neoPixelType pixelType)
      : count_(count), pin_(pin), pixelType_(pixelType) {}
  ShowduinoFastLedStrip(const ShowduinoFastLedStrip&) = delete;
  ShowduinoFastLedStrip& operator=(const ShowduinoFastLedStrip&) = delete;
  ~ShowduinoFastLedStrip() {
    if (controller_) controller_->setLeds(nullptr, 0);
    delete[] leds_;
    leds_ = nullptr;
  }

  void begin() {
    if (leds_) return;
    if (pin_ != DATA_GPIO || !count_) {
      Serial.printf("[PIXEL] FastLED GPIO/count invalid: gpio=%d expected=%d count=%u\n",
                    (int)pin_, DATA_GPIO, (unsigned)count_);
      return;
    }
    leds_ = new (std::nothrow) CRGB[count_];
    if (!leds_) {
      Serial.println("[PIXEL] FastLED allocation failed");
      return;
    }
    for (uint16_t i = 0; i < count_; i++) leds_[i] = CRGB::Black;
    // One registered controller per fixed hardware line. Length changes
    // detach/rebind rather than adding duplicate FastLED controllers.
    if (!registeredController()) {
      registeredController() = &FastLED.addLeds<WS2812, DATA_GPIO, GRB>(leds_, count_);
      registeredController()->setCorrection(UncorrectedColor);
      registeredController()->setDither(0);
    } else {
      registeredController()->setLeds(leds_, count_);
    }
    controller_ = registeredController();
    Serial.printf("[PIXEL] FastLED ready gpio=%d count=%u\n", DATA_GPIO, (unsigned)count_);
  }

  bool ready() const { return leds_ && controller_; }
  uint16_t numPixels() const { return count_; }
  void setBrightness(uint8_t brightness) { brightness_ = brightness; }
  uint8_t getBrightness() const { return brightness_; }

  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }
  void setPixelColor(uint16_t n, uint32_t rgb) {
    if (!leds_ || n >= count_) return;
    leds_[n] = CRGB((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
  }
  void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
    if (!leds_ || n >= count_) return;
    leds_[n] = CRGB(r, g, b);
  }
  void clear() {
    if (!leds_) return;
    for (uint16_t i = 0; i < count_; ++i) leds_[i] = CRGB::Black;
  }
  void show() {
    if (controller_ && leds_) controller_->showLeds(brightness_);
  }

 private:
  static CLEDController*& registeredController() {
    static CLEDController *value = nullptr;
    return value;
  }
  uint16_t count_;
  int16_t pin_;
  neoPixelType pixelType_;
  CRGB *leds_ = nullptr;
  CLEDController *controller_ = nullptr;
};

using ShowduinoLedStrip = ShowduinoFastLedStrip<SHOWDUINO_LED_GPIO>;
#else
using ShowduinoLedStrip = Adafruit_NeoPixel;
#endif

#endif  // SHOWDUINO_LED_STRIP_BACKEND_H
