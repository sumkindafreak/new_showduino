#ifndef SHOWDUINO_MOSFET_ENGINE_H
#define SHOWDUINO_MOSFET_ENGINE_H

/*
 * Host-testable non-blocking 4-channel MOSFET output engine.
 * Levels are 0–100%. Hardware write is optional via callback.
 */

#include <stdint.h>
#include <string.h>
#include "showduino_mosfet_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ShowduinoMosfetChMode {
  SHOWDUINO_MOSFET_MODE_OFF = 0,
  SHOWDUINO_MOSFET_MODE_HOLD,
  SHOWDUINO_MOSFET_MODE_PULSE,
  SHOWDUINO_MOSFET_MODE_FADE
} ShowduinoMosfetChMode;

typedef struct ShowduinoMosfetChannel {
  uint8_t level;       /* current 0–100 */
  uint8_t target;      /* fade target */
  uint8_t fadeStart;   /* fade start level */
  uint32_t fadeStartMs;
  uint32_t fadeDurMs;
  uint32_t pulseDeadlineMs;
  ShowduinoMosfetChMode mode;
  uint8_t active;
} ShowduinoMosfetChannel;

typedef void (*ShowduinoMosfetWriteFn)(uint8_t channelIndex0, uint8_t levelPct);

typedef struct ShowduinoMosfetEngine {
  ShowduinoMosfetChannel ch[SHOWDUINO_MOSFET_OUT_COUNT];
  ShowduinoMosfetWriteFn writeFn;
} ShowduinoMosfetEngine;

static inline void showduino_mosfet_engine_write(ShowduinoMosfetEngine *e,
                                                 uint8_t idx, uint8_t level) {
  if (!e || idx >= SHOWDUINO_MOSFET_OUT_COUNT) return;
  e->ch[idx].level = level;
  e->ch[idx].active = level > 0 ? 1u : 0u;
  if (e->writeFn) e->writeFn(idx, level);
}

static inline void showduino_mosfet_engine_begin(ShowduinoMosfetEngine *e,
                                                 ShowduinoMosfetWriteFn writeFn) {
  uint8_t i;
  if (!e) return;
  memset(e, 0, sizeof(*e));
  e->writeFn = writeFn;
  for (i = 0; i < SHOWDUINO_MOSFET_OUT_COUNT; ++i) {
    showduino_mosfet_engine_write(e, i, 0);
  }
}

static inline void showduino_mosfet_engine_all_off(ShowduinoMosfetEngine *e,
                                                   const char *reason) {
  uint8_t i;
  (void)reason;
  if (!e) return;
  for (i = 0; i < SHOWDUINO_MOSFET_OUT_COUNT; ++i) {
    e->ch[i].mode = SHOWDUINO_MOSFET_MODE_OFF;
    e->ch[i].target = 0;
    e->ch[i].fadeStart = 0;
    e->ch[i].fadeStartMs = 0;
    e->ch[i].fadeDurMs = 0;
    e->ch[i].pulseDeadlineMs = 0;
    showduino_mosfet_engine_write(e, i, 0);
  }
}

static inline int showduino_mosfet_engine_set_level(ShowduinoMosfetEngine *e,
                                                    uint8_t channel1to4,
                                                    uint8_t level) {
  uint8_t idx;
  if (!e || !showduino_mosfet_channel_ok(channel1to4) ||
      !showduino_mosfet_level_ok(level)) {
    return 0;
  }
  idx = (uint8_t)(channel1to4 - 1u);
  e->ch[idx].mode = level ? SHOWDUINO_MOSFET_MODE_HOLD : SHOWDUINO_MOSFET_MODE_OFF;
  e->ch[idx].target = level;
  e->ch[idx].fadeDurMs = 0;
  e->ch[idx].pulseDeadlineMs = 0;
  showduino_mosfet_engine_write(e, idx, level);
  return 1;
}

static inline int showduino_mosfet_engine_on(ShowduinoMosfetEngine *e,
                                             uint8_t channel1to4) {
  return showduino_mosfet_engine_set_level(e, channel1to4, 100);
}

static inline int showduino_mosfet_engine_off(ShowduinoMosfetEngine *e,
                                              uint8_t channel1to4) {
  return showduino_mosfet_engine_set_level(e, channel1to4, 0);
}

