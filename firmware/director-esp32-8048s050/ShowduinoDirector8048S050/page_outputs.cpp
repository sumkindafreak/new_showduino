#include "page_outputs.h"
#include "ShowduinoOsUi.h"
#include "../../../protocol/showduino_mosfet_node.h"
#include <stdio.h>
#include <string.h>

static OutputsModel sModel;
static outputs_command_fn sCallback;
static lv_obj_t *sSummary, *sFeedback, *sIdentify, *sAllOff;
static lv_obj_t *sSlider[4], *sLive[4], *sRequested[4], *sApply[4], *sOff[4];
static bool sDirty[4];
static bool sWaiting = false;
static bool canReach() {
  return sModel.linked && showduino_mosfet_id_ok(sModel.detail.firstId);
}
static bool canSet() {
  return canReach() && sModel.online && !sModel.emergency && !sModel.showRunning;
}
static void enabled(lv_obj_t *obj, bool on) { ShowduinoOsTheme::setEnabled(obj, on); }
static lv_obj_t *label(lv_obj_t *parent, int x, int y, const char *text) {
  lv_obj_t *obj = lv_label_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(obj, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_label_set_text(obj, text);
  return obj;
}
static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, int h,
                        const char *text, const char *command) {
  lv_obj_t *obj = lv_button_create(parent);
  lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
  lv_obj_set_style_radius(obj, OS_BTN_RADIUS, 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  showduino_theme_register(obj, SHOWDUINO_THEME_ROLE_BORDER);
  lv_obj_set_style_opa(obj, LV_OPA_50, LV_STATE_DISABLED);
  lv_obj_t *lab = label(obj, 0, 0, text); lv_obj_center(lab);
  if (command) lv_obj_add_event_cb(obj, [](lv_event_t *event) {
    if (sCallback) sCallback((const char *)lv_event_get_user_data(event));
  }, LV_EVENT_CLICKED, (void *)command);
  return obj;
}
static void channelAction(lv_event_t *event) {
  const unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(event);
  const bool off = lv_event_get_target(event) == sOff[index];
  if (off ? !canReach() : !canSet()) return;
  char command[56];
  if (off) snprintf(command, sizeof(command), "OUTPUTS:MOSFET:OUT:%u:OFF", index + 1);
  else snprintf(command, sizeof(command), "OUTPUTS:MOSFET:OUT:%u:LEVEL:%u",
                index + 1, (unsigned)lv_slider_get_value(sSlider[index]));
  if (sCallback) sCallback(command);
}
void page_outputs_create(lv_obj_t *parent, outputs_command_fn callback) {
  sCallback = callback;
  button(parent, OS_MARGIN, OS_TITLE_Y, 84, OS_TITLE_H, "BACK", "OUTPUTS:BACK");
  label(parent, 108, OS_TITLE_Y + 10, "OUTPUTS / MOSFET");
  sSummary = label(parent, OS_MARGIN, OS_SUMMARY_Y, "Waiting for MOSFET status...");
  lv_obj_set_width(sSummary, OS_CONTENT_FULL_W);
  const int footerY = OS_DOCK_Y - OS_GAP - 44;
  const int rowsY = footerY - 16 - (4 * 48) + 8;
  for (unsigned i = 0; i < 4; ++i) {
    const int y = rowsY + i * 48;
    char channel[20]; snprintf(channel, sizeof(channel), "OUT %u", i + 1);
    label(parent, 20, y + 11, channel);
    sLive[i] = label(parent, 94, y + 11, "Actual: --");
    sSlider[i] = lv_slider_create(parent);
    lv_obj_set_pos(sSlider[i], 226, y + 16); lv_obj_set_size(sSlider[i], 184, 12);
    lv_slider_set_range(sSlider[i], 0, 100);
    lv_slider_set_value(sSlider[i], 0, LV_ANIM_OFF);
    showduino_theme_register(sSlider[i], SHOWDUINO_THEME_ROLE_BORDER);
    sRequested[i] = label(parent, 436, y + 11, "Set: 0%");
    lv_obj_add_event_cb(sSlider[i], [](lv_event_t *event) {
      const unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(event);
      sDirty[index] = true;
      char text[20]; snprintf(text, sizeof(text), "Set: %u%%",
                             (unsigned)lv_slider_get_value(sSlider[index]));
      lv_label_set_text(sRequested[index], text);
    }, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);
    sApply[i] = button(parent, 530, y + 4, 112, 36, "APPLY", nullptr);
    sOff[i] = button(parent, 654, y + 4, 112, 36, "OFF", nullptr);
    lv_obj_add_event_cb(sApply[i], channelAction, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    lv_obj_add_event_cb(sOff[i], channelAction, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
  }
  button(parent, 12, footerY, 112, 44, "REFRESH", "OUTPUTS:REFRESH");
  sIdentify = button(parent, 132, footerY, 132, 44, "IDENTIFY", "OUTPUTS:IDENTIFY");
  sAllOff = button(parent, 272, footerY, 140, 44, "ALL OFF", "OUTPUTS:ALL:OFF");
  sFeedback = label(parent, 426, footerY + 2, "Select a level, then APPLY.");
  lv_obj_set_size(sFeedback, 362, 44);
  page_outputs_set_model(sModel);
}
void page_outputs_set_model(const OutputsModel &model) {
  const bool changedNode = strcmp(model.detail.firstId, sModel.detail.firstId) != 0;
  sModel = model;
  if (!sSummary) return;
  char summary[140];
  snprintf(summary, sizeof(summary), "%s | %s\n%s",
           canReach() ? sModel.detail.firstId : "NO MOSFET NODE",
           !sModel.linked ? "LINK LOST" : sModel.emergency ? "EMERGENCY" :
           sModel.online ? "ONLINE" : "OFFLINE / FAULT",
           sModel.showRunning ? "Show active: level changes locked." :
           sModel.emergency ? "Emergency: level changes locked. OFF remains available." :
           "Four channels. Actual levels come from P4 / node feedback.");
  lv_label_set_text(sSummary, summary);
  for (unsigned i = 0; i < 4; ++i) {
    if (changedNode) { sDirty[i] = false; lv_slider_set_value(sSlider[i], 0, LV_ANIM_OFF); }
    const bool known = sModel.linked && sModel.online && sModel.detail.haveLevels;
    char text[24];
    if (known) snprintf(text, sizeof(text), "Actual: %u%%", sModel.detail.levels[i]);
    else snprintf(text, sizeof(text), "Actual: --");
    lv_label_set_text(sLive[i], text);
    if (known && (unsigned)lv_slider_get_value(sSlider[i]) == sModel.detail.levels[i]) sDirty[i] = false;
    if (known && !sDirty[i]) lv_slider_set_value(sSlider[i], sModel.detail.levels[i], LV_ANIM_OFF);
    snprintf(text, sizeof(text), "Set: %u%%", (unsigned)lv_slider_get_value(sSlider[i]));
    lv_label_set_text(sRequested[i], text);
    lv_obj_set_style_bg_color(sSlider[i], showduino_theme_get_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sSlider[i], showduino_theme_get_accent(), LV_PART_KNOB);
    enabled(sSlider[i], canSet()); enabled(sApply[i], canSet()); enabled(sOff[i], canReach());
  }
  enabled(sIdentify, canReach() && sModel.online && !sModel.emergency);
  enabled(sAllOff, canReach());
}
void page_outputs_feedback(const char *text) {
  sWaiting = text && !strncmp(text, "Request sent.", 13);
  if (sFeedback) lv_label_set_text(sFeedback, text);
}
void page_outputs_report_received() {
  if (sWaiting) page_outputs_feedback("Node status received. Check Actual levels.");
}
void page_outputs_apply_theme() { page_outputs_set_model(sModel); }
