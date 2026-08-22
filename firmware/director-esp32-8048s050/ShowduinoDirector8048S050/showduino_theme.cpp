#include "showduino_theme.h"

#include <Arduino.h>
#include <string.h>
#include <strings.h>
#include "ShowduinoOsPalette.h"

#ifndef SHOWDUINO_THEME_MAX_OBJECTS
#define SHOWDUINO_THEME_MAX_OBJECTS 64
#endif

struct ThemeEntry {
  lv_obj_t *obj;
  showduino_theme_role_t role;
};

struct ThemeTestColour {
  const char *name;
  uint32_t hex;
};

static ThemeEntry s_entries[SHOWDUINO_THEME_MAX_OBJECTS];
static uint16_t s_count = 0;
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

void showduino_theme_init(void) {
  if (s_ready) {
    return;
  }
  s_accent = lv_color_hex(ShowduinoPalette::Accent);
  s_count = 0;
  for (uint16_t i = 0; i < SHOWDUINO_THEME_MAX_OBJECTS; i++) {
    s_entries[i].obj = nullptr;
    s_entries[i].role = SHOWDUINO_THEME_ROLE_BORDER;
  }
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

  for (uint16_t i = 0; i < s_count; i++) {
    if (s_entries[i].obj == obj) {
      s_entries[i].role = role;
      apply_one(obj, role);
      return;
    }
  }

  if (s_count >= SHOWDUINO_THEME_MAX_OBJECTS) {
    Serial.println("[Theme] registry full — object not registered");
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
  for (uint16_t i = 0; i < s_count; i++) {
    if (s_entries[i].obj == obj) {
      for (uint16_t j = i; j + 1 < s_count; j++) {
        s_entries[j] = s_entries[j + 1];
      }
      s_count--;
      s_entries[s_count].obj = nullptr;
      return;
    }
  }
}

void showduino_theme_clear_registry(void) {
  for (uint16_t i = 0; i < s_count; i++) {
    s_entries[i].obj = nullptr;
  }
  s_count = 0;
}

void showduino_theme_apply(void) {
  if (!s_ready) {
    showduino_theme_init();
  }
  for (uint16_t i = 0; i < s_count; i++) {
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