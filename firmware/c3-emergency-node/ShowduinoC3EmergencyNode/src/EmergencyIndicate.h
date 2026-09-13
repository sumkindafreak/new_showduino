#ifndef SHOWDUINO_EMERGENCY_INDICATE_H
#define SHOWDUINO_EMERGENCY_INDICATE_H

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"

void emergencyIndicateBegin();
void emergencyIndicateService(const ShowduinoEmergencyMachine *m, int radioUp);

#endif
