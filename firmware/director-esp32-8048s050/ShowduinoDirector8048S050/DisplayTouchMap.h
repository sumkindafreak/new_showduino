#ifndef SHOWDUINO_DISPLAY_TOUCH_MAP_H
#define SHOWDUINO_DISPLAY_TOUCH_MAP_H

#include "DisplayTypes.h"

class DisplayTouchMap {
 public:
  void clear();
  void setRegions(const TouchRegion *regions, uint16_t count);

  /** Returns command string on hit, or nullptr. */
  const char *hitTest(int32_t x, int32_t y) const;

  /**
   * Press/release tracking: emit command on release if press+release in same region.
   * Returns command to fire, or nullptr.
   */
  const char *onTouch(int32_t x, int32_t y, bool pressed);

 private:
  const TouchRegion *regions_ = nullptr;
  uint16_t count_ = 0;
  bool pressed_ = false;
  const char *armedCommand_ = nullptr;
};

#endif
