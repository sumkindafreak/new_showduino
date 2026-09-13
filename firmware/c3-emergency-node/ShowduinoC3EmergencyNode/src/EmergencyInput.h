#ifndef SHOWDUINO_EMERGENCY_INPUT_H
#define SHOWDUINO_EMERGENCY_INPUT_H

#include <Arduino.h>

void emergencyInputBegin();
void emergencyInputService();
int emergencyInputOpen();
int emergencyInputRaw();
int emergencyInputRearmHeld(uint32_t nowMs);

#endif
