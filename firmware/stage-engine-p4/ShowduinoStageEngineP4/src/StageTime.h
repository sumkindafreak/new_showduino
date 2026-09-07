#ifndef SHOWDUINO_STAGE_TIME_H
#define SHOWDUINO_STAGE_TIME_H

#include <stddef.h>
#include <stdint.h>

/**
 * P4 internal RTC — authoritative Show Engine wall clock.
 * No DS3231. Director displays TIME: wires only; it does not own time.
 */
void stageTimeBegin();
void stageTimeLoop(uint32_t nowMs, void (*sendFn)(const char *line));

bool stageTimeSetEpoch(uint32_t epoch);
bool stageTimeParseSet(const char *arg, uint32_t *epochOut);
bool stageTimeFormatDirectorWire(char *out, size_t outLen);

uint32_t stageTimeEpoch();
bool stageTimeSynced();
const char *stageTimeHealth();
const char *stageTimeSource();
void stageTimeIso(char *out, size_t outLen);
void stageTimeClock(char *out, size_t outLen);
void stageTimeDate(char *out, size_t outLen);
void stageTimeLongDate(char *out, size_t outLen);

#endif
