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
 * Press latches Emergency immediately. Release never clears.
 * The same uninterrupted press requests Director Locate after
 * SHOWDUINO_ESTOP_LOCATE_HOLD_MS. The physical button never clears.
 *
 * This module owns debounce, Locate-hold tracking, and pending-clear
 * authorisation. The existing P4 emergency latch remains authoritative.
 */

struct EmergencyInputEvents {
  bool loopOpened;
  bool loopClosed;
  bool locateRequested;
  bool clearExpired;
  bool clearSuperseded;
};

void emergencyInputBegin();
EmergencyInputEvents emergencyInputService(uint32_t nowMs);

bool emergencyInputLoopOpen();
bool emergencyInputLoopHealthy();
int emergencyInputRawGpio();
int emergencyInputStableOpen(); /* -1 unknown, 0 released/HIGH, 1 pressed/LOW */
bool emergencyInputLocateHoldActive();
bool emergencyInputLocateHoldFired();

bool emergencyInputPendingClear();
bool emergencyInputPendingClearValid(uint32_t nowMs);
int emergencyInputBeginDirectorClear(uint32_t nowMs);
int emergencyInputConfirmDirectorClear(uint32_t nowMs);
void emergencyInputCancelClear();
void emergencyInputNoteAssertion();
bool emergencyInputTakeClearSuperseded();
uint32_t emergencyInputAssertionSeq();

#endif
