#include "page_08_settings.h"

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
static const int16_t kCardW = 248;
static const int16_t kCardH = 118;
static const int16_t kCardGap = 8;
static const int16_t kStripH = 44;
static const int16_t kStripY = (int16_t)(kHeaderY + kHeaderH + OS_GAP);
static const int16_t kGridY = (int16_t)(kStripY + kStripH + 8);
static const int16_t kSheetW = 760;
static const int16_t kSheetH = 338;
static const uint8_t kActionMax = 10;

struct Page08Card {
  lv_obj_t *panel;
  lv_obj_t *accent;
  lv_obj_t *title;
  lv_obj_t *dot;
  lv_obj_t *status;
  lv_obj_t *detail;
  lv_obj_t *open_hint;
  const char *name;
  bool present;
  uint32_t status_color;
  char status_text[28];
  char detail_text[96];
  char sheet_body[360];
};

struct Page08Action {
  lv_obj_t *btn;
  lv_obj_t *lab;
};

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_header = nullptr;
static lv_obj_t *s_btn_back = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_header_status = nullptr;
static lv_obj_t *s_header_accent = nullptr;
static lv_obj_t *s_strip = nullptr;
static lv_obj_t *s_strip_title = nullptr;
static lv_obj_t *s_strip_status = nullptr;
static lv_obj_t *s_btn_clear = nullptr;
static lv_obj_t *s_sheet = nullptr;
static lv_obj_t *s_sheet_title = nullptr;
static lv_obj_t *s_sheet_dot = nullptr;
static lv_obj_t *s_sheet_status = nullptr;
static lv_obj_t *s_sheet_body = nullptr;
static lv_obj_t *s_btn_close = nullptr;
static Page08Action s_actions[kActionMax];
static Page08Card s_cards[PAGE08_CARD_COUNT];
static page08_command_fn s_command_cb = nullptr;
static bool s_active = false;
static int s_open = -1;

static void emit(const char *cmd) {
  if (s_command_cb != nullptr && cmd != nullptr) s_command_cb(cmd);
}

static void set_label(lv_obj_t *lab, const char *text) {
  ShowduinoOsTheme::setTextIfChanged(lab, text ? text : "");
}

static void style_back_button(lv_obj_t *btn) {
  ShowduinoOsTheme::styleAppBackButton(btn);
}

static void style_card(lv_obj_t *panel, bool present) {
  ShowduinoOsTheme::styleRaisedCard(panel, present);
}

static void close_sheet(void);
static void open_sheet(Page08CardId id);
static void fill_sheet(void);

static void back_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (s_open >= 0) {
    close_sheet();
    return;
  }
  emit(PAGE08_CMD_BACK);
}

static lv_obj_t *make_tick(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h) {
  return ShowduinoOsTheme::makeCornerTick(parent, x, y, w, h);
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
  lv_label_set_text(s_title, "SETTINGS");
  lv_obj_set_pos(s_title, 110, 10);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_title, lv_color_hex(ShowduinoPalette::Text), 0);
  showduino_theme_register(s_title, SHOWDUINO_THEME_ROLE_TEXT);

  s_header_status = lv_label_create(s_header);
  lv_label_set_text(s_header_status, "DIRECTOR");
  lv_obj_set_pos(s_header_status, 300, 12);
  lv_obj_set_width(s_header_status, 480);
  lv_label_set_long_mode(s_header_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_header_status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_header_status, lv_color_hex(ShowduinoPalette::Accent), 0);
}

static void card_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const Page08CardId id = (Page08CardId)(intptr_t)lv_event_get_user_data(e);
  if (id < 0 || id >= PAGE08_CARD_COUNT) return;
  open_sheet(id);
}

