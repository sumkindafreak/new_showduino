#include "TouchCalibrationStore.h"

#include <Arduino.h>
#include <Preferences.h>
#include "BoardConfig.h"

static const char *kNs = "showduino_touch";
static const char *kKey = "cal";

bool touchCalibrationStoreLoad(ShowduinoTouchCalibrationRecord *out, int *failCode) {
  if (failCode) *failCode = SHOWDUINO_TOUCH_CAL_FAIL_SIZE;
  if (!out) return false;
  memset(out, 0, sizeof(*out));

  Preferences prefs;
  if (!prefs.begin(kNs, true)) {
    if (failCode) *failCode = SHOWDUINO_TOUCH_CAL_FAIL_SIZE;
    return false;
  }
  const size_t n = prefs.getBytesLength(kKey);
  ShowduinoTouchCalibrationRecord rec;
  memset(&rec, 0, sizeof(rec));
  size_t got = 0;
  if (n == sizeof(rec)) {
    got = prefs.getBytes(kKey, &rec, sizeof(rec));
  }
  prefs.end();

  if (n == 0) {
    if (failCode) *failCode = SHOWDUINO_TOUCH_CAL_FAIL_SIZE;
    return false;
  }
  const int rc = showduino_touch_cal_record_valid(&rec, got, SCREEN_WIDTH, SCREEN_HEIGHT);
  if (failCode) *failCode = rc;
  if (rc != SHOWDUINO_TOUCH_CAL_OK) return false;
  *out = rec;
  return true;
}

bool touchCalibrationStoreSave(const ShowduinoTouchCalibrationRecord *rec) {
  if (!rec) return false;
  if (showduino_touch_cal_record_valid(rec, sizeof(*rec), SCREEN_WIDTH, SCREEN_HEIGHT) !=
      SHOWDUINO_TOUCH_CAL_OK) {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin(kNs, false)) return false;
  const size_t wrote = prefs.putBytes(kKey, rec, sizeof(*rec));
  prefs.end();
  return wrote == sizeof(*rec);
}

bool touchCalibrationStoreErase() {
  Preferences prefs;
  if (!prefs.begin(kNs, false)) return false;
  const bool ok = prefs.remove(kKey);
  prefs.end();
  return ok;
}
