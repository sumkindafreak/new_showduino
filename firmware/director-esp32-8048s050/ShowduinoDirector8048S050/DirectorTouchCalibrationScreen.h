#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "TouchCalibrationMath.h"

/**
 * Operator touchscreen calibration wizard.
 * Owns touch while visible. Emergency and Locate abort it without saving.
 */
class DirectorTouchCalibrationScreen {
public:
  void start(uint32_t nowMs);
  void startResetConfirm(uint32_t nowMs);
  void hide();
  void tick(uint32_t nowMs);
  bool isActive() const { return active_; }
  /** Consume the press/release so underlying LVGL controls cannot fire. */
  bool onTouch(int32_t x, int32_t y, bool pressed);

private:
  enum class Phase : uint8_t {
    Hidden = 0,
    Capture,
    Test,
    Failed,
    Saved,
    ResetConfirm
  };

  lv_obj_t *root_ = nullptr;
  lv_obj_t *title_ = nullptr;
  lv_obj_t *pointLab_ = nullptr;
  lv_obj_t *hint_ = nullptr;
  lv_obj_t *targetOuter_ = nullptr;
  lv_obj_t *targetInner_ = nullptr;
  lv_obj_t *targetDot_ = nullptr;
  lv_obj_t *ack_ = nullptr;
  lv_obj_t *marker_ = nullptr;
  lv_obj_t *testDots_[5] = {};
  lv_obj_t *failBox_ = nullptr;
  lv_obj_t *btnCaptureCancel_ = nullptr;
  lv_obj_t *btnSave_ = nullptr;
  lv_obj_t *btnRetry_ = nullptr;
  lv_obj_t *btnCancel_ = nullptr;
  lv_obj_t *btnReset_ = nullptr;
  bool active_ = false;
  bool waitingRelease_ = false;
  bool ignoreSample_ = false;
  bool acceptedThisPress_ = false;
  Phase phase_ = Phase::Hidden;
  uint8_t pointIndex_ = 0;
  uint32_t ackUntilMs_ = 0;
  uint32_t savedUntilMs_ = 0;
  int32_t pressX_ = 0;
  int32_t pressY_ = 0;
  bool pressed_ = false;
  int actionDown_ = 0;
  ShowduinoTouchCalPoint points_[SHOWDUINO_TOUCH_CAL_POINT_N];
  ShowduinoTouchCalSampleBuf samples_{};
  ShowduinoTouchCalibrationRecord pending_{};

  void buildUi();
  void destroy();
  void enterCapture(uint32_t nowMs);
  void layoutTarget();
  void refreshCopy();
  void setPhase(Phase phase);
  void abortUnsaved(const char *why);
  void onPointAccepted(int32_t rawX, int32_t rawY);
  void finishFit();
  void doSave();
  void doReset();
  void doCancel();
  int hitAction(int32_t x, int32_t y) const;
};

void directorTouchCalStart();
void directorTouchCalStartReset();
void directorTouchCalTick(uint32_t nowMs);
bool directorTouchCalActive();
bool directorTouchCalOnTouch(int32_t x, int32_t y, bool pressed);
void directorTouchCalAbortForSafety();

extern DirectorTouchCalibrationScreen gDirectorTouchCalibrationScreen;
