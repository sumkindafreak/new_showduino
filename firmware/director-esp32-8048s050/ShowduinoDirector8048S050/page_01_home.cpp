#include "page_01_home.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "showduino_theme.h"
#include "ShowduinoOsPalette.h"
#include "ShowduinoOsUi.h"
#include "DisplayTypes.h"

/* ================================================================
 * Page 01 geometry — 800×480, below the persistent status bar
 * ================================================================ */
static const int16_t kHeaderX = 0;
static const int16_t kHeaderY = (int16_t)OS_TITLE_Y;
static const int16_t kHeaderW = 800;
static const int16_t kHeaderH = OS_TITLE_H;

static const int16_t kHeroX = 24;
static const int16_t kHeroY = (int16_t)(kHeaderY + kHeaderH + OS_GAP);
static const int16_t kHeroW = 752;
static const int16_t kHeroH = 88;

static const int16_t kSecY = (int16_t)(kHeroY + kHeroH + OS_GAP);
static const int16_t kSecH = 64;
static const int16_t kSecLeftX = 24;
static const int16_t kSecRightX = 408;
static const int16_t kSecW = 368;

static const int16_t kToolY = (int16_t)(kSecY + kSecH + OS_GAP);
static const int16_t kToolH = 56;
static const int16_t kToolXs[4] = { 24, 216, 408, 600 };
static const int16_t kToolWs[4] = { 180, 180, 180, 176 };

static const int16_t kFooterX = 24;
static const int16_t kFooterH = 44;
static const int16_t kFooterY = (int16_t)(OS_DOCK_Y - kFooterH - OS_GAP);
static const int16_t kFooterW = 752;
static const int16_t kFooterSlotW = 90;

enum Page01ControlId : uint8_t {
  PAGE01_CTRL_HERO = 0,
  PAGE01_CTRL_PRODUCTIONS,
  PAGE01_CTRL_CUE_LIBRARY,
  PAGE01_CTRL_NODES,
  PAGE01_CTRL_OUTPUTS,
  PAGE01_CTRL_SETTINGS,
  PAGE01_CTRL_DIAGNOSTICS,
  PAGE01_CTRL_COUNT
};

enum Page01VisualRole : uint8_t {
  PAGE01_ROLE_HERO = 0,
  PAGE01_ROLE_SECONDARY,
  PAGE01_ROLE_TOOL,
  PAGE01_ROLE_QUIET
};

