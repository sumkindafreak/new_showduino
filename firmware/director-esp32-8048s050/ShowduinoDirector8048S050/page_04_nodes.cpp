#include "page_04_nodes.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "showduino_theme.h"
#include "ShowduinoOsPalette.h"
#include "ShowduinoOsUi.h"
#include "DisplayTypes.h"

static const int16_t kHeaderY = (int16_t)OS_TITLE_Y;
static const int16_t kHeaderH = OS_TITLE_H;
static const int16_t kGridX = 20;
static const int16_t kGridY = (int16_t)(kHeaderY + kHeaderH + OS_GAP);
static const int16_t kCardW = 248;
static const int16_t kCardH = 140;
static const int16_t kCardGap = 8;
static const int16_t kSheetW = 760;
static const int16_t kSheetH = 360;
static const uint8_t kActionMax = 4;

struct Page04Card {
  lv_obj_t *panel;
  lv_obj_t *accent;
  lv_obj_t *title;
  lv_obj_t *dot;
  lv_obj_t *status;
  lv_obj_t *detail;
  lv_obj_t *open_hint;
  const char *name;
  const char *idle_detail;
  bool present;
  uint32_t status_color;
  char status_text[28];
  char detail_text[96];
};

struct Page04Action {
  lv_obj_t *btn;
  lv_obj_t *lab;
};

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_header = nullptr;
static lv_obj_t *s_btn_back = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_summary = nullptr;
static lv_obj_t *s_header_accent = nullptr;
static lv_obj_t *s_sheet = nullptr;
static lv_obj_t *s_sheet_title = nullptr;
static lv_obj_t *s_sheet_dot = nullptr;
static lv_obj_t *s_sheet_status = nullptr;
static lv_obj_t *s_sheet_detail = nullptr;
static lv_obj_t *s_sheet_hint = nullptr;
static lv_obj_t *s_sheet_avail = nullptr;
static lv_obj_t *s_btn_close = nullptr;
static Page04Action s_actions[kActionMax];
static Page04Card s_cards[PAGE04_ROLE_COUNT];
static page04_command_fn s_command_cb = nullptr;
static bool s_active = false;
static bool s_emergency = false;
static int s_open = -1;
static lv_obj_t *s_lamp_id = nullptr;
static lv_obj_t *s_lamp_presence = nullptr;
static lv_obj_t *s_lamp_banner = nullptr;
static lv_obj_t *s_lamp_live_k[4] = {};
static lv_obj_t *s_lamp_live_v[4] = {};
static lv_obj_t *s_lamp_health_k[5] = {};
static lv_obj_t *s_lamp_health_v[5] = {};
static ShowduinoLampDirectorSheet s_lamp_sheet;

static void emit(const char *cmd) {
  if (s_command_cb != nullptr && cmd != nullptr) {
    s_command_cb(cmd);
  }
}

static void style_back_button(lv_obj_t *btn) {
  lv_obj_remove_style_all(btn);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  showduino_theme_register(btn, SHOWDUINO_THEME_ROLE_BORDER);
}

static void style_card(lv_obj_t *panel, bool present) {
  lv_obj_set_style_bg_opa(panel, present ? LV_OPA_COVER : LV_OPA_70, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(panel, present ? 2 : 1, 0);
  lv_obj_set_style_border_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(panel, OS_PANEL_RADIUS, 0);
  const lv_color_t accent = showduino_theme_get_accent();
  const lv_color_t idle = lv_color_hex(ShowduinoPalette::AccentDark);
  lv_obj_set_style_border_color(panel, present ? accent : idle, 0);
}

static void close_sheet(void);
static void open_sheet(Page04Role role);
static void fill_sheet(void);

static void back_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (s_open >= 0) {
    Serial.println("[Page04] Close node settings");
    close_sheet();
    return;
  }
  Serial.println("[Page04] Back -> Home");
  emit(PAGE04_CMD_BACK);
}

static lv_obj_t *make_tick(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h) {
  lv_obj_t *t = lv_obj_create(parent);
  lv_obj_remove_style_all(t);
  lv_obj_set_pos(t, x, y);
  lv_obj_set_size(t, w, h);
  lv_obj_set_style_bg_color(t, lv_color_hex(ShowduinoPalette::Accent), 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_70, 0);
  lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  return t;
}

