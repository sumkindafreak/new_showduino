#include "page_10_live.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "showduino_theme.h"
#include "ShowduinoOsPalette.h"
#include "ShowduinoOsUi.h"
#include "DisplayTypes.h"

static const int16_t kHeaderY = (int16_t)OS_TITLE_Y;
static const int16_t kHeaderH = OS_TITLE_H;
static const int16_t kBodyX = 16;
static const int16_t kStripY = (int16_t)(kHeaderY + kHeaderH + OS_GAP);
static const int16_t kStripH = 72;
static const int16_t kBodyW = 768;
static const int16_t kMainY = (int16_t)(kStripY + kStripH + OS_GAP);
static const int16_t kMainH = (int16_t)(OS_DOCK_Y - kMainY - OS_GAP);

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_header_accent = nullptr;
static lv_obj_t *s_btn_back = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_header_status = nullptr;
static lv_obj_t *s_strip = nullptr;
static lv_obj_t *s_cue = nullptr;
static lv_obj_t *s_elapsed = nullptr;
static lv_obj_t *s_remain = nullptr;
static lv_obj_t *s_pending = nullptr;
static lv_obj_t *s_main = nullptr;
static lv_obj_t *s_dot = nullptr;
static lv_obj_t *s_progress = nullptr;
static lv_obj_t *s_progress_lab = nullptr;
static page10_command_fn s_cb = nullptr;
static bool s_active = false;

static void emit(const char *cmd) {
  if (s_cb && cmd) s_cb(cmd);
}

static void back_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  emit(PAGE10_CMD_BACK);
}

static void action_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *cmd = (const char *)lv_obj_get_user_data(lv_event_get_target_obj(e));
  if (cmd) emit(cmd);
}

