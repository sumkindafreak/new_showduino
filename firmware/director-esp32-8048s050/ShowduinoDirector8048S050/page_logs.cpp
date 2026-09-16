#include "page_logs.h"

#include <Arduino.h>
#include <string.h>
#include "showduino_theme.h"
#include "ShowduinoOsPalette.h"
#include "ShowduinoOsUi.h"
#include "DisplayTypes.h"

static const int16_t kHeaderY = (int16_t)OS_TITLE_Y;
static const int16_t kHeaderH = OS_TITLE_H;
static const int16_t kBodyX = 16;
static const int16_t kStripY = (int16_t)(kHeaderY + kHeaderH + OS_GAP);
static const int16_t kStripH = 88;
static const int16_t kBodyW = 768;
static const int16_t kListY = (int16_t)(kStripY + kStripH + OS_GAP);
static const int16_t kListH = (int16_t)(OS_DOCK_Y - kListY - OS_GAP);

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_header_accent = nullptr;
static lv_obj_t *s_btn_back = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_header_status = nullptr;
static lv_obj_t *s_strip = nullptr;
static lv_obj_t *s_count = nullptr;
static lv_obj_t *s_newest = nullptr;
static lv_obj_t *s_filter = nullptr;
static lv_obj_t *s_list = nullptr;
static lv_obj_t *s_scroll = nullptr;
static lv_obj_t *s_body = nullptr;
static page_logs_command_fn s_cb = nullptr;
static bool s_active = false;

static void emit(const char *cmd) {
  if (s_cb && cmd) s_cb(cmd);
}

static void back_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  emit("SCREEN:SETTINGS");
}

