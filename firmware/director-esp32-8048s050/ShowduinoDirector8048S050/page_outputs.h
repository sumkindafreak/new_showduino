#pragma once
#include <lvgl.h>
#include "../../../protocol/showduino_state_wire.h"
typedef void (*outputs_command_fn)(const char *);
struct OutputsModel {
  ShowduinoMosfetDetailWire detail{};
  bool linked = false;
  bool online = false;
  bool emergency = false;
  bool showRunning = false;
};
void page_outputs_create(lv_obj_t *parent, outputs_command_fn callback);
void page_outputs_set_model(const OutputsModel &model);
void page_outputs_feedback(const char *text);
void page_outputs_apply_theme();
void page_outputs_report_received();

void page_outputs_select(unsigned tab);
