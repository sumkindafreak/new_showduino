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

static TAMC_GT911 *s_touch = nullptr;
static uint16_t s_w = 0;
static uint16_t s_h = 0;
static bool s_ready = false;
static bool s_eatUntilRelease = false;

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

  if (pressed) {
    const bool wasOff = !backlightIsOn();
    backlightNotifyActivity();
    /* First tap after screen-off only wakes — don't fire UI buttons. */
    if (wasOff) {
      s_eatUntilRelease = true;
      return;
    }
  }

  if (s_eatUntilRelease) {
    if (!pressed) s_eatUntilRelease = false;
    return;
  }

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
