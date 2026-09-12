#ifndef SHOWDUINO_CARBIDE_LAMP_H
#define SHOWDUINO_CARBIDE_LAMP_H

/*
 * Host-testable carbide-lamp machine, blow detector, and local-audio map.
 * No Arduino, NeoPixel, ADC, UART, or ESP-NOW side effects.
 *
 * Ownership (SEARCHING / SHOW_CONTROLLED / EMERGENCY) stays in
 * showduino_lamp_node.h. This file is the interactive flame machine only.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "showduino_lamp_node.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_CARBIDE_LOGICAL_DEFAULT  "LAMP-01"
#define SHOWDUINO_CARBIDE_LOGICAL_MAX      12
#define SHOWDUINO_CARBIDE_STRIKE_MS        220u
#define SHOWDUINO_CARBIDE_IGNITE_MS        1600u
#define SHOWDUINO_CARBIDE_EXTINGUISH_MS    420u
#define SHOWDUINO_CARBIDE_FLARE_MS         900u
#define SHOWDUINO_CARBIDE_PUFF_MS          80u
#define SHOWDUINO_CARBIDE_BLOW_MS          350u
#define SHOWDUINO_CARBIDE_FLINT_CORE_MS    45u
#define SHOWDUINO_CARBIDE_FLINT_NEAR_MS    40u
#define SHOWDUINO_CARBIDE_FLINT_DARK_MS    80u
#define SHOWDUINO_CARBIDE_RECOVER_MS       450u
#define SHOWDUINO_CARBIDE_JEWEL_PIXELS     7u
#define SHOWDUINO_CARBIDE_IDENTIFY_DWELL_MS 700u
#define SHOWDUINO_CARBIDE_BASELINE_SHIFT   6
#define SHOWDUINO_CARBIDE_DEFAULT_THRESH   80
#define SHOWDUINO_CARBIDE_FAIL_STRIKE_OFF  0

typedef enum ShowduinoCarbideState {
  SHOWDUINO_CARBIDE_OFF = 0,
  SHOWDUINO_CARBIDE_STRIKING,
  SHOWDUINO_CARBIDE_IGNITING,
  SHOWDUINO_CARBIDE_BURNING,
  SHOWDUINO_CARBIDE_LOW_FLAME,
  SHOWDUINO_CARBIDE_UNSTABLE,
  SHOWDUINO_CARBIDE_FLARE,
  SHOWDUINO_CARBIDE_EXTINGUISHING
} ShowduinoCarbideState;

typedef enum ShowduinoCarbideEvent {
  SHOWDUINO_CARBIDE_EV_NONE = 0,
  SHOWDUINO_CARBIDE_EV_IGNITE,
  SHOWDUINO_CARBIDE_EV_EXTINGUISH,
  SHOWDUINO_CARBIDE_EV_LOW_FLAME,
  SHOWDUINO_CARBIDE_EV_UNSTABLE,
  SHOWDUINO_CARBIDE_EV_FLARE,
  SHOWDUINO_CARBIDE_EV_STEADY,
  SHOWDUINO_CARBIDE_EV_PUFF,
  SHOWDUINO_CARBIDE_EV_BLOW,
  SHOWDUINO_CARBIDE_EV_BLOW_END,
  SHOWDUINO_CARBIDE_EV_TICK,
  SHOWDUINO_CARBIDE_EV_FORCE_OFF
} ShowduinoCarbideEvent;

typedef enum ShowduinoLampSound {
  SHOWDUINO_LAMP_SND_NONE = 0,
  SHOWDUINO_LAMP_SND_STRIKE,
  SHOWDUINO_LAMP_SND_IGNITION,
  SHOWDUINO_LAMP_SND_BURN_LOOP,
  SHOWDUINO_LAMP_SND_EMERGENCY
} ShowduinoLampSound;

typedef enum ShowduinoBlowClass {
  SHOWDUINO_BLOW_NONE = 0,
  SHOWDUINO_BLOW_PUFF,
  SHOWDUINO_BLOW_SUSTAINED,
  SHOWDUINO_BLOW_RELEASE
} ShowduinoBlowClass;

typedef struct ShowduinoCarbideConfig {
  uint32_t strikeMs;
  uint32_t igniteMs;
  uint32_t extinguishMs;
  uint32_t flareMs;
  uint8_t failStrikePercent; /* 0 = never fail (commissioning default) */
} ShowduinoCarbideConfig;

