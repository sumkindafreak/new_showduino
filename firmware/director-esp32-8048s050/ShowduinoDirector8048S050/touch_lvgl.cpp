#include "touch_lvgl.h"
#include "BoardConfig.h"
#include "backlight.h"
#include "TouchCalibrationStore.h"

#include <string.h>
#include <Wire.h>
#include <lvgl.h>

/*
 * TAMC_GT911 rotation enums are NOT Arduino_GFX / DISPLAY_ROTATION values:
 *   0 = ROTATION_LEFT
 *   1 = ROTATION_INVERTED
 *   2 = ROTATION_RIGHT
 *   3 = ROTATION_NORMAL
 */
#ifndef TOUCH_GT911_LIB_ROTATION
#define TOUCH_GT911_LIB_ROTATION ROTATION_NORMAL
#endif

/* Temporary Serial diagnostics for scroll bring-up - remove after field verify. */
#ifndef SHOWDUINO_TOUCH_SCROLL_DIAG
#define SHOWDUINO_TOUCH_SCROLL_DIAG 1
#endif

static TAMC_GT911 *s_touch = nullptr;
static uint16_t s_w = 0;
static uint16_t s_h = 0;
static bool s_ready = false;
static bool s_eatUntilRelease = false;
static int32_t s_lastTouchX = 0;
static int32_t s_lastTouchY = 0;
static bool s_hadPress = false;
static TouchLvglHook s_touchHook = nullptr;
static ShowduinoTouchCalMode s_calMode = SHOWDUINO_TOUCH_CAL_MODE_FACTORY;
static ShowduinoTouchCalibrationRecord s_nvsCal;
static bool s_calLogged = false;

/* TAMC_GT911::read() always writes 0 to POINT_INFO. A second I2C poll in the
 * same frame looks like a lift even while the finger is down. One poll is
 * stashed and reused until the next real I2C cycle. Empty packets keep the
 * last contact briefly so inter-report gaps are not treated as a release. */
static bool s_haveRaw = false;
static TouchRawPoint s_raw = {0, 0};
static uint32_t s_rawStampMs = 0;
static uint32_t s_lastI2cMs = 0;
static bool s_didI2c = false;
#ifndef SHOWDUINO_TOUCH_I2C_MIN_GAP_MS
#define SHOWDUINO_TOUCH_I2C_MIN_GAP_MS 4
#endif
#ifndef SHOWDUINO_TOUCH_HOLD_MS
#define SHOWDUINO_TOUCH_HOLD_MS 40
#endif

static void pollGt911() {
  if (!s_touch || !s_ready) {
    s_haveRaw = false;
    return;
  }
  const uint32_t now = millis();
  if (s_didI2c && (uint32_t)(now - s_lastI2cMs) < SHOWDUINO_TOUCH_I2C_MIN_GAP_MS) {
    return;
  }
  s_didI2c = true;
  s_lastI2cMs = now;
  s_touch->read();
  if (s_touch->isTouched) {
    const TP_Point p = s_touch->points[0];
    s_raw.x = (int32_t)p.x;
    s_raw.y = (int32_t)p.y;
    s_haveRaw = true;
    s_rawStampMs = now;
    return;
  }
  if (!s_haveRaw || (uint32_t)(now - s_rawStampMs) >= SHOWDUINO_TOUCH_HOLD_MS) {
    s_haveRaw = false;
  }
}

void touchLvglSetHook(TouchLvglHook hook) {
  s_touchHook = hook;
}

void touchLvglConsumeUntilRelease() {
  s_eatUntilRelease = true;
}

#if SHOWDUINO_TOUCH_SCROLL_DIAG
static bool s_diagWasPressed = false;
static int32_t s_diagLastX = -1;
static int32_t s_diagLastY = -1;
static uint32_t s_diagLastMoveMs = 0;
static uint32_t s_diagLastScrollMs = 0;
static lv_indev_t *s_indev = nullptr;

static void diagScrollPos(const char *tag) {
  if (!s_indev) return;
  lv_obj_t *scrollObj = lv_indev_get_scroll_obj(s_indev);
  if (!scrollObj) {
    Serial.printf("[Scroll] %s no_scroll_obj\n", tag);
    return;
  }
  const int32_t y = lv_obj_get_scroll_y(scrollObj);
  const int32_t top = lv_obj_get_scroll_top(scrollObj);
  const int32_t bot = lv_obj_get_scroll_bottom(scrollObj);
  const bool scrollable = lv_obj_has_flag(scrollObj, LV_OBJ_FLAG_SCROLLABLE);
  Serial.printf("[Scroll] %s y=%ld top=%ld bot=%ld scrollable=%u dir=0x%x\n",
                tag, (long)y, (long)top, (long)bot, (unsigned)scrollable,
                (unsigned)lv_obj_get_scroll_dir(scrollObj));
}
#endif

