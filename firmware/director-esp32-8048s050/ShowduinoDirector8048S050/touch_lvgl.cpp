#include "touch_lvgl.h"
#include "BoardConfig.h"

#include <Wire.h>
#include <lvgl.h>

static TAMC_GT911 *s_touch = nullptr;
static uint16_t s_w = 0;
static uint16_t s_h = 0;
static uint8_t s_rot = 0;
static bool s_ready = false;

static void touchReadCb(lv_indev_t *indev, lv_indev_data_t *data) {
  (void)indev;
  data->state = LV_INDEV_STATE_RELEASED;

  if (!s_touch || !s_ready) return;

  s_touch->read();
  if (!s_touch->isTouched) return;

  TP_Point p = s_touch->points[0];
  int32_t x = p.x;
  int32_t y = p.y;

  /* Landscape (rot 0): use native GT911 coordinates.
   * Portrait rot 1 (BankOfDad): swap/flip — kept for completeness, unused. */
  if (s_rot == 1) {
    int32_t tx = y;
    int32_t ty = s_w - 1 - x;
    x = tx;
    y = ty;
  }

  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= (int32_t)s_w) x = s_w - 1;
  if (y >= (int32_t)s_h) y = s_h - 1;

  data->point.x = (lv_coord_t)x;
  data->point.y = (lv_coord_t)y;
  data->state = LV_INDEV_STATE_PRESSED;
}

void touchLvglInit(TAMC_GT911 &touch, uint16_t width, uint16_t height, uint8_t displayRotation) {
  s_touch = &touch;
  s_w = width;
  s_h = height;
  s_rot = displayRotation;
  s_ready = false;

  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
  Wire.setTimeOut(100);
  Wire.setClock(400000);

  touch.begin();
  touch.setRotation(displayRotation);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchReadCb);

  s_ready = true;
  Serial.printf("Touch: GT911 LVGL ready (landscape rot=%u %ux%u)\n",
                (unsigned)displayRotation, (unsigned)width, (unsigned)height);
}

void touchLvglRestoreAfterSd() {
  if (!s_ready) return;
  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
  Wire.setTimeOut(100);
  Wire.setClock(400000);
  Serial.println("Touch: I2C soft-restore after SD (GT911 kept).");
}

bool touchLvglReady() {
  return s_ready;
}
