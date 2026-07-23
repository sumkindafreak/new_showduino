#include "DirectorUnlockScreen.h"
#include "BoardConfig.h"

DirectorUnlockScreen gDirectorUnlockScreen;

namespace {
constexpr uint32_t COL_GREEN       = 0x84FF22;
constexpr uint32_t COL_GREEN_DARK  = 0x2E690D;
constexpr uint32_t COL_GREEN_DIM   = 0x17370B;
constexpr uint32_t COL_PANEL       = 0x071007;
constexpr uint32_t COL_BACKGROUND  = 0x020502;
constexpr uint32_t COL_TEXT        = 0xF3F6F1;
constexpr uint32_t COL_MUTED       = 0xA9B4A4;
constexpr uint32_t COL_WARN        = 0xFFD54A;
constexpr uint32_t COL_DANGER      = 0xFF4545;

constexpr uint32_t STEP_INTERVAL_MS = 420UL;
constexpr uint32_t MIN_VISIBLE_MS   = 4200UL;
constexpr uint32_t LINK_WAIT_MS     = 6500UL;
constexpr uint32_t EXIT_HOLD_MS     = 900UL;

const char *STEP_STATUS[DirectorUnlockScreen::STEP_COUNT] = {
  "INITIALISING DISPLAY",
  "LOADING SHOWDUINO OS",
  "CHECKING STORAGE",
  "STARTING COMMUNICATIONS",
  "DISCOVERING SUE",
  "SYNCHRONISING SYSTEM CLOCK",
  "CONNECTING TO IAN",
  "CHECKING EMERGENCY STATE",
  "VERIFYING CREDENTIALS"
};

const char *STEP_PRIMARY[DirectorUnlockScreen::STEP_COUNT] = {
  "Preparing the Director control surface.",
  "Loading the Showduino OS operator shell.",
  "Checking shows, settings and local assets.",
  "Starting ESP-NOW and service communications.",
  "Looking for the SUE communications controller.",
  "Requesting authoritative time from SUE.",
  "Waiting for the IAN Show Engine.",
  "Confirming the stage is safe to operate.",
  "Unlocking the Director system."
};

const char *STEP_SECONDARY[DirectorUnlockScreen::STEP_COUNT] = {
  "Display and touch services online.",
  "Desktop modules are being prepared.",
  "Recovery mode remains available if required.",
  "The network fabric is coming online.",
  "ESP-NOW discovery is active.",
  "The Director does not invent its own show clock.",
  "Runtime state will be mirrored from the stage.",
  "Emergency control always remains available.",
  "This will only take a moment."
};
}

