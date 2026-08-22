#include "touch_lvgl.h"
#include "BoardConfig.h"
#include "backlight.h"

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

#ifndef TOUCH_CAL_X_LEFT
#define TOUCH_CAL_X_LEFT  790
#endif
#ifndef TOUCH_CAL_X_RIGHT
#define TOUCH_CAL_X_RIGHT 18
#endif
#ifndef TOUCH_CAL_Y_TOP
#define TOUCH_CAL_Y_TOP   465
#endif
#ifndef TOUCH_CAL_Y_BOT
#define TOUCH_CAL_Y_BOT   25
#endif

/* Temporary Serial diagnostics for scroll bring-up — remove after field verify. */
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

void touchLvglSetHook(TouchLvglHook hook) {
  s_touchHook = hook;
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

static int32_t mapTouchAxis(int32_t v, int32_t inA, int32_t inB, int32_t outMax) {
  if (inA == inB) return 0;
  int32_t mapped = (v - inA) * outMax / (inB - inA);
  if (mapped < 0) mapped = 0;
  if (mapped > outMax) mapped = outMax;
  return mapped;
}

static bool sampleTouch(int32_t &x, int32_t &y) {
  if (!s_touch || !s_ready) return false;
  s_touch->read();
  if (!s_touch->isTouched) return false;
  TP_Point p = s_touch->points[0];
  x = mapTouchAxis((int32_t)p.x, TOUCH_CAL_X_LEFT, TOUCH_CAL_X_RIGHT, (int32_t)s_w - 1);
  y = mapTouchAxis((int32_t)p.y, TOUCH_CAL_Y_TOP, TOUCH_CAL_Y_BOT, (int32_t)s_h - 1);
  return true;
}

static void touchReadCb(lv_indev_t *indev, lv_indev_data_t *data) {
  (void)indev;
  data->state = LV_INDEV_STATE_RELEASED;

  int32_t x = 0, y = 0;
  bool pressed = sampleTouch(x, y);

  /* Preserve last pressed coords for release — GT911 reports no sample when up. */
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
    /* First tap after screen-off only wakes — don't fire UI buttons. */
    if (wasOff) {
      s_eatUntilRelease = true;
      s_hadPress = false;
#if SHOWDUINO_TOUCH_SCROLL_DIAG
      Serial.printf("[Touch] WAKE_EAT x=%ld y=%ld\n", (long)x, (long)y);
#endif
      return;
    }
  }

  if (s_touchHook && !s_eatUntilRelease) {
    s_touchHook(x, y, pressed);
  }

  if (s_eatUntilRelease) {
    if (!pressed) s_eatUntilRelease = false;
#if SHOWDUINO_TOUCH_SCROLL_DIAG
    if (!pressed) Serial.println("[Touch] WAKE_EAT release");
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
  Serial.printf("Touch: GT911 LVGL ready (libRot=%u %ux%u landscape)\n",
                (unsigned)TOUCH_GT911_LIB_ROTATION, (unsigned)width, (unsigned)height);
}

void touchLvglRestoreAfterSd() {
  if (!s_touch) return;
  touchWireBegin();
  s_touch->begin();
  s_touch->setRotation(TOUCH_GT911_LIB_ROTATION);
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