typedef struct ShowduinoCarbideMachine {
  ShowduinoCarbideState state;
  ShowduinoCarbideState beforePuff;
  uint32_t enteredMs;
  uint32_t nowMs;
  uint8_t failStrikePercent;
  uint8_t lastRoll; /* test injection; 255 = unused */
  ShowduinoLampSound sound;
  uint8_t soundLoop;
} ShowduinoCarbideMachine;

typedef struct ShowduinoBlowConfig {
  int32_t threshold;
  uint32_t puffMs;
  uint32_t blowMs;
  uint8_t baselineShift;
} ShowduinoBlowConfig;

typedef struct ShowduinoBlowDetector {
  int32_t raw;
  int32_t filtered;
  int32_t baseline;
  int32_t threshold;
  uint32_t aboveMs;
  uint32_t puffMs;
  uint32_t blowMs;
  uint8_t baselineShift;
  uint8_t armed;
  ShowduinoBlowClass classified;
} ShowduinoBlowDetector;

typedef struct ShowduinoLampSoundMap {
  ShowduinoLampSound id;
  const char *role;
  const char *file;
  uint16_t track;
  uint8_t loop;
} ShowduinoLampSoundMap;

static const ShowduinoLampSoundMap SHOWDUINO_LAMP_SOUND_TABLE[] = {
  { SHOWDUINO_LAMP_SND_NONE,        "NONE",        "",                 0, 0 },
  { SHOWDUINO_LAMP_SND_STRIKE,      "STRIKE",      "flick.mp3",        1, 0 },
  { SHOWDUINO_LAMP_SND_IGNITION,    "IGNITION",    "fire_ignite.mp3",  2, 0 },
  { SHOWDUINO_LAMP_SND_BURN_LOOP,   "BURN_LOOP",   "flameloop.mp3",    3, 1 },
  { SHOWDUINO_LAMP_SND_EMERGENCY,   "EMERGENCY",   "emergency.mp3",    4, 1 }
};

#define SHOWDUINO_LAMP_V1_FILE_COUNT 4
#define SHOWDUINO_LAMP_FILE_QUERY_STATUS "UNSUPPORTED"

static const char * const SHOWDUINO_LAMP_V1_FILES[SHOWDUINO_LAMP_V1_FILE_COUNT] = {
  "flick.mp3",
  "fire_ignite.mp3",
  "flameloop.mp3",
  "emergency.mp3"
};

#define SHOWDUINO_LAMP_SOUND_TABLE_LEN \
  (sizeof(SHOWDUINO_LAMP_SOUND_TABLE) / sizeof(SHOWDUINO_LAMP_SOUND_TABLE[0]))

static inline const char *showduino_carbide_state_name(ShowduinoCarbideState st) {
  switch (st) {
    case SHOWDUINO_CARBIDE_STRIKING: return "STRIKING";
    case SHOWDUINO_CARBIDE_IGNITING: return "IGNITING";
    case SHOWDUINO_CARBIDE_BURNING: return "BURNING";
    case SHOWDUINO_CARBIDE_LOW_FLAME: return "LOW_FLAME";
    case SHOWDUINO_CARBIDE_UNSTABLE: return "UNSTABLE";
    case SHOWDUINO_CARBIDE_FLARE: return "FLARE";
    case SHOWDUINO_CARBIDE_EXTINGUISHING: return "EXTINGUISHING";
    default: return "OFF";
  }
}

static inline int showduino_carbide_is_lit(ShowduinoCarbideState st) {
  return st == SHOWDUINO_CARBIDE_STRIKING ||
         st == SHOWDUINO_CARBIDE_IGNITING ||
         st == SHOWDUINO_CARBIDE_BURNING ||
         st == SHOWDUINO_CARBIDE_LOW_FLAME ||
         st == SHOWDUINO_CARBIDE_UNSTABLE ||
         st == SHOWDUINO_CARBIDE_FLARE ||
         st == SHOWDUINO_CARBIDE_EXTINGUISHING;
}

static inline int showduino_carbide_is_flame(ShowduinoCarbideState st) {
  return st == SHOWDUINO_CARBIDE_BURNING ||
         st == SHOWDUINO_CARBIDE_LOW_FLAME ||
         st == SHOWDUINO_CARBIDE_UNSTABLE ||
         st == SHOWDUINO_CARBIDE_FLARE;
}

