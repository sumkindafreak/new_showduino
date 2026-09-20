#ifndef SHOWDUINO_EMERGENCY_DISPLAY_H
#define SHOWDUINO_EMERGENCY_DISPLAY_H

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"

bool emergencyDisplayBegin();
void emergencyDisplayService(const ShowduinoEmergencyMachine *machine);

#endif
