#ifndef SHOWDUINO_MOSFET_IDENTIFIER_H
#define SHOWDUINO_MOSFET_IDENTIFIER_H

#include <stdint.h>
#include "showduino_mosfet_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Host-testable mapping: MOSFET OUT levels → local identifier NeoPixel green.
 * Pixel i mirrors OUT(i+1). Not a theatrical Pixel Line.
 */

#define SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_FOR_OUT(out1to4) ((uint8_t)((out1to4) - 1u))

static inline uint8_t showduino_mosfet_identifier_green(
    uint8_t levelPct, uint8_t maxBrightness) {
  if (!levelPct || !maxBrightness) return 0;
  if (levelPct >= 100) return maxBrightness;
  return (uint8_t)(((uint16_t)levelPct * (uint16_t)maxBrightness) / 100u);
}

static inline void showduino_mosfet_identifier_frame(
    const uint8_t levels[SHOWDUINO_MOSFET_OUT_COUNT],
    uint8_t maxBrightness,
    uint8_t greenOut[SHOWDUINO_MOSFET_OUT_COUNT]) {
  uint8_t i;
  if (!levels || !greenOut) return;
  for (i = 0; i < SHOWDUINO_MOSFET_OUT_COUNT; ++i) {
    greenOut[i] = showduino_mosfet_identifier_green(levels[i], maxBrightness);
  }
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_MOSFET_IDENTIFIER_H */
