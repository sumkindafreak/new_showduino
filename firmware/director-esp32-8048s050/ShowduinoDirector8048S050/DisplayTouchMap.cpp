#include "DisplayTouchMap.h"

void DisplayTouchMap::clear() {
  regions_ = nullptr;
  count_ = 0;
  pressed_ = false;
  armedCommand_ = nullptr;
}

void DisplayTouchMap::setRegions(const TouchRegion *regions, uint16_t count) {
  regions_ = regions;
  count_ = count;
  pressed_ = false;
  armedCommand_ = nullptr;
}

const char *DisplayTouchMap::hitTest(int32_t x, int32_t y) const {
  if (!regions_ || count_ == 0) return nullptr;
  for (uint16_t i = 0; i < count_; i++) {
    const lv_area_t &a = regions_[i].bounds;
    if (x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2) {
      return regions_[i].command;
    }
  }
  return nullptr;
}

const char *DisplayTouchMap::onTouch(int32_t x, int32_t y, bool pressed) {
  if (pressed) {
    if (!pressed_) {
      armedCommand_ = hitTest(x, y);
      pressed_ = true;
    }
    return nullptr;
  }

  /* Release */
  if (!pressed_) return nullptr;
  pressed_ = false;
  const char *cmd = nullptr;
  if (armedCommand_ && hitTest(x, y) == armedCommand_) {
    cmd = armedCommand_;
  }
  armedCommand_ = nullptr;
  return cmd;
}
