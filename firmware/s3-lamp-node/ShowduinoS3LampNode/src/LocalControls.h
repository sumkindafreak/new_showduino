#ifndef SHOWDUINO_S3_LAMP_LOCAL_CONTROLS_H
#define SHOWDUINO_S3_LAMP_LOCAL_CONTROLS_H

#include <Arduino.h>

enum LampLocalButton : uint8_t {
  LAMP_BTN_NONE = 0,
  LAMP_BTN_IGNITE = 1
};

struct LampLocalEvent {
  LampLocalButton btn;
  bool longPress;
};

void lampLocalBegin();
LampLocalEvent lampLocalPoll();
void lampLocalPrintStatus();
bool lampLocalPinConfirmed();
bool lampLocalPressed();
const char *lampLocalStatus();

#endif