static void action_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *cmd = (const char *)lv_obj_get_user_data(lv_event_get_target_obj(e));
  if (cmd) emit(cmd);
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *label, int16_t x, int16_t y,
                          int16_t w, const char *cmd, bool danger) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, 36);
  lv_obj_set_style_radius(btn, OS_BTN_RADIUS, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(danger ? ShowduinoPalette::DangerPanel
                                                     : ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(danger ? ShowduinoPalette::Danger
                                                         : ShowduinoPalette::AccentDark), 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
  lv_obj_set_user_data(btn, (void *)cmd);
  lv_obj_add_event_cb(btn, action_event, LV_EVENT_CLICKED, nullptr);
  showduino_theme_register(btn, SHOWDUINO_THEME_ROLE_BORDER);
  lv_obj_t *lab = lv_label_create(btn);
  lv_label_set_text(lab, label);
  lv_obj_set_style_text_color(lab, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(lab);
  return btn;
}

void page_logs_create(lv_obj_t *parent, page_logs_command_fn command_cb) {
  if (!parent) return;
  if (s_active) page_logs_destroy();
  showduino_theme_init();
  s_cb = command_cb;
  s_root = parent;

  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_remove_style_all(header);
  lv_obj_set_pos(header, 0, kHeaderY);
  lv_obj_set_size(header, DISPLAY_WIDTH, kHeaderH);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  s_header_accent = ShowduinoOsTheme::makeHairline(header, 110, 34, 168, 3, OsColor::Accent);
  showduino_theme_register(s_header_accent, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);

  s_btn_back = lv_button_create(header);
  ShowduinoOsTheme::styleAppBackButton(s_btn_back);
  lv_obj_set_pos(s_btn_back, 12, 4);
  lv_obj_set_size(s_btn_back, 88, 40);
  lv_obj_add_event_cb(s_btn_back, back_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *bl = lv_label_create(s_btn_back);
  lv_label_set_text(bl, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_color(bl, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(bl);

  s_title = lv_label_create(header);
  lv_label_set_text(s_title, "SYSTEM LOGS");
  lv_obj_set_pos(s_title, 110, 10);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_title, lv_color_hex(ShowduinoPalette::Text), 0);
  showduino_theme_register(s_title, SHOWDUINO_THEME_ROLE_TEXT);

  s_header_status = lv_label_create(header);
  lv_label_set_text(s_header_status, "ALL");
  lv_obj_set_pos(s_header_status, 360, 12);
  lv_obj_set_width(s_header_status, 420);
  lv_label_set_long_mode(s_header_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_header_status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_header_status, lv_color_hex(ShowduinoPalette::Accent), 0);

  s_strip = lv_obj_create(parent);
  lv_obj_remove_style_all(s_strip);
  lv_obj_set_pos(s_strip, kBodyX, kStripY);
  lv_obj_set_size(s_strip, kBodyW, kStripH);
  lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_SCROLLABLE);
  ShowduinoOsTheme::styleRaisedCard(s_strip, true);
  ShowduinoOsTheme::decorateCard(s_strip, true);
  showduino_theme_register(s_strip, SHOWDUINO_THEME_ROLE_BORDER);

  s_count = lv_label_create(s_strip);
  lv_label_set_text(s_count, "Events: 0");
  lv_obj_set_pos(s_count, 14, 14);
  lv_obj_set_style_text_font(s_count, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_count, lv_color_hex(ShowduinoPalette::Text), 0);

  s_newest = lv_label_create(s_strip);
  lv_label_set_text(s_newest, "Newest: -");
  lv_obj_set_pos(s_newest, 160, 14);
  lv_obj_set_width(s_newest, 420);
  lv_label_set_long_mode(s_newest, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_newest, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_newest, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_filter = lv_label_create(s_strip);
  lv_label_set_text(s_filter, "Filter: All");
  lv_obj_set_pos(s_filter, 590, 14);
  lv_obj_set_style_text_font(s_filter, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_filter, lv_color_hex(ShowduinoPalette::Muted), 0);

  make_btn(s_strip, "ALL", 14, 44, 70, "UI:LOGS:FILTER:0", false);
  make_btn(s_strip, "SYS", 90, 44, 70, "UI:LOGS:FILTER:1", false);
  make_btn(s_strip, "SHOW", 166, 44, 78, "UI:LOGS:FILTER:2", false);
  make_btn(s_strip, "AUDIO", 250, 44, 86, "UI:LOGS:FILTER:3", false);
  make_btn(s_strip, "NET", 342, 44, 70, "UI:LOGS:FILTER:4", false);
  make_btn(s_strip, "E-STOP", 418, 44, 86, "UI:LOGS:FILTER:5", false);
  make_btn(s_strip, "CLEAR", 512, 44, 78, "UI:LOGS:CLEAR", true);
  make_btn(s_strip, "PAUSE", 596, 44, 78, "UI:LOGS:PAUSE", false);
  make_btn(s_strip, "RESUME", 680, 44, 74, "UI:LOGS:RESUME", false);

  s_list = lv_obj_create(parent);
  lv_obj_remove_style_all(s_list);
  lv_obj_set_pos(s_list, kBodyX, kListY);
  lv_obj_set_size(s_list, kBodyW, kListH);
  lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
  ShowduinoOsTheme::styleRaisedCard(s_list, true);
  ShowduinoOsTheme::decorateCard(s_list, true);
  showduino_theme_register(s_list, SHOWDUINO_THEME_ROLE_BORDER);

  s_scroll = lv_obj_create(s_list);
  lv_obj_remove_style_all(s_scroll);
  lv_obj_set_pos(s_scroll, 12, 16);
  lv_obj_set_size(s_scroll, kBodyW - 24, kListH - 24);
  lv_obj_set_style_bg_opa(s_scroll, LV_OPA_TRANSP, 0);
  ShowduinoOsTheme::enableVerticalScroll(s_scroll);

  s_body = lv_label_create(s_scroll);
  lv_obj_set_pos(s_body, 2, 0);
  lv_obj_set_width(s_body, kBodyW - 40);
  lv_obj_set_style_text_font(s_body, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_body, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
  lv_label_set_text(s_body, "(no events)\n");

  page_logs_apply_theme();
  s_active = true;
}

void page_logs_destroy(void) {
  if (!s_active && !s_root) return;
  showduino_theme_unregister(s_header_accent);
  showduino_theme_unregister(s_title);
  showduino_theme_unregister(s_btn_back);
  showduino_theme_unregister(s_strip);
  showduino_theme_unregister(s_list);
  if (s_root) lv_obj_clean(s_root);
  s_root = s_header_accent = s_btn_back = s_title = s_header_status = nullptr;
  s_strip = s_count = s_newest = s_filter = s_list = s_scroll = s_body = nullptr;
  s_cb = nullptr;
  s_active = false;
}

bool page_logs_is_active(void) { return s_active; }

void page_logs_apply_theme(void) {
  showduino_theme_apply();
  const lv_color_t accent = showduino_theme_get_accent();
  if (s_header_accent) lv_obj_set_style_bg_color(s_header_accent, accent, 0);
  if (s_btn_back) lv_obj_set_style_border_color(s_btn_back, accent, 0);
  if (s_strip) lv_obj_set_style_border_color(s_strip, accent, 0);
  if (s_list) lv_obj_set_style_border_color(s_list, accent, 0);
}

void page_logs_set_header(const char *status, uint32_t color) {
  ShowduinoOsTheme::setTextIfChanged(s_header_status, status ? status : "");
  if (s_header_status) lv_obj_set_style_text_color(s_header_status, lv_color_hex(color), 0);
}

void page_logs_set_count(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_count, text ? text : "");
}

void page_logs_set_newest(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_newest, text ? text : "");
}

void page_logs_set_filter(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_filter, text ? text : "");
}

void page_logs_set_body(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_body, text ? text : "(no events)\n");
  if (s_scroll) lv_obj_scroll_to_y(s_scroll, 0, LV_ANIM_OFF);
}

lv_obj_t *page_logs_scroll(void) { return s_scroll; }
lv_obj_t *page_logs_body_label(void) { return s_body; }
