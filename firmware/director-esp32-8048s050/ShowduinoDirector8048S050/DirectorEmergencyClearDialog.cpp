#include "DirectorEmergencyClearDialog.h"
#include "ShowduinoOsUi.h"
#include "ShowduinoOsPalette.h"

DirectorEmergencyClearDialog gDirectorEmergencyClearDialog;

static ShowduinoOsTheme sClearDialogTheme;

void DirectorEmergencyClearDialog::buildUi() {
  if (root_) return;

  sClearDialogTheme.begin();
  ShowduinoOsTheme &theme = sClearDialogTheme;

  root_ = theme.makeDialogScrim(lv_layer_top());
  lv_obj_t *box = theme.makeDialogBox(root_, 500, 236, true);

  lv_obj_t *kicker = lv_label_create(box);
  lv_label_set_text(kicker, "EMERGENCY CLEARANCE REQUESTED");
  lv_obj_set_style_text_color(kicker, lv_color_hex(ShowduinoPalette::DangerText), 0);
  lv_obj_set_style_text_font(kicker, &lv_font_montserrat_12, 0);
  lv_obj_set_pos(kicker, 24, 16);

  lv_obj_t *title = lv_label_create(box);
  lv_label_set_text(title, "Clear Emergency Stop?");
  lv_obj_set_style_text_color(title, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_pos(title, 24, 40);

  lv_obj_t *body = lv_label_create(box);
  lv_label_set_text(body,
                    "The physical emergency loop created this request.\n"
                    "CONFIRM CLEAR does not resume the show. Release the "
                    "physical stop first or the P4 will reject clearance.");
  lv_obj_set_style_text_color(body, lv_color_hex(ShowduinoPalette::Muted), 0);
  lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(body, 24, 80);
  lv_obj_set_width(body, 452);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);

  theme.makeButton(box, "CANCEL", 24, 168, 170, OS_BTN_H,
                   onCancelClicked, this, nullptr, false, false);
  theme.makeButton(box, "CONFIRM CLEAR", 214, 168, 260, OS_BTN_H,
                   onConfirmClicked, this, nullptr, true, false);
}

void DirectorEmergencyClearDialog::destroy() {
  if (root_) {
    lv_obj_delete(root_);
    root_ = nullptr;
  }
  visible_ = false;
}

void DirectorEmergencyClearDialog::show(uint32_t nowMs) {
  buildUi();
  if (!root_) return;
  shownMs_ = nowMs;
  visible_ = true;
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(root_);
  Serial.println("[ESTOP] Clear request popup shown");
}

void DirectorEmergencyClearDialog::hide() {
  if (root_) lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
  visible_ = false;
}

void DirectorEmergencyClearDialog::raise() {
  if (isVisible() && root_) lv_obj_move_foreground(root_);
}

void DirectorEmergencyClearDialog::expire() {
  if (!isVisible()) return;
  hide();
  Serial.println("[ESTOP] Clear request popup expired");
}

void DirectorEmergencyClearDialog::tick(uint32_t nowMs) {
  if (!isVisible()) return;
  if ((nowMs - shownMs_) >= SHOWDUINO_ESTOP_CLEAR_REQUEST_TIMEOUT_MS) {
    handleCancel(true);
  }
}

void DirectorEmergencyClearDialog::handleConfirm() {
  if (!isVisible()) return;
  hide();
  Serial.println("[ESTOP] Clear confirmation sent");
  if (confirmFn_) confirmFn_();
}

void DirectorEmergencyClearDialog::handleCancel(bool notifyStage) {
  if (!isVisible() && !notifyStage) return;
  hide();
  Serial.println("[ESTOP] Clear request cancelled");
  if (notifyStage && cancelFn_) cancelFn_();
}

void DirectorEmergencyClearDialog::onConfirmClicked(lv_event_t *event) {
  auto *self = static_cast<DirectorEmergencyClearDialog *>(lv_event_get_user_data(event));
  if (self) self->handleConfirm();
}

void DirectorEmergencyClearDialog::onCancelClicked(lv_event_t *event) {
  auto *self = static_cast<DirectorEmergencyClearDialog *>(lv_event_get_user_data(event));
  if (self) self->handleCancel(true);
}
