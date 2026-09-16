#include "page_audio_system.h"

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
static const int16_t kStripH = 52;
static const int16_t kBodyW = 768;
static const int16_t kScrollY = (int16_t)(kStripY + kStripH + OS_GAP);
static const int16_t kScrollH = (int16_t)(OS_DOCK_Y - kScrollY - OS_GAP);

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_header_accent = nullptr;
static lv_obj_t *s_btn_back = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_header_status = nullptr;
static lv_obj_t *s_strip = nullptr;
static lv_obj_t *s_scroll = nullptr;
static lv_obj_t *s_local_status = nullptr;
static lv_obj_t *s_local_detail = nullptr;
static lv_obj_t *s_nodes = nullptr;
static lv_obj_t *s_routing = nullptr;
static lv_obj_t *s_cmd = nullptr;
static page_audio_system_command_fn s_cb = nullptr;
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

static lv_obj_t *make_section(lv_obj_t *parent, const char *title, int16_t y, int16_t h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_pos(card, 0, y);
  lv_obj_set_size(card, kBodyW - 8, h);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  ShowduinoOsTheme::styleRaisedCard(card, true);
  ShowduinoOsTheme::decorateCard(card, true);
  ShowduinoOsTheme::disableNestedScroll(card);
  showduino_theme_register(card, SHOWDUINO_THEME_ROLE_BORDER);
  lv_obj_t *lab = lv_label_create(card);
  lv_label_set_text(lab, title);
  lv_obj_set_pos(lab, 14, 14);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lab, lv_color_hex(ShowduinoPalette::Muted), 0);
  return card;
}

static lv_obj_t *make_body(lv_obj_t *card, int16_t y, int16_t w) {
  lv_obj_t *lab = lv_label_create(card);
  lv_label_set_text(lab, "NOT REPORTED");
  lv_obj_set_pos(lab, 14, y);
  lv_obj_set_width(lab, w);
  lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lab, lv_color_hex(ShowduinoPalette::Text), 0);
  return lab;
}

void page_audio_system_create(lv_obj_t *parent, page_audio_system_command_fn command_cb) {
  if (!parent) return;
  if (s_active) page_audio_system_destroy();
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
  lv_label_set_text(s_title, "AUDIO SYSTEM");
  lv_obj_set_pos(s_title, 110, 10);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_title, lv_color_hex(ShowduinoPalette::Text), 0);
  showduino_theme_register(s_title, SHOWDUINO_THEME_ROLE_TEXT);

  s_header_status = lv_label_create(header);
  lv_label_set_text(s_header_status, "P4 LOCAL");
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
  showduino_theme_register(s_strip, SHOWDUINO_THEME_ROLE_BORDER);

  lv_obj_t *hint = lv_label_create(s_strip);
  lv_label_set_text(hint, "P4 local output. Stop is live. Play needs a loaded show asset.");
  lv_obj_set_pos(hint, 14, 16);
  lv_obj_set_width(hint, 560);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(ShowduinoPalette::Muted), 0);

  lv_obj_t *stop = lv_button_create(s_strip);
  lv_obj_remove_style_all(stop);
  lv_obj_set_pos(stop, kBodyW - 128, 6);
  lv_obj_set_size(stop, 112, 40);
  lv_obj_set_style_radius(stop, OS_BTN_RADIUS, 0);
  lv_obj_set_style_bg_opa(stop, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(stop, lv_color_hex(ShowduinoPalette::DangerPanel), 0);
  lv_obj_set_style_border_width(stop, 2, 0);
  lv_obj_set_style_border_color(stop, lv_color_hex(ShowduinoPalette::Danger), 0);
  lv_obj_set_style_bg_color(stop, lv_color_hex(ShowduinoPalette::DangerDark), LV_STATE_PRESSED);
  lv_obj_set_user_data(stop, (void *)"AUDIO:LOCAL:STOP");
  lv_obj_add_event_cb(stop, action_event, LV_EVENT_CLICKED, nullptr);
  showduino_theme_register(stop, SHOWDUINO_THEME_ROLE_BORDER);
  lv_obj_t *sl = lv_label_create(stop);
  lv_label_set_text(sl, "STOP");
  lv_obj_set_style_text_color(sl, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(sl);

  s_scroll = lv_obj_create(parent);
  lv_obj_remove_style_all(s_scroll);
  lv_obj_set_pos(s_scroll, kBodyX, kScrollY);
  lv_obj_set_size(s_scroll, kBodyW, kScrollH);
  lv_obj_set_style_bg_opa(s_scroll, LV_OPA_TRANSP, 0);
  ShowduinoOsTheme::enableVerticalScroll(s_scroll);

  lv_obj_t *local = make_section(s_scroll, "LOCAL OUTPUT - P4 AUDIO", 0, 168);
  s_local_status = lv_label_create(local);
  lv_label_set_text(s_local_status, "Status: UNKNOWN");
  lv_obj_set_pos(s_local_status, 14, 36);
  lv_obj_set_style_text_font(s_local_status, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_local_status, lv_color_hex(ShowduinoPalette::Text), 0);
  s_local_detail = make_body(local, 64, kBodyW - 40);

  lv_obj_t *nodes = make_section(s_scroll, "REMOTE AUDIO NODES", 176, 120);
  s_nodes = make_body(nodes, 40, kBodyW - 40);

  lv_obj_t *routing = make_section(s_scroll, "AUDIO ROUTING", 304, 100);
  s_routing = make_body(routing, 40, kBodyW - 40);

  lv_obj_t *cmd = make_section(s_scroll, "COMMAND STATUS", 412, 88);
  s_cmd = make_body(cmd, 40, kBodyW - 40);

  lv_obj_set_style_min_height(s_scroll, 520, 0);

  page_audio_system_apply_theme();
  s_active = true;
}

void page_audio_system_destroy(void) {
  if (!s_active && !s_root) return;
  showduino_theme_unregister(s_header_accent);
  showduino_theme_unregister(s_title);
  showduino_theme_unregister(s_btn_back);
  showduino_theme_unregister(s_strip);
  if (s_root) lv_obj_clean(s_root);
  s_root = s_header_accent = s_btn_back = s_title = s_header_status = nullptr;
  s_strip = s_scroll = s_local_status = s_local_detail = nullptr;
  s_nodes = s_routing = s_cmd = nullptr;
  s_cb = nullptr;
  s_active = false;
}

bool page_audio_system_is_active(void) { return s_active; }

void page_audio_system_apply_theme(void) {
  showduino_theme_apply();
  const lv_color_t accent = showduino_theme_get_accent();
  if (s_header_accent) lv_obj_set_style_bg_color(s_header_accent, accent, 0);
  if (s_btn_back) lv_obj_set_style_border_color(s_btn_back, accent, 0);
  if (s_strip) lv_obj_set_style_border_color(s_strip, accent, 0);
}

void page_audio_system_set_header(const char *status, uint32_t color) {
  ShowduinoOsTheme::setTextIfChanged(s_header_status, status ? status : "");
  if (s_header_status) lv_obj_set_style_text_color(s_header_status, lv_color_hex(color), 0);
}

void page_audio_system_set_local_status(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_local_status, text ? text : "");
}

void page_audio_system_set_local_detail(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_local_detail, text ? text : "NOT REPORTED");
}

void page_audio_system_set_nodes(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_nodes, text ? text : "NOT DETECTED");
}

void page_audio_system_set_routing(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_routing, text ? text : "NOT REPORTED");
}

void page_audio_system_set_command_status(const char *text) {
  ShowduinoOsTheme::setTextIfChanged(s_cmd, text ? text : "NOT REPORTED");
}