static lv_obj_t *make_metric(lv_obj_t *parent, const char *caption, int16_t x,
                             lv_obj_t **value_out) {
  lv_obj_t *cap = lv_label_create(parent);
  lv_label_set_text(cap, caption);
  lv_obj_set_pos(cap, x, 8);
  lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(cap, lv_color_hex(ShowduinoPalette::Muted), 0);
  lv_obj_t *val = lv_label_create(parent);
  lv_label_set_text(val, "-");
  lv_obj_set_pos(val, x, 32);
  lv_obj_set_width(val, 150);
  lv_label_set_long_mode(val, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(val, lv_color_hex(ShowduinoPalette::Text), 0);
  if (value_out) *value_out = val;
  return val;
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *label, int16_t x, int16_t y,
                          int16_t w, const char *cmd, bool danger, bool lock_tap) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, 48);
  lv_obj_set_style_radius(btn, OS_BTN_RADIUS, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(danger ? ShowduinoPalette::DangerPanel
                                                     : ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(danger ? ShowduinoPalette::Danger
                                                         : ShowduinoPalette::AccentDark), 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(danger ? ShowduinoPalette::DangerDark
                                                     : ShowduinoPalette::AccentDim),
                            LV_STATE_PRESSED);
  if (lock_tap) lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_user_data(btn, (void *)cmd);
  lv_obj_add_event_cb(btn, action_event, LV_EVENT_CLICKED, nullptr);
  showduino_theme_register(btn, SHOWDUINO_THEME_ROLE_BORDER);
  lv_obj_t *lab = lv_label_create(btn);
  lv_label_set_text(lab, label);
  lv_obj_set_style_text_color(lab, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(lab);
  return btn;
}

void page_10_live_create(lv_obj_t *parent, page10_command_fn command_cb) {
  if (!parent) return;
  if (s_active) page_10_live_destroy();
  showduino_theme_init();
  s_cb = command_cb;
  s_root = parent;

  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_remove_style_all(header);
  lv_obj_set_pos(header, 0, kHeaderY);
  lv_obj_set_size(header, DISPLAY_WIDTH, kHeaderH);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  s_header_accent = ShowduinoOsTheme::makeHairline(header, 110, 34, 72, 3, OsColor::Accent);
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
  lv_label_set_text(s_title, "LIVE");
  lv_obj_set_pos(s_title, 110, 10);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_title, lv_color_hex(ShowduinoPalette::Text), 0);
  showduino_theme_register(s_title, SHOWDUINO_THEME_ROLE_TEXT);

  s_header_status = lv_label_create(header);
  lv_label_set_text(s_header_status, "IDLE");
  lv_obj_set_pos(s_header_status, 300, 12);
  lv_obj_set_width(s_header_status, 480);
  lv_label_set_long_mode(s_header_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_header_status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_header_status, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_strip = lv_obj_create(parent);
  lv_obj_remove_style_all(s_strip);
  lv_obj_set_pos(s_strip, kBodyX, kStripY);
  lv_obj_set_size(s_strip, kBodyW, kStripH);
  lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_SCROLLABLE);
  ShowduinoOsTheme::styleRaisedCard(s_strip, true);
  ShowduinoOsTheme::decorateCard(s_strip, true);
  showduino_theme_register(s_strip, SHOWDUINO_THEME_ROLE_BORDER);
  make_metric(s_strip, "CUE", 16, &s_cue);
  make_metric(s_strip, "ELAPSED", 180, &s_elapsed);
  make_metric(s_strip, "REMAINING", 344, &s_remain);

  s_pending = lv_label_create(s_strip);
  lv_label_set_text(s_pending, "");
  lv_obj_set_pos(s_pending, 508, 32);
  lv_obj_set_width(s_pending, 244);
  lv_label_set_long_mode(s_pending, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_pending, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_pending, lv_color_hex(ShowduinoPalette::Pending), 0);

  s_main = lv_obj_create(parent);
  lv_obj_remove_style_all(s_main);
  lv_obj_set_pos(s_main, kBodyX, kMainY);
  lv_obj_set_size(s_main, kBodyW, kMainH);
  lv_obj_clear_flag(s_main, LV_OBJ_FLAG_SCROLLABLE);
  ShowduinoOsTheme::styleRaisedCard(s_main, true);
  ShowduinoOsTheme::decorateCard(s_main, true);
  showduino_theme_register(s_main, SHOWDUINO_THEME_ROLE_BORDER);

  lv_obj_t *th = lv_label_create(s_main);
  lv_label_set_text(th, "TRANSPORT");
  lv_obj_set_pos(th, 16, 16);
  lv_obj_set_style_text_font(th, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(th, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_dot = lv_obj_create(s_main);
  lv_obj_remove_style_all(s_dot);
  lv_obj_set_pos(s_dot, kBodyW - 28, 18);
  lv_obj_set_size(s_dot, 12, 12);
  lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_dot, lv_color_hex(ShowduinoPalette::Disabled), 0);

  make_btn(s_main, "START", 16, 44, 100, "SHOW:START", false, false);
  make_btn(s_main, "PAUSE", 124, 44, 100, "SHOW:PAUSE", false, false);
  make_btn(s_main, "RESUME", 232, 44, 110, "SHOW:RESUME", false, false);
  make_btn(s_main, "STOP", 350, 44, 90, "SHOW:STOP", true, false);
  make_btn(s_main, "STATUS", 448, 44, 110, "STATUS:REQUEST", false, false);
  make_btn(s_main, "E-CLEAR", 566, 44, 180, "EMERGENCY:CLEAR", false, true);

  lv_obj_t *ph = lv_label_create(s_main);
  lv_label_set_text(ph, "PROGRESS");
  lv_obj_set_pos(ph, 16, 108);
  lv_obj_set_style_text_font(ph, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ph, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_progress_lab = lv_label_create(s_main);
  lv_label_set_text(s_progress_lab, "0%");
  lv_obj_set_pos(s_progress_lab, 16, 136);
  lv_obj_set_style_text_font(s_progress_lab, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_progress_lab, lv_color_hex(ShowduinoPalette::Text), 0);

  s_progress = lv_bar_create(s_main);
  lv_obj_set_pos(s_progress, 80, 142);
  lv_obj_set_size(s_progress, kBodyW - 112, 14);
  lv_bar_set_range(s_progress, 0, 100);
  lv_bar_set_value(s_progress, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_progress, lv_color_hex(OsColor::ScanLine), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_progress, lv_color_hex(OsColor::Accent), LV_PART_INDICATOR);
  lv_obj_set_style_radius(s_progress, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(s_progress, 4, LV_PART_INDICATOR);

  lv_obj_t *note = lv_label_create(s_main);
  lv_label_set_text(note, "Director requests only. Stage remains the show authority.");
  lv_obj_set_pos(note, 16, 176);
  lv_obj_set_width(note, kBodyW - 32);
  lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(note, lv_color_hex(ShowduinoPalette::Muted), 0);

  page_10_live_apply_theme();
  s_active = true;
}

void page_10_live_destroy(void) {
  if (!s_active && !s_root) return;
  showduino_theme_unregister(s_header_accent);
  showduino_theme_unregister(s_title);
  showduino_theme_unregister(s_btn_back);
  showduino_theme_unregister(s_strip);
  showduino_theme_unregister(s_main);
  if (s_root) lv_obj_clean(s_root);
  s_root = s_header_accent = s_btn_back = s_title = s_header_status = nullptr;
  s_strip = s_cue = s_elapsed = s_remain = s_pending = nullptr;
  s_main = s_dot = s_progress = s_progress_lab = nullptr;
  s_cb = nullptr;
  s_active = false;
}

bool page_10_live_is_active(void) { return s_active; }

void page_10_live_apply_theme(void) {
  showduino_theme_apply();
  const lv_color_t accent = showduino_theme_get_accent();
  if (s_header_accent) lv_obj_set_style_bg_color(s_header_accent, accent, 0);
  if (s_btn_back) lv_obj_set_style_border_color(s_btn_back, accent, 0);
  if (s_strip) lv_obj_set_style_border_color(s_strip, accent, 0);
  if (s_main) lv_obj_set_style_border_color(s_main, accent, 0);
  if (s_progress) lv_obj_set_style_bg_color(s_progress, accent, LV_PART_INDICATOR);
}

void page_10_live_set_header(const char *word, uint32_t color) {
  ShowduinoOsTheme::setTextIfChanged(s_header_status, word ? word : "");
  if (s_header_status) lv_obj_set_style_text_color(s_header_status, lv_color_hex(color), 0);
}

void page_10_live_set_cue(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_cue, text ? text : "-");
}

void page_10_live_set_elapsed(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_elapsed, text ? text : "-");
}

void page_10_live_set_remain(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_remain, text ? text : "-");
}

void page_10_live_set_pending(const char *text, uint32_t color) {
  ShowduinoOsTheme::setTextIfChanged(s_pending, text ? text : "");
  if (s_pending) lv_obj_set_style_text_color(s_pending, lv_color_hex(color), 0);
}

void page_10_live_set_progress(uint8_t pct) {
  if (s_progress) lv_bar_set_value(s_progress, pct, LV_ANIM_ON);
  if (s_progress_lab) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
    ShowduinoOsTheme::setTextIfChanged(s_progress_lab, buf);
  }
}

void page_10_live_set_emergency(bool active) {
  if (!s_dot) return;
  lv_obj_set_style_bg_color(s_dot,
      lv_color_hex(active ? ShowduinoPalette::Danger : ShowduinoPalette::Disabled), 0);
}
