#ifndef SHOWDUINO_EMERGENCY_DISPLAY_H
#define SHOWDUINO_EMERGENCY_DISPLAY_H

#include <Arduino.h>

bool emergencyDisplayBegin();
bool emergencyDisplayReady();
void emergencyDisplayService();
void emergencyDisplayForce();
void emergencyDisplayOledTest();
const char *emergencyDisplayController();

#endif
