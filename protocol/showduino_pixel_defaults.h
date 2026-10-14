#ifndef SHOWDUINO_PIXEL_DEFAULTS_H
#define SHOWDUINO_PIXEL_DEFAULTS_H
#include <stdint.h>
// Uncommissioned programmable lines start with ten pixels, never zero.
#define SHOWDUINO_PIXEL_DEFAULT_COUNT 10u
static inline uint16_t showduino_pixel_boot_count(uint16_t stored, uint16_t maximum) {
  return stored > 0 && stored <= maximum ? stored
      : (maximum < SHOWDUINO_PIXEL_DEFAULT_COUNT ? maximum : SHOWDUINO_PIXEL_DEFAULT_COUNT);
}
#endif
