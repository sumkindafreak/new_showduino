#include "DirectorTouchCalibrationScreen.h"

#include "DirectorEmergencyScreen.h"
#include "DirectorLocateScreen.h"
#include "ShowduinoOsPalette.h"
#include "backlight.h"
#include "touch_lvgl.h"
#include "BoardConfig.h"
#include <string.h>

DirectorTouchCalibrationScreen gDirectorTouchCalibrationScreen;

namespace {
constexpr uint32_t COL_BG     = ShowduinoPalette::Background;
constexpr uint32_t COL_TEXT   = ShowduinoPalette::Text;
constexpr uint32_t COL_MUTED  = ShowduinoPalette::Muted;
constexpr uint32_t COL_ACCENT = ShowduinoPalette::Accent;
constexpr uint32_t COL_PANEL  = ShowduinoPalette::Panel;
constexpr uint32_t COL_DANGER = ShowduinoPalette::Danger;
constexpr int16_t kTargetD = 54;
constexpr int16_t kInnerD  = 28;
constexpr int16_t kDotD    = 8;
constexpr int kActCancel = 1;
constexpr int kActSave   = 2;
constexpr int kActRetry  = 3;
constexpr int kActReset  = 4;

bool inRect(int32_t x, int32_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && y >= ry && x < (rx + rw) && y < (ry + rh);
}

lv_obj_t *makeBtn(lv_obj_t *parent, const char *label, int16_t x, int16_t y,
                  int16_t w, int16_t h, uint32_t border) {
  lv_obj_t *btn = lv_obj_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(border), 0);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *lab = lv_label_create(btn);
  lv_label_set_text(lab, label);
  lv_obj_set_style_text_color(lab, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
  lv_obj_center(lab);
  return btn;
}
}