typedef enum ShowduinoCarbideVisual {
  SHOWDUINO_CARBIDE_VIS_BLACK = 0,
  SHOWDUINO_CARBIDE_VIS_FLINT_CORE,
  SHOWDUINO_CARBIDE_VIS_FLINT_NEAR,
  SHOWDUINO_CARBIDE_VIS_FLINT_DARK,
  SHOWDUINO_CARBIDE_VIS_CATCH_A,
  SHOWDUINO_CARBIDE_VIS_CATCH_B,
  SHOWDUINO_CARBIDE_VIS_CATCH_DIP,
  SHOWDUINO_CARBIDE_VIS_CATCH_C,
  SHOWDUINO_CARBIDE_VIS_CATCH_D,
  SHOWDUINO_CARBIDE_VIS_BURN,
  SHOWDUINO_CARBIDE_VIS_PUFF,
  SHOWDUINO_CARBIDE_VIS_RECOVER,
  SHOWDUINO_CARBIDE_VIS_EXT_OUTER,
  SHOWDUINO_CARBIDE_VIS_EXT_CORE,
  SHOWDUINO_CARBIDE_VIS_EXT_REMNANT,
  SHOWDUINO_CARBIDE_VIS_EXT_EMBER,
  SHOWDUINO_CARBIDE_VIS_EMERGENCY,
  SHOWDUINO_CARBIDE_VIS_IDENTIFY
} ShowduinoCarbideVisual;

static inline const char *showduino_carbide_visual_name(ShowduinoCarbideVisual v) {
  switch (v) {
    case SHOWDUINO_CARBIDE_VIS_FLINT_CORE: return "FLINT_CORE";
    case SHOWDUINO_CARBIDE_VIS_FLINT_NEAR: return "FLINT_NEAR";
    case SHOWDUINO_CARBIDE_VIS_FLINT_DARK: return "FLINT_DARK";
    case SHOWDUINO_CARBIDE_VIS_CATCH_A: return "CATCH_A";
    case SHOWDUINO_CARBIDE_VIS_CATCH_B: return "CATCH_B";
    case SHOWDUINO_CARBIDE_VIS_CATCH_DIP: return "CATCH_DIP";
    case SHOWDUINO_CARBIDE_VIS_CATCH_C: return "CATCH_C";
    case SHOWDUINO_CARBIDE_VIS_CATCH_D: return "CATCH_D";
    case SHOWDUINO_CARBIDE_VIS_BURN: return "BURN";
    case SHOWDUINO_CARBIDE_VIS_PUFF: return "PUFF";
    case SHOWDUINO_CARBIDE_VIS_RECOVER: return "RECOVER";
    case SHOWDUINO_CARBIDE_VIS_EXT_OUTER: return "EXT_OUTER";
    case SHOWDUINO_CARBIDE_VIS_EXT_CORE: return "EXT_CORE";
    case SHOWDUINO_CARBIDE_VIS_EXT_REMNANT: return "EXT_REMNANT";
    case SHOWDUINO_CARBIDE_VIS_EXT_EMBER: return "EXT_EMBER";
    case SHOWDUINO_CARBIDE_VIS_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_CARBIDE_VIS_IDENTIFY: return "IDENTIFY";
    default: return "BLACK";
  }
}

static inline uint32_t showduino_carbide_elapsed_ms(const ShowduinoCarbideMachine *m) {
  if (!m || m->nowMs < m->enteredMs) return 0;
  return m->nowMs - m->enteredMs;
}

static inline uint8_t showduino_jewel_core_index(uint8_t core) {
  return (core < SHOWDUINO_CARBIDE_JEWEL_PIXELS) ? core : 0;
}

static inline uint8_t showduino_jewel_ring_index(uint8_t core, uint8_t ringI) {
  uint8_t i;
  uint8_t n = 0;
  core = showduino_jewel_core_index(core);
  ringI = (uint8_t)(ringI % 6u);
  for (i = 0; i < SHOWDUINO_CARBIDE_JEWEL_PIXELS; i++) {
    if (i == core) continue;
    if (n == ringI) return i;
    n++;
  }
  return 0;
}