static void applyFactoryMode() {
  s_calMode = SHOWDUINO_TOUCH_CAL_MODE_FACTORY;
  memset(&s_nvsCal, 0, sizeof(s_nvsCal));
}

static void logCalibrationOnce() {
  if (s_calLogged) return;
  s_calLogged = true;
  if (s_calMode == SHOWDUINO_TOUCH_CAL_MODE_NVS) {
    Serial.printf("[Touch] calibration: NVS v%u\n", (unsigned)s_nvsCal.version);
  } else {
    Serial.println("[Touch] calibration: FACTORY FALLBACK");
  }
}

static void loadCalibrationLocked() {
  ShowduinoTouchCalibrationRecord rec;
  int fail = SHOWDUINO_TOUCH_CAL_FAIL_SIZE;
  if (touchCalibrationStoreLoad(&rec, &fail)) {
    s_nvsCal = rec;
    s_calMode = SHOWDUINO_TOUCH_CAL_MODE_NVS;
  } else {
    applyFactoryMode();
  }
}

void touchLvglMapWith(const ShowduinoTouchCalibrationRecord *rec,
                      int32_t rawX, int32_t rawY, int32_t *screenX, int32_t *screenY) {
  if (rec) {
    showduino_touch_cal_apply_i(rec, rawX, rawY, (int32_t)s_w, (int32_t)s_h, screenX, screenY);
    return;
  }
  showduino_touch_cal_factory_map(rawX, rawY, (int32_t)s_w, (int32_t)s_h, screenX, screenY);
}

void touchLvglMapRaw(int32_t rawX, int32_t rawY, int32_t *screenX, int32_t *screenY) {
  if (s_calMode == SHOWDUINO_TOUCH_CAL_MODE_NVS) {
    touchLvglMapWith(&s_nvsCal, rawX, rawY, screenX, screenY);
  } else {
    touchLvglMapWith(nullptr, rawX, rawY, screenX, screenY);
  }
}

bool touchLvglReadRaw(TouchRawPoint &point) {
  point.x = 0;
  point.y = 0;
  if (!s_haveRaw) return false;
  point = s_raw;
  return true;
}

uint32_t touchLvglRawStampMs() {
  return s_rawStampMs;
}

static bool sampleTouch(int32_t &x, int32_t &y) {
  pollGt911();
  if (!s_haveRaw) return false;
  touchLvglMapRaw(s_raw.x, s_raw.y, &x, &y);
  return true;
}

static void touchReadCb(lv_indev_t *indev, lv_indev_data_t *data) {
  (void)indev;
  data->state = LV_INDEV_STATE_RELEASED;

  int32_t x = 0, y = 0;
  bool pressed = sampleTouch(x, y);

  /* Preserve last pressed coords for release - GT911 reports no sample when up. */
  if (pressed) {
    s_lastTouchX = x;
    s_lastTouchY = y;
    s_hadPress = true;
  } else if (s_hadPress) {
    x = s_lastTouchX;
    y = s_lastTouchY;
    s_hadPress = false;
  }

  if (pressed) {
    const bool wasOff = !backlightIsOn();
    backlightNotifyActivity();
    /* First tap after screen-off only wakes - don't fire LVGL buttons. */
    if (wasOff) {
      s_eatUntilRelease = true;
      s_hadPress = false;
#if SHOWDUINO_TOUCH_SCROLL_DIAG
      Serial.printf("[Touch] WAKE_EAT x=%ld y=%ld\n", (long)x, (long)y);
#endif
    }
  }

  bool consumed = false;
  if (s_touchHook) {
    consumed = s_touchHook(x, y, pressed);
    if (consumed && pressed) s_eatUntilRelease = true;
  }

  /* Swallow LVGL delivery for wake/overlay consume, but the hook above still
   * sees hold and release so calibration and Locate can finish the gesture. */
  if (s_eatUntilRelease || consumed) {
    if (!pressed) s_eatUntilRelease = false;
#if SHOWDUINO_TOUCH_SCROLL_DIAG
    if (pressed && !s_diagWasPressed) {
      Serial.printf("[Touch] HOOK_EAT x=%ld y=%ld pressed=1\n", (long)x, (long)y);
    } else if (!pressed && s_diagWasPressed) {
      Serial.println("[Touch] EAT release");
    }
    s_diagWasPressed = pressed;
#endif
    return;
  }

#if SHOWDUINO_TOUCH_SCROLL_DIAG
  const uint32_t now = millis();
  if (pressed && !s_diagWasPressed) {
    Serial.printf("[Touch] PRESS x=%ld y=%ld raw_ok=1\n", (long)x, (long)y);
    s_diagLastX = x;
    s_diagLastY = y;
    s_diagLastMoveMs = now;
  } else if (pressed && s_diagWasPressed) {
    const int32_t dx = x - s_diagLastX;
    const int32_t dy = y - s_diagLastY;
    if ((dx != 0 || dy != 0) && (now - s_diagLastMoveMs) >= 80) {
      Serial.printf("[Touch] MOVE x=%ld y=%ld dx=%ld dy=%ld\n",
                    (long)x, (long)y, (long)dx, (long)dy);
      s_diagLastX = x;
      s_diagLastY = y;
      s_diagLastMoveMs = now;
      if ((now - s_diagLastScrollMs) >= 120) {
        diagScrollPos("drag");
        s_diagLastScrollMs = now;
      }
    }
  } else if (!pressed && s_diagWasPressed) {
    Serial.printf("[Touch] RELEASE last=(%ld,%ld)\n", (long)s_diagLastX, (long)s_diagLastY);
    diagScrollPos("release");
  }
  s_diagWasPressed = pressed;
#endif

  if (!pressed) return;

  data->point.x = (lv_coord_t)x;
  data->point.y = (lv_coord_t)y;
  data->state = LV_INDEV_STATE_PRESSED;
}

