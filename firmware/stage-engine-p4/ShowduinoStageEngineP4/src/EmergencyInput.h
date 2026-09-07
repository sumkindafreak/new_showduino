#ifndef SHOWDUINO_EMERGENCY_INPUT_H
#define SHOWDUINO_EMERGENCY_INPUT_H

#include <Arduino.h>
#include "../BoardConfig.h"

/*
 * Momentary emergency pushbutton on GPIO25:
 *   P4 GPIO25 → NO pushbutton → GND
 *
 * Released : GPIO HIGH  → healthy
 * Pressed  : GPIO LOW   → emergency trigger
 *
 * Press event  = HIGH → LOW after debounce
 * Release event = LOW → HIGH after debounce
 *
 * This module owns debounce and gesture tracking only.
 * The existing P4 emergency latch remains authoritative.
 */

struct EmergencyInputEvents {
  bool loopOpened;
  bool loopClosed;
  bool locateRequested;
  bool clearRequested;
  bool clearExpired;
};

void emergencyInputBegin();
EmergencyInputEvents emergencyInputService(uint32_t nowMs);

bool emergencyInputLoopOpen();
bool emergencyInputLoopHealthy();
int emergencyInputRawGpio();
int emergencyInputStableOpen(); /* -1 unknown, 0 released/HIGH, 1 pressed/LOW */
uint8_t emergencyInputLocateCount();

bool emergencyInputPendingClear();
bool emergencyInputPendingClearValid(uint32_t nowMs);
void emergencyInputCancelClear();

#endif
