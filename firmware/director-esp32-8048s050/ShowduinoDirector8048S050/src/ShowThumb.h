#ifndef SHOWDUINO_SHOW_THUMB_H
#define SHOWDUINO_SHOW_THUMB_H

#include <Arduino.h>
#include <SD.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "StorageConfig.h"
#include "FileUtil.h"

/**
 * Lightweight show thumbnail helper.
 * - Default Showduino icon drawn with LVGL objects (no image assets required).
 * - Optional thumbnail.bmp (24-bit uncompressed, max 96x64) for details view.
 * Unknown / unsupported files are ignored gracefully.
 */
namespace ShowduinoShowThumb {

inline lv_obj_t *makeDefaultIcon(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, w, h);
  lv_obj_set_style_bg_color(box, lv_color_hex(0x7F1D1D), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(0xEF4444), 0);
  lv_obj_set_style_border_width(box, 2, 0);
  lv_obj_set_style_radius(box, 6, 0);

  lv_obj_t *mark = lv_label_create(box);
  lv_label_set_text(mark, "SD");
  lv_obj_set_style_text_color(mark, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(mark);
  return box;
}

inline void formatDuration(uint32_t seconds, char *out, size_t outLen) {
  if (!out || outLen < 4) return;
  uint32_t m = seconds / 60UL;
  uint32_t s = seconds % 60UL;
  if (m >= 60) {
    uint32_t h = m / 60UL;
    m = m % 60UL;
    snprintf(out, outLen, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  } else {
    snprintf(out, outLen, "%lu:%02lu", (unsigned long)m, (unsigned long)s);
  }
}

/**
 * Load a small 24-bit BMP into an LVGL canvas.
 * Returns false on missing/unsupported files (caller keeps default icon).
 */
inline bool loadBmpToCanvas(lv_obj_t *canvas, const char *path, uint16_t maxW, uint16_t maxH) {
  if (!canvas || !path || !ShowduinoFileUtil::pathLooksSafe(path) || !SD.exists(path)) {
    return false;
  }

  File f = SD.open(path, FILE_READ);
  if (!f || f.size() < 54) {
    if (f) f.close();
    return false;
  }

  uint8_t hdr[54];
  if (f.read(hdr, 54) != 54) {
    f.close();
    return false;
  }
  if (hdr[0] != 'B' || hdr[1] != 'M') {
    f.close();
    return false;
  }

  uint32_t dataOffset = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) |
                        ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
  int32_t width = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) |
                            ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
  int32_t height = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) |
                             ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
  uint16_t bpp = (uint16_t)hdr[28] | ((uint16_t)hdr[29] << 8);
  uint32_t compression = (uint32_t)hdr[30] | ((uint32_t)hdr[31] << 8) |
                         ((uint32_t)hdr[32] << 16) | ((uint32_t)hdr[33] << 24);

  bool bottomUp = true;
  if (height < 0) {
    bottomUp = false;
    height = -height;
  }

  if (bpp != 24 || compression != 0 || width <= 0 || height <= 0 ||
      width > (int32_t)maxW || height > (int32_t)maxH) {
    f.close();
    return false;
  }

  const size_t pxCount = (size_t)width * (size_t)height;
  uint16_t *buf = (uint16_t *)heap_caps_malloc(pxCount * sizeof(uint16_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) {
    buf = (uint16_t *)heap_caps_malloc(pxCount * sizeof(uint16_t),
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  if (!buf) {
    f.close();
    return false;
  }

  const uint32_t rowBytes = ((uint32_t)width * 3UL + 3UL) & ~3UL;
  uint8_t *row = (uint8_t *)malloc(rowBytes);
  if (!row) {
    heap_caps_free(buf);
    f.close();
    return false;
  }

  if (!f.seek(dataOffset)) {
    free(row);
    heap_caps_free(buf);
    f.close();
    return false;
  }

  for (int32_t y = 0; y < height; y++) {
    if (f.read(row, rowBytes) != (int)rowBytes) {
      free(row);
      heap_caps_free(buf);
      f.close();
      return false;
    }
    int32_t destY = bottomUp ? (height - 1 - y) : y;
    for (int32_t x = 0; x < width; x++) {
      uint8_t b = row[x * 3 + 0];
      uint8_t g = row[x * 3 + 1];
      uint8_t r = row[x * 3 + 2];
      buf[destY * width + x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
    yield();
  }

  free(row);
  f.close();

  lv_obj_set_size(canvas, width, height);
  lv_canvas_set_buffer(canvas, buf, width, height, LV_COLOR_FORMAT_RGB565);
  lv_obj_set_user_data(canvas, buf);
  return true;
}

inline void freeCanvasBuffer(lv_obj_t *canvas) {
  if (!canvas) return;
  void *buf = lv_obj_get_user_data(canvas);
  if (buf) {
    heap_caps_free(buf);
    lv_obj_set_user_data(canvas, nullptr);
  }
}

}  // namespace ShowduinoShowThumb

#endif