static inline int showduino_mosfet_engine_pulse(ShowduinoMosfetEngine *e,
                                                uint8_t channel1to4,
                                                uint8_t level,
                                                uint32_t durationMs,
                                                uint32_t nowMs) {
  uint8_t idx;
  if (!e || !showduino_mosfet_channel_ok(channel1to4) ||
      !showduino_mosfet_level_ok(level) ||
      !showduino_mosfet_duration_ok(durationMs)) {
    return 0;
  }
  idx = (uint8_t)(channel1to4 - 1u);
  e->ch[idx].mode = SHOWDUINO_MOSFET_MODE_PULSE;
  e->ch[idx].target = level;
  e->ch[idx].pulseDeadlineMs = nowMs + durationMs;
  e->ch[idx].fadeDurMs = 0;
  showduino_mosfet_engine_write(e, idx, level);
  return 1;
}

static inline int showduino_mosfet_engine_fade(ShowduinoMosfetEngine *e,
                                               uint8_t channel1to4,
                                               uint8_t level,
                                               uint32_t durationMs,
                                               uint32_t nowMs) {
  uint8_t idx;
  if (!e || !showduino_mosfet_channel_ok(channel1to4) ||
      !showduino_mosfet_level_ok(level) ||
      !showduino_mosfet_duration_ok(durationMs)) {
    return 0;
  }
  idx = (uint8_t)(channel1to4 - 1u);
  e->ch[idx].mode = SHOWDUINO_MOSFET_MODE_FADE;
  e->ch[idx].fadeStart = e->ch[idx].level;
  e->ch[idx].target = level;
  e->ch[idx].fadeStartMs = nowMs;
  e->ch[idx].fadeDurMs = durationMs;
  e->ch[idx].pulseDeadlineMs = 0;
  if (durationMs == 0) {
    showduino_mosfet_engine_write(e, idx, level);
    e->ch[idx].mode = level ? SHOWDUINO_MOSFET_MODE_HOLD : SHOWDUINO_MOSFET_MODE_OFF;
  }
  return 1;
}

static inline void showduino_mosfet_engine_tick(ShowduinoMosfetEngine *e,
                                                uint32_t nowMs) {
  uint8_t i;
  if (!e) return;
  for (i = 0; i < SHOWDUINO_MOSFET_OUT_COUNT; ++i) {
    ShowduinoMosfetChannel *c = &e->ch[i];
    if (c->mode == SHOWDUINO_MOSFET_MODE_PULSE) {
      if ((int32_t)(nowMs - c->pulseDeadlineMs) >= 0) {
        c->mode = SHOWDUINO_MOSFET_MODE_OFF;
        c->pulseDeadlineMs = 0;
        showduino_mosfet_engine_write(e, i, 0);
      }
    } else if (c->mode == SHOWDUINO_MOSFET_MODE_FADE) {
      uint32_t elapsed;
      uint32_t next;
      if (c->fadeDurMs == 0) {
        showduino_mosfet_engine_write(e, i, c->target);
        c->mode = c->target ? SHOWDUINO_MOSFET_MODE_HOLD : SHOWDUINO_MOSFET_MODE_OFF;
        continue;
      }
      elapsed = nowMs - c->fadeStartMs;
      if (elapsed >= c->fadeDurMs) {
        showduino_mosfet_engine_write(e, i, c->target);
        c->mode = c->target ? SHOWDUINO_MOSFET_MODE_HOLD : SHOWDUINO_MOSFET_MODE_OFF;
        c->fadeDurMs = 0;
      } else {
        int32_t delta = (int32_t)c->target - (int32_t)c->fadeStart;
        next = (uint32_t)((int32_t)c->fadeStart +
                          (delta * (int32_t)elapsed) / (int32_t)c->fadeDurMs);
        if (next > 100u) next = 100u;
        showduino_mosfet_engine_write(e, i, (uint8_t)next);
      }
    }
  }
}

static inline void showduino_mosfet_engine_levels(const ShowduinoMosfetEngine *e,
                                                  uint8_t out[4]) {
  uint8_t i;
  if (!out) return;
  for (i = 0; i < 4; ++i) out[i] = e ? e->ch[i].level : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_MOSFET_ENGINE_H */
