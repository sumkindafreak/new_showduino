#ifndef SHOWDUINO_LAMP_DISPLAY_H
#define SHOWDUINO_LAMP_DISPLAY_H

#include <Arduino.h>

bool lampDisplayBegin();
bool lampDisplayReady();
void lampDisplayService();
void lampDisplayForce();
void lampDisplayNextPage();
uint8_t lampDisplayPage();
void lampDisplayOledTest();
const char *lampDisplayController();

#endif
