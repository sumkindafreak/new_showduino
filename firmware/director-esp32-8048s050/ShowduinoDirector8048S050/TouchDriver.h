#ifndef SHOWDUINO_TOUCH_DRIVER_H
#define SHOWDUINO_TOUCH_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "BoardConfig.h"

#if SHOWDUINO_TOUCH_GT911
#include <TAMC_GT911.h>
#endif

#if SHOWDUINO_TOUCH_XPT2046
#if __has_include(<XPT2046_Touchscreen.h>)
#include <XPT2046_Touchscreen.h>
#define SHOWDUINO_HAS_XPT2046_LIB 1
#else
#warning "Install XPT2046_Touchscreen library for ESP32-8048S043R resistive touch"
#endif
#endif

class ShowduinoTouchDriver {
public:
#if SHOWDUINO_TOUCH_GT911
  ShowduinoTouchDriver()
  : gt911(TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_INT_PIN, TOUCH_RST_PIN, SCREEN_WIDTH, SCREEN_HEIGHT) {}
#endif

  void begin() {
    ready = false;
    backend = TouchBackend::None;

#if SHOWDUINO_TOUCH_GT911
    if (beginGt911()) return;
#endif

#if SHOWDUINO_TOUCH_XPT2046
    if (beginXpt2046()) return;
#endif

    Serial.println("Touch: no supported controller detected.");
  }

  bool read(uint16_t &x, uint16_t &y) {
    if (!ready) return false;

#if SHOWDUINO_TOUCH_GT911
    if (backend == TouchBackend::Gt911) return readGt911(x, y);
#endif

#if SHOWDUINO_TOUCH_XPT2046
    if (backend == TouchBackend::Xpt2046) return readXpt2046(x, y);
#endif

    return false;
  }

  bool isReady() const { return ready; }
  bool isTouched() const { return touchedNow; }
  uint16_t getLastX() const { return lastX; }
  uint16_t getLastY() const { return lastY; }

private:
  enum class TouchBackend : uint8_t { None, Gt911, Xpt2046 };

  TouchBackend backend = TouchBackend::None;
  uint16_t lastX = 0;
  uint16_t lastY = 0;
  bool touchedNow = false;
  bool ready = false;

#if SHOWDUINO_TOUCH_GT911
  TAMC_GT911 gt911;

  bool beginGt911() {
    Serial.println("Touch: probing GT911 (capacitive / I2C)...");

    if (TOUCH_INT_PIN < 0) {
      Serial.println("Touch: invalid TOUCH_INT_PIN; GT911 init aborted.");
      return false;
    }

    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    Wire.setTimeOut(100);
    Wire.setClock(400000);

    uint8_t addr = TOUCH_I2C_ADDR;
    gt911.begin(addr);

    if (!probeGt911(addr)) {
      Serial.println("Touch: GT911 not at 0x5D, trying 0x14...");
      addr = GT911_ADDR2;
      gt911.begin(addr);
    }

    if (!probeGt911(addr)) {
      Serial.println("Touch: GT911 not found on I2C bus.");
      return false;
    }

    gt911.setRotation(TOUCH_GT911_ROTATION);
    backend = TouchBackend::Gt911;
    ready = true;
    Serial.printf("Touch: GT911 ready at I2C 0x%02X.\n", addr);
    return true;
  }

  bool probeGt911(uint8_t addr) {
    Wire.beginTransmission(addr);
    Wire.write(highByte(GT911_PRODUCT_ID));
    Wire.write(lowByte(GT911_PRODUCT_ID));
    if (Wire.endTransmission() != 0) return false;

    uint8_t bytesRead = Wire.requestFrom(addr, (uint8_t)4);
    if (bytesRead < 4) return false;

    char productId[5] = {0};
    for (uint8_t i = 0; i < 4 && Wire.available(); i++) {
      productId[i] = (char)Wire.read();
    }

    Serial.printf("Touch: GT911 product ID = '%s'\n", productId);
    return productId[0] == '9' && productId[1] == '1' && productId[2] == '1';
  }

  bool readGt911(uint16_t &x, uint16_t &y) {
    gt911.read();

    if (!gt911.isTouched) {
      touchedNow = false;
      return false;
    }

    int rawX = gt911.points[0].x;
    int rawY = gt911.points[0].y;
    int mappedX;
    int mappedY;

#if TOUCH_CAL_ENABLE
    mappedX = map(rawX, TOUCH_CAL_RAW_X_LEFT, TOUCH_CAL_RAW_X_RIGHT, 0, SCREEN_WIDTH - 1);
    mappedY = map(rawY, TOUCH_CAL_RAW_Y_TOP, TOUCH_CAL_RAW_Y_BOT, 0, SCREEN_HEIGHT - 1);
#else
    mappedX = rawX;
    mappedY = rawY;
#endif

    mappedX = constrain(mappedX, 0, SCREEN_WIDTH - 1);
    mappedY = constrain(mappedY, 0, SCREEN_HEIGHT - 1);

    lastX = (uint16_t)mappedX;
    lastY = (uint16_t)mappedY;
    x = lastX;
    y = lastY;
    touchedNow = true;
    return true;
  }
#endif

#if SHOWDUINO_TOUCH_XPT2046
#ifdef SHOWDUINO_HAS_XPT2046_LIB
  XPT2046_Touchscreen xpt2046(TOUCH_XPT_CS_PIN, TOUCH_XPT_IRQ_PIN);
#endif

  bool beginXpt2046() {
#ifdef SHOWDUINO_HAS_XPT2046_LIB
    Serial.println("Touch: probing XPT2046 (resistive / SPI)...");

    SPI.begin(TOUCH_XPT_SCK_PIN, TOUCH_XPT_MISO_PIN, TOUCH_XPT_MOSI_PIN, TOUCH_XPT_CS_PIN);
    if (!xpt2046.begin()) {
      Serial.println("Touch: XPT2046 begin() failed.");
      return false;
    }

    xpt2046.setRotation(TOUCH_XPT_ROTATION);
    backend = TouchBackend::Xpt2046;
    ready = true;
    Serial.println("Touch: XPT2046 ready.");
    return true;
#else
    return false;
#endif
  }

  bool readXpt2046(uint16_t &x, uint16_t &y) {
#ifdef SHOWDUINO_HAS_XPT2046_LIB
    if (!xpt2046.touched()) {
      touchedNow = false;
      return false;
    }

    TS_Point p = xpt2046.getPoint();
    int mappedX = map(p.x, TOUCH_XPT_RAW_MIN, TOUCH_XPT_RAW_MAX, 0, SCREEN_WIDTH - 1);
    int mappedY = map(p.y, TOUCH_XPT_RAW_MIN, TOUCH_XPT_RAW_MAX, 0, SCREEN_HEIGHT - 1);
    mappedX = constrain(mappedX, 0, SCREEN_WIDTH - 1);
    mappedY = constrain(mappedY, 0, SCREEN_HEIGHT - 1);

    lastX = (uint16_t)mappedX;
    lastY = (uint16_t)mappedY;
    x = lastX;
    y = lastY;
    touchedNow = true;
    return true;
#else
    (void)x;
    (void)y;
    return false;
#endif
  }
#endif
};

#endif