static inline int showduino_carbide_visual_is_black(ShowduinoCarbideVisual v) {
  return v == SHOWDUINO_CARBIDE_VIS_BLACK || v == SHOWDUINO_CARBIDE_VIS_FLINT_DARK;
}

static inline ShowduinoCarbideVisual showduino_carbide_visual(
    ShowduinoCarbideState st,
    uint32_t elapsedMs,
    uint32_t igniteMs,
    uint32_t extinguishMs,
    uint8_t puffStruggle,
    uint8_t recovering,
    uint8_t emergency,
    int identifyPixel) {
  uint32_t a, b, dip, c;
  uint32_t o, k, r;
  if (emergency) return SHOWDUINO_CARBIDE_VIS_EMERGENCY;
  if (identifyPixel >= 0) return SHOWDUINO_CARBIDE_VIS_IDENTIFY;
  if (st == SHOWDUINO_CARBIDE_OFF) return SHOWDUINO_CARBIDE_VIS_BLACK;
  if (st == SHOWDUINO_CARBIDE_STRIKING) {
    if (elapsedMs < SHOWDUINO_CARBIDE_FLINT_CORE_MS) {
      return SHOWDUINO_CARBIDE_VIS_FLINT_CORE;
    }
    if (elapsedMs < (SHOWDUINO_CARBIDE_FLINT_CORE_MS +
                     SHOWDUINO_CARBIDE_FLINT_NEAR_MS)) {
      return SHOWDUINO_CARBIDE_VIS_FLINT_NEAR;
    }
    return SHOWDUINO_CARBIDE_VIS_FLINT_DARK;
  }
  if (st == SHOWDUINO_CARBIDE_IGNITING) {
    if (igniteMs == 0) igniteMs = SHOWDUINO_CARBIDE_IGNITE_MS;
    a = (igniteMs * 18u) / 100u;
    b = (igniteMs * 26u) / 100u;
    dip = (igniteMs * 10u) / 100u;
    c = (igniteMs * 26u) / 100u;
    if (elapsedMs < a) return SHOWDUINO_CARBIDE_VIS_CATCH_A;
    if (elapsedMs < a + b) return SHOWDUINO_CARBIDE_VIS_CATCH_B;
    if (elapsedMs < a + b + dip) return SHOWDUINO_CARBIDE_VIS_CATCH_DIP;
    if (elapsedMs < a + b + dip + c) return SHOWDUINO_CARBIDE_VIS_CATCH_C;
    return SHOWDUINO_CARBIDE_VIS_CATCH_D;
  }
  if (st == SHOWDUINO_CARBIDE_EXTINGUISHING) {
    if (extinguishMs == 0) extinguishMs = SHOWDUINO_CARBIDE_EXTINGUISH_MS;
    o = (extinguishMs * 28u) / 100u;
    k = (extinguishMs * 28u) / 100u;
    r = (extinguishMs * 24u) / 100u;
    if (elapsedMs < o) return SHOWDUINO_CARBIDE_VIS_EXT_OUTER;
    if (elapsedMs < o + k) return SHOWDUINO_CARBIDE_VIS_EXT_CORE;
    if (elapsedMs < o + k + r) return SHOWDUINO_CARBIDE_VIS_EXT_REMNANT;
    return SHOWDUINO_CARBIDE_VIS_EXT_EMBER;
  }
  if (puffStruggle) return SHOWDUINO_CARBIDE_VIS_PUFF;
  if (recovering) return SHOWDUINO_CARBIDE_VIS_RECOVER;
  if (showduino_carbide_is_flame(st)) return SHOWDUINO_CARBIDE_VIS_BURN;
  return SHOWDUINO_CARBIDE_VIS_BLACK;
}

static inline ShowduinoCarbideConfig showduino_carbide_config_defaults(void) {
  ShowduinoCarbideConfig c;
  c.strikeMs = SHOWDUINO_CARBIDE_STRIKE_MS;
  c.igniteMs = SHOWDUINO_CARBIDE_IGNITE_MS;
  c.extinguishMs = SHOWDUINO_CARBIDE_EXTINGUISH_MS;
  c.flareMs = SHOWDUINO_CARBIDE_FLARE_MS;
  c.failStrikePercent = SHOWDUINO_CARBIDE_FAIL_STRIKE_OFF;
  return c;
}