static void build_card(Page08CardId id, int16_t col, int16_t row) {
  Page08Card *c = &s_cards[id];
  const int16_t x = (int16_t)(kGridX + col * (kCardW + kCardGap));
  const int16_t y = (int16_t)(kGridY + row * (kCardH + kCardGap));

  c->panel = lv_obj_create(s_root);
  lv_obj_remove_style_all(c->panel);
  lv_obj_set_pos(c->panel, x, y);
  lv_obj_set_size(c->panel, kCardW, kCardH);
  lv_obj_clear_flag(c->panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(c->panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c->panel, card_event, LV_EVENT_CLICKED, (void *)(intptr_t)id);
  lv_obj_set_style_bg_color(c->panel, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
  style_card(c->panel, true);
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
  lv_label_set_long_mode(c->title, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(c->title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->title, lv_color_hex(ShowduinoPalette::Text), 0);

  c->dot = lv_obj_create(c->panel);
  lv_obj_remove_style_all(c->dot);
  lv_obj_set_pos(c->dot, 14, 48);
  lv_obj_set_size(c->dot, 10, 10);
  lv_obj_set_style_radius(c->dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(c->dot, lv_color_hex(ShowduinoPalette::Accent), 0);
  lv_obj_set_style_bg_opa(c->dot, LV_OPA_COVER, 0);

  c->status = lv_label_create(c->panel);
  lv_label_set_text(c->status, "READY");
  lv_obj_set_pos(c->status, 32, 44);
  lv_obj_set_width(c->status, kCardW - 44);
  lv_label_set_long_mode(c->status, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(c->status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->status, lv_color_hex(ShowduinoPalette::Accent), 0);

  c->detail = lv_label_create(c->panel);
  lv_label_set_text(c->detail, "");
  lv_obj_set_pos(c->detail, 12, 68);
  lv_obj_set_width(c->detail, kCardW - 24);
  lv_label_set_long_mode(c->detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(c->detail, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->detail, lv_color_hex(ShowduinoPalette::Muted), 0);

  c->open_hint = lv_label_create(c->panel);
  lv_label_set_text(c->open_hint, "OPEN  >");
  lv_obj_set_pos(c->open_hint, 12, 96);
  lv_obj_set_style_text_font(c->open_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(c->open_hint, lv_color_hex(ShowduinoPalette::Accent), 0);
}

static void set_hidden(lv_obj_t *obj, bool hide) {
  if (!obj) return;
  if (hide) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void set_grid_visible(bool show) {
  for (int i = 0; i < PAGE08_CARD_COUNT; i++) {
    if (s_cards[i].panel) set_hidden(s_cards[i].panel, !show);
  }
  if (s_strip) set_hidden(s_strip, !show);
}

static void hide_actions(void) {
  for (uint8_t i = 0; i < kActionMax; i++) {
    if (s_actions[i].btn) lv_obj_add_flag(s_actions[i].btn, LV_OBJ_FLAG_HIDDEN);
  }
}

static void show_actions(const char *const *labels, const char *const *cmds, uint8_t n) {
  if (n > kActionMax) n = kActionMax;
  for (uint8_t i = 0; i < kActionMax; i++) {
    if (!s_actions[i].btn) continue;
    if (i >= n) {
      lv_obj_add_flag(s_actions[i].btn, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(s_actions[i].btn, LV_OBJ_FLAG_HIDDEN);
    if (s_actions[i].lab) lv_label_set_text(s_actions[i].lab, labels[i]);
    lv_obj_set_user_data(s_actions[i].btn, (void *)cmds[i]);
  }
}

static void fill_sheet(void) {
  if (!s_sheet || s_open < 0 || s_open >= PAGE08_CARD_COUNT) return;
  hide_actions();
  const Page08Card *c = &s_cards[s_open];
  if (s_title) set_label(s_title, c->name);
  if (s_sheet_title) set_label(s_sheet_title, c->name);
  if (s_sheet_dot) {
    lv_obj_set_style_bg_color(s_sheet_dot, lv_color_hex(c->status_color), 0);
  }
  if (s_sheet_status) {
    set_label(s_sheet_status, c->status_text[0] ? c->status_text : "UNKNOWN");
    lv_obj_set_style_text_color(s_sheet_status, lv_color_hex(c->status_color), 0);
  }
  if (s_sheet_body) {
    set_label(s_sheet_body, c->sheet_body[0] ? c->sheet_body : "NOT REPORTED");
  }

  static const char *kTimeoutLab[] = {
    "NEVER", "1 MIN", "3 MIN", "5 MIN", "10 MIN", "30 MIN", "CYCLE",
    "CALIBRATE", "RESET TOUCH"
  };
  static const char *kTimeoutCmd[] = {
    "SETTINGS:TIMEOUT:0", "SETTINGS:TIMEOUT:1", "SETTINGS:TIMEOUT:3",
    "SETTINGS:TIMEOUT:5", "SETTINGS:TIMEOUT:10", "SETTINGS:TIMEOUT:30",
    "SETTINGS:TIMEOUT:CYCLE",
    PAGE08_CMD_TOUCH_CAL, PAGE08_CMD_TOUCH_RESET
  };
  static const char *kAtmoLab[] = { "LEDS", "LED BRI", "UI MOTION" };
  static const char *kAtmoCmd[] = {
    "SETTINGS:AMBIENT:TOGGLE", "SETTINGS:AMBIENT:BRI:CYCLE", "SETTINGS:ANIM:CYCLE"
  };
  static const char *kAudioLab[] = { "OPEN AUDIO SYSTEM" };
  static const char *kAudioCmd[] = { PAGE08_CMD_AUDIO };
  static const char *kLogsLab[] = { "OPEN SYSTEM LOGS" };
  static const char *kLogsCmd[] = { PAGE08_CMD_LOGS };
  static const char *kStorLab[] = { "BACKUP", "EXPORT" };
  static const char *kStorCmd[] = { "STORAGE:BACKUP", "STORAGE:EXPORT" };
  static const char *kSysLab[] = { "ABOUT", "NETWORK", "SOFTWARE" };
  static const char *kSysCmd[] = { "SETTINGS:ABOUT", "SETTINGS:NETWORK", "SETTINGS:SOFTWARE" };

  switch ((Page08CardId)s_open) {
    case PAGE08_CARD_DISPLAY: show_actions(kTimeoutLab, kTimeoutCmd, 9); break;
    case PAGE08_CARD_ATMOSPHERE: show_actions(kAtmoLab, kAtmoCmd, 3); break;
    case PAGE08_CARD_AUDIO: show_actions(kAudioLab, kAudioCmd, 1); break;
    case PAGE08_CARD_LOGS: show_actions(kLogsLab, kLogsCmd, 1); break;
    case PAGE08_CARD_STORAGE: show_actions(kStorLab, kStorCmd, 2); break;
    case PAGE08_CARD_SYSTEM: show_actions(kSysLab, kSysCmd, 3); break;
    default: break;
  }
}

static void open_sheet(Page08CardId id) {
  s_open = (int)id;
  set_grid_visible(false);
  if (s_sheet) lv_obj_clear_flag(s_sheet, LV_OBJ_FLAG_HIDDEN);
  fill_sheet();
}

static void close_sheet(void) {
  s_open = -1;
  hide_actions();
  if (s_sheet) lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_HIDDEN);
  if (s_title) set_label(s_title, "SETTINGS");
  set_grid_visible(true);
}

static void action_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *cmd = (const char *)lv_obj_get_user_data(lv_event_get_target_obj(e));
  if (cmd) emit(cmd);
}

static void build_strip(lv_obj_t *parent) {
  s_strip = lv_obj_create(parent);
  lv_obj_remove_style_all(s_strip);
  lv_obj_set_pos(s_strip, kGridX, kStripY);
  lv_obj_set_size(s_strip, kSheetW, kStripH);
  lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_SCROLLABLE);
  ShowduinoOsTheme::styleRaisedCard(s_strip, true);
  showduino_theme_register(s_strip, SHOWDUINO_THEME_ROLE_BORDER);

  s_strip_title = lv_label_create(s_strip);
  lv_label_set_text(s_strip_title, "SAFETY");
  lv_obj_set_pos(s_strip_title, 14, 12);
  lv_obj_set_style_text_font(s_strip_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_strip_title, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_strip_status = lv_label_create(s_strip);
  lv_label_set_text(s_strip_status, "CLEAR is a request. P4 remains the latch.");
  lv_obj_set_pos(s_strip_status, 90, 12);
  lv_obj_set_width(s_strip_status, 480);
  lv_label_set_long_mode(s_strip_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_strip_status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_strip_status, lv_color_hex(ShowduinoPalette::Text), 0);

  s_btn_clear = lv_button_create(s_strip);
  ShowduinoOsTheme::styleAppBackButton(s_btn_clear);
  lv_obj_set_pos(s_btn_clear, kSheetW - 168, 4);
  lv_obj_set_size(s_btn_clear, 156, 36);
  lv_obj_clear_flag(s_btn_clear, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_user_data(s_btn_clear, (void *)"EMERGENCY:CLEAR");
  lv_obj_add_event_cb(s_btn_clear, action_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cl = lv_label_create(s_btn_clear);
  lv_label_set_text(cl, "CLEAR E-STOP");
  lv_obj_set_style_text_color(cl, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(cl);
}

static void build_sheet(lv_obj_t *parent) {
  s_sheet = lv_obj_create(parent);
  lv_obj_remove_style_all(s_sheet);
  lv_obj_set_pos(s_sheet, kGridX, kStripY);
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
  lv_label_set_text(s_sheet_title, "DETAIL");
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
  lv_label_set_text(s_sheet_status, "UNKNOWN");
  lv_obj_set_pos(s_sheet_status, 38, 56);
  lv_obj_set_width(s_sheet_status, 600);
  lv_obj_set_style_text_font(s_sheet_status, &lv_font_montserrat_16, 0);

  s_sheet_body = lv_label_create(s_sheet);
  lv_label_set_text(s_sheet_body, "");
  lv_obj_set_pos(s_sheet_body, 16, 84);
  lv_obj_set_width(s_sheet_body, kSheetW - 32);
  lv_label_set_long_mode(s_sheet_body, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_sheet_body, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_sheet_body, lv_color_hex(ShowduinoPalette::Text), 0);

  for (uint8_t i = 0; i < kActionMax; i++) {
    const int16_t col = (int16_t)(i % 4);
    const int16_t row = (int16_t)(i / 4);
    lv_obj_t *btn = lv_button_create(s_sheet);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, 16 + col * 184, 178 + row * 50);
    lv_obj_set_size(btn, 172, 42);
    lv_obj_set_style_radius(btn, OS_BTN_RADIUS, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(ShowduinoPalette::AccentDark), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
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

void page_08_settings_create(lv_obj_t *parent, page08_command_fn command_cb) {
  if (!parent) return;
  if (s_active) page_08_settings_destroy();

  showduino_theme_init();
  s_command_cb = command_cb;
  s_root = parent;
  s_open = -1;

  static const char *kNames[PAGE08_CARD_COUNT] = {
    "DISPLAY", "ATMOSPHERE", "AUDIO SYSTEM", "SYSTEM LOGS", "STORAGE", "SYSTEM"
  };
  static const char *kBodies[PAGE08_CARD_COUNT] = {
    "Auto backlight dims at half timeout, then off. Touch wakes the panel.\nNEVER keeps the display on.\nCalibrate the touchscreen if taps no longer line up.",
    "Atmosphere LEDs follow the Director theme. Emergency indication always remains available.",
    "P4 local output: Stop is live. Play needs an asset path from a loaded show.",
    "Operator event history for this Director session. Filters do not change Stage logs.",
    "Backup and export act on Director-local SD only. They do not copy Stage runtime state.",
    "About and Software show versions. OTA install is not implemented. Network is SUE gateway state."
  };

  memset(s_cards, 0, sizeof(s_cards));
  for (int i = 0; i < PAGE08_CARD_COUNT; i++) {
    s_cards[i].name = kNames[i];
    strncpy(s_cards[i].sheet_body, kBodies[i], sizeof(s_cards[i].sheet_body) - 1);
    s_cards[i].present = true;
    s_cards[i].status_color = ShowduinoPalette::Accent;
  }

  build_header(parent);
  build_strip(parent);
  build_card(PAGE08_CARD_DISPLAY, 0, 0);
  build_card(PAGE08_CARD_ATMOSPHERE, 1, 0);
  build_card(PAGE08_CARD_AUDIO, 2, 0);
  build_card(PAGE08_CARD_LOGS, 0, 1);
  build_card(PAGE08_CARD_STORAGE, 1, 1);
  build_card(PAGE08_CARD_SYSTEM, 2, 1);
  build_sheet(parent);
  page_08_settings_apply_theme();
  s_active = true;
}

void page_08_settings_destroy(void) {
  if (!s_active && !s_root) return;
  showduino_theme_unregister(s_header_accent);
  showduino_theme_unregister(s_title);
  showduino_theme_unregister(s_btn_back);
  showduino_theme_unregister(s_sheet);
  showduino_theme_unregister(s_btn_close);
  showduino_theme_unregister(s_strip);
  showduino_theme_unregister(s_btn_clear);
  for (int i = 0; i < PAGE08_CARD_COUNT; i++) {
    showduino_theme_unregister(s_cards[i].panel);
    showduino_theme_unregister(s_cards[i].accent);
  }
  for (uint8_t i = 0; i < kActionMax; i++) showduino_theme_unregister(s_actions[i].btn);
  if (s_root) lv_obj_clean(s_root);
  memset(&s_cards, 0, sizeof(s_cards));
  memset(&s_actions, 0, sizeof(s_actions));
  s_root = s_header = s_btn_back = s_title = s_header_status = s_header_accent = nullptr;
  s_strip = s_strip_title = s_strip_status = s_btn_clear = nullptr;
  s_sheet = s_sheet_title = s_sheet_dot = s_sheet_status = s_sheet_body = s_btn_close = nullptr;
  s_command_cb = nullptr;
  s_open = -1;
  s_active = false;
}

bool page_08_settings_is_active(void) { return s_active; }

void page_08_settings_apply_theme(void) {
  showduino_theme_apply();
  const lv_color_t accent = showduino_theme_get_accent();
  if (s_header_accent) lv_obj_set_style_bg_color(s_header_accent, accent, 0);
  if (s_btn_back) lv_obj_set_style_border_color(s_btn_back, accent, 0);
  if (s_btn_close) lv_obj_set_style_border_color(s_btn_close, accent, 0);
  if (s_btn_clear) lv_obj_set_style_border_color(s_btn_clear, accent, 0);
  if (s_strip) lv_obj_set_style_border_color(s_strip, accent, 0);
  if (s_sheet) lv_obj_set_style_border_color(s_sheet, accent, 0);
  for (int i = 0; i < PAGE08_CARD_COUNT; i++) {
    if (s_cards[i].panel) style_card(s_cards[i].panel, s_cards[i].present);
    if (s_cards[i].accent) lv_obj_set_style_bg_color(s_cards[i].accent, accent, 0);
  }
}

void page_08_settings_close_sheet(void) { close_sheet(); }

void page_08_settings_set_header(const char *status, uint32_t color) {
  if (!s_header_status) return;
  set_label(s_header_status, status ? status : "");
  lv_obj_set_style_text_color(s_header_status, lv_color_hex(color), 0);
}

void page_08_settings_set_strip(const char *title, const char *status, uint32_t color) {
  if (s_strip_title) set_label(s_strip_title, title ? title : "SAFETY");
  if (s_strip_status) {
    set_label(s_strip_status, status ? status : "");
    lv_obj_set_style_text_color(s_strip_status, lv_color_hex(color), 0);
  }
}

void page_08_settings_set_card(Page08CardId id, bool present,
                               const char *status, const char *detail,
                               uint32_t status_color) {
  if (id < 0 || id >= PAGE08_CARD_COUNT) return;
  Page08Card *c = &s_cards[id];
  c->present = present;
  c->status_color = status_color;
  strncpy(c->status_text, status && status[0] ? status : "UNKNOWN", sizeof(c->status_text) - 1);
  c->status_text[sizeof(c->status_text) - 1] = 0;
  strncpy(c->detail_text, (detail && detail[0]) ? detail : "NOT REPORTED",
          sizeof(c->detail_text) - 1);
  c->detail_text[sizeof(c->detail_text) - 1] = 0;
  if (c->panel) style_card(c->panel, present);
  if (c->dot) lv_obj_set_style_bg_color(c->dot, lv_color_hex(status_color), 0);
  if (c->status) {
    set_label(c->status, c->status_text);
    lv_obj_set_style_text_color(c->status, lv_color_hex(status_color), 0);
  }
  if (c->detail) set_label(c->detail, c->detail_text);
  if (s_open == (int)id) fill_sheet();
}

void page_08_settings_set_sheet_body(Page08CardId id, const char *body) {
  if (id < 0 || id >= PAGE08_CARD_COUNT) return;
  strncpy(s_cards[id].sheet_body, body ? body : "", sizeof(s_cards[id].sheet_body) - 1);
  s_cards[id].sheet_body[sizeof(s_cards[id].sheet_body) - 1] = 0;
  if (s_open == (int)id) fill_sheet();
}
