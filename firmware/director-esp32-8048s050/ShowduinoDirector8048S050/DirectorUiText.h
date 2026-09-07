#ifndef SHOWDUINO_DIRECTOR_UI_TEXT_H
#define SHOWDUINO_DIRECTOR_UI_TEXT_H

#include <stddef.h>

/*
 * Director live UI strings should remain ASCII-safe unless glyph support
 * is explicitly verified in the compiled LVGL Montserrat fonts.
 *
 * Verified extra glyphs (same cmap in every enabled Montserrat size):
 *   U+00B0 degree
 *   U+2022 bullet
 *   LV_SYMBOL_* FontAwesome codes bundled with those fonts
 *
 * Not present (LV_USE_FONT_PLACEHOLDER draws a square):
 *   middle dot, em/en dash, ellipsis, arrows, emoji
 *
 * Preferred separators in new copy: ASCII | and -
 */

static inline int director_ui_ascii_ok(unsigned char c) {
  return (c >= 0x20 && c <= 0x7E) || c == '\n' || c == '\r' || c == '\t';
}

/* Presentation-only. Does not mutate stored production/asset/protocol data. */
static inline void director_ui_sanitize_copy(char *dst, size_t dst_len, const char *src) {
  size_t o;
  const unsigned char *p;
  if (!dst || dst_len == 0) return;
  dst[0] = '\0';
  if (!src) return;
  o = 0;
  p = (const unsigned char *)src;
  while (*p && o + 1 < dst_len) {
    if (p[0] == 0xC2 && p[1] == 0xB0 && o + 2 < dst_len) {
      dst[o++] = (char)p[0];
      dst[o++] = (char)p[1];
      p += 2;
      continue;
    }
    if (p[0] == 0xE2 && p[1] == 0x80 && p[2] == 0xA2 && o + 3 < dst_len) {
      dst[o++] = (char)p[0];
      dst[o++] = (char)p[1];
      dst[o++] = (char)p[2];
      p += 3;
      continue;
    }
    if (director_ui_ascii_ok(*p)) {
      dst[o++] = (char)*p++;
      continue;
    }
    if ((*p & 0x80) != 0) {
      unsigned extra = 0;
      if ((*p & 0xE0) == 0xC0) extra = 1;
      else if ((*p & 0xF0) == 0xE0) extra = 2;
      else if ((*p & 0xF8) == 0xF0) extra = 3;
      p++;
      while (extra && (*p & 0xC0) == 0x80) {
        p++;
        extra--;
      }
    } else {
      p++;
    }
    dst[o++] = '?';
  }
  dst[o] = '\0';
}

#endif