static inline void showduino_carbide_reset(ShowduinoCarbideMachine *m, uint32_t nowMs) {
  if (!m) return;
  memset(m, 0, sizeof(*m));
  m->state = SHOWDUINO_CARBIDE_OFF;
  m->enteredMs = nowMs;
  m->nowMs = nowMs;
  m->lastRoll = 255;
  m->sound = SHOWDUINO_LAMP_SND_NONE;
}

static inline ShowduinoLampSound showduino_carbide_sound_for_state(
    ShowduinoCarbideState st) {
  switch (st) {
    case SHOWDUINO_CARBIDE_STRIKING: return SHOWDUINO_LAMP_SND_STRIKE;
    case SHOWDUINO_CARBIDE_IGNITING: return SHOWDUINO_LAMP_SND_IGNITION;
    case SHOWDUINO_CARBIDE_BURNING:
    case SHOWDUINO_CARBIDE_LOW_FLAME:
    case SHOWDUINO_CARBIDE_UNSTABLE:
    case SHOWDUINO_CARBIDE_FLARE:
      return SHOWDUINO_LAMP_SND_BURN_LOOP;
    default:
      return SHOWDUINO_LAMP_SND_NONE;
  }
}

static inline uint8_t showduino_lamp_sound_loops(ShowduinoLampSound sound) {
  return sound == SHOWDUINO_LAMP_SND_BURN_LOOP ||
         sound == SHOWDUINO_LAMP_SND_EMERGENCY;
}

/* Carbide states use semantic roles only. Filenames live in the table above.
 * sound/loop arguments are ignored so later reserved roles cannot leak files
 * into the machine. */
static inline void showduino_carbide_enter(ShowduinoCarbideMachine *m,
                                           ShowduinoCarbideState st,
                                           uint32_t nowMs,
                                           ShowduinoLampSound sound,
                                           uint8_t loop) {
  const ShowduinoLampSound actual = showduino_carbide_sound_for_state(st);
  (void)sound;
  (void)loop;
  m->state = st;
  m->enteredMs = nowMs;
  m->nowMs = nowMs;
  m->sound = actual;
  m->soundLoop = showduino_lamp_sound_loops(actual);
}

static inline ShowduinoLampSound showduino_lamp_effective_sound(
    ShowduinoLampSound theatrical, int emergencyActive) {
  return emergencyActive ? SHOWDUINO_LAMP_SND_EMERGENCY : theatrical;
}

static inline ShowduinoCarbideEvent showduino_carbide_event_from_cmd(
    ShowduinoLampCmd cmd, ShowduinoLampFx fx) {
  if (cmd == SHOWDUINO_LAMP_CMD_IGNITE)
    return SHOWDUINO_CARBIDE_EV_IGNITE;
  if (cmd == SHOWDUINO_LAMP_CMD_EXTINGUISH ||
      cmd == SHOWDUINO_LAMP_CMD_OFF || cmd == SHOWDUINO_LAMP_CMD_STOP)
    return SHOWDUINO_CARBIDE_EV_EXTINGUISH;
  if (cmd != SHOWDUINO_LAMP_CMD_FX) return SHOWDUINO_CARBIDE_EV_NONE;
  switch (fx) {
    case SHOWDUINO_LAMP_FX_LOW_FLAME: return SHOWDUINO_CARBIDE_EV_LOW_FLAME;
    case SHOWDUINO_LAMP_FX_UNSTABLE: return SHOWDUINO_CARBIDE_EV_UNSTABLE;
    case SHOWDUINO_LAMP_FX_FLARE: return SHOWDUINO_CARBIDE_EV_FLARE;
    case SHOWDUINO_LAMP_FX_STEADY_FLAME:
    case SHOWDUINO_LAMP_FX_CARBIDE_FLAME:
    case SHOWDUINO_LAMP_FX_MINER_FLAME:
      return SHOWDUINO_CARBIDE_EV_STEADY;
    case SHOWDUINO_LAMP_FX_DYING_FLAME:
    case SHOWDUINO_LAMP_FX_LOW_FUEL:
      return SHOWDUINO_CARBIDE_EV_LOW_FLAME;
    case SHOWDUINO_LAMP_FX_CARBIDE_FLUTTER:
    case SHOWDUINO_LAMP_FX_GAS_LEAK:
      return SHOWDUINO_CARBIDE_EV_UNSTABLE;
    default: return SHOWDUINO_CARBIDE_EV_STEADY;
  }
}

