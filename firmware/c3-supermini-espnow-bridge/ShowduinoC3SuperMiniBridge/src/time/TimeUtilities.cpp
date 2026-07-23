#include "TimeUtilities.h"
#include <stdio.h>
#include <string.h>

namespace TimeUtilities {

static void explodeEpoch(uint32_t epoch, int &Y, int &M, int &D, int &h, int &m, int &s) {
  /* Civil from days — Howard Hinnant algorithm (UTC). */
  uint32_t z = epoch / 86400UL + 719468UL;
  uint32_t era = z / 146097UL;
  uint32_t doe = z - era * 146097UL;
  uint32_t yoe = (doe - doe / 1460UL + doe / 36524UL - doe / 146096UL) / 365UL;
  uint32_t y = yoe + era * 400UL;
  uint32_t doy = doe - (365UL * yoe + yoe / 4UL - yoe / 100UL);
  uint32_t mp = (5UL * doy + 2UL) / 153UL;
  D = (int)(doy - (153UL * mp + 2UL) / 5UL + 1UL);
  M = (int)(mp < 10 ? mp + 3 : mp - 9);
  Y = (int)(y + (M <= 2 ? 1 : 0));
  uint32_t tod = epoch % 86400UL;
  h = (int)(tod / 3600UL);
  m = (int)((tod % 3600UL) / 60UL);
  s = (int)(tod % 60UL);
}

void formatIso8601(uint32_t epochSec, char *out, size_t outLen) {
  if (!out || outLen < 21) return;
  int Y, M, D, h, m, s;
  explodeEpoch(epochSec, Y, M, D, h, m, s);
  snprintf(out, outLen, "%04d-%02d-%02dT%02d:%02d:%02dZ", Y, M, D, h, m, s);
}


void formatLongDate(uint32_t epochSec, char *out, size_t outLen) {
  if (!out || outLen < 16) return;
  static const char *mon[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  int Y, M, D, h, m, s;
  explodeEpoch(epochSec, Y, M, D, h, m, s);
  (void)h; (void)m; (void)s;
  const char *dow = dayOfWeekName(dayOfWeekFromEpoch(epochSec));
  char dow3[4] = {0};
  strncpy(dow3, dow, 3);
  const char *month = (M >= 1 && M <= 12) ? mon[M - 1] : "???";
  snprintf(out, outLen, "%s %d %s %04d", dow3, D, month, Y);
}
void formatDate(uint32_t epochSec, char *out, size_t outLen) {
  if (!out || outLen < 11) return;
  int Y, M, D, h, m, s;
  explodeEpoch(epochSec, Y, M, D, h, m, s);
  (void)h; (void)m; (void)s;
  snprintf(out, outLen, "%04d-%02d-%02d", Y, M, D);
}

void formatTime(uint32_t epochSec, char *out, size_t outLen) {
  if (!out || outLen < 9) return;
  int Y, M, D, h, m, s;
  explodeEpoch(epochSec, Y, M, D, h, m, s);
  (void)Y; (void)M; (void)D;
  snprintf(out, outLen, "%02d:%02d:%02d", h, m, s);
}

const char *dayOfWeekName(uint8_t dow) {
  static const char *names[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
  };
  if (dow > 6) return "Unknown";
  return names[dow];
}

uint8_t dayOfWeekFromEpoch(uint32_t epochSec) {
  /* 1970-01-01 was Thursday = 4; we want Sunday=0 */
  return (uint8_t)((epochSec / 86400UL + 4UL) % 7UL);
}

uint32_t uptimeSeconds() {
  return millis() / 1000UL;
}

void formatUptime(char *out, size_t outLen) {
  if (!out || outLen < 12) return;
  uint32_t s = uptimeSeconds();
  uint32_t h = s / 3600UL;
  uint32_t m = (s % 3600UL) / 60UL;
  uint32_t sec = s % 60UL;
  snprintf(out, outLen, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)sec);
}

void firmwareBuildTimestamp(char *out, size_t outLen) {
  if (!out || outLen < 24) return;
  snprintf(out, outLen, "%s %s", __DATE__, __TIME__);
}

}  // namespace TimeUtilities