static void build_header(lv_obj_t *parent) {
  s_header = lv_obj_create(parent);
  lv_obj_remove_style_all(s_header);
  lv_obj_set_pos(s_header, 0, kHeaderY);
  lv_obj_set_size(s_header, DISPLAY_WIDTH, kHeaderH);
  lv_obj_set_style_bg_opa(s_header, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

  s_header_accent = lv_obj_create(s_header);
  lv_obj_remove_style_all(s_header_accent);
  lv_obj_set_pos(s_header_accent, 110, 34);
  lv_obj_set_size(s_header_accent, 120, 3);
  lv_obj_set_style_bg_opa(s_header_accent, LV_OPA_COVER, 0);
  showduino_theme_register(s_header_accent, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);

  s_btn_back = lv_button_create(s_header);
  style_back_button(s_btn_back);
  lv_obj_set_pos(s_btn_back, 12, 4);
  lv_obj_set_size(s_btn_back, 88, 40);
  lv_obj_add_event_cb(s_btn_back, back_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *bl = lv_label_create(s_btn_back);
  lv_label_set_text(bl, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_color(bl, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(bl);

  s_title = lv_label_create(s_header);
  lv_label_set_text(s_title, "NODES");
  lv_obj_set_pos(s_title, 110, 10);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_title, lv_color_hex(ShowduinoPalette::Text), 0);
  showduino_theme_register(s_title, SHOWDUINO_THEME_ROLE_TEXT);

  s_summary = lv_label_create(s_header);
  lv_label_set_text(s_summary, "FABRIC  |  0 ONLINE  |  5 SPECIALISTS");
  lv_obj_set_pos(s_summary, 280, 12);
  lv_obj_set_width(s_summary, 500);
  lv_label_set_long_mode(s_summary, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_summary, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_summary, lv_color_hex(ShowduinoPalette::Accent), 0);
}

static void card_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const Page04Role role = (Page04Role)(intptr_t)lv_event_get_user_data(e);
  if (role < 0 || role >= PAGE04_ROLE_COUNT) return;
  open_sheet(role);
}

static void build_card(Page04Role role, int16_t col, int16_t row) {
  Page04Card *c = &s_cards[role];
  const int16_t x = (int16_t)(kGridX + col * (kCardW + kCardGap));
  const int16_t y = (int16_t)(kGridY + row * (kCardH + kCardGap));

  c->panel = lv_obj_create(s_root);
  lv_obj_remove_style_all(c->panel);
  lv_obj_set_pos(c->panel, x, y);
  lv_obj_set_size(c->panel, kCardW, kCardH);
  lv_obj_clear_flag(c->panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(c->panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c->panel, card_event, LV_EVENT_CLICKED, (void *)(intptr_t)role);
  lv_obj_set_style_bg_color(c->panel, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
  style_card(c->panel, false);
  showduino_theme_register(c->panel, SHOWDUINO_THEME_ROLE_BORDER);

  c->accent = lv_obj_create(c->panel);
  lv_obj_remove_style_all(c->accent);
  lv_obj_set_pos(c->accent, 10, 8);
  lv_obj_set_size(c->accent, 36, 3);
  lv_obj_set_style_bg_color(c->accent, lv_color_hex(ShowduinoPalette::Accent), 0);
  lv_obj_set_style_bg_opa(c->accent, LV_OPA_COVER, 0);
  showduino_theme_register(c->accent, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);

  make_tick(c->panel, 0, 0, 14, 2);
  make_tick(c->panel, 0, 0, 2, 14);
  make_tick(c->panel, kCardW - 14, 0, 14, 2);
  make_tick(c->panel, kCardW - 2, 0, 2, 14);

  c->title = lv_label_create(c->panel);
  lv_label_set_text(c->title, c->name);
  lv_obj_set_pos(c->title, 12, 18);
  lv_obj_set_width(c->title, kCardW - 24);
  lv_obj_set_style_text_font(c->title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->title, lv_color_hex(ShowduinoPalette::Text), 0);

  c->dot = lv_obj_create(c->panel);
  lv_obj_remove_style_all(c->dot);
  lv_obj_set_pos(c->dot, 14, 48);
  lv_obj_set_size(c->dot, 10, 10);
  lv_obj_set_style_radius(c->dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(c->dot, lv_color_hex(ShowduinoPalette::Disabled), 0);
  lv_obj_set_style_bg_opa(c->dot, LV_OPA_COVER, 0);

  c->status = lv_label_create(c->panel);
  lv_label_set_text(c->status, "NOT DETECTED");
  lv_obj_set_pos(c->status, 32, 44);
  lv_obj_set_width(c->status, kCardW - 44);
  lv_label_set_long_mode(c->status, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(c->status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->status, lv_color_hex(ShowduinoPalette::Muted), 0);

  c->detail = lv_label_create(c->panel);
  lv_label_set_text(c->detail, c->idle_detail);
  lv_obj_set_pos(c->detail, 12, 68);
  lv_obj_set_width(c->detail, kCardW - 24);
  lv_label_set_long_mode(c->detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(c->detail, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->detail, lv_color_hex(ShowduinoPalette::Muted), 0);

  c->open_hint = lv_label_create(c->panel);
  lv_label_set_text(c->open_hint, "SETTINGS  >");
  lv_obj_set_pos(c->open_hint, 12, 118);
  lv_obj_set_style_text_font(c->open_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->open_hint, lv_color_hex(ShowduinoPalette::Accent), 0);
}

static void apply_card(Page04Role role, bool present, const char *status,
                       const char *detail, uint32_t status_color) {
  Page04Card *c = &s_cards[role];
  if (!c->panel) return;
  c->present = present;
  c->status_color = status_color;
  strncpy(c->status_text, status && status[0] ? status : "NOT DETECTED",
          sizeof(c->status_text) - 1);
  c->status_text[sizeof(c->status_text) - 1] = 0;
  strncpy(c->detail_text, (detail && detail[0]) ? detail : c->idle_detail,
          sizeof(c->detail_text) - 1);
  c->detail_text[sizeof(c->detail_text) - 1] = 0;

  style_card(c->panel, present);
  if (c->dot) {
    lv_obj_set_style_bg_color(c->dot, lv_color_hex(status_color), 0);
  }
  if (c->status) {
    lv_label_set_text(c->status, c->status_text);
    lv_obj_set_style_text_color(c->status, lv_color_hex(status_color), 0);
  }
  if (c->detail) {
    lv_label_set_text(c->detail, c->detail_text);
    lv_obj_set_style_text_color(c->detail,
        lv_color_hex(present ? ShowduinoPalette::Text : ShowduinoPalette::Muted), 0);
  }
  if (c->accent) {
    lv_obj_set_style_bg_opa(c->accent, present ? LV_OPA_COVER : LV_OPA_40, 0);
  }
  if (s_open == (int)role) fill_sheet();
}

static void set_action(uint8_t i, const char *label, const char *cmd, bool enable, bool danger) {
  if (i >= kActionMax || !s_actions[i].btn) return;
  lv_obj_clear_flag(s_actions[i].btn, LV_OBJ_FLAG_HIDDEN);
  if (s_actions[i].lab && label) lv_label_set_text(s_actions[i].lab, label);
  lv_obj_set_user_data(s_actions[i].btn, (void *)cmd);
  lv_obj_set_style_bg_color(s_actions[i].btn,
      lv_color_hex(danger ? ShowduinoPalette::DangerPanel : ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_color(s_actions[i].btn,
      lv_color_hex(danger ? ShowduinoPalette::Danger : ShowduinoPalette::AccentDark), 0);
  ShowduinoOsTheme::setEnabled(s_actions[i].btn, enable);
}

static void hide_actions(void) {
  for (uint8_t i = 0; i < kActionMax; i++) {
    if (s_actions[i].btn) lv_obj_add_flag(s_actions[i].btn, LV_OBJ_FLAG_HIDDEN);
  }
}

static void set_hidden(lv_obj_t *obj, bool hide) {
  if (!obj) return;
  if (hide) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void place_actions(bool lamp_layout) {
  for (uint8_t i = 0; i < kActionMax; i++) {
    if (!s_actions[i].btn) continue;
    if (lamp_layout) {
      lv_obj_set_pos(s_actions[i].btn, 16 + (int16_t)i * 184, 244);
      lv_obj_set_size(s_actions[i].btn, 176, 56);
    } else {
      lv_obj_set_pos(s_actions[i].btn, 16 + (int16_t)i * 184, 248);
      lv_obj_set_size(s_actions[i].btn, 176, 48);
    }
  }
}

static void set_lamp_body_visible(bool show) {
  set_hidden(s_lamp_id, !show);
  set_hidden(s_lamp_presence, !show);
  set_hidden(s_lamp_banner, !show);
  for (int i = 0; i < 4; i++) {
    set_hidden(s_lamp_live_k[i], !show);
    set_hidden(s_lamp_live_v[i], !show);
  }
  for (int i = 0; i < 5; i++) {
    set_hidden(s_lamp_health_k[i], !show);
    set_hidden(s_lamp_health_v[i], !show);
  }
}

static uint32_t lamp_state_color(const char *state, uint32_t fallback) {
  if (!state) return fallback;
  if (strcmp(state, "EMERGENCY") == 0 || strcmp(state, "FAULT") == 0) {
    return ShowduinoPalette::Danger;
  }
  if (strcmp(state, "OFFLINE") == 0 || strcmp(state, "--") == 0) {
    return ShowduinoPalette::Muted;
  }
  if (strcmp(state, "BURNING") == 0 || strcmp(state, "FLARE") == 0) {
    return ShowduinoPalette::AccentBright;
  }
  if (strcmp(state, "STRIKING") == 0 || strcmp(state, "IGNITING") == 0 ||
      strcmp(state, "LOW_FLAME") == 0 || strcmp(state, "UNSTABLE") == 0 ||
      strcmp(state, "EXTINGUISHING") == 0) {
    return ShowduinoPalette::Warn;
  }
  return fallback;
}

static void apply_lamp_sheet_widgets(void) {
  const ShowduinoLampDirectorSheet *m = &s_lamp_sheet;
  if (s_sheet_title) lv_label_set_text(s_sheet_title, m->title[0] ? m->title : "LAMP NODE");
  if (s_lamp_id) lv_label_set_text(s_lamp_id, m->logical_id[0] ? m->logical_id : "LAMP");
  if (s_lamp_presence) {
    lv_label_set_text(s_lamp_presence,
                      m->presence_line[0] ? m->presence_line : "OFFLINE");
    lv_obj_set_style_text_color(s_lamp_presence,
        lv_color_hex(lamp_state_color(m->presence_line, ShowduinoPalette::Muted)), 0);
  }
  if (s_sheet_dot) {
    lv_obj_set_style_bg_color(s_sheet_dot,
        lv_color_hex(lamp_state_color(m->presence_line, ShowduinoPalette::Disabled)), 0);
  }

  const char *live_k[] = { "STATE", "MOTION", "BLOW", "AUDIO" };
  const char *live_v[] = { m->state, m->motion, m->blow, m->audio };
  for (int i = 0; i < 4; i++) {
    if (s_lamp_live_k[i]) lv_label_set_text(s_lamp_live_k[i], live_k[i]);
    if (s_lamp_live_v[i]) {
      lv_label_set_text(s_lamp_live_v[i], live_v[i] && live_v[i][0] ? live_v[i] : "--");
      lv_obj_set_style_text_color(s_lamp_live_v[i],
          lv_color_hex(i == 0 ? lamp_state_color(m->state, ShowduinoPalette::Text)
                              : ShowduinoPalette::Muted), 0);
    }
  }

  const char *hk[] = { "JEWEL", "AUDIO", "MOTION", "MIC", "VOLTAGE" };
  const char *hv[] = { m->jewel_health, m->audio_health, m->motion_health,
                       m->mic_health, m->voltage_health };
  for (int i = 0; i < 5; i++) {
    if (s_lamp_health_k[i]) lv_label_set_text(s_lamp_health_k[i], hk[i]);
    if (s_lamp_health_v[i]) {
      lv_label_set_text(s_lamp_health_v[i], hv[i] && hv[i][0] ? hv[i] : "--");
      lv_obj_set_style_text_color(s_lamp_health_v[i],
          lv_color_hex(ShowduinoPalette::Muted), 0);
    }
  }

  if (s_lamp_banner) {
    lv_label_set_text(s_lamp_banner, m->banner);
    lv_obj_set_style_text_color(s_lamp_banner,
        lv_color_hex(m->emergency ? ShowduinoPalette::Danger
                     : (m->online ? ShowduinoPalette::Muted : ShowduinoPalette::Warn)), 0);
  }
}

static void fill_sheet(void) {
  if (!s_sheet || s_open < 0 || s_open >= PAGE04_ROLE_COUNT) return;
  const Page04Card *c = &s_cards[s_open];
  const bool live = c->present && !s_emergency;
  const bool lamp = (s_open == (int)PAGE04_ROLE_LAMP);

  if (s_title) lv_label_set_text(s_title, c->name);
  if (s_sheet_title) lv_label_set_text(s_sheet_title, c->name);
  if (s_sheet_dot) {
    lv_obj_set_style_bg_color(s_sheet_dot, lv_color_hex(c->status_color), 0);
  }
  if (s_sheet_status) {
    lv_label_set_text(s_sheet_status, c->status_text[0] ? c->status_text : "NOT DETECTED");
    lv_obj_set_style_text_color(s_sheet_status, lv_color_hex(c->status_color), 0);
  }
  if (s_sheet_detail) {
    lv_label_set_text(s_sheet_detail, c->detail_text[0] ? c->detail_text : c->idle_detail);
  }

  const char *hint = "Requests go to the P4. This desk does not drive node hardware.";
  const char *avail = "";
  hide_actions();
  place_actions(lamp);
  set_lamp_body_visible(lamp);
  set_hidden(s_sheet_status, lamp);
  set_hidden(s_sheet_detail, lamp);
  set_hidden(s_sheet_hint, lamp);
  set_hidden(s_sheet_avail, lamp);
  if (s_sheet_dot) {
    if (lamp) lv_obj_set_pos(s_sheet_dot, 16, 70);
    else lv_obj_set_pos(s_sheet_dot, 18, 60);
  }

  switch ((Page04Role)s_open) {
    case PAGE04_ROLE_AUDIO:
      hint = "Programme audio on the Audio Node. P4 stays authoritative.";
      avail = c->present ? "" : "No compatible Audio Node detected.";
      set_action(0, "AUDIO DESK", PAGE04_CMD_AUDIO, true, false);
      set_action(1, "TEST", PAGE04_CMD_AUDIO_TEST, live, false);
      set_action(2, "STOP", PAGE04_CMD_AUDIO_STOP, live, false);
      set_action(3, "REFRESH", PAGE04_CMD_STATUS, true, false);
      break;
    case PAGE04_ROLE_LAMP:
      apply_lamp_sheet_widgets();
      set_action(0, "IGNITE", PAGE04_CMD_LAMP_IGNITE,
                 s_lamp_sheet.ignite_enabled != 0, false);
      set_action(1, "EXTINGUISH", PAGE04_CMD_LAMP_EXTINGUISH,
                 s_lamp_sheet.extinguish_enabled != 0, true);
      set_action(2, "FLARE", PAGE04_CMD_LAMP_FLARE,
                 s_lamp_sheet.flare_enabled != 0, false);
      set_action(3, "REFRESH", PAGE04_CMD_LAMP_STATUS, true, false);
      break;
    case PAGE04_ROLE_NEOPIXEL:
      hint = "Remote Show Pixel Line. P4 owns FX; this page is status only.";
      avail = c->present ? "" : "No compatible Pixel Node detected.";
      set_action(0, "REFRESH", PAGE04_CMD_STATUS, true, false);
      break;
    case PAGE04_ROLE_MOSFET:
      hint = "PWM / dimming specialist. Role reserved.";
      avail = "No compatible MOSFET Node detected.";
      break;
    case PAGE04_ROLE_DMX:
      hint = "Dedicated DMX universe. Role reserved.";
      avail = "No compatible DMX Node detected.";
      break;
    case PAGE04_ROLE_STAGE:
      hint = "Director -> Comms S3 -> P4. Stage is the show engine.";
      avail = "";
      set_action(0, "REFRESH", PAGE04_CMD_STATUS, true, false);
      break;
    default:
      break;
  }
  if (!lamp && s_emergency) {
    avail = "EMERGENCY - theatrical node controls locked.";
  }
  if (s_sheet_hint) lv_label_set_text(s_sheet_hint, hint);
  if (s_sheet_avail) {
    lv_label_set_text(s_sheet_avail, avail);
    lv_obj_set_style_text_color(s_sheet_avail,
        lv_color_hex(avail[0] ? ShowduinoPalette::Warn : ShowduinoPalette::Muted), 0);
  }
}

static void set_grid_visible(bool show) {
  for (int i = 0; i < PAGE04_ROLE_COUNT; i++) {
    if (!s_cards[i].panel) continue;
    if (show) lv_obj_clear_flag(s_cards[i].panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_cards[i].panel, LV_OBJ_FLAG_HIDDEN);
  }
}

static void open_sheet(Page04Role role) {
  s_open = (int)role;
  set_grid_visible(false);
  if (s_sheet) lv_obj_clear_flag(s_sheet, LV_OBJ_FLAG_HIDDEN);
  fill_sheet();
  Serial.printf("[Page04] Open settings %s\n", s_cards[role].name);
}

static void close_sheet(void) {
  s_open = -1;
  if (s_sheet) lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_HIDDEN);
  if (s_title) lv_label_set_text(s_title, "NODES");
  set_grid_visible(true);
}

static void action_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *cmd = (const char *)lv_obj_get_user_data(lv_event_get_target_obj(e));
  if (cmd) emit(cmd);
}

static void build_sheet(lv_obj_t *parent) {
  s_sheet = lv_obj_create(parent);
  lv_obj_remove_style_all(s_sheet);
  lv_obj_set_pos(s_sheet, kGridX, kGridY);
  lv_obj_set_size(s_sheet, kSheetW, kSheetH);
  lv_obj_set_style_bg_opa(s_sheet, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_sheet, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(s_sheet, 2, 0);
  lv_obj_set_style_border_opa(s_sheet, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_sheet, OS_PANEL_RADIUS, 0);
  lv_obj_clear_flag(s_sheet, LV_OBJ_FLAG_SCROLLABLE);
  showduino_theme_register(s_sheet, SHOWDUINO_THEME_ROLE_BORDER);
  make_tick(s_sheet, 0, 0, 16, 2);
  make_tick(s_sheet, 0, 0, 2, 16);
  make_tick(s_sheet, kSheetW - 16, 0, 16, 2);
  make_tick(s_sheet, kSheetW - 2, 0, 2, 16);

  lv_obj_t *accent = lv_obj_create(s_sheet);
  lv_obj_remove_style_all(accent);
  lv_obj_set_pos(accent, 16, 10);
  lv_obj_set_size(accent, 48, 3);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
  showduino_theme_register(accent, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);

  s_sheet_title = lv_label_create(s_sheet);
  lv_label_set_text(s_sheet_title, "NODE");
  lv_obj_set_pos(s_sheet_title, 16, 20);
  lv_obj_set_style_text_font(s_sheet_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_sheet_title, lv_color_hex(ShowduinoPalette::Text), 0);

  s_btn_close = lv_button_create(s_sheet);
  style_back_button(s_btn_close);
  lv_obj_set_pos(s_btn_close, kSheetW - 100, 12);
  lv_obj_set_size(s_btn_close, 84, 40);
  lv_obj_add_event_cb(s_btn_close, [](lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    close_sheet();
  }, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cl = lv_label_create(s_btn_close);
  lv_label_set_text(cl, "CLOSE");
  lv_obj_set_style_text_color(cl, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(cl);

  s_sheet_dot = lv_obj_create(s_sheet);
  lv_obj_remove_style_all(s_sheet_dot);
  lv_obj_set_pos(s_sheet_dot, 18, 60);
  lv_obj_set_size(s_sheet_dot, 12, 12);
  lv_obj_set_style_radius(s_sheet_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_sheet_dot, LV_OPA_COVER, 0);

  s_sheet_status = lv_label_create(s_sheet);
  lv_label_set_text(s_sheet_status, "NOT DETECTED");
  lv_obj_set_pos(s_sheet_status, 38, 56);
  lv_obj_set_width(s_sheet_status, 600);
  lv_obj_set_style_text_font(s_sheet_status, &lv_font_montserrat_16, 0);

  s_sheet_detail = lv_label_create(s_sheet);
  lv_label_set_text(s_sheet_detail, "");
  lv_obj_set_pos(s_sheet_detail, 16, 84);
  lv_obj_set_width(s_sheet_detail, kSheetW - 32);
  lv_label_set_long_mode(s_sheet_detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_sheet_detail, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_sheet_detail, lv_color_hex(ShowduinoPalette::Text), 0);

  s_sheet_hint = lv_label_create(s_sheet);
  lv_label_set_text(s_sheet_hint, "");
  lv_obj_set_pos(s_sheet_hint, 16, 160);
  lv_obj_set_width(s_sheet_hint, kSheetW - 32);
  lv_label_set_long_mode(s_sheet_hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_sheet_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_sheet_hint, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_sheet_avail = lv_label_create(s_sheet);
  lv_label_set_text(s_sheet_avail, "");
  lv_obj_set_pos(s_sheet_avail, 16, 196);
  lv_obj_set_width(s_sheet_avail, kSheetW - 32);
  lv_obj_set_style_text_font(s_sheet_avail, &lv_font_montserrat_14, 0);

  s_lamp_id = lv_label_create(s_sheet);
  lv_label_set_text(s_lamp_id, "LAMP-01");
  lv_obj_set_pos(s_lamp_id, 16, 42);
  lv_obj_set_width(s_lamp_id, 400);
  lv_obj_set_style_text_font(s_lamp_id, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_lamp_id, lv_color_hex(ShowduinoPalette::Text), 0);

  s_lamp_presence = lv_label_create(s_sheet);
  lv_label_set_text(s_lamp_presence, "OFFLINE");
  lv_obj_set_pos(s_lamp_presence, 36, 66);
  lv_obj_set_width(s_lamp_presence, 500);
  lv_obj_set_style_text_font(s_lamp_presence, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_lamp_presence, lv_color_hex(ShowduinoPalette::Muted), 0);

  {
    const char *lk[] = { "STATE", "MOTION", "BLOW", "AUDIO" };
    for (int i = 0; i < 4; i++) {
      const int16_t y = (int16_t)(92 + i * 20);
      s_lamp_live_k[i] = lv_label_create(s_sheet);
      lv_label_set_text(s_lamp_live_k[i], lk[i]);
      lv_obj_set_pos(s_lamp_live_k[i], 16, y);
      lv_obj_set_width(s_lamp_live_k[i], 100);
      lv_obj_set_style_text_font(s_lamp_live_k[i], &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(s_lamp_live_k[i], lv_color_hex(ShowduinoPalette::Muted), 0);

      s_lamp_live_v[i] = lv_label_create(s_sheet);
      lv_label_set_text(s_lamp_live_v[i], "--");
      lv_obj_set_pos(s_lamp_live_v[i], 120, y);
      lv_obj_set_width(s_lamp_live_v[i], 200);
      lv_obj_set_style_text_font(s_lamp_live_v[i], &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(s_lamp_live_v[i], lv_color_hex(ShowduinoPalette::Muted), 0);
    }
    const char *hk[] = { "JEWEL", "AUDIO", "MOTION", "MIC", "VOLTAGE" };
    for (int i = 0; i < 5; i++) {
      const int16_t y = (int16_t)(92 + i * 20);
      s_lamp_health_k[i] = lv_label_create(s_sheet);
      lv_label_set_text(s_lamp_health_k[i], hk[i]);
      lv_obj_set_pos(s_lamp_health_k[i], 390, y);
      lv_obj_set_width(s_lamp_health_k[i], 110);
      lv_obj_set_style_text_font(s_lamp_health_k[i], &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(s_lamp_health_k[i], lv_color_hex(ShowduinoPalette::Muted), 0);

      s_lamp_health_v[i] = lv_label_create(s_sheet);
      lv_label_set_text(s_lamp_health_v[i], "--");
      lv_obj_set_pos(s_lamp_health_v[i], 508, y);
      lv_obj_set_width(s_lamp_health_v[i], 220);
      lv_obj_set_style_text_font(s_lamp_health_v[i], &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(s_lamp_health_v[i], lv_color_hex(ShowduinoPalette::Muted), 0);
    }
  }

  s_lamp_banner = lv_label_create(s_sheet);
  lv_label_set_text(s_lamp_banner, "");
  lv_obj_set_pos(s_lamp_banner, 16, 188);
  lv_obj_set_width(s_lamp_banner, kSheetW - 32);
  lv_label_set_long_mode(s_lamp_banner, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_lamp_banner, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_lamp_banner, lv_color_hex(ShowduinoPalette::Muted), 0);

  set_lamp_body_visible(false);

  for (uint8_t i = 0; i < kActionMax; i++) {
    lv_obj_t *btn = lv_button_create(s_sheet);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, 16 + (int16_t)i * 184, 248);
    lv_obj_set_size(btn, 176, 48);
    lv_obj_set_style_radius(btn, OS_BTN_RADIUS, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(ShowduinoPalette::AccentDark), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
    lv_obj_set_style_opa(btn, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn, action_event, LV_EVENT_CLICKED, nullptr);
    showduino_theme_register(btn, SHOWDUINO_THEME_ROLE_BORDER);
    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, "");
    lv_obj_set_style_text_color(lab, lv_color_hex(ShowduinoPalette::Text), 0);
    lv_obj_center(lab);
    s_actions[i].btn = btn;
    s_actions[i].lab = lab;
    lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
  }

  lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_HIDDEN);
}

void page_04_nodes_create(lv_obj_t *parent, page04_command_fn command_cb) {
  if (!parent) {
    Serial.println("[Page04] create failed - parent null");
    return;
  }
  if (s_active) page_04_nodes_destroy();

  showduino_theme_init();
  s_command_cb = command_cb;
  s_root = parent;
  s_open = -1;
  s_emergency = false;

  s_cards[PAGE04_ROLE_AUDIO].name = "AUDIO NODE";
  s_cards[PAGE04_ROLE_AUDIO].idle_detail = "No compatible node detected.\nProgramme WAV stays on this role.";
  s_cards[PAGE04_ROLE_LAMP].name = "LAMP NODE";
  s_cards[PAGE04_ROLE_LAMP].idle_detail = "No compatible node detected.\nCarbide / theatrical lamp FX.";
  s_cards[PAGE04_ROLE_MOSFET].name = "MOSFET NODE";
  s_cards[PAGE04_ROLE_MOSFET].idle_detail = "No compatible node detected.\nPWM / dimming outputs.";
  s_cards[PAGE04_ROLE_NEOPIXEL].name = "PIXEL NODE";
  s_cards[PAGE04_ROLE_NEOPIXEL].idle_detail = "No compatible node detected.\nRemote Show Pixel Line (same model as P4 GPIO23).";
  s_cards[PAGE04_ROLE_DMX].name = "DMX NODE";
  s_cards[PAGE04_ROLE_DMX].idle_detail = "No compatible node detected.\nDedicated universe output.";
  s_cards[PAGE04_ROLE_STAGE].name = "STAGE / COMMS";
  s_cards[PAGE04_ROLE_STAGE].idle_detail = "Director  ->  Comms S3  ->  P4.\nAwaiting Stage link.";

  Serial.println("[Page04] creating Nodes page...");
  build_header(parent);
  build_card(PAGE04_ROLE_AUDIO, 0, 0);
  build_card(PAGE04_ROLE_LAMP, 1, 0);
  build_card(PAGE04_ROLE_MOSFET, 2, 0);
  build_card(PAGE04_ROLE_NEOPIXEL, 0, 1);
  build_card(PAGE04_ROLE_DMX, 1, 1);
  build_card(PAGE04_ROLE_STAGE, 2, 1);
  build_sheet(parent);
  {
    ShowduinoLampDirectorInput lampIn;
    memset(&lampIn, 0, sizeof(lampIn));
    lampIn.offline = 1;
    showduino_lamp_director_build_sheet(&lampIn, &s_lamp_sheet);
  }
  page_04_nodes_apply_theme();

  s_active = true;
  Serial.println("[Page04] Nodes page ready");
}

void page_04_nodes_destroy(void) {
  if (!s_active && !s_root) return;
  Serial.println("[Page04] destroying Nodes page");
  showduino_theme_unregister(s_header_accent);
  showduino_theme_unregister(s_title);
  showduino_theme_unregister(s_btn_back);
  showduino_theme_unregister(s_sheet);
  showduino_theme_unregister(s_btn_close);
  for (int i = 0; i < PAGE04_ROLE_COUNT; i++) {
    showduino_theme_unregister(s_cards[i].panel);
    showduino_theme_unregister(s_cards[i].accent);
  }
  for (uint8_t i = 0; i < kActionMax; i++) {
    showduino_theme_unregister(s_actions[i].btn);
  }
  if (s_root) lv_obj_clean(s_root);

  s_root = nullptr;
  s_header = s_btn_back = s_title = s_summary = s_header_accent = nullptr;
  s_sheet = s_sheet_title = s_sheet_dot = s_sheet_status = nullptr;
  s_sheet_detail = s_sheet_hint = s_sheet_avail = s_btn_close = nullptr;
  s_lamp_id = s_lamp_presence = s_lamp_banner = nullptr;
  memset(s_lamp_live_k, 0, sizeof(s_lamp_live_k));
  memset(s_lamp_live_v, 0, sizeof(s_lamp_live_v));
  memset(s_lamp_health_k, 0, sizeof(s_lamp_health_k));
  memset(s_lamp_health_v, 0, sizeof(s_lamp_health_v));
  memset(s_cards, 0, sizeof(s_cards));
  memset(s_actions, 0, sizeof(s_actions));
  memset(&s_lamp_sheet, 0, sizeof(s_lamp_sheet));
  s_command_cb = nullptr;
  s_open = -1;
  s_emergency = false;
  s_active = false;
}

bool page_04_nodes_is_active(void) { return s_active; }
bool page_04_nodes_sheet_open(void) { return s_open >= 0; }

void page_04_nodes_close_sheet(void) {
  if (s_open >= 0) close_sheet();
}

void page_04_nodes_set_lock(bool emergency) {
  s_emergency = emergency;
  if (s_open >= 0) fill_sheet();
}

void page_04_nodes_apply_theme(void) {
  showduino_theme_apply();
  const lv_color_t accent = showduino_theme_get_accent();
  if (s_btn_back) {
    lv_obj_set_style_border_color(s_btn_back, accent, 0);
    lv_obj_set_style_border_color(s_btn_back, accent, LV_STATE_PRESSED);
  }
  if (s_btn_close) {
    lv_obj_set_style_border_color(s_btn_close, accent, 0);
    lv_obj_set_style_border_color(s_btn_close, accent, LV_STATE_PRESSED);
  }
  if (s_summary) {
    lv_obj_set_style_text_color(s_summary, accent, 0);
  }
  if (s_sheet) {
    lv_obj_set_style_border_color(s_sheet, accent, 0);
  }
  for (int i = 0; i < PAGE04_ROLE_COUNT; i++) {
    if (s_cards[i].open_hint) {
      lv_obj_set_style_text_color(s_cards[i].open_hint, accent, 0);
    }
    if (s_cards[i].panel) style_card(s_cards[i].panel, s_cards[i].present);
  }
}

void page_04_nodes_set_summary(const char *text) {
  if (!s_summary || !text) return;
  lv_label_set_text(s_summary, text);
}

void page_04_nodes_set_card(Page04Role role, bool present,
                            const char *status, const char *detail,
                            uint32_t status_color) {
  if (role < 0 || role >= PAGE04_ROLE_COUNT) return;
  apply_card(role, present, status, detail, status_color);
}

void page_04_nodes_set_lamp_sheet(const ShowduinoLampDirectorSheet *model) {
  if (model) s_lamp_sheet = *model;
  if (s_open == (int)PAGE04_ROLE_LAMP) fill_sheet();
}
