#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "BoardConfig.h"

/**
 * Dual-action emergency clear confirmation.
 *
 * Shown only after the P4 reports EMERGENCY:CLEAR_REQUEST.
 * Does not change the emergency latch itself.
 */
class DirectorEmergencyClearDialog {
public:
  using ActionFn = void (*)();

  void setConfirmHandler(ActionFn fn) { confirmFn_ = fn; }
  void setCancelHandler(ActionFn fn) { cancelFn_ = fn; }

  void show(uint32_t nowMs);
  void hide();
  void raise();
  void expire();
  void tick(uint32_t nowMs);

  bool isVisible() const { return visible_ && root_ != nullptr; }

private:
  lv_obj_t *root_ = nullptr;
  bool visible_ = false;
  uint32_t shownMs_ = 0;
  ActionFn confirmFn_ = nullptr;
  ActionFn cancelFn_ = nullptr;

  void buildUi();
  void destroy();
  void handleConfirm();
  void handleCancel(bool notifyStage);

  static void onConfirmClicked(lv_event_t *event);
  static void onCancelClicked(lv_event_t *event);
};

extern DirectorEmergencyClearDialog gDirectorEmergencyClearDialog;