void DirectorTouchCalibrationScreen::buildUi() {
  root_ = lv_obj_create(lv_layer_sys());
  lv_obj_remove_style_all(root_);
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_set_size(root_, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(root_, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

  title_ = lv_label_create(root_);
  lv_label_set_text(title_, "TOUCHSCREEN CALIBRATION");
  lv_obj_set_style_text_color(title_, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(title_, &lv_font_montserrat_20, 0);
  lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 18);

  pointLab_ = lv_label_create(root_);
  lv_label_set_text(pointLab_, "Point 1 of 5");
  lv_obj_set_style_text_color(pointLab_, lv_color_hex(COL_ACCENT), 0);
  lv_obj_set_style_text_font(pointLab_, &lv_font_montserrat_16, 0);
  lv_obj_align(pointLab_, LV_ALIGN_TOP_MID, 0, 48);

  hint_ = lv_label_create(root_);
  lv_label_set_text(hint_, "Touch the centre of each target.\nUse the same finger you normally use to operate Showduino.");
  lv_obj_set_style_text_color(hint_, lv_color_hex(COL_MUTED), 0);
  lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(hint_, 640);
  lv_obj_align(hint_, LV_ALIGN_TOP_MID, 0, 78);

  targetOuter_ = lv_obj_create(root_);
  lv_obj_remove_style_all(targetOuter_);
  lv_obj_set_size(targetOuter_, kTargetD, kTargetD);
  lv_obj_set_style_radius(targetOuter_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(targetOuter_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(targetOuter_, 3, 0);
  lv_obj_set_style_border_color(targetOuter_, lv_color_hex(COL_ACCENT), 0);
  lv_obj_clear_flag(targetOuter_, LV_OBJ_FLAG_CLICKABLE);

  targetInner_ = lv_obj_create(root_);
  lv_obj_remove_style_all(targetInner_);
  lv_obj_set_size(targetInner_, kInnerD, kInnerD);
  lv_obj_set_style_radius(targetInner_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(targetInner_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(targetInner_, 2, 0);
  lv_obj_set_style_border_color(targetInner_, lv_color_hex(COL_TEXT), 0);
  lv_obj_clear_flag(targetInner_, LV_OBJ_FLAG_CLICKABLE);

  targetDot_ = lv_obj_create(root_);
  lv_obj_remove_style_all(targetDot_);
  lv_obj_set_size(targetDot_, kDotD, kDotD);
  lv_obj_set_style_radius(targetDot_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(targetDot_, lv_color_hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(targetDot_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(targetDot_, LV_OBJ_FLAG_CLICKABLE);

  ack_ = lv_label_create(root_);
  lv_label_set_text(ack_, LV_SYMBOL_OK);
  lv_obj_set_style_text_color(ack_, lv_color_hex(COL_ACCENT), 0);
  lv_obj_set_style_text_font(ack_, &lv_font_montserrat_28, 0);
  lv_obj_add_flag(ack_, LV_OBJ_FLAG_HIDDEN);

  for (uint8_t i = 0; i < 5; i++) {
    testDots_[i] = lv_obj_create(root_);
    lv_obj_remove_style_all(testDots_[i]);
    lv_obj_set_size(testDots_[i], 22, 22);
    lv_obj_set_style_radius(testDots_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(testDots_[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(testDots_[i], 2, 0);
    lv_obj_set_style_border_color(testDots_[i], lv_color_hex(COL_MUTED), 0);
    lv_obj_add_flag(testDots_[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(testDots_[i], LV_OBJ_FLAG_CLICKABLE);
    float sx, sy;
    showduino_touch_cal_target(i, SCREEN_WIDTH, SCREEN_HEIGHT, &sx, &sy);
    lv_obj_set_pos(testDots_[i], (int32_t)sx - 11, (int32_t)sy - 11);
  }

  marker_ = lv_obj_create(root_);
  lv_obj_remove_style_all(marker_);
  lv_obj_set_size(marker_, 16, 16);
  lv_obj_set_style_radius(marker_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(marker_, lv_color_hex(ShowduinoPalette::Pending), 0);
  lv_obj_set_style_bg_opa(marker_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(marker_, 2, 0);
  lv_obj_set_style_border_color(marker_, lv_color_hex(COL_TEXT), 0);
  lv_obj_add_flag(marker_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(marker_, LV_OBJ_FLAG_CLICKABLE);

  failBox_ = lv_obj_create(root_);
  lv_obj_remove_style_all(failBox_);
  lv_obj_set_size(failBox_, 640, 120);
  lv_obj_align(failBox_, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_bg_color(failBox_, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(failBox_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(failBox_, 2, 0);
  lv_obj_set_style_border_color(failBox_, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_radius(failBox_, 8, 0);
  lv_obj_add_flag(failBox_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(failBox_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *failTitle = lv_label_create(failBox_);
  lv_label_set_text(failTitle, "CALIBRATION FAILED");
  lv_obj_set_style_text_color(failTitle, lv_color_hex(COL_DANGER), 0);
  lv_obj_set_style_text_font(failTitle, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(failTitle, 24, 18);
  lv_obj_t *failBody = lv_label_create(failBox_);
  lv_label_set_text(failBody, "The touchscreen calibration could not be verified.\nYour previous calibration has not been changed.");
  lv_obj_set_style_text_color(failBody, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(failBody, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(failBody, 24, 50);
  lv_obj_set_width(failBody, 590);

  btnCaptureCancel_ = makeBtn(root_, "CANCEL", 16, 210, 124, 44, ShowduinoPalette::Muted);
  btnSave_ = makeBtn(root_, "SAVE CALIBRATION", 16, 160, 188, 44, COL_ACCENT);
  btnRetry_ = makeBtn(root_, "TRY AGAIN", 16, 214, 188, 44, ShowduinoPalette::Pending);
  btnCancel_ = makeBtn(root_, "CANCEL", 16, 268, 188, 44, ShowduinoPalette::Muted);
  btnReset_ = makeBtn(root_, "RESET", 16, 322, 188, 44, COL_DANGER);

  lv_obj_move_foreground(root_);
}

void DirectorTouchCalibrationScreen::destroy() {
  if (root_) {
    lv_obj_delete(root_);
    root_ = nullptr;
  }
  title_ = pointLab_ = hint_ = targetOuter_ = targetInner_ = targetDot_ = nullptr;
  ack_ = marker_ = failBox_ = nullptr;
  btnCaptureCancel_ = btnSave_ = btnRetry_ = btnCancel_ = btnReset_ = nullptr;
  memset(testDots_, 0, sizeof(testDots_));
  active_ = false;
  phase_ = Phase::Hidden;
}

void DirectorTouchCalibrationScreen::layoutTarget() {
  if (!targetOuter_) return;
  float sx, sy;
  showduino_touch_cal_target(pointIndex_, SCREEN_WIDTH, SCREEN_HEIGHT, &sx, &sy);
  const int32_t x = (int32_t)sx;
  const int32_t y = (int32_t)sy;
  lv_obj_set_pos(targetOuter_, x - kTargetD / 2, y - kTargetD / 2);
  lv_obj_set_pos(targetInner_, x - kInnerD / 2, y - kInnerD / 2);
  lv_obj_set_pos(targetDot_, x - kDotD / 2, y - kDotD / 2);
  lv_obj_set_pos(ack_, x - 14, y - 18);
}

void DirectorTouchCalibrationScreen::setPhase(Phase phase) {
  phase_ = phase;
  const bool cap = phase == Phase::Capture;
  const bool test = phase == Phase::Test;
  const bool fail = phase == Phase::Failed;
  const bool saved = phase == Phase::Saved;
  const bool reset = phase == Phase::ResetConfirm;
  auto hide = [](lv_obj_t *o, bool h) {
    if (!o) return;
    if (h) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
  };
  hide(targetOuter_, !cap);
  hide(targetInner_, !cap);
  hide(targetDot_, !cap);
  hide(failBox_, !fail);
  hide(marker_, true);
  for (uint8_t i = 0; i < 5; i++) hide(testDots_[i], !test);
  hide(btnCaptureCancel_, !cap);
  hide(btnSave_, !test);
  hide(btnRetry_, !(test || fail));
  hide(btnCancel_, !(test || fail || reset));
  hide(btnReset_, !reset);
  refreshCopy();
  (void)saved;
  layoutTarget();
}

void DirectorTouchCalibrationScreen::refreshCopy() {
  if (!title_ || !pointLab_ || !hint_) return;
  if (phase_ == Phase::Capture) {
    lv_label_set_text(title_, "TOUCHSCREEN CALIBRATION");
    char buf[40];
    snprintf(buf, sizeof(buf), "Point %u of 5", (unsigned)(pointIndex_ + 1));
    lv_label_set_text(pointLab_, buf);
    lv_label_set_text(hint_,
                      "Touch the centre of each target.\n"
                      "Use the same finger you normally use to operate Showduino.");
  } else if (phase_ == Phase::Test) {
    lv_label_set_text(title_, "CALIBRATION TEST");
    lv_label_set_text(pointLab_, "Check alignment");
    lv_label_set_text(hint_, "Touch the circles below to check alignment.\nA marker shows where Showduino sees the touch.");
  } else if (phase_ == Phase::Failed) {
    lv_label_set_text(title_, "TOUCHSCREEN CALIBRATION");
    lv_label_set_text(pointLab_, "Not saved");
    lv_label_set_text(hint_, "");
  } else if (phase_ == Phase::Saved) {
    lv_label_set_text(title_, "CALIBRATION SAVED");
    lv_label_set_text(pointLab_, "Touch alignment is ready.");
    lv_label_set_text(hint_, "The new calibration is active now and will load automatically at every boot.");
  } else if (phase_ == Phase::ResetConfirm) {
    lv_label_set_text(title_, "RESET TOUCH CALIBRATION?");
    lv_label_set_text(pointLab_, "Factory mapping");
    lv_label_set_text(hint_,
                      "This will remove the saved touchscreen calibration\n"
                      "and restore the factory touch mapping.");
  }
}

void DirectorTouchCalibrationScreen::enterCapture(uint32_t nowMs) {
  (void)nowMs;
  pointIndex_ = 0;
  acceptedThisPress_ = false;
  waitingRelease_ = true;
  ignoreSample_ = false;
  showduino_touch_cal_sample_reset(&samples_);
  memset(points_, 0, sizeof(points_));
  setPhase(Phase::Capture);
  layoutTarget();
  Serial.println("[TouchCal] started");
}

void DirectorTouchCalibrationScreen::start(uint32_t nowMs) {
  if (gDirectorEmergencyScreen.isVisible() || directorLocateActive()) return;
  if (active_ && root_) {
    enterCapture(nowMs);
    backlightCalHold(true);
    return;
  }
  destroy();
  buildUi();
  active_ = true;
  backlightCalHold(true);
  enterCapture(nowMs);
}

void DirectorTouchCalibrationScreen::startResetConfirm(uint32_t nowMs) {
  (void)nowMs;
  if (gDirectorEmergencyScreen.isVisible() || directorLocateActive()) return;
  destroy();
  buildUi();
  active_ = true;
  backlightCalHold(true);
  waitingRelease_ = true;
  setPhase(Phase::ResetConfirm);
}

void DirectorTouchCalibrationScreen::hide() {
  const bool was = active_;
  destroy();
  if (was) backlightCalHold(false);
}

void DirectorTouchCalibrationScreen::abortUnsaved(const char *why) {
  Serial.printf("[TouchCal] cancelled%s%s\n", why && why[0] ? " — " : "", why ? why : "");
  hide();
}

void DirectorTouchCalibrationScreen::doCancel() {
  abortUnsaved("");
}

void DirectorTouchCalibrationScreen::doReset() {
  touchLvglResetCalibration();
  hide();
}

void DirectorTouchCalibrationScreen::doSave() {
  if (!touchLvglSaveCalibration(pending_)) {
    setPhase(Phase::Failed);
    return;
  }
  setPhase(Phase::Saved);
  savedUntilMs_ = millis() + 1200UL;
}

void DirectorTouchCalibrationScreen::onPointAccepted(int32_t rawX, int32_t rawY) {
  float sx, sy;
  showduino_touch_cal_target(pointIndex_, SCREEN_WIDTH, SCREEN_HEIGHT, &sx, &sy);
  points_[pointIndex_].rawX = (float)rawX;
  points_[pointIndex_].rawY = (float)rawY;
  points_[pointIndex_].screenX = sx;
  points_[pointIndex_].screenY = sy;
  Serial.printf("[TouchCal] point %u captured raw=(%ld,%ld)\n",
                (unsigned)(pointIndex_ + 1), (long)rawX, (long)rawY);
  acceptedThisPress_ = true;
  waitingRelease_ = true;
  ackUntilMs_ = millis() + 280UL;
  if (ack_) lv_obj_clear_flag(ack_, LV_OBJ_FLAG_HIDDEN);
}

void DirectorTouchCalibrationScreen::finishFit() {
  const int rc = showduino_touch_cal_fit(points_, SHOWDUINO_TOUCH_CAL_POINT_N,
                                         SCREEN_WIDTH, SCREEN_HEIGHT, &pending_);
  if (rc != SHOWDUINO_TOUCH_CAL_OK) {
    Serial.println("[TouchCal] invalid — previous calibration preserved");
    setPhase(Phase::Failed);
    return;
  }
  const double rms = showduino_touch_cal_residual_rms(&pending_, points_,
                                                      SHOWDUINO_TOUCH_CAL_POINT_N);
  Serial.printf("[TouchCal] solution valid residual=%.2f\n", rms);
  setPhase(Phase::Test);
}

int DirectorTouchCalibrationScreen::hitAction(int32_t x, int32_t y) const {
  if (phase_ == Phase::Capture) {
    if (inRect(x, y, 16, 210, 124, 44)) return kActCancel;
  } else if (phase_ == Phase::Test) {
    if (inRect(x, y, 16, 160, 188, 44)) return kActSave;
    if (inRect(x, y, 16, 214, 188, 44)) return kActRetry;
    if (inRect(x, y, 16, 268, 188, 44)) return kActCancel;
  } else if (phase_ == Phase::Failed) {
    if (inRect(x, y, 16, 214, 188, 44)) return kActRetry;
    if (inRect(x, y, 16, 268, 188, 44)) return kActCancel;
  } else if (phase_ == Phase::ResetConfirm) {
    if (inRect(x, y, 16, 268, 188, 44)) return kActCancel;
    if (inRect(x, y, 16, 322, 188, 44)) return kActReset;
  }
  return 0;
}

bool DirectorTouchCalibrationScreen::onTouch(int32_t x, int32_t y, bool pressed) {
  if (!active_) return false;
  if (pressed && !pressed_) {
    pressX_ = x;
    pressY_ = y;
    actionDown_ = hitAction(x, y);
    if (actionDown_ != 0) ignoreSample_ = true;
  }
  if (!pressed && pressed_) {
    const int up = hitAction(x, y);
    const int act = (up != 0 && up == actionDown_) ? up : 0;
    actionDown_ = 0;
    if (act == kActCancel) doCancel();
    else if (act == kActRetry) start(millis());
    else if (act == kActSave) doSave();
    else if (act == kActReset) doReset();
  }
  pressed_ = pressed;
  if (!pressed) ignoreSample_ = false;
  return true;
}

void DirectorTouchCalibrationScreen::tick(uint32_t nowMs) {
  if (!active_) return;
  if (gDirectorEmergencyScreen.isVisible() || directorLocateActive()) {
    abortUnsaved("safety");
    return;
  }
  if (root_) lv_obj_move_foreground(root_);

  if (phase_ == Phase::Saved) {
    if ((int32_t)(nowMs - savedUntilMs_) >= 0) hide();
    return;
  }

  if (ack_ && ackUntilMs_ && (int32_t)(nowMs - ackUntilMs_) >= 0) {
    lv_obj_add_flag(ack_, LV_OBJ_FLAG_HIDDEN);
    ackUntilMs_ = 0;
  }

  TouchRawPoint raw;
  const bool down = touchLvglReadRaw(raw);

  if (phase_ == Phase::Test && down) {
    int32_t sx = 0, sy = 0;
    touchLvglMapWith(&pending_, raw.x, raw.y, &sx, &sy);
    if (marker_) {
      lv_obj_set_pos(marker_, sx - 8, sy - 8);
      lv_obj_clear_flag(marker_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (phase_ != Phase::Capture) {
    if (!down) waitingRelease_ = false;
    return;
  }

  if (waitingRelease_) {
    if (!down) {
      waitingRelease_ = false;
      if (acceptedThisPress_) {
        acceptedThisPress_ = false;
        if (pointIndex_ + 1 >= SHOWDUINO_TOUCH_CAL_POINT_N) finishFit();
        else {
          pointIndex_++;
          showduino_touch_cal_sample_reset(&samples_);
          refreshCopy();
          layoutTarget();
        }
      }
    }
    return;
  }

  if (!down) {
    showduino_touch_cal_sample_reset(&samples_);
    return;
  }
  if (ignoreSample_) return;

  showduino_touch_cal_sample_add(&samples_, raw.x, raw.y);
  if (samples_.n >= SHOWDUINO_TOUCH_CAL_SAMPLES) {
    int32_t mx = 0, my = 0;
    showduino_touch_cal_sample_median(&samples_, &mx, &my);
    onPointAccepted(mx, my);
    showduino_touch_cal_sample_reset(&samples_);
  }
}

void directorTouchCalStart() {
  gDirectorTouchCalibrationScreen.start(millis());
}

void directorTouchCalStartReset() {
  gDirectorTouchCalibrationScreen.startResetConfirm(millis());
}

void directorTouchCalTick(uint32_t nowMs) {
  gDirectorTouchCalibrationScreen.tick(nowMs);
}

bool directorTouchCalActive() {
  return gDirectorTouchCalibrationScreen.isActive();
}

bool directorTouchCalOnTouch(int32_t x, int32_t y, bool pressed) {
  return gDirectorTouchCalibrationScreen.onTouch(x, y, pressed);
}

void directorTouchCalAbortForSafety() {
  if (gDirectorTouchCalibrationScreen.isActive()) {
    gDirectorTouchCalibrationScreen.hide();
    Serial.println("[TouchCal] cancelled — safety");
  }
}
