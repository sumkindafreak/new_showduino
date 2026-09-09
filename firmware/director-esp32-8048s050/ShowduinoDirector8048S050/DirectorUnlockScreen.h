#pragma once

#include <Arduino.h>
#include <lvgl.h>

/* LVGL provides opacity steps in tens on some 9.x releases. */
#ifndef LV_OPA_85
#define LV_OPA_85 LV_OPA_80
#endif

/**
 * Showduino Director boot screen.
 *
 * This is a dedicated LVGL screen, not an overlay on the operator desk.
 * It must be created and loaded before any application chrome (status bar,
 * dock, page headers). The first flushed frame is this screen.
 */
class DirectorUnlockScreen {
public:
  static constexpr uint8_t STEP_COUNT = 9;
  using FinishedFn = void (*)();

  void setFinishedHandler(FinishedFn fn) { finishedFn_ = fn; }

  /** Create and load the boot screen. Safe to call repeatedly. */
  void begin(uint32_t nowMs);

  /** Animate and advance the verification sequence. Call from the main loop. */
  void tick(uint32_t nowMs, bool espNowReady, uint8_t linkState, bool emergencyLocked);

  /** Emergency wins immediately - no cosmetic boot exit. */
  void abortForEmergency();

  bool isVisible() const { return visible_; }
  bool isFinished() const { return finished_; }
  /** True until the boot screen has completed or been aborted. */
  bool ownsDisplay() const { return !finished_; }
  lv_obj_t *screen() const { return root_; }

private:
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

  FinishedFn finishedFn_ = nullptr;
  uint32_t startedMs_ = 0;
  uint32_t readySinceMs_ = 0;
  uint8_t currentStep_ = 0;
  bool visible_ = false;
  bool finished_ = false;
  bool finalStateApplied_ = false;
  bool exiting_ = false;

  void buildUi();
  void buildFrameDecorations();
  void buildScanner();
  void buildTextAndProgress();
  void setStep(uint8_t step, const char *status, const char *primary, const char *secondary);
  void applyFinalState(bool stageLinked, bool emergencyLocked, uint32_t nowMs);
  void finish(bool emergency);

  static void styleTransparent(lv_obj_t *obj);
  static lv_obj_t *makeLine(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t w, int32_t h, uint32_t colour,
                            lv_opa_t opacity = LV_OPA_COVER);
};

extern DirectorUnlockScreen gDirectorUnlockScreen;
