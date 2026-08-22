#include "DirectorEmergencyScreen.h"
#include "ShowduinoOsPalette.h"

DirectorEmergencyScreen gDirectorEmergencyScreen;

namespace {
constexpr uint32_t COL_GREEN       = ShowduinoPalette::Accent;
constexpr uint32_t COL_GREEN_DARK  = ShowduinoPalette::AccentDark;
constexpr uint32_t COL_GREEN_DIM   = ShowduinoPalette::AccentDim;
constexpr uint32_t COL_PANEL       = ShowduinoPalette::Panel;
constexpr uint32_t COL_BACKGROUND  = ShowduinoPalette::Background;
constexpr uint32_t COL_TEXT        = ShowduinoPalette::Text;
constexpr uint32_t COL_MUTED       = ShowduinoPalette::Muted;
constexpr uint32_t COL_WARN        = ShowduinoPalette::Warn;
constexpr uint32_t COL_DANGER      = ShowduinoPalette::Danger;
constexpr uint32_t COL_DANGER_DARK = ShowduinoPalette::DangerDark;
constexpr uint32_t COL_DANGER_PANEL = ShowduinoPalette::DangerPanel;
}

void DirectorEmergencyScreen::styleTransparent(lv_obj_t *obj) {
  if (!obj) return;
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

lv_obj_t *DirectorEmergencyScreen::makeLine(lv_obj_t *parent, int32_t x, int32_t y,
                                            int32_t w, int32_t h, uint32_t colour,
                                            lv_opa_t opacity) {
  lv_obj_t *line = lv_obj_create(parent);
  lv_obj_remove_style_all(line);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_size(line, w, h);
  lv_obj_set_style_bg_color(line, lv_color_hex(colour), 0);
  lv_obj_set_style_bg_opa(line, opacity, 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
  return line;
}

const char *DirectorEmergencyScreen::sourceText(Source source) {
  switch (source) {
    case Source::Director: return "TRIGGERED BY: DIRECTOR";
    case Source::Physical: return "TRIGGERED BY: PHYSICAL E-STOP";
    default: return "TRIGGERED BY: STAGE";
  }
}

const char *DirectorEmergencyScreen::linkText(uint8_t linkState) {
  if (linkState == LINK_READY) return "LINK: READY";
  if (linkState == LINK_DISCONNECTED) return "LINK: DISCONNECTED";
  return "LINK: SEARCHING";
}

void DirectorEmergencyScreen::show(uint32_t nowMs) {
  if (lv_display_get_default() == nullptr) return;

  if (visible_ && root_) {
    shownMs_ = nowMs;
    if (phase_ == Phase::ClearedHold) return;
    if (phase_ != Phase::ClearRejected) phase_ = Phase::Active;
    refreshCopy();
    refreshStatus();
    applyPhaseStyles();
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root_);
    return;
  }

  destroy();
  shownMs_ = nowMs;
  if (activeSinceMs_ == 0) activeSinceMs_ = nowMs;
  awaitingStage_ = false;
  phase_ = Phase::Active;
  buildUi();
  visible_ = true;
  refreshCopy();
  refreshStatus();
  applyPhaseStyles();
  Serial.println("[E-Stop] Emergency screen shown (LVGL)");
}

void DirectorEmergencyScreen::hide() {
  destroy();
}

void DirectorEmergencyScreen::setLatchActive(bool active, uint32_t nowMs) {
  const bool was = latchActive_;
  latchActive_ = active;
  if (active && !was) {
    physicalAsserted_ = false;
    awaitingStage_ = false;
    if (activeSinceMs_ == 0) activeSinceMs_ = nowMs;
    if (visible_ && phase_ != Phase::ClearRejected) phase_ = Phase::Active;
  }
  if (!active && was && visible_) {
    enterClearedHold(nowMs);
  }
  if (visible_) {
    refreshCopy();
    refreshStatus();
    applyPhaseStyles();
  }
}

void DirectorEmergencyScreen::setSource(Source source) {
  source_ = source;
  if (visible_) refreshCopy();
}

void DirectorEmergencyScreen::setShowName(const char *name) {
  if (name && name[0]) {
    strncpy(showName_, name, sizeof(showName_) - 1);
    showName_[sizeof(showName_) - 1] = '\0';
  } else {
    strncpy(showName_, "-", sizeof(showName_));
    showName_[sizeof(showName_) - 1] = '\0';
  }
  if (visible_) refreshStatus();
}

void DirectorEmergencyScreen::setActiveSince(uint32_t startedMs) {
  activeSinceMs_ = startedMs;
}

void DirectorEmergencyScreen::noteClearRequested(uint32_t nowMs) {
  if (!latchActive_) return;
  awaitingStage_ = true;
  lastClearMs_ = nowMs;
  if (visible_) {
    refreshCopy();
    applyPhaseStyles();
  }
}

void DirectorEmergencyScreen::noteClearRejected(uint32_t nowMs) {
  awaitingStage_ = false;
  physicalAsserted_ = true;
  source_ = Source::Physical;
  if (!visible_) show(nowMs);
  phase_ = Phase::ClearRejected;
  refreshCopy();
  refreshStatus();
  applyPhaseStyles();
  Serial.println("[E-Stop] CLEAR REJECTED shown — physical E-stop still asserted");
}

void DirectorEmergencyScreen::enterClearedHold(uint32_t nowMs) {
  awaitingStage_ = false;
  physicalAsserted_ = false;
  latchActive_ = false;
  phase_ = Phase::ClearedHold;
  clearedSinceMs_ = nowMs;
  refreshCopy();
  refreshStatus();
  applyPhaseStyles();
  Serial.println("[E-Stop] Stage confirmed CLEAR — holding then leaving screen");
}

void DirectorEmergencyScreen::tick(uint32_t nowMs, uint8_t linkState) {
  if (!visible_ || !root_) return;
  linkState_ = linkState;
  lv_obj_move_foreground(root_);

  if (warnMark_) {
    const lv_opa_t pulse = ((nowMs / 400UL) % 2UL) ? LV_OPA_COVER : LV_OPA_60;
    lv_obj_set_style_bg_opa(warnMark_, pulse, 0);
    lv_obj_set_style_shadow_opa(warnMark_, pulse == LV_OPA_COVER ? LV_OPA_50 : LV_OPA_20, 0);
  }

  if ((nowMs / 500UL) != ((nowMs - 30UL) / 500UL)) {
    refreshStatus();
  }

  if (phase_ == Phase::ClearedHold &&
      (nowMs - clearedSinceMs_) >= CLEARED_HOLD_MS) {
    FinishedFn done = finishedFn_;
    destroy();
    if (done) done();
  }
}

void DirectorEmergencyScreen::buildUi() {
  root_ = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(root_);
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_set_size(root_, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(root_, lv_color_hex(COL_BACKGROUND), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  /* Absorb stray taps so the desk underneath cannot fire. Not a CLEAR control. */
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(root_, onRootClicked, LV_EVENT_CLICKED, this);

  buildFrameDecorations();
  buildWarningMark();
  buildCopyAndStatus();
  buildClearControl();
  lv_obj_move_foreground(root_);
}

void DirectorEmergencyScreen::buildFrameDecorations() {
  lv_obj_t *frame = lv_obj_create(root_);
  lv_obj_remove_style_all(frame);
  lv_obj_set_pos(frame, 7, 7);
  lv_obj_set_size(frame, SCREEN_WIDTH - 14, SCREEN_HEIGHT - 14);
  lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(frame, lv_color_hex(COL_DANGER_DARK), 0);
  lv_obj_set_style_border_width(frame, 1, 0);
  lv_obj_set_style_radius(frame, 4, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

  makeLine(root_, 18, 18, 150, 2, COL_DANGER, LV_OPA_70);
  makeLine(root_, SCREEN_WIDTH - 168, 18, 150, 2, COL_DANGER, LV_OPA_70);
  makeLine(root_, 18, SCREEN_HEIGHT - 20, 150, 2, COL_DANGER, LV_OPA_50);
  makeLine(root_, SCREEN_WIDTH - 168, SCREEN_HEIGHT - 20, 150, 2, COL_DANGER, LV_OPA_50);
  makeLine(root_, 18, 18, 2, 64, COL_DANGER, LV_OPA_70);
  makeLine(root_, SCREEN_WIDTH - 20, 18, 2, 64, COL_DANGER, LV_OPA_70);
  makeLine(root_, 18, SCREEN_HEIGHT - 82, 2, 64, COL_DANGER, LV_OPA_50);
  makeLine(root_, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 82, 2, 64, COL_DANGER, LV_OPA_50);

  for (int i = 0; i < 7; ++i) {
    makeLine(root_, 27, 142 + i * 15, 4, 4, COL_DANGER, i < 5 ? LV_OPA_COVER : LV_OPA_40);
    makeLine(root_, SCREEN_WIDTH - 31, 142 + i * 15, 4, 4, COL_DANGER,
             i < 5 ? LV_OPA_COVER : LV_OPA_40);
  }

  lv_obj_t *brand = lv_label_create(root_);
  lv_label_set_text(brand, "SHOWDUINO");
  lv_obj_set_style_text_color(brand, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(brand, 34, 27);
  lv_obj_clear_flag(brand, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *tagline = lv_label_create(root_);
  lv_label_set_text(tagline, "THE MODULAR SHOW CONTROL ECOSYSTEM");
  lv_obj_set_style_text_color(tagline, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_text_font(tagline, &lv_font_montserrat_10, 0);
  lv_obj_set_pos(tagline, 35, 58);
  lv_obj_clear_flag(tagline, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *system = lv_label_create(root_);
  lv_label_set_text(system, "///  EMERGENCY STATE");
  lv_obj_set_style_text_color(system, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_text_font(system, &lv_font_montserrat_16, 0);
  lv_obj_align(system, LV_ALIGN_TOP_RIGHT, -34, 29);
  lv_obj_clear_flag(system, LV_OBJ_FLAG_CLICKABLE);
}

void DirectorEmergencyScreen::buildWarningMark() {
  lv_obj_t *markPanel = lv_obj_create(root_);
  lv_obj_remove_style_all(markPanel);
  lv_obj_set_size(markPanel, 168, 168);
  lv_obj_set_pos(markPanel, 48, 88);
  lv_obj_set_style_bg_color(markPanel, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(markPanel, LV_OPA_50, 0);
  lv_obj_set_style_border_color(markPanel, lv_color_hex(COL_DANGER_DARK), 0);
  lv_obj_set_style_border_width(markPanel, 1, 0);
  lv_obj_set_style_radius(markPanel, LV_RADIUS_CIRCLE, 0);
  lv_obj_clear_flag(markPanel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(markPanel, LV_OBJ_FLAG_SCROLLABLE);

  warnMark_ = lv_obj_create(markPanel);
  lv_obj_remove_style_all(warnMark_);
  lv_obj_set_size(warnMark_, 78, 78);
  lv_obj_align(warnMark_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(warnMark_, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_bg_opa(warnMark_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(warnMark_, lv_color_hex(0xFFB0B0), 0);
  lv_obj_set_style_border_width(warnMark_, 2, 0);
  lv_obj_set_style_radius(warnMark_, 8, 0);
  lv_obj_set_style_shadow_color(warnMark_, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_shadow_width(warnMark_, 22, 0);
  lv_obj_set_style_shadow_opa(warnMark_, LV_OPA_50, 0);
  lv_obj_clear_flag(warnMark_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(warnMark_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *bang = lv_label_create(warnMark_);
  lv_label_set_text(bang, "!");
  lv_obj_set_style_text_color(bang, lv_color_hex(COL_BACKGROUND), 0);
  lv_obj_set_style_text_font(bang, &lv_font_montserrat_36, 0);
  lv_obj_center(bang);
  lv_obj_clear_flag(bang, LV_OBJ_FLAG_CLICKABLE);
}

void DirectorEmergencyScreen::buildCopyAndStatus() {
  title_ = lv_label_create(root_);
  lv_label_set_text(title_, "EMERGENCY");
  lv_obj_set_style_text_color(title_, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(title_, &lv_font_montserrat_36, 0);
  lv_obj_set_pos(title_, 240, 96);
  lv_obj_clear_flag(title_, LV_OBJ_FLAG_CLICKABLE);

  subtitle_ = lv_label_create(root_);
  lv_label_set_text(subtitle_, "SHOW STOPPED");
  lv_obj_set_style_text_color(subtitle_, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_text_font(subtitle_, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(subtitle_, 242, 140);
  lv_obj_clear_flag(subtitle_, LV_OBJ_FLAG_CLICKABLE);

  explain_ = lv_label_create(root_);
  lv_obj_set_width(explain_, 500);
  lv_label_set_long_mode(explain_, LV_LABEL_LONG_WRAP);
  lv_label_set_text(explain_, "All show outputs have been placed into their emergency state.");
  lv_obj_set_style_text_color(explain_, lv_color_hex(COL_MUTED), 0);
  lv_obj_set_style_text_font(explain_, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(explain_, 242, 168);
  lv_obj_clear_flag(explain_, LV_OBJ_FLAG_CLICKABLE);

  sourceLabel_ = lv_label_create(root_);
  lv_label_set_text(sourceLabel_, sourceText(source_));
  lv_obj_set_style_text_color(sourceLabel_, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(sourceLabel_, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(sourceLabel_, 242, 214);
  lv_obj_clear_flag(sourceLabel_, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *infoBox = lv_obj_create(root_);
  lv_obj_remove_style_all(infoBox);
  lv_obj_set_size(infoBox, 704, 78);
  lv_obj_set_pos(infoBox, 48, 258);
  lv_obj_set_style_bg_color(infoBox, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(infoBox, LV_OPA_85, 0);
  lv_obj_set_style_border_color(infoBox, lv_color_hex(COL_DANGER_DARK), 0);
  lv_obj_set_style_border_width(infoBox, 1, 0);
  lv_obj_set_style_radius(infoBox, 6, 0);
  lv_obj_clear_flag(infoBox, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(infoBox, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *infoIcon = lv_label_create(infoBox);
  lv_label_set_text(infoIcon, "!");
  lv_obj_set_style_text_color(infoIcon, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_text_font(infoIcon, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(infoIcon, 22, 24);
  lv_obj_clear_flag(infoIcon, LV_OBJ_FLAG_CLICKABLE);

  statusPrimary_ = lv_label_create(infoBox);
  lv_obj_set_width(statusPrimary_, 630);
  lv_label_set_long_mode(statusPrimary_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(statusPrimary_, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(statusPrimary_, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(statusPrimary_, 58, 14);
  lv_obj_clear_flag(statusPrimary_, LV_OBJ_FLAG_CLICKABLE);

  statusSecondary_ = lv_label_create(infoBox);
  lv_obj_set_width(statusSecondary_, 630);
  lv_label_set_long_mode(statusSecondary_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(statusSecondary_, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_text_font(statusSecondary_, &lv_font_montserrat_12, 0);
  lv_obj_set_pos(statusSecondary_, 58, 42);
  lv_obj_clear_flag(statusSecondary_, LV_OBJ_FLAG_CLICKABLE);

  rejectBox_ = lv_obj_create(root_);
  lv_obj_remove_style_all(rejectBox_);
  lv_obj_set_size(rejectBox_, 704, 54);
  lv_obj_set_pos(rejectBox_, 48, 344);
  lv_obj_set_style_bg_color(rejectBox_, lv_color_hex(COL_DANGER_PANEL), 0);
  lv_obj_set_style_bg_opa(rejectBox_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(rejectBox_, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_border_width(rejectBox_, 1, 0);
  lv_obj_set_style_radius(rejectBox_, 6, 0);
  lv_obj_clear_flag(rejectBox_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(rejectBox_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(rejectBox_, LV_OBJ_FLAG_HIDDEN);

  rejectTitle_ = lv_label_create(rejectBox_);
  lv_label_set_text(rejectTitle_, "CLEAR REJECTED");
  lv_obj_set_style_text_color(rejectTitle_, lv_color_hex(COL_WARN), 0);
  lv_obj_set_style_text_font(rejectTitle_, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(rejectTitle_, 16, 6);
  lv_obj_clear_flag(rejectTitle_, LV_OBJ_FLAG_CLICKABLE);

  rejectBody_ = lv_label_create(rejectBox_);
  lv_label_set_text(rejectBody_, "Release the physical emergency button before clearing the emergency.");
  lv_obj_set_style_text_color(rejectBody_, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(rejectBody_, &lv_font_montserrat_12, 0);
  lv_obj_set_pos(rejectBody_, 16, 28);
  lv_obj_clear_flag(rejectBody_, LV_OBJ_FLAG_CLICKABLE);
}

void DirectorEmergencyScreen::buildClearControl() {
  clearBtn_ = lv_button_create(root_);
  lv_obj_remove_style_all(clearBtn_);
  lv_obj_set_size(clearBtn_, 420, 64);
  lv_obj_align(clearBtn_, LV_ALIGN_BOTTOM_MID, 0, -22);
  lv_obj_set_style_bg_color(clearBtn_, lv_color_hex(COL_DANGER_PANEL), 0);
  lv_obj_set_style_bg_opa(clearBtn_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(clearBtn_, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_border_width(clearBtn_, 2, 0);
  lv_obj_set_style_radius(clearBtn_, 6, 0);
  lv_obj_clear_flag(clearBtn_, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_add_flag(clearBtn_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(clearBtn_, onClearClicked, LV_EVENT_CLICKED, this);

  lv_obj_set_style_bg_color(clearBtn_, lv_color_hex(0x6B1212), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(clearBtn_, lv_color_hex(COL_WARN), LV_STATE_PRESSED);

  clearBtnLabel_ = lv_label_create(clearBtn_);
  lv_label_set_text(clearBtnLabel_, "CLEAR EMERGENCY");
  lv_obj_set_style_text_color(clearBtnLabel_, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(clearBtnLabel_, &lv_font_montserrat_16, 0);
  lv_obj_center(clearBtnLabel_);
  lv_obj_clear_flag(clearBtnLabel_, LV_OBJ_FLAG_CLICKABLE);
}

void DirectorEmergencyScreen::refreshCopy() {
  if (!title_ || !subtitle_ || !explain_ || !sourceLabel_) return;

  if (phase_ == Phase::ClearedHold) {
    lv_label_set_text(title_, "CLEARED");
    lv_label_set_text(subtitle_, "SAFE STATE");
    lv_obj_set_style_text_color(subtitle_, lv_color_hex(COL_GREEN), 0);
    lv_label_set_text(explain_, "Emergency latch released. The show has not been restarted.");
    lv_label_set_text(sourceLabel_, "Returning to the Director operating screen.");
    return;
  }

  lv_label_set_text(title_, "EMERGENCY");
  lv_label_set_text(subtitle_, "SHOW STOPPED");
  lv_obj_set_style_text_color(subtitle_, lv_color_hex(COL_DANGER), 0);
  lv_label_set_text(explain_, "All show outputs have been placed into their emergency state.");
  lv_label_set_text(sourceLabel_, sourceText(source_));
}

void DirectorEmergencyScreen::refreshStatus() {
  if (!statusPrimary_ || !statusSecondary_) return;

  char primary[128];
  char secondary[160];
  const char *physical =
      physicalAsserted_ ? "PHYSICAL E-STOP: ASSERTED" : "PHYSICAL E-STOP: NOT REPORTED";

  if (phase_ == Phase::ClearedHold) {
    snprintf(primary, sizeof(primary), "EMERGENCY CLEARED    SHOW: STOPPED    OUTPUTS: SAFE");
    snprintf(secondary, sizeof(secondary), "%s    %s", physical, linkText(linkState_));
  } else {
    snprintf(primary, sizeof(primary),
             "EMERGENCY ACTIVE    SHOW: STOPPED    OUTPUTS: EMERGENCY STATE");
    snprintf(secondary, sizeof(secondary), "%s    %s    SHOW: %s",
             physical, linkText(linkState_), showName_);
  }
  lv_label_set_text(statusPrimary_, primary);
  lv_label_set_text(statusSecondary_, secondary);
}

void DirectorEmergencyScreen::applyPhaseStyles() {
  if (!root_) return;

  if (rejectBox_) {
    if (phase_ == Phase::ClearRejected) lv_obj_clear_flag(rejectBox_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(rejectBox_, LV_OBJ_FLAG_HIDDEN);
  }

  if (!clearBtn_ || !clearBtnLabel_) return;

  if (phase_ == Phase::ClearedHold) {
    lv_obj_add_flag(clearBtn_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(clearBtn_, LV_STATE_DISABLED);
    return;
  }

  lv_obj_clear_flag(clearBtn_, LV_OBJ_FLAG_HIDDEN);
  if (awaitingStage_) {
    lv_obj_add_state(clearBtn_, LV_STATE_DISABLED);
    lv_label_set_text(clearBtnLabel_, "AWAITING STAGE");
    lv_obj_set_style_bg_opa(clearBtn_, LV_OPA_70, 0);
  } else {
    lv_obj_clear_state(clearBtn_, LV_STATE_DISABLED);
    lv_label_set_text(clearBtnLabel_, "CLEAR EMERGENCY");
    lv_obj_set_style_bg_opa(clearBtn_, LV_OPA_COVER, 0);
  }
}

void DirectorEmergencyScreen::handleClearClicked() {
  if (phase_ == Phase::ClearedHold || phase_ == Phase::Hidden) return;
  if (!latchActive_) return;
  if (awaitingStage_) return;

  const uint32_t now = millis();
  if (lastClearMs_ != 0 && (now - lastClearMs_) < CLEAR_COOLDOWN_MS) return;

  noteClearRequested(now);
  Serial.println("[E-Stop] CLEAR EMERGENCY pressed — requesting Stage");
  if (clearRequestFn_) clearRequestFn_();
}

void DirectorEmergencyScreen::onRootClicked(lv_event_t *event) {
  (void)event;
  /* Swallow accidental taps. CLEAR is a dedicated control. */
}

void DirectorEmergencyScreen::onClearClicked(lv_event_t *event) {
  DirectorEmergencyScreen *self =
      static_cast<DirectorEmergencyScreen *>(lv_event_get_user_data(event));
  if (self) self->handleClearClicked();
}

void DirectorEmergencyScreen::destroy() {
  if (root_) {
    lv_obj_delete(root_);
    root_ = nullptr;
  }
  warnMark_ = nullptr;
  title_ = nullptr;
  subtitle_ = nullptr;
  explain_ = nullptr;
  sourceLabel_ = nullptr;
  statusPrimary_ = nullptr;
  statusSecondary_ = nullptr;
  rejectTitle_ = nullptr;
  rejectBody_ = nullptr;
  rejectBox_ = nullptr;
  clearBtn_ = nullptr;
  clearBtnLabel_ = nullptr;
  visible_ = false;
  awaitingStage_ = false;
  if (phase_ != Phase::ClearedHold) phase_ = Phase::Hidden;
  else phase_ = Phase::Hidden;
}
