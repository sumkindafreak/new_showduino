#ifndef SHOWDUINO_EMERGENCY_INPUT_H
#define SHOWDUINO_EMERGENCY_INPUT_H

#include <Arduino.h>

void emergencyInputBegin();
void emergencyInputService();
int emergencyInputPressed();
int emergencyInputRaw();
int emergencyInputRearmHeld(uint32_t nowMs);

/* Legacy alias: input_open wire semantics = button pressed. */
static inline int emergencyInputOpen() { return emergencyInputPressed(); }

#endif
