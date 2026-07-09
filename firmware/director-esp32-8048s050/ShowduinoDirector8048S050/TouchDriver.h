#ifndef SHOWDUINO_TOUCH_DRIVER_H
#define SHOWDUINO_TOUCH_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include <TAMC_GT911.h>
#include "BoardConfig.h"

// =========================================================
// GT911 capacitive touch wrapper
// Keeps the touch logic separate from the main Showduino sketch.
// =========================================================

class ShowduinoTouchDriver {
public:
  ShowduinoTouchDriver()
  : touch(TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_INT_PIN, TOUCH_RST_PIN, SCREEN_WIDTH, SCREEN_HEIGHT) {}

  void begin() {
    Serial.println("Touch: starting GT911 on I2C...");
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    touch.begin();
    touch.setRotation(ROTATION_NORMAL);
    lastX = 0;
    lastY = 0;
    touchedNow = false;
    Serial.println("Touch: GT911 ready.");
  }

  bool read(uint16_t &x, uint16_t &y) {
    touch.read();

    if (!touch.isTouched) {
      touchedNow = false;
      return false;
    }

    // Map raw SDK coordinates to the 800x480 LVGL screen space.
    int mappedX = map(touch.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, SCREEN_WIDTH - 1);
    int mappedY = map(touch.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, SCREEN_HEIGHT - 1);

    mappedX = constrain(mappedX, 0, SCREEN_WIDTH - 1);
    mappedY = constrain(mappedY, 0, SCREEN_HEIGHT - 1);

    lastX = (uint16_t)mappedX;
    lastY = (uint16_t)mappedY;
    x = lastX;
    y = lastY;
    touchedNow = true;
    return true;
  }

  bool isTouched() const {
    return touchedNow;
  }

  uint16_t getLastX() const {
    return lastX;
  }

  uint16_t getLastY() const {
    return lastY;
  }

private:
  TAMC_GT911 touch;
  uint16_t lastX = 0;
  uint16_t lastY = 0;
  bool touchedNow = false;
};

#endif
