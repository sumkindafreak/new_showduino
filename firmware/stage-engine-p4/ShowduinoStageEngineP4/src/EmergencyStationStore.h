#ifndef SHOWDUINO_EMERGENCY_STATION_STORE_H
#define SHOWDUINO_EMERGENCY_STATION_STORE_H

/*
 * NVS persistence for the wireless station clear-blocker set. Shares the
 * "showduino" namespace with the latch. Codec and rules live in
 * protocol/showduino_station_blockers.h.
 */

#include <Arduino.h>
#include "../../../../protocol/showduino_station_blockers.h"

/* Reads and decodes the stored set. Absent, unreadable and corrupt are distinct
 * results. None of them may be treated as "no blockers". */
ShowduinoBlockerStatus emergencyStationStoreLoad(ShowduinoBlocker *out, uint8_t *count);

/* Writes the encoded set, then reads it back and compares. */
bool emergencyStationStoreSave(const char *encoded);

#endif
