#ifndef SHOWDUINO_TIME_UTILITIES_H
#define SHOWDUINO_TIME_UTILITIES_H

#include <Arduino.h>
#include <stdint.h>

namespace TimeUtilities {
  /** Format UTC epoch seconds as ISO-8601: YYYY-MM-DDTHH:MM:SSZ */
  void formatIso8601(uint32_t epochSec, char *out, size_t outLen);
  void formatDate(uint32_t epochSec, char *out, size_t outLen);
  void formatLongDate(uint32_t epochSec, char *out, size_t outLen); /* Wed 22 Jul 2026 */
  void formatTime(uint32_t epochSec, char *out, size_t outLen);
  const char *dayOfWeekName(uint8_t dow); /* 0=Sunday .. 6=Saturday */
  uint8_t dayOfWeekFromEpoch(uint32_t epochSec);
  uint32_t uptimeSeconds();
  void formatUptime(char *out, size_t outLen);
  void firmwareBuildTimestamp(char *out, size_t outLen);
}

#endif