static inline void showduino_carbide_apply(ShowduinoCarbideMachine *m,
                                           ShowduinoCarbideEvent ev,
                                           const ShowduinoCarbideConfig *cfg) {
  ShowduinoCarbideConfig d;
  uint32_t now;
  if (!m) return;
  if (!cfg) {
    d = showduino_carbide_config_defaults();
    cfg = &d;
  }
  now = m->nowMs;

  if (ev == SHOWDUINO_CARBIDE_EV_FORCE_OFF) {
    showduino_carbide_enter(m, SHOWDUINO_CARBIDE_OFF, now,
                            SHOWDUINO_LAMP_SND_NONE, 0);
    return;
  }

  if (ev == SHOWDUINO_CARBIDE_EV_IGNITE) {
    if (m->state == SHOWDUINO_CARBIDE_OFF ||
        m->state == SHOWDUINO_CARBIDE_EXTINGUISHING) {
      showduino_carbide_enter(m, SHOWDUINO_CARBIDE_STRIKING, now,
                              SHOWDUINO_LAMP_SND_STRIKE, 0);
    }
    return;
  }

  if (ev == SHOWDUINO_CARBIDE_EV_EXTINGUISH) {
    if (m->state != SHOWDUINO_CARBIDE_OFF &&
        m->state != SHOWDUINO_CARBIDE_EXTINGUISHING) {
      showduino_carbide_enter(m, SHOWDUINO_CARBIDE_EXTINGUISHING, now,
                              SHOWDUINO_LAMP_SND_NONE, 0);
    }
    return;
  }

  if (ev == SHOWDUINO_CARBIDE_EV_BLOW) {
    if (showduino_carbide_is_flame(m->state) ||
        m->state == SHOWDUINO_CARBIDE_IGNITING) {
      showduino_carbide_enter(m, SHOWDUINO_CARBIDE_EXTINGUISHING, now,
                              SHOWDUINO_LAMP_SND_NONE, 0);
    }
    return;
  }

  if (ev == SHOWDUINO_CARBIDE_EV_PUFF) {
    if (showduino_carbide_is_flame(m->state)) {
      m->beforePuff = m->state;
      showduino_carbide_enter(m, SHOWDUINO_CARBIDE_UNSTABLE, now,
                              SHOWDUINO_LAMP_SND_NONE, 0);
    }
    return;
  }

  if (ev == SHOWDUINO_CARBIDE_EV_BLOW_END) {
    if (m->state == SHOWDUINO_CARBIDE_UNSTABLE &&
        showduino_carbide_is_flame(m->beforePuff)) {
      showduino_carbide_enter(m, m->beforePuff, now,
                              SHOWDUINO_LAMP_SND_BURN_LOOP, 1);
    }
    return;
  }

  if (ev == SHOWDUINO_CARBIDE_EV_LOW_FLAME && showduino_carbide_is_flame(m->state)) {
    showduino_carbide_enter(m, SHOWDUINO_CARBIDE_LOW_FLAME, now,
                            SHOWDUINO_LAMP_SND_BURN_LOOP, 1);
    return;
  }
  if (ev == SHOWDUINO_CARBIDE_EV_UNSTABLE && showduino_carbide_is_flame(m->state)) {
    showduino_carbide_enter(m, SHOWDUINO_CARBIDE_UNSTABLE, now,
                            SHOWDUINO_LAMP_SND_BURN_LOOP, 1);
    return;
  }
  if (ev == SHOWDUINO_CARBIDE_EV_FLARE && showduino_carbide_is_flame(m->state)) {
    showduino_carbide_enter(m, SHOWDUINO_CARBIDE_FLARE, now,
                            SHOWDUINO_LAMP_SND_BURN_LOOP, 0);
    return;
  }
  if (ev == SHOWDUINO_CARBIDE_EV_STEADY && showduino_carbide_is_flame(m->state)) {
    showduino_carbide_enter(m, SHOWDUINO_CARBIDE_BURNING, now,
                            SHOWDUINO_LAMP_SND_BURN_LOOP, 1);
    return;
  }

  if (ev != SHOWDUINO_CARBIDE_EV_TICK) return;

  if (m->state == SHOWDUINO_CARBIDE_STRIKING &&
      (now - m->enteredMs) >= cfg->strikeMs) {
    uint8_t roll = m->lastRoll;
    if (roll == 255) roll = 0; /* default: never fail without an injected roll */
    if (cfg->failStrikePercent > 0 && roll < cfg->failStrikePercent) {
      showduino_carbide_enter(m, SHOWDUINO_CARBIDE_OFF, now,
                              SHOWDUINO_LAMP_SND_NONE, 0);
    } else {
      showduino_carbide_enter(m, SHOWDUINO_CARBIDE_IGNITING, now,
                              SHOWDUINO_LAMP_SND_IGNITION, 0);
    }
    return;
  }
  if (m->state == SHOWDUINO_CARBIDE_IGNITING &&
      (now - m->enteredMs) >= cfg->igniteMs) {
    showduino_carbide_enter(m, SHOWDUINO_CARBIDE_BURNING, now,
                            SHOWDUINO_LAMP_SND_BURN_LOOP, 1);
    return;
  }
  if (m->state == SHOWDUINO_CARBIDE_FLARE &&
      (now - m->enteredMs) >= cfg->flareMs) {
    showduino_carbide_enter(m, SHOWDUINO_CARBIDE_BURNING, now,
                            SHOWDUINO_LAMP_SND_BURN_LOOP, 1);
    return;
  }
  if (m->state == SHOWDUINO_CARBIDE_EXTINGUISHING &&
      (now - m->enteredMs) >= cfg->extinguishMs) {
    showduino_carbide_enter(m, SHOWDUINO_CARBIDE_OFF, now,
                            SHOWDUINO_LAMP_SND_NONE, 0);
  }
}

