#include "page_04_nodes.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
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

struct Page04Card {
  lv_obj_t *panel;
  lv_obj_t *accent;
  lv_obj_t *tick_tl;
  lv_obj_t *tick_tr;
  lv_obj_t *title;
  lv_obj_t *dot;
  lv_obj_t *status;
  lv_obj_t *detail;
  const char *name;
  const char *idle_detail;
};

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_header = nullptr;
static lv_obj_t *s_btn_back = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_summary = nullptr;
static lv_obj_t *s_header_accent = nullptr;
static Page04Card s_cards[PAGE04_ROLE_COUNT];
static page04_command_fn s_command_cb = nullptr;
static bool s_active = false;

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

static void back_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
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

static void build_card(Page04Role role, int16_t col, int16_t row) {
  Page04Card *c = &s_cards[role];
  const int16_t x = (int16_t)(kGridX + col * (kCardW + kCardGap));
  const int16_t y = (int16_t)(kGridY + row * (kCardH + kCardGap));

  c->panel = lv_obj_create(s_root);
  lv_obj_remove_style_all(c->panel);
  lv_obj_set_pos(c->panel, x, y);
  lv_obj_set_size(c->panel, kCardW, kCardH);
  lv_obj_clear_flag(c->panel, LV_OBJ_FLAG_SCROLLABLE);
  if (role == PAGE04_ROLE_AUDIO) {
    lv_obj_add_flag(c->panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c->panel, [](lv_event_t *e) {
      if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
      emit(PAGE04_CMD_AUDIO);
    }, LV_EVENT_CLICKED, nullptr);
  } else {
    lv_obj_clear_flag(c->panel, LV_OBJ_FLAG_CLICKABLE);
  }
  style_card(c->panel, false);
  showduino_theme_register(c->panel, SHOWDUINO_THEME_ROLE_BORDER);

  c->accent = lv_obj_create(c->panel);
  lv_obj_remove_style_all(c->accent);
  lv_obj_set_pos(c->accent, 10, 8);
  lv_obj_set_size(c->accent, 36, 3);
  lv_obj_set_style_bg_color(c->accent, lv_color_hex(ShowduinoPalette::Accent), 0);
  lv_obj_set_style_bg_opa(c->accent, LV_OPA_COVER, 0);
  showduino_theme_register(c->accent, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);

  c->tick_tl = make_tick(c->panel, 0, 0, 14, 2);
  make_tick(c->panel, 0, 0, 2, 14);
  c->tick_tr = make_tick(c->panel, kCardW - 14, 0, 14, 2);
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
  lv_obj_set_pos(c->detail, 12, 72);
  lv_obj_set_width(c->detail, kCardW - 24);
  lv_label_set_long_mode(c->detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(c->detail, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->detail, lv_color_hex(ShowduinoPalette::Muted), 0);
}

static void apply_card(Page04Role role, bool present, const char *status,
                       const char *detail, uint32_t status_color) {
  Page04Card *c = &s_cards[role];
  if (!c->panel) return;
  style_card(c->panel, present);
  if (c->dot) {
    lv_obj_set_style_bg_color(c->dot, lv_color_hex(status_color), 0);
  }
  if (c->status && status) {
    lv_label_set_text(c->status, status);
    lv_obj_set_style_text_color(c->status, lv_color_hex(status_color), 0);
  }
  if (c->detail) {
    lv_label_set_text(c->detail, (detail && detail[0]) ? detail : c->idle_detail);
    lv_obj_set_style_text_color(c->detail,
        lv_color_hex(present ? ShowduinoPalette::Text : ShowduinoPalette::Muted), 0);
  }
  if (c->accent) {
    lv_obj_set_style_bg_opa(c->accent, present ? LV_OPA_COVER : LV_OPA_40, 0);
  }
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
  for (int i = 0; i < PAGE04_ROLE_COUNT; i++) {
    showduino_theme_unregister(s_cards[i].panel);
    showduino_theme_unregister(s_cards[i].accent);
  }
  if (s_root) lv_obj_clean(s_root);

  s_root = nullptr;
  s_header = s_btn_back = s_title = s_summary = s_header_accent = nullptr;
  memset(s_cards, 0, sizeof(s_cards));
  s_command_cb = nullptr;
  s_active = false;
}

bool page_04_nodes_is_active(void) { return s_active; }

void page_04_nodes_apply_theme(void) {
  showduino_theme_apply();
  const lv_color_t accent = showduino_theme_get_accent();
  if (s_btn_back) {
    lv_obj_set_style_border_color(s_btn_back, accent, 0);
    lv_obj_set_style_border_color(s_btn_back, accent, LV_STATE_PRESSED);
  }
  if (s_summary) {
    lv_obj_set_style_text_color(s_summary, accent, 0);
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