static void touchWireBegin() {
  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
  Wire.setTimeOut(100);
  Wire.setClock(400000);
}

void touchLvglInit(TAMC_GT911 &touch, uint16_t width, uint16_t height, uint8_t displayRotation) {
  (void)displayRotation;
  s_touch = &touch;
  s_w = width;
  s_h = height;
  s_ready = false;
  s_eatUntilRelease = false;
  s_calLogged = false;
  s_haveRaw = false;
  s_didI2c = false;
  s_rawStampMs = 0;
  s_lastI2cMs = 0;
  loadCalibrationLocked();
  logCalibrationOnce();

  touchWireBegin();
  touch.begin();
  touch.setRotation(TOUCH_GT911_LIB_ROTATION);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchReadCb);
#if SHOWDUINO_TOUCH_SCROLL_DIAG
  s_indev = indev;
  s_diagWasPressed = false;
  Serial.println("[Touch] scroll diagnostics ON (PRESS/MOVE/RELEASE + Scroll y)");
#endif

  s_ready = true;
  Serial.println("[Touch] GT911 ready");
  Serial.printf("Touch: GT911 LVGL ready (libRot=%u %ux%u landscape)\n",
                (unsigned)TOUCH_GT911_LIB_ROTATION, (unsigned)width, (unsigned)height);
}

void touchLvglRestoreAfterSd() {
  if (!s_touch) return;
  touchWireBegin();
  s_touch->begin();
  s_touch->setRotation(TOUCH_GT911_LIB_ROTATION);
  s_haveRaw = false;
  s_didI2c = false;
  s_ready = true;
  Serial.println("Touch: GT911 re-init after SD");
}

bool touchLvglReady() {
  return s_ready;
}

bool touchLvglPollActivity() {
  int32_t x = 0, y = 0;
  if (!sampleTouch(x, y)) return false;
  backlightNotifyActivity();
  s_eatUntilRelease = true;
  return true;
}

ShowduinoTouchCalMode touchLvglCalibrationMode() {
  return s_calMode;
}

bool touchLvglCalibrationIsNvs() {
  return s_calMode == SHOWDUINO_TOUCH_CAL_MODE_NVS;
}

uint16_t touchLvglCalibrationVersion() {
  return s_calMode == SHOWDUINO_TOUCH_CAL_MODE_NVS ? s_nvsCal.version : 0;
}

void touchLvglPrintCalibrationStatus() {
  Serial.println("TOUCH CALIBRATION");
  if (s_calMode == SHOWDUINO_TOUCH_CAL_MODE_NVS) {
    Serial.println("MODE NVS");
    Serial.printf("VERSION %u\n", (unsigned)s_nvsCal.version);
    Serial.printf("DISPLAY %ux%u\n", (unsigned)s_nvsCal.width, (unsigned)s_nvsCal.height);
    Serial.println("VALID YES");
  } else {
    Serial.println("MODE FACTORY");
    Serial.printf("DISPLAY %ux%u\n", (unsigned)s_w, (unsigned)s_h);
    Serial.println("VALID NO_SAVED_CALIBRATION");
  }
}

bool touchLvglSaveCalibration(const ShowduinoTouchCalibrationRecord &rec) {
  if (showduino_touch_cal_record_valid(&rec, sizeof(rec), SCREEN_WIDTH, SCREEN_HEIGHT) !=
      SHOWDUINO_TOUCH_CAL_OK) {
    Serial.println("[TouchCal] invalid — previous calibration preserved");
    return false;
  }
  if (!touchCalibrationStoreSave(&rec)) {
    Serial.println("[TouchCal] invalid — previous calibration preserved");
    return false;
  }
  s_nvsCal = rec;
  s_calMode = SHOWDUINO_TOUCH_CAL_MODE_NVS;
  Serial.println("[TouchCal] saved NVS v1");
  return true;
}

bool touchLvglResetCalibration() {
  touchCalibrationStoreErase();
  applyFactoryMode();
  Serial.println("[TouchCal] reset to factory");
  return true;
}
