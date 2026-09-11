#ifndef SHOWDUINO_PIXEL_LOCAL_CONTROLS_H
#define SHOWDUINO_PIXEL_LOCAL_CONTROLS_H

#include <Arduino.h>

enum PixelLocalButton : uint8_t {
  PIXEL_BTN_NONE = 0,
  PIXEL_BTN_A = 1,
  PIXEL_BTN_B = 2
};

struct PixelLocalEvent {
  PixelLocalButton btn;
  bool longPress;
};

void pixelLocalBegin();
PixelLocalEvent pixelLocalPoll();
void pixelLocalPrintStatus();

#endif
