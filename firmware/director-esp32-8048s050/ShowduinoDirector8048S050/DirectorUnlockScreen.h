#pragma once

#include <Arduino.h>
#include <lvgl.h>

/**
 * Showduino OS Director unlock / verification overlay.
 *
 * The overlay is deliberately drawn with native LVGL 9 objects so it does not
 * require an SD-card image or a large full-screen canvas. Decorative geometry
 * is static; only the scanner rings, lock glow and progress indicators update.
 */
class DirectorUnlockScreen {
public:
  /** Create the overlay on LVGL's top layer. Safe to call repeatedly. */
  void begin(uint32_t nowMs);

  /** Animate and advance the verification sequence. Call from the main loop. */
  void tick(uint32_t nowMs, bool espNowReady, uint8_t linkState, bool emergencyLocked);

  bool isVisible() const { return visible_; }
  bool isFinished() const { return finished_; }

private:
  static constexpr uint8_t STEP_COUNT = 9;

  lv_obj_t *root_ = nullptr;
  lv_obj_t *scannerOuter_ = nullptr;
  lv_obj_t *scannerMiddle_ = nullptr;
  lv_obj_t *scannerInner_ = nullptr;
  lv_obj_t *lockBody_ = nullptr;
  lv_obj_t *lockShackle_ = nullptr;
  lv_obj_t *title_ = nullptr;
  lv_obj_t *status_ = nullptr;
  lv_obj_t *infoPrimary_ = nullptr;
  lv_obj_t *infoSecondary_ = nullptr;
  lv_obj_t *dots_[STEP_COUNT] = {};

  uint32_t startedMs_ = 0;
  uint32_t readySinceMs_ = 0;
  uint8_t currentStep_ = 0;
  bool visible_ = false;
  bool finished_ = false;
  bool finalStateApplied_ = false;

  void buildUi();
  void buildFrameDecorations();
  void buildScanner();
  void buildTextAndProgress();
  void setStep(uint8_t step, const char *status, const char *primary, const char *secondary);
  void applyFinalState(bool stageLinked, bool emergencyLocked, uint32_t nowMs);
  void destroy();

  static void styleTransparent(lv_obj_t *obj);
  static lv_obj_t *makeLine(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t w, int32_t h, uint32_t colour,
                            lv_opa_t opacity = LV_OPA_COVER);
};

extern DirectorUnlockScreen gDirectorUnlockScreen;
