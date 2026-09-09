#ifndef SHOWDUINO_LAMP_LOCAL_CONTROLS_H
#define SHOWDUINO_LAMP_LOCAL_CONTROLS_H

#include <Arduino.h>

enum LampLocalButton : uint8_t {
  LAMP_BTN_NONE = 0,
  LAMP_BTN_A = 1,
  LAMP_BTN_B = 2
};

struct LampLocalEvent {
  LampLocalButton btn;
  bool longPress;
};

void lampLocalBegin();
LampLocalEvent lampLocalPoll();
void lampLocalPrintStatus();

#endif
