#include "EmergencyStationStore.h"
#include <Preferences.h>
#include "showduino_emergency_latch.h"

/* Namespace is opened read-write so a fresh device creates it instead of
 * reporting an unreadable store. */
ShowduinoBlockerStatus emergencyStationStoreLoad(ShowduinoBlocker *out, uint8_t *count) {
  static char buf[SHOWDUINO_BLOCKER_BUF];
  if (count) *count = 0;
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return SHOWDUINO_BLOCKER_UNREADABLE;
  if (!p.isKey(SHOWDUINO_BLOCKER_NVS_KEY)) {
    p.end();
    return SHOWDUINO_BLOCKER_ABSENT;
  }
  const size_t n = p.getString(SHOWDUINO_BLOCKER_NVS_KEY, buf, sizeof(buf));
  p.end();
  if (n == 0 || n >= sizeof(buf)) return SHOWDUINO_BLOCKER_CORRUPT;
  uint8_t c = 0;
  const ShowduinoBlockerStatus st = showduino_blocker_decode(buf, out, &c);
  if (count) *count = c;
  return st;
}

bool emergencyStationStoreSave(const char *encoded) {
  if (!encoded) return false;
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  p.putString(SHOWDUINO_BLOCKER_NVS_KEY, encoded);
  static char back[SHOWDUINO_BLOCKER_BUF];
  const size_t n = p.getString(SHOWDUINO_BLOCKER_NVS_KEY, back, sizeof(back));
  p.end();
  return n > 0 && n < sizeof(back) && strcmp(back, encoded) == 0;
}
