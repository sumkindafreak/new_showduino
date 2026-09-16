#include "DirectorLocateScreen.h"
#include "DirectorAmbientPixels.h"
#include "ShowduinoOsPalette.h"
#include "backlight.h"

DirectorLocateScreen gDirectorLocateScreen;
ShowduinoDirectorLocateState gDirectorLocateState;

namespace {
constexpr uint32_t COL_BACKGROUND  = ShowduinoPalette::Background;
constexpr uint32_t COL_TEXT        = ShowduinoPalette::Text;
constexpr uint32_t COL_MUTED       = ShowduinoPalette::Muted;
constexpr uint32_t COL_WARN        = ShowduinoPalette::Warn;
constexpr uint32_t COL_DANGER      = ShowduinoPalette::Danger;
constexpr uint32_t COL_DANGER_DARK = ShowduinoPalette::DangerDark;
constexpr uint32_t COL_PANEL       = ShowduinoPalette::Panel;
constexpr uint32_t COL_PENDING     = ShowduinoPalette::Pending;
}

static void locateAckInternal(const char *why) {
  (void)why;
  if (!gDirectorLocateScreen.isVisible() &&
      !directorAmbientLocatorActive()) {
    return;
  }
  Serial.println("[LOCATOR] Acknowledged by touchscreen");
  gDirectorLocateScreen.hide();
  directorAmbientStopLocator();
  backlightLocateHold(false);
}

void directorLocateAcknowledge() {
  locateAckInternal("touch");
}

void directorLocateOnCommand(uint32_t nowMs) {
  Serial.println("[LOCATOR] Director locate request received");
  const bool fresh = showduino_director_locate_start(&gDirectorLocateState) != 0;
  const bool wasOff = !backlightIsOn() || backlightIsHeldOff();
  if (wasOff) Serial.println("[LOCATOR] Display wake forced");
  backlightLocateHold(true);
  if (!fresh) Serial.println("[LOCATOR] already active");
  directorAmbientStartLocator(nowMs);
  gDirectorLocateScreen.show(nowMs);
}

bool directorLocateOnTouch(int32_t x, int32_t y, bool pressed) {
  (void)x;
  (void)y;
  int acked = 0;
  const int consumed = showduino_director_locate_on_touch(&gDirectorLocateState, pressed ? 1 : 0, &acked);
  if (acked) directorLocateAcknowledge();
  return consumed != 0;
}

void directorLocateTick(uint32_t nowMs) {
  gDirectorLocateScreen.tick(nowMs);
}

bool directorLocateActive() {
  return showduino_director_locate_active(&gDirectorLocateState) != 0 ||
         gDirectorLocateScreen.isVisible();
}

void DirectorLocateScreen::show(uint32_t nowMs) {
  if (lv_display_get_default() == nullptr) return;
  shownMs_ = nowMs;
  if (visible_ && root_) {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root_);
    return;
  }
  destroy();
  buildUi();
  visible_ = true;
  Serial.println("[LOCATOR] LOCATE ACTIVE shown");
}

void DirectorLocateScreen::hide() {
  destroy();
}

void DirectorLocateScreen::tick(uint32_t nowMs) {
  if (!visible_ || !root_) return;
  lv_obj_move_foreground(root_);
  if (pulse_) {
    const lv_opa_t pulse = ((nowMs / 350UL) % 2UL) ? LV_OPA_COVER : LV_OPA_50;
    lv_obj_set_style_bg_opa(pulse_, pulse, 0);
  }
}

void DirectorLocateScreen::buildUi() {
  /* Sys layer sits above the Emergency overlay so Locate remains obvious. */
  root_ = lv_obj_create(lv_layer_sys());
  lv_obj_remove_style_all(root_);
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_set_size(root_, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(root_, lv_color_hex(COL_BACKGROUND), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_90, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *frame = lv_obj_create(root_);
  lv_obj_remove_style_all(frame);
  lv_obj_set_pos(frame, 24, 24);
  lv_obj_set_size(frame, SCREEN_WIDTH - 48, SCREEN_HEIGHT - 48);
  lv_obj_set_style_bg_color(frame, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(frame, lv_color_hex(COL_PENDING), 0);
  lv_obj_set_style_border_width(frame, 2, 0);
  lv_obj_set_style_radius(frame, 8, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *brand = lv_label_create(frame);
  lv_label_set_text(brand, "SHOWDUINO");
  lv_obj_set_style_text_color(brand, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
  lv_obj_set_pos(brand, 28, 22);

  lv_obj_t *kicker = lv_label_create(frame);
  lv_label_set_text(kicker, "DIRECTOR LOCATOR");
  lv_obj_set_style_text_color(kicker, lv_color_hex(COL_PENDING), 0);
  lv_obj_set_style_text_font(kicker, &lv_font_montserrat_14, 0);
  lv_obj_align(kicker, LV_ALIGN_TOP_RIGHT, -28, 26);

  pulse_ = lv_obj_create(frame);
  lv_obj_remove_style_all(pulse_);
  lv_obj_set_size(pulse_, 72, 72);
  lv_obj_set_pos(pulse_, 28, 78);
  lv_obj_set_style_bg_color(pulse_, lv_color_hex(COL_PENDING), 0);
  lv_obj_set_style_bg_opa(pulse_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(pulse_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(pulse_, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_border_width(pulse_, 3, 0);
  lv_obj_clear_flag(pulse_, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *title = lv_label_create(frame);
  lv_label_set_text(title, "LOCATE ACTIVE");
  lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
  lv_obj_set_pos(title, 120, 84);

  lv_obj_t *body = lv_label_create(frame);
  lv_label_set_text(body, "Director locator is active.\nTouch anywhere to acknowledge.");
  lv_obj_set_style_text_color(body, lv_color_hex(COL_MUTED), 0);
  lv_obj_set_style_text_font(body, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(body, 120, 140);
  lv_obj_set_width(body, 560);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);

  lv_obj_t *warn = lv_obj_create(frame);
  lv_obj_remove_style_all(warn);
  lv_obj_set_size(warn, SCREEN_WIDTH - 48 - 56, 72);
  lv_obj_align(warn, LV_ALIGN_BOTTOM_MID, 0, -28);
  lv_obj_set_style_bg_color(warn, lv_color_hex(COL_DANGER_DARK), 0);
  lv_obj_set_style_bg_opa(warn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(warn, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_border_width(warn, 1, 0);
  lv_obj_set_style_radius(warn, 6, 0);
  lv_obj_clear_flag(warn, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *warnTitle = lv_label_create(warn);
  lv_label_set_text(warnTitle, "EMERGENCY REMAINS ACTIVE.");
  lv_obj_set_style_text_color(warnTitle, lv_color_hex(COL_WARN), 0);
  lv_obj_set_style_text_font(warnTitle, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(warnTitle, 20, 12);

  lv_obj_t *warnBody = lv_label_create(warn);
  lv_label_set_text(warnBody, "Acknowledging Locate does not reset the attraction.");
  lv_obj_set_style_text_color(warnBody, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(warnBody, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(warnBody, 20, 40);

  lv_obj_move_foreground(root_);
}

void DirectorLocateScreen::destroy() {
  if (root_) {
    lv_obj_delete(root_);
    root_ = nullptr;
  }
  pulse_ = nullptr;
  visible_ = false;
}
