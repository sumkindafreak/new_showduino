#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "BoardConfig.h"

#ifndef LV_OPA_85
#define LV_OPA_85 LV_OPA_80
#endif

/**
 * Full-screen Showduino emergency latch UI.
 *
 * Visual language is taken from DirectorUnlockScreen: native LVGL objects,
 * framed 800×480 chassis, technical typography, panel cards, and no image
 * assets. Safety state is displayed only; the Stage Controller remains
 * authoritative for the latch.
 */
class DirectorEmergencyScreen {
public:
  enum class Source : uint8_t {
    Unknown = 0,
    Director,
    Physical
  };

  using ClearRequestFn = void (*)();
  using FinishedFn = void (*)();

  void setClearRequestHandler(ClearRequestFn fn) { clearRequestFn_ = fn; }
  void setFinishedHandler(FinishedFn fn) { finishedFn_ = fn; }

  /** Create or re-show the screen. Safe to call while already visible. */
  void show(uint32_t nowMs);
  /** Delete all LVGL objects and reset pointers. */
  void hide();

  void tick(uint32_t nowMs, uint8_t linkState);

  void setLatchActive(bool active, uint32_t nowMs);
  void setSource(Source source);
  void setShowName(const char *name);
  void setActiveSince(uint32_t startedMs);
  void noteClearRequested(uint32_t nowMs);
  void noteClearRejected(uint32_t nowMs);

  bool isVisible() const { return visible_ && root_ != nullptr; }
  bool isAwaitingStage() const { return awaitingStage_; }

private:
  enum class Phase : uint8_t {
    Hidden = 0,
    Active,
    ClearRejected,
    ClearedHold
  };

  lv_obj_t *root_ = nullptr;
  lv_obj_t *warnMark_ = nullptr;
  lv_obj_t *title_ = nullptr;
  lv_obj_t *subtitle_ = nullptr;
  lv_obj_t *explain_ = nullptr;
  lv_obj_t *sourceLabel_ = nullptr;
  lv_obj_t *statusPrimary_ = nullptr;
  lv_obj_t *statusSecondary_ = nullptr;
  lv_obj_t *rejectTitle_ = nullptr;
  lv_obj_t *rejectBody_ = nullptr;
  lv_obj_t *rejectBox_ = nullptr;
  lv_obj_t *clearBtn_ = nullptr;
  lv_obj_t *clearBtnLabel_ = nullptr;

  Phase phase_ = Phase::Hidden;
  Source source_ = Source::Unknown;
  bool visible_ = false;
  bool latchActive_ = false;
  bool physicalAsserted_ = false;
  bool awaitingStage_ = false;
  uint8_t linkState_ = LINK_SEARCHING;
  uint32_t shownMs_ = 0;
  uint32_t activeSinceMs_ = 0;
  uint32_t lastClearMs_ = 0;
  uint32_t clearedSinceMs_ = 0;
  char showName_[64] = "-";

  ClearRequestFn clearRequestFn_ = nullptr;
  FinishedFn finishedFn_ = nullptr;

  void buildUi();
  void buildFrameDecorations();
  void buildWarningMark();
  void buildCopyAndStatus();
  void buildClearControl();
  void refreshCopy();
  void refreshStatus();
  void applyPhaseStyles();
  void destroy();
  void handleClearClicked();
  void enterClearedHold(uint32_t nowMs);

  static void styleTransparent(lv_obj_t *obj);
  static lv_obj_t *makeLine(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t w, int32_t h, uint32_t colour,
                            lv_opa_t opacity = LV_OPA_COVER);
  static void onRootClicked(lv_event_t *event);
  static void onClearClicked(lv_event_t *event);
  static const char *sourceText(Source source);
  static const char *linkText(uint8_t linkState);

  static constexpr uint32_t CLEAR_COOLDOWN_MS = 900UL;
  static constexpr uint32_t CLEARED_HOLD_MS = 900UL;
};

extern DirectorEmergencyScreen gDirectorEmergencyScreen;