static inline ShowduinoBlowConfig showduino_blow_config_defaults(void) {
  ShowduinoBlowConfig c;
  c.threshold = SHOWDUINO_CARBIDE_DEFAULT_THRESH;
  c.puffMs = SHOWDUINO_CARBIDE_PUFF_MS;
  c.blowMs = SHOWDUINO_CARBIDE_BLOW_MS;
  c.baselineShift = SHOWDUINO_CARBIDE_BASELINE_SHIFT;
  return c;
}

static inline void showduino_blow_reset(ShowduinoBlowDetector *d,
                                        const ShowduinoBlowConfig *cfg) {
  ShowduinoBlowConfig def;
  if (!d) return;
  if (!cfg) {
    def = showduino_blow_config_defaults();
    cfg = &def;
  }
  memset(d, 0, sizeof(*d));
  d->threshold = cfg->threshold;
  d->puffMs = cfg->puffMs;
  d->blowMs = cfg->blowMs;
  d->baselineShift = cfg->baselineShift ? cfg->baselineShift : 1;
}

static inline ShowduinoBlowClass showduino_blow_feed(ShowduinoBlowDetector *d,
                                                    int32_t raw,
                                                    uint32_t dtMs) {
  int32_t delta;
  if (!d) return SHOWDUINO_BLOW_NONE;
  if (dtMs > 50) dtMs = 50;
  d->raw = raw;
  if (!d->armed) {
    d->filtered = raw;
    d->baseline = raw;
    d->armed = 1;
    d->aboveMs = 0;
    d->classified = SHOWDUINO_BLOW_NONE;
    return SHOWDUINO_BLOW_NONE;
  }
  d->filtered = (d->filtered * 3 + raw) / 4;
  if (d->filtered <= d->baseline + (d->threshold / 4)) {
    d->baseline += (d->filtered - d->baseline) / (int32_t)d->baselineShift;
  }
  delta = d->filtered - d->baseline;
  /* Release on raw falling edge so IIR attack lag does not hide a stopped puff. */
  if (raw <= d->baseline + (d->threshold / 2)) {
    const ShowduinoBlowClass was = d->classified;
    d->aboveMs = 0;
    d->classified = SHOWDUINO_BLOW_NONE;
    if (was == SHOWDUINO_BLOW_PUFF) return SHOWDUINO_BLOW_RELEASE;
    return SHOWDUINO_BLOW_NONE;
  }
  if (delta >= d->threshold) {
    d->aboveMs += dtMs;
  } else {
    const ShowduinoBlowClass was = d->classified;
    d->aboveMs = 0;
    d->classified = SHOWDUINO_BLOW_NONE;
    if (was == SHOWDUINO_BLOW_PUFF) return SHOWDUINO_BLOW_RELEASE;
    return SHOWDUINO_BLOW_NONE;
  }
  if (d->aboveMs >= d->blowMs) {
    if (d->classified != SHOWDUINO_BLOW_SUSTAINED) {
      d->classified = SHOWDUINO_BLOW_SUSTAINED;
      return SHOWDUINO_BLOW_SUSTAINED;
    }
    return SHOWDUINO_BLOW_NONE;
  }
  if (d->aboveMs >= d->puffMs) {
    if (d->classified != SHOWDUINO_BLOW_PUFF &&
        d->classified != SHOWDUINO_BLOW_SUSTAINED) {
      d->classified = SHOWDUINO_BLOW_PUFF;
      return SHOWDUINO_BLOW_PUFF;
    }
    return SHOWDUINO_BLOW_NONE;
  }
  return SHOWDUINO_BLOW_NONE;
}