struct Page01Control {
  lv_obj_t *btn;
  lv_obj_t *icon;
  lv_obj_t *label;
  lv_obj_t *sublabel; /* hero only */
  lv_obj_t *debug_label;
  const char *title;
  const char *symbol;
  const char *command;
  Page01VisualRole role;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct Page01FooterSlot {
  lv_obj_t *dot;
  lv_obj_t *label;
  const char *title;
  bool (*cap_ok)(const ShowduinoCapabilities *);
};

static bool cap_always(const ShowduinoCapabilities *) { return true; }
static bool cap_relay(const ShowduinoCapabilities *c) { return c && c->relay; }
static bool cap_mosfet(const ShowduinoCapabilities *c) { return c && c->mosfet; }
static bool cap_neopixel(const ShowduinoCapabilities *c) { return c && c->neopixel; }
static bool cap_audio(const ShowduinoCapabilities *c) { return c && c->audio; }
static bool cap_dmx(const ShowduinoCapabilities *c) { return c && c->dmx; }

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_header = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_production = nullptr;
static lv_obj_t *s_link = nullptr;
static lv_obj_t *s_readiness = nullptr;
static lv_obj_t *s_clock = nullptr;
static lv_obj_t *s_header_accent = nullptr;
static lv_obj_t *s_footer = nullptr;
static lv_obj_t *s_notify = nullptr;

static Page01Control s_ctrls[PAGE01_CTRL_COUNT];
static Page01FooterSlot s_footer_slots[7];
static ShowduinoCapabilities s_caps;
static page01_command_fn s_command_cb = nullptr;
static bool s_active = false;
static bool s_has_production = false;
static char s_production_name[64] = "";
static char s_link_text[32] = "OFFLINE";
static char s_readiness_text[24] = "NO PRODUCTION";

static void set_label_safe(lv_obj_t *lab, const char *text) {
  if (lab == nullptr || text == nullptr) {
    return;
  }
  lv_label_set_text(lab, text);
}

static bool production_name_is_empty(const char *name) {
  if (name == nullptr || name[0] == '\0') return true;
  if (strcmp(name, "—") == 0) return true;
  if (strcmp(name, "-") == 0) return true;
  if (strcmp(name, "NO PRODUCTION") == 0) return true;
  if (strcmp(name, "No Show Loaded") == 0) return true;
  if (strcmp(name, "No Production") == 0) return true;
  return false;
}

static void style_control(lv_obj_t *obj, Page01VisualRole role) {
  lv_obj_remove_style_all(obj);
  lv_opa_t bg_def = LV_OPA_20;
  lv_opa_t bg_pr = LV_OPA_40;
  lv_opa_t border_opa = LV_OPA_80;
  uint8_t border_w = 2;
  uint8_t radius = OS_PANEL_RADIUS;

  if (role == PAGE01_ROLE_HERO) {
    bg_def = LV_OPA_30;
    bg_pr = LV_OPA_50;
    border_opa = LV_OPA_COVER;
    border_w = 2;
    radius = OS_PANEL_RADIUS;
  } else if (role == PAGE01_ROLE_QUIET) {
    bg_def = LV_OPA_10;
    bg_pr = LV_OPA_20;
    border_opa = LV_OPA_40;
    border_w = 1;
  } else if (role == PAGE01_ROLE_TOOL) {
    bg_def = LV_OPA_10;
    bg_pr = LV_OPA_30;
    border_opa = LV_OPA_60;
  }

  lv_obj_set_style_bg_opa(obj, bg_def, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(obj, lv_color_hex(ShowduinoPalette::PanelRaised),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(obj, bg_pr, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(obj, lv_color_hex(ShowduinoPalette::PanelRaised),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(obj, border_w, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_opa(obj, border_opa, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(obj, border_w + 1, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(obj, border_w + 1, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

static void control_event_cb(lv_event_t *event) {
  const lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(event);
  if (target == nullptr) {
    return;
  }

#if SHOWDUINO_PAGE01_ALIGNMENT_DEBUG
  if (code == LV_EVENT_PRESSED) {
    lv_point_t p;
    lv_indev_t *indev = lv_indev_active();
    if (indev) {
      lv_indev_get_point(indev, &p);
      Serial.printf("[Page01][Align] press touch=(%ld,%ld) obj=(%d,%d %dx%d)\n",
                    (long)p.x, (long)p.y,
                    (int)lv_obj_get_x(target), (int)lv_obj_get_y(target),
                    (int)lv_obj_get_width(target), (int)lv_obj_get_height(target));
    }
  }
#endif

  if (code != LV_EVENT_CLICKED) {
    return;
  }
  if (lv_obj_has_state(target, LV_STATE_DISABLED)) {
    Serial.println("[Page01] control clicked but disabled");
    return;
  }
  const char *cmd = (const char *)lv_obj_get_user_data(target);
  if (cmd == nullptr) {
    return;
  }
  Serial.printf("[Page01] navigate %s\n", cmd);
  if (s_command_cb != nullptr) {
    s_command_cb(cmd);
  }
}

static void apply_footer_cap_style(Page01FooterSlot *slot, bool available) {
  if (slot == nullptr) {
    return;
  }
  const lv_opa_t opa = available ? LV_OPA_COVER : LV_OPA_40;
  if (slot->dot) {
    lv_obj_clear_flag(slot->dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(slot->dot, opa, 0);
  }
  if (slot->label) {
    lv_obj_clear_flag(slot->label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(slot->label, opa, 0);
  }
}

static void apply_footer_visibility(void) {
  for (uint8_t i = 0; i < 7; i++) {
    Page01FooterSlot *slot = &s_footer_slots[i];
    const bool available = slot->cap_ok == nullptr || slot->cap_ok(&s_caps);
    apply_footer_cap_style(slot, available);
  }
}

static void recompute_readiness(void) {
  const char *next = "UNKNOWN";
  if (!s_has_production) {
    next = "NO PRODUCTION";
  } else if (strcmp(s_link_text, "OFFLINE") == 0 ||
             strcmp(s_link_text, "LINK LOST") == 0) {
    next = "OFFLINE";
  } else if (strcmp(s_link_text, "SEARCHING") == 0 ||
             strcmp(s_link_text, "NO STAGE") == 0 ||
             strcmp(s_link_text, "LINK NO STAGE") == 0 ||
             strcmp(s_link_text, "LINK SEARCH") == 0) {
    next = "DEGRADED";
  } else if (strcmp(s_link_text, "LINK OK") == 0) {
    next = "READY";
  } else {
    next = "DEGRADED";
  }

  if (strcmp(s_readiness_text, next) != 0) {
    strncpy(s_readiness_text, next, sizeof(s_readiness_text) - 1);
    s_readiness_text[sizeof(s_readiness_text) - 1] = '\0';
    Serial.printf("[Page01] readiness → %s (link=%s prod=%d)\n",
                  s_readiness_text, s_link_text, (int)s_has_production);
  }
  if (s_readiness) {
    lv_label_set_text(s_readiness, s_readiness_text);
    lv_color_t col = lv_color_hex(ShowduinoPalette::Muted);
    if (strcmp(s_readiness_text, "READY") == 0) {
      col = lv_color_hex(ShowduinoPalette::Accent);
    } else if (strcmp(s_readiness_text, "DEGRADED") == 0) {
      col = lv_color_hex(ShowduinoPalette::Warn);
    } else if (strcmp(s_readiness_text, "OFFLINE") == 0 ||
               strcmp(s_readiness_text, "FAULT") == 0) {
      col = lv_color_hex(ShowduinoPalette::Danger);
    }
    lv_obj_set_style_text_color(s_readiness, col, 0);
  }
}

static void set_tile_enabled(Page01Control *c, bool enabled) {
  if (c == nullptr || c->btn == nullptr) {
    return;
  }
  if (enabled) {
    lv_obj_clear_state(c->btn, LV_STATE_DISABLED);
    lv_obj_add_flag(c->btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(c->btn, LV_OPA_COVER, 0);
  } else {
    lv_obj_add_state(c->btn, LV_STATE_DISABLED);
    lv_obj_clear_flag(c->btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(c->btn, LV_OPA_40, 0);
  }
}

static void refresh_hero(void) {
  Page01Control *hero = &s_ctrls[PAGE01_CTRL_HERO];
  if (hero->btn == nullptr) {
    return;
  }

  if (!s_has_production) {
    hero->command = PAGE01_CMD_PRODUCTIONS;
    lv_obj_set_user_data(hero->btn, (void *)PAGE01_CMD_PRODUCTIONS);
    if (hero->label) {
      lv_label_set_text(hero->label, "SELECT A PRODUCTION");
    }
    if (hero->sublabel) {
      lv_label_set_text(hero->sublabel, "No production loaded · Tap to open Productions");
      lv_obj_set_style_text_color(hero->sublabel,
                                  lv_color_hex(ShowduinoPalette::Muted), 0);
    }
    if (hero->icon) {
      lv_label_set_text(hero->icon, LV_SYMBOL_DIRECTORY);
    }
  } else {
    hero->command = PAGE01_CMD_RUN_SHOW;
    lv_obj_set_user_data(hero->btn, (void *)PAGE01_CMD_RUN_SHOW);
    if (hero->label) {
      lv_label_set_text(hero->label, "RUN SHOW");
    }
    if (hero->sublabel) {
      char sub[96];
      snprintf(sub, sizeof(sub), "%s · %s", s_production_name, s_readiness_text);
      lv_label_set_text(hero->sublabel, sub);
      lv_obj_set_style_text_color(hero->sublabel,
                                  lv_color_hex(ShowduinoPalette::Text), 0);
    }
    if (hero->icon) {
      lv_label_set_text(hero->icon, LV_SYMBOL_PLAY);
    }
    const bool canRun = (strcmp(s_link_text, "LINK OK") == 0);
    set_tile_enabled(hero, canRun);
    if (!canRun && hero->sublabel) {
      char sub[96];
      snprintf(sub, sizeof(sub), "%s · %s — cannot run until Stage is linked",
               s_production_name, s_readiness_text);
      lv_label_set_text(hero->sublabel, sub);
    }
    return;
  }
  set_tile_enabled(hero, true);
}

static void build_header(lv_obj_t *parent) {
  s_header = lv_obj_create(parent);
  lv_obj_remove_style_all(s_header);
  lv_obj_set_pos(s_header, kHeaderX, kHeaderY);
  lv_obj_set_size(s_header, kHeaderW, kHeaderH);
  lv_obj_set_style_bg_opa(s_header, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(s_header, LV_OBJ_FLAG_CLICKABLE);

  s_header_accent = lv_obj_create(s_header);
  lv_obj_remove_style_all(s_header_accent);
  lv_obj_set_pos(s_header_accent, 16, 34);
  lv_obj_set_size(s_header_accent, 72, 3);
  lv_obj_set_style_bg_opa(s_header_accent, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_header_accent, 1, 0);
  showduino_theme_register(s_header_accent, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);

  s_title = lv_label_create(s_header);
  lv_label_set_text(s_title, "HOME");
  lv_obj_set_pos(s_title, 16, 8);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_title, lv_color_hex(ShowduinoPalette::Text), 0);
  showduino_theme_register(s_title, SHOWDUINO_THEME_ROLE_TEXT);

  s_production = lv_label_create(s_header);
  lv_label_set_text(s_production, "NO PRODUCTION");
  lv_obj_set_pos(s_production, 100, 10);
  lv_obj_set_width(s_production, 280);
  lv_label_set_long_mode(s_production, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_production, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_production, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_link = lv_label_create(s_header);
  lv_label_set_text(s_link, "OFFLINE");
  lv_obj_set_pos(s_link, 400, 10);
  lv_obj_set_width(s_link, 180);
  lv_label_set_long_mode(s_link, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_link, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_link, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_readiness = lv_label_create(s_header);
  lv_label_set_text(s_readiness, "NO PRODUCTION");
  lv_obj_set_pos(s_readiness, 590, 10);
  lv_obj_set_width(s_readiness, 190);
  lv_label_set_long_mode(s_readiness, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_readiness, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_readiness, lv_color_hex(ShowduinoPalette::Muted), 0);

  /* Wall clock lives on the persistent status bar. */
  s_clock = nullptr;
}

static void build_control(uint8_t index) {
  Page01Control *c = &s_ctrls[index];
  c->btn = lv_obj_create(s_root);
  style_control(c->btn, c->role);
  lv_obj_set_pos(c->btn, c->x, c->y);
  lv_obj_set_size(c->btn, c->w, c->h);
  lv_obj_set_user_data(c->btn, (void *)c->command);
  lv_obj_add_event_cb(c->btn, control_event_cb, LV_EVENT_CLICKED, nullptr);
#if SHOWDUINO_PAGE01_ALIGNMENT_DEBUG
  lv_obj_add_event_cb(c->btn, control_event_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_set_style_border_width(c->btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_opa(c->btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(c->btn, lv_color_hex(0xFFEE58),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
#else
  showduino_theme_register(c->btn, SHOWDUINO_THEME_ROLE_BORDER);
#endif

  const lv_font_t *title_font = &lv_font_montserrat_14;
  int16_t icon_dx = -56;
  if (c->role == PAGE01_ROLE_HERO) {
    title_font = &lv_font_montserrat_20;
    icon_dx = -160;
  } else if (c->role == PAGE01_ROLE_SECONDARY) {
    title_font = &lv_font_montserrat_16;
    icon_dx = -90;
  } else if (c->role == PAGE01_ROLE_QUIET) {
    title_font = &lv_font_montserrat_14;
    icon_dx = -40;
  }

  c->icon = lv_label_create(c->btn);
  lv_label_set_text(c->icon, c->symbol);
  lv_obj_set_style_text_font(c->icon, &lv_font_montserrat_16, 0);
  if (c->role == PAGE01_ROLE_QUIET) {
    lv_obj_set_style_text_opa(c->icon, LV_OPA_60, 0);
  }
  showduino_theme_register(c->icon, SHOWDUINO_THEME_ROLE_TEXT);

  c->label = lv_label_create(c->btn);
  lv_label_set_text(c->label, c->title);
  lv_obj_set_style_text_font(c->label, title_font, 0);
  if (c->role == PAGE01_ROLE_QUIET) {
    lv_obj_set_style_text_color(c->label, lv_color_hex(ShowduinoPalette::Muted), 0);
    lv_obj_set_style_text_opa(c->label, LV_OPA_70, 0);
  } else {
    lv_obj_set_style_text_color(c->label, lv_color_hex(ShowduinoPalette::Text), 0);
  }

  c->sublabel = nullptr;
  if (c->role == PAGE01_ROLE_HERO) {
    lv_obj_align(c->icon, LV_ALIGN_LEFT_MID, 28, -10);
    lv_obj_align(c->label, LV_ALIGN_LEFT_MID, 72, -12);
    c->sublabel = lv_label_create(c->btn);
    lv_label_set_text(c->sublabel, "No production loaded · Tap to open Productions");
    lv_obj_set_style_text_font(c->sublabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(c->sublabel, lv_color_hex(ShowduinoPalette::Muted), 0);
    lv_obj_set_width(c->sublabel, (int16_t)(c->w - 100));
    lv_label_set_long_mode(c->sublabel, LV_LABEL_LONG_CLIP);
    lv_obj_align(c->sublabel, LV_ALIGN_LEFT_MID, 72, 16);
  } else {
    lv_obj_align(c->icon, LV_ALIGN_CENTER, icon_dx, 0);
    lv_obj_align(c->label, LV_ALIGN_CENTER, 12, 0);
  }

  c->debug_label = nullptr;
#if SHOWDUINO_PAGE01_ALIGNMENT_DEBUG
  c->debug_label = lv_label_create(c->btn);
  char num[4];
  snprintf(num, sizeof(num), "%u", (unsigned)(index + 1));
  lv_label_set_text(c->debug_label, num);
  lv_obj_set_style_text_font(c->debug_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->debug_label, lv_color_hex(0xFFEE58), 0);
  lv_obj_align(c->debug_label, LV_ALIGN_TOP_RIGHT, -6, 4);
  Serial.printf("[Page01][Align] ctrl %u '%s' x=%d y=%d w=%d h=%d\n",
                (unsigned)(index + 1), c->title,
                (int)c->x, (int)c->y, (int)c->w, (int)c->h);
#endif
}

static void build_controls(void) {
  /* Hero — command swaps between RUN_SHOW and PRODUCTIONS via refresh_hero(). */
  s_ctrls[PAGE01_CTRL_HERO] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    "RUN SHOW", LV_SYMBOL_PLAY, PAGE01_CMD_PRODUCTIONS, PAGE01_ROLE_HERO,
    kHeroX, kHeroY, kHeroW, kHeroH
  };
  s_ctrls[PAGE01_CTRL_PRODUCTIONS] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    "PRODUCTIONS", LV_SYMBOL_DIRECTORY, PAGE01_CMD_PRODUCTIONS, PAGE01_ROLE_SECONDARY,
    kSecLeftX, kSecY, kSecW, kSecH
  };
  s_ctrls[PAGE01_CTRL_CUE_LIBRARY] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    "CUE LIBRARY", LV_SYMBOL_LIST, PAGE01_CMD_CUE_LIBRARY, PAGE01_ROLE_SECONDARY,
    kSecRightX, kSecY, kSecW, kSecH
  };
  s_ctrls[PAGE01_CTRL_NODES] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    "NODES", LV_SYMBOL_GPS, PAGE01_CMD_NODES, PAGE01_ROLE_TOOL,
    kToolXs[0], kToolY, kToolWs[0], kToolH
  };
  s_ctrls[PAGE01_CTRL_OUTPUTS] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    "OUTPUTS", LV_SYMBOL_CHARGE, PAGE01_CMD_OUTPUTS, PAGE01_ROLE_TOOL,
    kToolXs[1], kToolY, kToolWs[1], kToolH
  };
  s_ctrls[PAGE01_CTRL_SETTINGS] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    "SETTINGS", LV_SYMBOL_SETTINGS, PAGE01_CMD_SETTINGS, PAGE01_ROLE_TOOL,
    kToolXs[2], kToolY, kToolWs[2], kToolH
  };
  s_ctrls[PAGE01_CTRL_DIAGNOSTICS] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    "DIAGNOSTICS", LV_SYMBOL_WARNING, PAGE01_CMD_DIAGNOSTICS, PAGE01_ROLE_QUIET,
    kToolXs[3], kToolY, kToolWs[3], kToolH
  };

  for (uint8_t i = 0; i < PAGE01_CTRL_COUNT; i++) {
    build_control(i);
  }
  /* Cue Library and Outputs have no live page yet — visible, not tappable. */
  set_tile_enabled(&s_ctrls[PAGE01_CTRL_CUE_LIBRARY], false);
  set_tile_enabled(&s_ctrls[PAGE01_CTRL_OUTPUTS], false);
  refresh_hero();

  for (uint8_t i = 0; i < PAGE01_CTRL_COUNT; i++) {
    const Page01Control *c = &s_ctrls[i];
    const int16_t x2 = (int16_t)(c->x + c->w);
    const int16_t y2 = (int16_t)(c->y + c->h);
    if (c->x < 0 || c->y < kHeaderH || x2 > (int16_t)DISPLAY_WIDTH || y2 > kFooterY) {
      Serial.printf("[Page01] WARN ctrl '%s' may overlap chrome (%d,%d)-(%d,%d)\n",
                    c->title, (int)c->x, (int)c->y, (int)x2, (int)y2);
    }
  }
}

static void build_footer(lv_obj_t *parent) {
  s_footer = lv_obj_create(parent);
  lv_obj_remove_style_all(s_footer);
  lv_obj_set_pos(s_footer, kFooterX, kFooterY);
  lv_obj_set_size(s_footer, kFooterW, kFooterH);
  lv_obj_set_style_bg_opa(s_footer, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(s_footer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(s_footer, LV_OBJ_FLAG_CLICKABLE);

  static const char *titles[7] = {
    "SUE", "P4", "Relay", "MOSFET", "NeoPixel", "Audio", "DMX"
  };
  static bool (*cap_fns[7])(const ShowduinoCapabilities *) = {
    cap_always, cap_always, cap_relay, cap_mosfet, cap_neopixel, cap_audio, cap_dmx
  };

  for (uint8_t i = 0; i < 7; i++) {
    const int16_t x = (int16_t)(4 + i * kFooterSlotW);
    s_footer_slots[i].title = titles[i];
    s_footer_slots[i].cap_ok = cap_fns[i];

    s_footer_slots[i].dot = lv_obj_create(s_footer);
    lv_obj_remove_style_all(s_footer_slots[i].dot);
    lv_obj_set_pos(s_footer_slots[i].dot, x, 10);
    lv_obj_set_size(s_footer_slots[i].dot, 10, 10);
    lv_obj_set_style_radius(s_footer_slots[i].dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_footer_slots[i].dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_footer_slots[i].dot, 1, 0);
    showduino_theme_register(s_footer_slots[i].dot, SHOWDUINO_THEME_ROLE_INDICATOR);

    s_footer_slots[i].label = lv_label_create(s_footer);
    char buf[28];
    snprintf(buf, sizeof(buf), "%s —", titles[i]);
    lv_label_set_text(s_footer_slots[i].label, buf);
    lv_obj_set_pos(s_footer_slots[i].label, (int16_t)(x + 14), 8);
    lv_obj_set_width(s_footer_slots[i].label, (int16_t)(kFooterSlotW - 18));
    lv_label_set_long_mode(s_footer_slots[i].label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(s_footer_slots[i].label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_footer_slots[i].label,
                                lv_color_hex(ShowduinoPalette::Muted), 0);
  }

  s_notify = lv_label_create(s_footer);
  lv_label_set_text(s_notify, "—");
  lv_obj_set_pos(s_notify, 640, 28);
  lv_obj_set_width(s_notify, 100);
  lv_label_set_long_mode(s_notify, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_notify, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_notify, lv_color_hex(ShowduinoPalette::Muted), 0);
}

void page_01_home_create(lv_obj_t *parent, page01_command_fn command_cb) {
  if (parent == nullptr) {
    Serial.println("[Page01] create failed — parent null");
    return;
  }
  if (s_active) {
    page_01_home_destroy();
  }

  showduino_theme_init();
  s_command_cb = command_cb;
  s_caps = showduino_capabilities_defaults();
  s_root = parent;
  s_has_production = false;
  s_production_name[0] = '\0';
  strncpy(s_link_text, "OFFLINE", sizeof(s_link_text) - 1);
  strncpy(s_readiness_text, "NO PRODUCTION", sizeof(s_readiness_text) - 1);

  Serial.println("[Page01] creating Home page (status-bar layout)…");
#if SHOWDUINO_PAGE01_ALIGNMENT_DEBUG
  Serial.println("[Page01] ALIGNMENT DEBUG ENABLED");
  Serial.printf("[Page01][Align] header=(%d,%d %dx%d) hero=(%d,%d %dx%d) footer=(%d,%d %dx%d)\n",
                (int)kHeaderX, (int)kHeaderY, (int)kHeaderW, (int)kHeaderH,
                (int)kHeroX, (int)kHeroY, (int)kHeroW, (int)kHeroH,
                (int)kFooterX, (int)kFooterY, (int)kFooterW, (int)kFooterH);
#endif

  build_header(parent);
  build_controls();
  build_footer(parent);
  page_01_home_set_capabilities(&s_caps);
  recompute_readiness();
  refresh_hero();
  page_01_home_apply_theme();

  s_active = true;
  Serial.println("[Page01] Home page ready");
}

void page_01_home_destroy(void) {
  if (!s_active && s_root == nullptr) {
    return;
  }
  Serial.println("[Page01] destroying Home page");

  showduino_theme_clear_registry();

  if (s_root != nullptr) {
    lv_obj_clean(s_root);
  }

  s_root = nullptr;
  s_header = nullptr;
  s_title = nullptr;
  s_production = nullptr;
  s_link = nullptr;
  s_readiness = nullptr;
  s_clock = nullptr;
  s_header_accent = nullptr;
  s_footer = nullptr;
  s_notify = nullptr;
  s_command_cb = nullptr;
  memset(s_ctrls, 0, sizeof(s_ctrls));
  memset(s_footer_slots, 0, sizeof(s_footer_slots));
  s_has_production = false;
  s_production_name[0] = '\0';
  s_active = false;
}

bool page_01_home_is_active(void) {
  return s_active;
}

void page_01_home_set_capabilities(const ShowduinoCapabilities *caps) {
  if (caps != nullptr) {
    s_caps = *caps;
  }
  set_tile_enabled(&s_ctrls[PAGE01_CTRL_CUE_LIBRARY], false);
  set_tile_enabled(&s_ctrls[PAGE01_CTRL_OUTPUTS], false);
  apply_footer_visibility();
  refresh_hero();
  Serial.printf("[Page01] caps relay=%d mosfet=%d neo=%d audio=%d dmx=%d\n",
                (int)s_caps.relay, (int)s_caps.mosfet, (int)s_caps.neopixel,
                (int)s_caps.audio, (int)s_caps.dmx);
}

ShowduinoCapabilities page_01_home_get_capabilities(void) {
  return s_caps;
}

void page_01_home_set_production(const char *name) {
  if (name == nullptr || s_production == nullptr) {
    return;
  }

  const bool empty = production_name_is_empty(name);
  char prev_name[64];
  strncpy(prev_name, s_production_name, sizeof(prev_name) - 1);
  prev_name[sizeof(prev_name) - 1] = '\0';
  const bool prev = s_has_production;

  if (empty) {
    s_has_production = false;
    s_production_name[0] = '\0';
    lv_label_set_text(s_production, "NO PRODUCTION");
    lv_obj_set_style_text_color(s_production, lv_color_hex(ShowduinoPalette::Muted), 0);
  } else {
    s_has_production = true;
    strncpy(s_production_name, name, sizeof(s_production_name) - 1);
    s_production_name[sizeof(s_production_name) - 1] = '\0';
    lv_label_set_text(s_production, s_production_name);
    lv_obj_set_style_text_color(s_production, lv_color_hex(ShowduinoPalette::Text), 0);
  }

  if (prev != s_has_production || strcmp(prev_name, s_production_name) != 0) {
    Serial.printf("[Page01] production → %s\n", empty ? "(none)" : s_production_name);
  }
  recompute_readiness();
  refresh_hero();
}

void page_01_home_set_link_text(const char *text) {
  if (text == nullptr) {
    return;
  }
  if (strcmp(s_link_text, text) != 0) {
    strncpy(s_link_text, text, sizeof(s_link_text) - 1);
    s_link_text[sizeof(s_link_text) - 1] = '\0';
    Serial.printf("[Page01] link → %s\n", s_link_text);
  }
  set_label_safe(s_link, text);
  recompute_readiness();
  refresh_hero();
}

void page_01_home_set_clock_text(const char *text) {
  set_label_safe(s_clock, text);
}

static void set_footer_slot(uint8_t index, const char *text) {
  if (index >= 7 || s_footer_slots[index].label == nullptr || text == nullptr) {
    return;
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%s %s", s_footer_slots[index].title, text);
  lv_label_set_text(s_footer_slots[index].label, buf);
}

void page_01_home_set_footer_sue(const char *text) { set_footer_slot(0, text); }
void page_01_home_set_footer_p4(const char *text) { set_footer_slot(1, text); }
void page_01_home_set_footer_relay(const char *text) { set_footer_slot(2, text); }
void page_01_home_set_footer_mosfet(const char *text) { set_footer_slot(3, text); }
void page_01_home_set_footer_neopixel(const char *text) { set_footer_slot(4, text); }
void page_01_home_set_footer_audio(const char *text) { set_footer_slot(5, text); }
void page_01_home_set_footer_dmx(const char *text) { set_footer_slot(6, text); }

void page_01_home_set_footer_notify(const char *text) {
  if (text == nullptr || s_notify == nullptr) {
    return;
  }
  lv_label_set_text(s_notify, text);
}

void page_01_home_apply_theme(void) {
  showduino_theme_apply();
#if !SHOWDUINO_PAGE01_ALIGNMENT_DEBUG
  const lv_color_t accent = showduino_theme_get_accent();
  for (uint8_t i = 0; i < PAGE01_CTRL_COUNT; i++) {
    if (s_ctrls[i].btn == nullptr) {
      continue;
    }
    lv_color_t border = accent;
    if (s_ctrls[i].role == PAGE01_ROLE_QUIET) {
      border = lv_color_hex(ShowduinoPalette::AccentDark);
    }
    lv_obj_set_style_border_color(s_ctrls[i].btn, border, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(s_ctrls[i].btn, accent, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(s_ctrls[i].btn, accent, LV_PART_MAIN | LV_STATE_FOCUSED);
  }
#endif
}