void DirectorUnlockScreen::styleTransparent(lv_obj_t *obj) {
  if (!obj) return;
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

lv_obj_t *DirectorUnlockScreen::makeLine(lv_obj_t *parent, int32_t x, int32_t y,
                                         int32_t w, int32_t h, uint32_t colour,
                                         lv_opa_t opacity) {
  lv_obj_t *line = lv_obj_create(parent);
  lv_obj_remove_style_all(line);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_size(line, w, h);
  lv_obj_set_style_bg_color(line, lv_color_hex(colour), 0);
  lv_obj_set_style_bg_opa(line, opacity, 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
  return line;
}

void DirectorUnlockScreen::begin(uint32_t nowMs) {
  if (visible_ || finished_) return;
  if (lv_display_get_default() == nullptr) return;

  startedMs_ = nowMs;
  currentStep_ = 0;
  readySinceMs_ = 0;
  finalStateApplied_ = false;
  buildUi();
  visible_ = true;
  setStep(0, STEP_STATUS[0], STEP_PRIMARY[0], STEP_SECONDARY[0]);
  Serial.println("[UnlockUI] Showduino OS verification overlay active");
}

void DirectorUnlockScreen::buildUi() {
  root_ = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(root_);
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_set_size(root_, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(root_, lv_color_hex(COL_BACKGROUND), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

  buildFrameDecorations();
  buildScanner();
  buildTextAndProgress();
}

void DirectorUnlockScreen::buildFrameDecorations() {
  lv_obj_t *frame = lv_obj_create(root_);
  lv_obj_remove_style_all(frame);
  lv_obj_set_pos(frame, 7, 7);
  lv_obj_set_size(frame, SCREEN_WIDTH - 14, SCREEN_HEIGHT - 14);
  lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(frame, lv_color_hex(COL_GREEN_DARK), 0);
  lv_obj_set_style_border_width(frame, 1, 0);
  lv_obj_set_style_radius(frame, 4, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

  makeLine(root_, 18, 18, 150, 2, COL_GREEN, LV_OPA_70);
  makeLine(root_, SCREEN_WIDTH - 168, 18, 150, 2, COL_GREEN, LV_OPA_70);
  makeLine(root_, 18, SCREEN_HEIGHT - 20, 150, 2, COL_GREEN, LV_OPA_50);
  makeLine(root_, SCREEN_WIDTH - 168, SCREEN_HEIGHT - 20, 150, 2, COL_GREEN, LV_OPA_50);
  makeLine(root_, 18, 18, 2, 64, COL_GREEN, LV_OPA_70);
  makeLine(root_, SCREEN_WIDTH - 20, 18, 2, 64, COL_GREEN, LV_OPA_70);
  makeLine(root_, 18, SCREEN_HEIGHT - 82, 2, 64, COL_GREEN, LV_OPA_50);
  makeLine(root_, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 82, 2, 64, COL_GREEN, LV_OPA_50);

  for (int i = 0; i < 7; ++i) {
    makeLine(root_, 27, 142 + i * 15, 4, 4, COL_GREEN, i < 5 ? LV_OPA_COVER : LV_OPA_40);
    makeLine(root_, SCREEN_WIDTH - 31, 142 + i * 15, 4, 4, COL_GREEN,
             i < 5 ? LV_OPA_COVER : LV_OPA_40);
  }

  lv_obj_t *brand = lv_label_create(root_);
  lv_label_set_text(brand, "SHOWDUINO");
  lv_obj_set_style_text_color(brand, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(brand, 34, 27);

  lv_obj_t *tagline = lv_label_create(root_);
  lv_label_set_text(tagline, "THE MODULAR SHOW CONTROL ECOSYSTEM");
  lv_obj_set_style_text_color(tagline, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_text_font(tagline, &lv_font_montserrat_10, 0);
  lv_obj_set_pos(tagline, 35, 58);

  lv_obj_t *system = lv_label_create(root_);
  lv_label_set_text(system, "///  DIRECTOR SYSTEM");
  lv_obj_set_style_text_color(system, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_text_font(system, &lv_font_montserrat_16, 0);
  lv_obj_align(system, LV_ALIGN_TOP_RIGHT, -34, 29);

  /* Subtle technical grid: inexpensive horizontal/vertical guide lines. */
  for (int x = 250; x <= 550; x += 50) {
    makeLine(root_, x, 68, 1, 250, COL_GREEN_DIM, LV_OPA_20);
  }
  for (int y = 78; y <= 310; y += 38) {
    makeLine(root_, 180, y, 440, 1, COL_GREEN_DIM, LV_OPA_20);
  }
}

void DirectorUnlockScreen::buildScanner() {
  lv_obj_t *scannerPanel = lv_obj_create(root_);
  lv_obj_remove_style_all(scannerPanel);
  lv_obj_set_size(scannerPanel, 250, 250);
  lv_obj_align(scannerPanel, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_set_style_bg_color(scannerPanel, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(scannerPanel, LV_OPA_50, 0);
  lv_obj_set_style_border_color(scannerPanel, lv_color_hex(COL_GREEN_DIM), 0);
  lv_obj_set_style_border_width(scannerPanel, 1, 0);
  lv_obj_set_style_radius(scannerPanel, LV_RADIUS_CIRCLE, 0);
  lv_obj_clear_flag(scannerPanel, LV_OBJ_FLAG_SCROLLABLE);

  scannerOuter_ = lv_arc_create(scannerPanel);
  lv_obj_set_size(scannerOuter_, 226, 226);
  lv_obj_center(scannerOuter_);
  lv_arc_set_bg_angles(scannerOuter_, 0, 360);
  lv_arc_set_angles(scannerOuter_, 18, 132);
  lv_obj_set_style_arc_width(scannerOuter_, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_color(scannerOuter_, lv_color_hex(COL_GREEN_DIM), LV_PART_MAIN);
  lv_obj_set_style_arc_width(scannerOuter_, 7, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(scannerOuter_, lv_color_hex(COL_GREEN), LV_PART_INDICATOR);
  lv_obj_remove_style(scannerOuter_, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(scannerOuter_, LV_OBJ_FLAG_CLICKABLE);

  scannerMiddle_ = lv_arc_create(scannerPanel);
  lv_obj_set_size(scannerMiddle_, 188, 188);
  lv_obj_center(scannerMiddle_);
  lv_arc_set_bg_angles(scannerMiddle_, 0, 360);
  lv_arc_set_angles(scannerMiddle_, 205, 310);
  lv_obj_set_style_arc_width(scannerMiddle_, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_color(scannerMiddle_, lv_color_hex(COL_GREEN_DIM), LV_PART_MAIN);
  lv_obj_set_style_arc_width(scannerMiddle_, 4, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(scannerMiddle_, lv_color_hex(0xB5FF6E), LV_PART_INDICATOR);
  lv_obj_remove_style(scannerMiddle_, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(scannerMiddle_, LV_OBJ_FLAG_CLICKABLE);

  scannerInner_ = lv_arc_create(scannerPanel);
  lv_obj_set_size(scannerInner_, 150, 150);
  lv_obj_center(scannerInner_);
  lv_arc_set_bg_angles(scannerInner_, 0, 360);
  lv_arc_set_angles(scannerInner_, 330, 55);
  lv_obj_set_style_arc_width(scannerInner_, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_color(scannerInner_, lv_color_hex(COL_GREEN_DIM), LV_PART_MAIN);
  lv_obj_set_style_arc_width(scannerInner_, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(scannerInner_, lv_color_hex(COL_GREEN), LV_PART_INDICATOR);
  lv_obj_remove_style(scannerInner_, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(scannerInner_, LV_OBJ_FLAG_CLICKABLE);

  /* Lock made entirely from LVGL primitives to avoid external image assets. */
  lockBody_ = lv_obj_create(scannerPanel);
  lv_obj_remove_style_all(lockBody_);
  lv_obj_set_size(lockBody_, 68, 61);
  lv_obj_align(lockBody_, LV_ALIGN_CENTER, 0, 17);
  lv_obj_set_style_bg_color(lockBody_, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_bg_opa(lockBody_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(lockBody_, lv_color_hex(0xD0FF9D), 0);
  lv_obj_set_style_border_width(lockBody_, 2, 0);
  lv_obj_set_style_radius(lockBody_, 7, 0);
  lv_obj_set_style_shadow_color(lockBody_, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_shadow_width(lockBody_, 20, 0);
  lv_obj_set_style_shadow_opa(lockBody_, LV_OPA_50, 0);

  lv_obj_t *keyHole = lv_obj_create(lockBody_);
  lv_obj_remove_style_all(keyHole);
  lv_obj_set_size(keyHole, 13, 13);
  lv_obj_align(keyHole, LV_ALIGN_CENTER, 0, -2);
  lv_obj_set_style_bg_color(keyHole, lv_color_hex(COL_BACKGROUND), 0);
  lv_obj_set_style_bg_opa(keyHole, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(keyHole, LV_RADIUS_CIRCLE, 0);
  makeLine(lockBody_, 31, 30, 6, 17, COL_BACKGROUND, LV_OPA_COVER);

  lockShackle_ = lv_arc_create(scannerPanel);
  lv_obj_set_size(lockShackle_, 56, 56);
  lv_obj_align(lockShackle_, LV_ALIGN_CENTER, 0, -33);
  lv_arc_set_bg_angles(lockShackle_, 195, 345);
  lv_arc_set_angles(lockShackle_, 195, 345);
  lv_obj_set_style_arc_width(lockShackle_, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_color(lockShackle_, lv_color_hex(COL_GREEN), LV_PART_MAIN);
  lv_obj_set_style_arc_width(lockShackle_, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(lockShackle_, lv_color_hex(COL_GREEN), LV_PART_INDICATOR);
  lv_obj_remove_style(lockShackle_, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(lockShackle_, LV_OBJ_FLAG_CLICKABLE);
}

void DirectorUnlockScreen::buildTextAndProgress() {
  title_ = lv_label_create(root_);
  lv_label_set_text(title_, "UNLOCKING");
  lv_obj_set_style_text_color(title_, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(title_, &lv_font_montserrat_36, 0);
  lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 286);

  status_ = lv_label_create(root_);
  lv_label_set_text(status_, "INITIALISING DISPLAY");
  lv_obj_set_style_text_color(status_, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_text_font(status_, &lv_font_montserrat_14, 0);
  lv_obj_align(status_, LV_ALIGN_TOP_MID, 0, 330);

  lv_obj_t *dotRow = lv_obj_create(root_);
  lv_obj_remove_style_all(dotRow);
  lv_obj_set_size(dotRow, 220, 18);
  lv_obj_align(dotRow, LV_ALIGN_TOP_MID, 0, 356);
  lv_obj_set_flex_flow(dotRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dotRow, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  for (uint8_t i = 0; i < STEP_COUNT; ++i) {
    dots_[i] = lv_obj_create(dotRow);
    lv_obj_remove_style_all(dots_[i]);
    lv_obj_set_size(dots_[i], 13, 13);
    lv_obj_set_style_radius(dots_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dots_[i], lv_color_hex(COL_GREEN_DIM), 0);
    lv_obj_set_style_bg_opa(dots_[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dots_[i], lv_color_hex(COL_GREEN_DARK), 0);
    lv_obj_set_style_border_width(dots_[i], 1, 0);
  }

  lv_obj_t *infoBox = lv_obj_create(root_);
  lv_obj_remove_style_all(infoBox);
  lv_obj_set_size(infoBox, 500, 68);
  lv_obj_align(infoBox, LV_ALIGN_BOTTOM_MID, 0, -25);
  lv_obj_set_style_bg_color(infoBox, lv_color_hex(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(infoBox, LV_OPA_85, 0);
  lv_obj_set_style_border_color(infoBox, lv_color_hex(COL_GREEN_DARK), 0);
  lv_obj_set_style_border_width(infoBox, 1, 0);
  lv_obj_set_style_radius(infoBox, 6, 0);
  lv_obj_clear_flag(infoBox, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *infoIcon = lv_label_create(infoBox);
  lv_label_set_text(infoIcon, "i");
  lv_obj_set_style_text_color(infoIcon, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_text_font(infoIcon, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(infoIcon, 24, 18);

  infoPrimary_ = lv_label_create(infoBox);
  lv_obj_set_width(infoPrimary_, 420);
  lv_label_set_long_mode(infoPrimary_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(infoPrimary_, lv_color_hex(COL_TEXT), 0);
  lv_obj_set_style_text_font(infoPrimary_, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(infoPrimary_, 66, 12);

  infoSecondary_ = lv_label_create(infoBox);
  lv_obj_set_width(infoSecondary_, 420);
  lv_label_set_long_mode(infoSecondary_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(infoSecondary_, lv_color_hex(COL_GREEN), 0);
  lv_obj_set_style_text_font(infoSecondary_, &lv_font_montserrat_12, 0);
  lv_obj_set_pos(infoSecondary_, 66, 37);
}

void DirectorUnlockScreen::setStep(uint8_t step, const char *status,
                                   const char *primary, const char *secondary) {
  if (!visible_ || !root_) return;
  if (step >= STEP_COUNT) step = STEP_COUNT - 1;
  currentStep_ = step;

  lv_label_set_text(status_, status ? status : "VERIFYING");
  lv_label_set_text(infoPrimary_, primary ? primary : "Please wait.");
  lv_label_set_text(infoSecondary_, secondary ? secondary : "");

  for (uint8_t i = 0; i < STEP_COUNT; ++i) {
    const bool complete = i <= currentStep_;
    lv_obj_set_style_bg_color(dots_[i],
                              lv_color_hex(complete ? COL_GREEN : COL_GREEN_DIM), 0);
    lv_obj_set_style_border_color(dots_[i],
                                  lv_color_hex(complete ? 0xC7FF8D : COL_GREEN_DARK), 0);
    lv_obj_set_style_shadow_width(dots_[i], complete ? 8 : 0, 0);
    lv_obj_set_style_shadow_color(dots_[i], lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_shadow_opa(dots_[i], complete ? LV_OPA_40 : LV_OPA_TRANSP, 0);
  }
}

void DirectorUnlockScreen::applyFinalState(bool stageLinked, bool emergencyLocked,
                                           uint32_t nowMs) {
  if (finalStateApplied_) return;
  finalStateApplied_ = true;
  readySinceMs_ = nowMs;

  if (emergencyLocked) {
    lv_label_set_text(title_, "SAFETY LOCK");
    lv_label_set_text(status_, "EMERGENCY STATE ACTIVE");
    lv_obj_set_style_text_color(status_, lv_color_hex(COL_DANGER), 0);
    lv_label_set_text(infoPrimary_, "Director started with an active emergency lock.");
    lv_label_set_text(infoSecondary_, "The emergency controls remain available on the desktop.");
    return;
  }

  if (stageLinked) {
    lv_label_set_text(title_, "UNLOCKED");
    lv_label_set_text(status_, "OPERATION COMPLETE");
    lv_label_set_text(infoPrimary_, "Showduino Director is ready.");
    lv_label_set_text(infoSecondary_, "SUE and the Show Engine are responding.");
  } else {
    lv_label_set_text(title_, "LOCAL MODE");
    lv_label_set_text(status_, "STAGE CONNECTION PENDING");
    lv_obj_set_style_text_color(status_, lv_color_hex(COL_WARN), 0);
    lv_label_set_text(infoPrimary_, "Director is ready while stage discovery continues.");
    lv_label_set_text(infoSecondary_, "Controls will update automatically when SUE responds.");
  }
}

void DirectorUnlockScreen::tick(uint32_t nowMs, bool espNowReady,
                                uint8_t linkState, bool emergencyLocked) {
  if (finished_) return;
  if (!visible_) {
    begin(nowMs);
    if (!visible_) return;
  }

  const uint32_t elapsed = nowMs - startedMs_;

  /* Scanner motion is updated manually: predictable and inexpensive on ESP32-S3. */
  lv_arc_set_rotation(scannerOuter_, (uint16_t)((elapsed / 9UL) % 360UL));
  lv_arc_set_rotation(scannerMiddle_, (uint16_t)(360UL - ((elapsed / 13UL) % 360UL)));
  lv_arc_set_rotation(scannerInner_, (uint16_t)((elapsed / 6UL) % 360UL));

  const lv_opa_t pulse = ((elapsed / 320UL) % 2UL) ? LV_OPA_COVER : LV_OPA_70;
  lv_obj_set_style_bg_opa(lockBody_, pulse, 0);
  lv_obj_set_style_arc_opa(lockShackle_, pulse, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(lockShackle_, pulse, LV_PART_INDICATOR);

  uint8_t timedStep = (uint8_t)(elapsed / STEP_INTERVAL_MS);
  if (timedStep >= STEP_COUNT) timedStep = STEP_COUNT - 1;
  if (!finalStateApplied_ && timedStep != currentStep_) {
    setStep(timedStep, STEP_STATUS[timedStep], STEP_PRIMARY[timedStep], STEP_SECONDARY[timedStep]);
  }

  const bool stageLinked = (linkState == LINK_READY);
  const bool minimumShown = elapsed >= MIN_VISIBLE_MS;
  const bool linkDecisionReady = stageLinked || elapsed >= LINK_WAIT_MS;

  if (!finalStateApplied_ && minimumShown && linkDecisionReady) {
    /* ESP-NOW failure is represented honestly by the local/pending final state. */
    (void)espNowReady;
    applyFinalState(stageLinked, emergencyLocked, nowMs);
  }

  if (finalStateApplied_ && (nowMs - readySinceMs_) >= EXIT_HOLD_MS) {
    destroy();
  }
}

void DirectorUnlockScreen::destroy() {
  if (root_) {
    lv_obj_delete(root_);
    root_ = nullptr;
  }
  scannerOuter_ = nullptr;
  scannerMiddle_ = nullptr;
  scannerInner_ = nullptr;
  lockBody_ = nullptr;
  lockShackle_ = nullptr;
  title_ = nullptr;
  status_ = nullptr;
  infoPrimary_ = nullptr;
  infoSecondary_ = nullptr;
  for (uint8_t i = 0; i < STEP_COUNT; ++i) dots_[i] = nullptr;
  visible_ = false;
  finished_ = true;
  Serial.println("[UnlockUI] Verification overlay complete");
}
