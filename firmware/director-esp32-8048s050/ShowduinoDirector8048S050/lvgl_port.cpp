#include "lvgl_port.h"

#include <esp_heap_caps.h>

Arduino_RGB_Display *gfx = nullptr;

static lv_display_t *s_disp = nullptr;
static lv_color_t *s_buf1 = nullptr;
static lv_color_t *s_buf2 = nullptr;

static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap) {
  if (!gfx) {
    lv_display_flush_ready(disp);
    return;
  }

  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)pxMap, w, h);
  lv_display_flush_ready(disp);
}

bool lvglPortInit(Arduino_RGB_Display *panel, Arduino_ESP32RGBPanel *rgbPanel) {
  (void)rgbPanel;
  gfx = panel;
  if (!gfx) return false;

  if (!gfx->begin()) {
    Serial.println("LVGL port: gfx->begin() failed");
    return false;
  }
  gfx->fillScreen(RGB565_BLACK);

  lv_init();
  Serial.println("LVGL: CLIB malloc (use lv_conf.h with LV_STDLIB_CLIB + PSRAM)");

  const uint32_t bufLines = LVGL_BUFFER_LINES;
  const size_t bufPx = (size_t)SCREEN_WIDTH * bufLines;
  s_buf1 = (lv_color_t *)heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  s_buf2 = (lv_color_t *)heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  if (!s_buf1) {
    Serial.println("LVGL port: PSRAM buf1 failed, trying internal...");
    s_buf1 = (lv_color_t *)heap_caps_malloc(bufPx * sizeof(lv_color_t),
                                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  if (!s_buf1) return false;

  s_disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(s_disp, flushCb);
  if (s_buf2) {
    lv_display_set_buffers(s_disp, s_buf1, s_buf2, bufPx * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("LVGL: dual PSRAM partial buffers (landscape).");
  } else {
    lv_display_set_buffers(s_disp, s_buf1, nullptr, bufPx * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("LVGL: single partial buffer (landscape).");
  }

  return true;
}

void lvglPortLoop() {
  lv_timer_handler();
}
