#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "../../../protocol/showduino_director_locate.h"

/**
 * Latched Director Locate presentation.
 *
 * Independent of Emergency. First consumed touch acknowledges Locate only.
 * No automatic timeout.
 */
class DirectorLocateScreen {
public:
  void show(uint32_t nowMs);
  void hide();
  void tick(uint32_t nowMs);

  bool isVisible() const { return visible_ && root_ != nullptr; }

private:
  lv_obj_t *root_ = nullptr;
  lv_obj_t *pulse_ = nullptr;
  bool visible_ = false;
  uint32_t shownMs_ = 0;

  void buildUi();
  void destroy();
};

void directorLocateOnCommand(uint32_t nowMs);
bool directorLocateOnTouch(int32_t x, int32_t y, bool pressed);
void directorLocateTick(uint32_t nowMs);
bool directorLocateActive();
void directorLocateAcknowledge();

extern DirectorLocateScreen gDirectorLocateScreen;
extern ShowduinoDirectorLocateState gDirectorLocateState;
