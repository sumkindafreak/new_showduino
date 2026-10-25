#include "showduino_theme.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <strings.h>
#include "ShowduinoOsPalette.h"

#ifndef SHOWDUINO_THEME_INITIAL_OBJECTS
#define SHOWDUINO_THEME_INITIAL_OBJECTS 128
#endif

struct ThemeEntry {
  lv_obj_t *obj;
  showduino_theme_role_t role;
};

struct ThemeTestColour {
  const char *name;
  uint32_t hex;
};

static ThemeEntry *s_entries = nullptr;
static size_t s_capacity = 0;
static size_t s_count = 0;
static bool s_allocationErrorLogged = false;
static lv_color_t s_accent;
static bool s_ready = false;
static uint8_t s_test_index = 0;

static const ThemeTestColour kTestColours[] = {
  { "lime",   0x84FF22 },
  { "purple", 0xB44CFF },
  { "blue",   0x3B82F6 },
  { "red",    0xFF4545 },
  { "amber",  0xFFB020 },
  { "green",  0x22C55E },
};
static const uint8_t kTestColourCount =
    (uint8_t)(sizeof(kTestColours) / sizeof(kTestColours[0]));

static void apply_one(lv_obj_t *obj, showduino_theme_role_t role) {
  if (obj == nullptr) {
    return;
  }

  switch (role) {
    case SHOWDUINO_THEME_ROLE_BORDER:
      lv_obj_set_style_border_color(obj, s_accent, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_border_color(obj, s_accent, LV_PART_MAIN | LV_STATE_PRESSED);
      lv_obj_set_style_border_color(obj, s_accent, LV_PART_MAIN | LV_STATE_FOCUSED);
      break;

    case SHOWDUINO_THEME_ROLE_BORDER_PRESSED:
      lv_obj_set_style_border_color(obj, s_accent, LV_PART_MAIN | LV_STATE_PRESSED);
      lv_obj_set_style_border_color(obj, s_accent, LV_PART_MAIN | LV_STATE_FOCUSED);
      break;

    case SHOWDUINO_THEME_ROLE_TEXT:
      lv_obj_set_style_text_color(obj, s_accent, LV_PART_MAIN | LV_STATE_DEFAULT);
      break;

    case SHOWDUINO_THEME_ROLE_INDICATOR:
      lv_obj_set_style_bg_color(obj, s_accent, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_border_color(obj, s_accent, LV_PART_MAIN | LV_STATE_DEFAULT);
      break;

    case SHOWDUINO_THEME_ROLE_HEADER_ACCENT:
      lv_obj_set_style_bg_color(obj, s_accent, LV_PART_MAIN | LV_STATE_DEFAULT);
      break;

    default:
      break;
  }
}

// Keep the small registry in PSRAM where available; retain existing entries
// if allocation fails, and fall back to ordinary byte-addressable memory.
static bool reserve_entry() {
  if (s_count < s_capacity) return true;
  const size_t next = s_capacity ? s_capacity * 2 : SHOWDUINO_THEME_INITIAL_OBJECTS;
  if (next <= s_capacity || next > SIZE_MAX / sizeof(ThemeEntry)) return false;
  const size_t bytes = next * sizeof(ThemeEntry);
  ThemeEntry *grown = (ThemeEntry *)heap_caps_realloc(
      s_entries, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!grown) grown = (ThemeEntry *)heap_caps_realloc(s_entries, bytes, MALLOC_CAP_8BIT);
  if (!grown) return false;
  s_entries = grown;
  s_capacity = next;
  s_allocationErrorLogged = false;
  Serial.printf("[Theme] registry capacity=%u\n", (unsigned)s_capacity);
  return true;
}

void showduino_theme_init(void) {
  if (s_ready) {
    return;
  }
  s_accent = lv_color_hex(ShowduinoPalette::Accent);
  s_count = 0;
  s_ready = true;
  Serial.println("[Theme] init accent=0x84FF22");
}

void showduino_theme_set_accent(lv_color_t colour) {
  if (!s_ready) {
    showduino_theme_init();
  }
  s_accent = colour;
  showduino_theme_apply();
  const uint32_t u = lv_color_to_u32(colour);
  Serial.printf("[Theme] accent set 0x%06lX\n", (unsigned long)(u & 0xFFFFFFu));
}

lv_color_t showduino_theme_get_accent(void) {
  if (!s_ready) {
    showduino_theme_init();
  }
  return s_accent;
}

void showduino_theme_register(lv_obj_t *obj, showduino_theme_role_t role) {
  if (obj == nullptr) {
    return;
  }
  if (!s_ready) {
    showduino_theme_init();
  }

  for (size_t i = 0; i < s_count; i++) {
    if (s_entries[i].obj == obj) {
      s_entries[i].role = role;
      apply_one(obj, role);
      return;
    }
  }

  if (!reserve_entry()) {
    // Initial styling still works even when future accent updates cannot track it.
    apply_one(obj, role);
    if (!s_allocationErrorLogged) {
      Serial.printf("[Theme][ERROR] registry allocation failed at %u objects\n", (unsigned)s_count);
      s_allocationErrorLogged = true;
    }
    return;
  }

  s_entries[s_count].obj = obj;
  s_entries[s_count].role = role;
  s_count++;
  apply_one(obj, role);
}

void showduino_theme_unregister(lv_obj_t *obj) {
  if (obj == nullptr || s_count == 0) {
    return;
  }
  for (size_t i = 0; i < s_count; i++) {
    if (s_entries[i].obj == obj) {
      for (size_t j = i; j + 1 < s_count; j++) {
        s_entries[j] = s_entries[j + 1];
      }
      s_count--;
      s_entries[s_count].obj = nullptr;
      return;
    }
  }
}

void showduino_theme_clear_registry(void) {
  for (size_t i = 0; i < s_count; i++) {
    s_entries[i].obj = nullptr;
  }
  s_count = 0;
}

void showduino_theme_apply(void) {
  if (!s_ready) {
    showduino_theme_init();
  }
  for (size_t i = 0; i < s_count; i++) {
    apply_one(s_entries[i].obj, s_entries[i].role);
  }
}

bool showduino_theme_test_apply_named(const char *name) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }
  for (uint8_t i = 0; i < kTestColourCount; i++) {
    if (strcasecmp(name, kTestColours[i].name) == 0) {
      s_test_index = i;
      showduino_theme_set_accent(lv_color_hex(kTestColours[i].hex));
      Serial.printf("[Theme] test colour '%s'\n", kTestColours[i].name);
      return true;
    }
  }
  Serial.printf("[Theme] unknown test colour '%s'\n", name);
  return false;
}

void showduino_theme_test_next(void) {
  s_test_index = (uint8_t)((s_test_index + 1) % kTestColourCount);
  showduino_theme_test_apply_named(kTestColours[s_test_index].name);
}