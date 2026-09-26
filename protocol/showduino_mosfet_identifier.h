#ifndef SHOWDUINO_MOSFET_IDENTIFIER_H
#define SHOWDUINO_MOSFET_IDENTIFIER_H

#include <stdint.h>
#include "showduino_mosfet_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Local identifier NeoPixels on MOSFET Node (GPIO25 × 4).
 * NOT a theatrical Pixel Line.
 *
 * GREEN is reserved exclusively for an energised MOSFET output.
 * Odd colours carry Wi-Fi / radio / ownership / health when that channel is OFF.
 */

#define SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_FOR_OUT(out1to4) ((uint8_t)((out1to4) - 1u))
#define SHOWDUINO_MOSFET_IDENTIFIER_STATUS_BRIGHTNESS_DEFAULT 10u

typedef struct ShowduinoMosfetRgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} ShowduinoMosfetRgb;

/* Colour language — green never used for status. */
static inline ShowduinoMosfetRgb showduino_mosfet_rgb(uint8_t r, uint8_t g, uint8_t b) {
  ShowduinoMosfetRgb c;
  c.r = r;
  c.g = g;
  c.b = b;
  return c;
}

static inline ShowduinoMosfetRgb showduino_mosfet_rgb_off(void) {
  return showduino_mosfet_rgb(0, 0, 0);
}

static inline ShowduinoMosfetRgb showduino_mosfet_rgb_green(uint8_t levelPct,
                                                            uint8_t maxBrightness) {
  uint8_t g;
  if (!levelPct || !maxBrightness) return showduino_mosfet_rgb_off();
  g = (levelPct >= 100)
          ? maxBrightness
          : (uint8_t)(((uint16_t)levelPct * (uint16_t)maxBrightness) / 100u);
  return showduino_mosfet_rgb(0, g, 0);
}

/* Dim status palette at idleStatusBright (typically 8–12). */
static inline ShowduinoMosfetRgb showduino_mosfet_rgb_purple(uint8_t b) {
  return showduino_mosfet_rgb(b, 0, b); /* Wi-Fi SoftAP */
}
static inline ShowduinoMosfetRgb showduino_mosfet_rgb_cyan(uint8_t b) {
  return showduino_mosfet_rgb(0, b, b); /* ESP-NOW */
}
static inline ShowduinoMosfetRgb showduino_mosfet_rgb_turquoise(uint8_t b) {
  return showduino_mosfet_rgb(0, b, (uint8_t)(b + b / 2u > 255 ? 255 : b + b / 2u));
}
static inline ShowduinoMosfetRgb showduino_mosfet_rgb_violet(uint8_t b) {
  return showduino_mosfet_rgb((uint8_t)(b + b / 3u > 255 ? 255 : b + b / 3u), 0,
                              (uint8_t)(b + b / 2u > 255 ? 255 : b + b / 2u));
}
static inline ShowduinoMosfetRgb showduino_mosfet_rgb_amber(uint8_t b) {
  return showduino_mosfet_rgb(b, (uint8_t)(b * 2u / 3u), 0); /* searching */
}
static inline ShowduinoMosfetRgb showduino_mosfet_rgb_magenta(uint8_t b) {
  return showduino_mosfet_rgb(b, 0, (uint8_t)(b * 2u / 3u)); /* WebUI / fault half */
}
static inline ShowduinoMosfetRgb showduino_mosfet_rgb_white(uint8_t b) {
  return showduino_mosfet_rgb(b, b, b); /* IDENTIFY */
}

static inline uint8_t showduino_mosfet_identifier_green(
    uint8_t levelPct, uint8_t maxBrightness) {
  return showduino_mosfet_rgb_green(levelPct, maxBrightness).g;
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

typedef struct ShowduinoMosfetIdentStatus {
  uint8_t softApUp;       /* Wi-Fi SoftAP running */
  uint8_t espNowLinked;   /* heard Comms recently */
  uint8_t searching;      /* no P4 ownership / searching */
  uint8_t owned;          /* P4 SHOW_CONTROLLED */
  uint8_t healthy;        /* not FAULT */
  uint8_t fault;          /* FAULT state */
  uint8_t statusBright;   /* idle status brightness 8–12 */
} ShowduinoMosfetIdentStatus;

/*
 * Steady compositing for OFF channels only.
 * Active levels always paint GREEN and win over status roles:
 *   P0 SoftAP purple | P1 ESP-NOW cyan | P2 ownership turquoise | P3 health violet
 * Searching (and not owned): amber on P1 role when that channel is OFF.
 */
static inline void showduino_mosfet_identifier_compose(
    const uint8_t levels[SHOWDUINO_MOSFET_OUT_COUNT],
    uint8_t maxGreen,
    const ShowduinoMosfetIdentStatus *st,
    ShowduinoMosfetRgb out[SHOWDUINO_MOSFET_OUT_COUNT]) {
  uint8_t i;
  uint8_t sb;
  if (!levels || !out) return;
  sb = (st && st->statusBright) ? st->statusBright
                                : SHOWDUINO_MOSFET_IDENTIFIER_STATUS_BRIGHTNESS_DEFAULT;
  for (i = 0; i < SHOWDUINO_MOSFET_OUT_COUNT; ++i) {
    if (levels[i] > 0) {
      out[i] = showduino_mosfet_rgb_green(levels[i], maxGreen);
      continue;
    }
    out[i] = showduino_mosfet_rgb_off();
    if (!st || st->fault) continue; /* fault handled by animation elsewhere */
    if (i == 0 && st->softApUp) out[i] = showduino_mosfet_rgb_purple(sb);
    else if (i == 1) {
      if (st->espNowLinked) out[i] = showduino_mosfet_rgb_cyan(sb);
      else if (st->searching) out[i] = showduino_mosfet_rgb_amber(sb);
    } else if (i == 2 && st->owned) {
      out[i] = showduino_mosfet_rgb_turquoise(sb);
    } else if (i == 3 && st->healthy) {
      out[i] = showduino_mosfet_rgb_violet(sb);
    }
  }
}

/* True if colour uses green channel for output indication (g>0 and r==0,b==0). */
static inline int showduino_mosfet_rgb_is_output_green(ShowduinoMosfetRgb c) {
  return c.g > 0 && c.r == 0 && c.b == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_MOSFET_IDENTIFIER_H */