static inline int showduino_blow_detected(const ShowduinoBlowDetector *d) {
  return d && d->classified == SHOWDUINO_BLOW_SUSTAINED;
}

static inline const ShowduinoLampSoundMap *showduino_lamp_sound_info(
    ShowduinoLampSound id) {
  size_t i;
  for (i = 0; i < SHOWDUINO_LAMP_SOUND_TABLE_LEN; ++i) {
    if (SHOWDUINO_LAMP_SOUND_TABLE[i].id == id) return &SHOWDUINO_LAMP_SOUND_TABLE[i];
  }
  return &SHOWDUINO_LAMP_SOUND_TABLE[0];
}

static inline ShowduinoLampSound showduino_lamp_sound_from_role(const char *role) {
  size_t i;
  if (!role) return SHOWDUINO_LAMP_SND_NONE;
  if (strcmp(role, "FLICK") == 0) return SHOWDUINO_LAMP_SND_STRIKE;
  if (strcmp(role, "FLAME_LOOP") == 0) return SHOWDUINO_LAMP_SND_BURN_LOOP;
  for (i = 0; i < SHOWDUINO_LAMP_SOUND_TABLE_LEN; ++i) {
    if (strcmp(SHOWDUINO_LAMP_SOUND_TABLE[i].role, role) == 0) {
      return SHOWDUINO_LAMP_SOUND_TABLE[i].id;
    }
  }
  return SHOWDUINO_LAMP_SND_NONE;
}

static inline int showduino_lamp_v1_file_known(const char *file) {
  size_t i;
  if (!file || !file[0]) return 0;
  for (i = 0; i < SHOWDUINO_LAMP_V1_FILE_COUNT; ++i) {
    if (strcmp(SHOWDUINO_LAMP_V1_FILES[i], file) == 0) return 1;
  }
  return 0;
}

static inline int showduino_lamp_audio_blocks_machine(void) {
  return 0;
}

/* Official DFR0768 PLAYMODE: 1 = repeat one, 3 = play one and pause.
 * PLAYMODE=2 is repeat-all, not a single-file burn loop. */
#define SHOWDUINO_LAMP_FERMION_PLAYMODE_LOOP  1u
#define SHOWDUINO_LAMP_FERMION_PLAYMODE_ONCE  3u
#define SHOWDUINO_LAMP_FERMION_FUNCTION_MUSIC 1u

static inline uint8_t showduino_lamp_fermion_playmode(uint8_t loop) {
  return loop ? (uint8_t)SHOWDUINO_LAMP_FERMION_PLAYMODE_LOOP
              : (uint8_t)SHOWDUINO_LAMP_FERMION_PLAYMODE_ONCE;
}

/* Wiki path form is AT+PLAYFILE=/name.mp3 — a bare filename is rejected. */
static inline int showduino_lamp_fermion_playfile_cmd(const char *file, char *out, size_t n) {
  int wrote;
  if (!file || !file[0] || !out || n < 18) return -1;
  if (file[0] == '/') {
    wrote = snprintf(out, n, "AT+PLAYFILE=%s", file);
  } else {
    wrote = snprintf(out, n, "AT+PLAYFILE=/%s", file);
  }
  if (wrote < 0 || (size_t)wrote >= n) return -1;
  return 0;
}

/* Fermion playback is fire-and-forget AT UART. flameloop.mp3 uses
 * PLAYMODE=1 (repeat one). Firmware must never wait for track completion. */
static inline int showduino_lamp_audio_transport_nonblocking(void) {
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_CARBIDE_LAMP_H */
