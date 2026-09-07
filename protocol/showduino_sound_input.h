#ifndef SHOWDUINO_SOUND_INPUT_H
#define SHOWDUINO_SOUND_INPUT_H

/*
 * Host-testable Audio Node sound-input / trigger engine.
 * No Arduino, I2S, SD, or ESP-NOW side effects.
 *
 * The Audio Node may detect sound. The P4 decides what happens next.
 * Events are logical inputs, never production cues.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "showduino_audio_node.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_SOUND_PROTOCOL           "1.2"
#define SHOWDUINO_SOUND_CALIBRATE_MS       4000u
#define SHOWDUINO_SOUND_CALIBRATE_MIN_MS   2000u
#define SHOWDUINO_SOUND_CALIBRATE_MAX_MS   10000u
#define SHOWDUINO_SOUND_RECORD_MAX_MS      8000u
#define SHOWDUINO_SOUND_RATE_HZ            16000u

typedef enum ShowduinoSoundMode {
  SHOWDUINO_SOUND_MODE_OFF = 0,
  SHOWDUINO_SOUND_MODE_LEVEL,
  SHOWDUINO_SOUND_MODE_TRANSIENT,
  SHOWDUINO_SOUND_MODE_SUSTAINED,
  SHOWDUINO_SOUND_MODE_LEVEL_TRANSIENT,
  SHOWDUINO_SOUND_MODE_ALL
} ShowduinoSoundMode;

typedef enum ShowduinoSoundEventType {
  SHOWDUINO_SOUND_EVT_NONE = 0,
  SHOWDUINO_SOUND_EVT_LEVEL,
  SHOWDUINO_SOUND_EVT_TRANSIENT,
  SHOWDUINO_SOUND_EVT_SUSTAINED,
  SHOWDUINO_SOUND_EVT_QUIET
} ShowduinoSoundEventType;

typedef enum ShowduinoSoundDuplex {
  SHOWDUINO_SOUND_DUPLEX_PLAYBACK_ONLY = 0,
  SHOWDUINO_SOUND_DUPLEX_INPUT_ONLY,
  SHOWDUINO_SOUND_DUPLEX_DUPLEX
} ShowduinoSoundDuplex;

typedef struct ShowduinoSoundInputConfig {
  uint8_t enabled;
  uint8_t mode;
  uint8_t threshold;
  uint8_t thresholdAboveNoiseFloor;
  uint16_t minimumDurationMs;
  uint8_t sustainedThreshold;
  uint16_t sustainedDurationMs;
  uint8_t quietEnabled;
  uint8_t quietThreshold;
  uint16_t quietDurationMs;
  uint16_t cooldownMs;
  uint8_t hysteresis;
  uint16_t postPlaybackInhibitMs;
  uint8_t triggerWhilePlaying;
  uint8_t autoNoiseFloor;
  uint8_t sensitivity;
  uint8_t duplex;
  uint16_t calibrateMs;
} ShowduinoSoundInputConfig;

typedef struct ShowduinoSoundEvent {
  uint8_t type;
  uint32_t id;
  uint32_t atMs;
  uint8_t level;
  uint8_t peak;
  uint8_t noiseFloor;
  uint8_t threshold;
} ShowduinoSoundEvent;

typedef struct ShowduinoSoundEngine {
  ShowduinoSoundInputConfig cfg;
  uint8_t instPeak;
  uint8_t shortRms;
  uint8_t smoothed;
  uint8_t noiseFloor;
  uint32_t aboveMs;
  uint32_t quietMs;
  uint32_t sustainMs;
  uint32_t cooldownUntil;
  uint32_t inhibitUntil;
  uint32_t lastPlaybackStopMs;
  uint32_t lastEventId;
  uint8_t lastEventType;
  uint32_t lastEventMs;
  uint8_t lastEventLevel;
  uint8_t armed;
  uint8_t calibrated;
  uint8_t calibrating;
  uint32_t calStartMs;
  uint32_t calAccum;
  uint32_t calSamples;
  uint8_t wasAbove;
  uint8_t sustainLatched;
  uint8_t quietLatched;
  uint8_t prevPeak;
  uint8_t inputReady;
  uint8_t inputFault;
  uint8_t localTest;
  uint8_t staleBlocked;
  ShowduinoSoundEvent pending;
} ShowduinoSoundEngine;

static inline const char *showduino_sound_event_name(uint8_t t) {
  switch (t) {
    case SHOWDUINO_SOUND_EVT_LEVEL: return "LEVEL";
    case SHOWDUINO_SOUND_EVT_TRANSIENT: return "TRANSIENT";
    case SHOWDUINO_SOUND_EVT_SUSTAINED: return "SUSTAINED";
    case SHOWDUINO_SOUND_EVT_QUIET: return "QUIET";
    default: return "NONE";
  }
}

static inline const char *showduino_sound_event_wire(uint8_t t) {
  switch (t) {
    case SHOWDUINO_SOUND_EVT_LEVEL: return "LEV";
    case SHOWDUINO_SOUND_EVT_TRANSIENT: return "TRAN";
    case SHOWDUINO_SOUND_EVT_SUSTAINED: return "SUST";
    case SHOWDUINO_SOUND_EVT_QUIET: return "QUIET";
    default: return "NONE";
  }
}

static inline uint8_t showduino_sound_event_from_wire(const char *s) {
  if (!s) return SHOWDUINO_SOUND_EVT_NONE;
  if (!strcmp(s, "LEV") || !strcmp(s, "LEVEL")) return SHOWDUINO_SOUND_EVT_LEVEL;
  if (!strcmp(s, "TRAN") || !strcmp(s, "TRANSIENT")) return SHOWDUINO_SOUND_EVT_TRANSIENT;
  if (!strcmp(s, "SUST") || !strcmp(s, "SUSTAINED")) return SHOWDUINO_SOUND_EVT_SUSTAINED;
  if (!strcmp(s, "QUIET")) return SHOWDUINO_SOUND_EVT_QUIET;
  return SHOWDUINO_SOUND_EVT_NONE;
}

static inline const char *showduino_sound_mode_name(uint8_t m) {
  switch (m) {
    case SHOWDUINO_SOUND_MODE_LEVEL: return "LEVEL";
    case SHOWDUINO_SOUND_MODE_TRANSIENT: return "TRANSIENT";
    case SHOWDUINO_SOUND_MODE_SUSTAINED: return "SUSTAINED";
    case SHOWDUINO_SOUND_MODE_LEVEL_TRANSIENT: return "LEVEL_TRANSIENT";
    case SHOWDUINO_SOUND_MODE_ALL: return "ALL";
    default: return "OFF";
  }
}

static inline int showduino_sound_mode_from_name(const char *s, uint8_t *out) {
  if (!s || !out) return 0;
  if (!strcmp(s, "OFF")) { *out = SHOWDUINO_SOUND_MODE_OFF; return 1; }
  if (!strcmp(s, "LEVEL")) { *out = SHOWDUINO_SOUND_MODE_LEVEL; return 1; }
  if (!strcmp(s, "TRANSIENT")) { *out = SHOWDUINO_SOUND_MODE_TRANSIENT; return 1; }
  if (!strcmp(s, "SUSTAINED")) { *out = SHOWDUINO_SOUND_MODE_SUSTAINED; return 1; }
  if (!strcmp(s, "LEVEL_TRANSIENT")) { *out = SHOWDUINO_SOUND_MODE_LEVEL_TRANSIENT; return 1; }
  if (!strcmp(s, "ALL")) { *out = SHOWDUINO_SOUND_MODE_ALL; return 1; }
  return 0;
}

static inline uint8_t showduino_sound_clamp100(int v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return (uint8_t)v;
}

static inline uint8_t showduino_sound_level_from_rms(uint32_t rms) {
  /* 16-bit PCM RMS -> 0-100. Not dB SPL. */
  if (rms >= 12000u) return 100;
  return (uint8_t)((rms * 100u) / 12000u);
}

static inline uint8_t showduino_sound_level_from_peak(uint32_t peak) {
  if (peak >= 20000u) return 100;
  return (uint8_t)((peak * 100u) / 20000u);
}

static inline void showduino_sound_config_defaults(ShowduinoSoundInputConfig *c) {
  if (!c) return;
  memset(c, 0, sizeof(*c));
  c->enabled = 1;
  c->mode = SHOWDUINO_SOUND_MODE_LEVEL_TRANSIENT;
  c->threshold = 70;
  c->thresholdAboveNoiseFloor = 20;
  c->minimumDurationMs = 80;
  c->sustainedThreshold = 70;
  c->sustainedDurationMs = 1500;
  c->quietEnabled = 0;
  c->quietThreshold = 18;
  c->quietDurationMs = 5000;
  c->cooldownMs = 3000;
  c->hysteresis = 10;
  c->postPlaybackInhibitMs = 1000;
  c->triggerWhilePlaying = 0;
  c->autoNoiseFloor = 1;
  c->sensitivity = 5;
  c->duplex = SHOWDUINO_SOUND_DUPLEX_PLAYBACK_ONLY;
  c->calibrateMs = SHOWDUINO_SOUND_CALIBRATE_MS;
}

static inline int showduino_sound_mode_has_level(uint8_t m) {
  return m == SHOWDUINO_SOUND_MODE_LEVEL || m == SHOWDUINO_SOUND_MODE_LEVEL_TRANSIENT ||
         m == SHOWDUINO_SOUND_MODE_ALL;
}
static inline int showduino_sound_mode_has_transient(uint8_t m) {
  return m == SHOWDUINO_SOUND_MODE_TRANSIENT || m == SHOWDUINO_SOUND_MODE_LEVEL_TRANSIENT ||
         m == SHOWDUINO_SOUND_MODE_ALL;
}
static inline int showduino_sound_mode_has_sustained(uint8_t m) {
  return m == SHOWDUINO_SOUND_MODE_SUSTAINED || m == SHOWDUINO_SOUND_MODE_ALL;
}

static inline uint8_t showduino_sound_fire_level(const ShowduinoSoundEngine *e) {
  int v;
  if (!e) return 70;
  if (e->cfg.autoNoiseFloor) {
    v = (int)e->noiseFloor + (int)e->cfg.thresholdAboveNoiseFloor;
    return showduino_sound_clamp100(v);
  }
  return e->cfg.threshold;
}

static inline uint8_t showduino_sound_rearm_level(const ShowduinoSoundEngine *e) {
  int fire = (int)showduino_sound_fire_level(e);
  int h = e ? (int)e->cfg.hysteresis : 10;
  if (fire < h) return 0;
  return (uint8_t)(fire - h);
}

static inline uint8_t showduino_sound_transient_delta(const ShowduinoSoundInputConfig *c) {
  int s = c ? (int)c->sensitivity : 5;
  if (s < 1) s = 1;
  if (s > 10) s = 10;
  return (uint8_t)(12 + (10 - s) * 3);
}

static inline void showduino_sound_engine_init(ShowduinoSoundEngine *e,
                                               const ShowduinoSoundInputConfig *cfg) {
  if (!e) return;
  memset(e, 0, sizeof(*e));
  if (cfg) e->cfg = *cfg;
  else showduino_sound_config_defaults(&e->cfg);
  e->armed = e->cfg.enabled ? 1 : 0;
  e->inputReady = 1;
}

static inline void showduino_sound_engine_set_enabled(ShowduinoSoundEngine *e, int on) {
  if (!e) return;
  e->cfg.enabled = on ? 1 : 0;
  if (!on) e->armed = 0;
}

static inline void showduino_sound_engine_start_calibrate(ShowduinoSoundEngine *e, uint32_t nowMs) {
  if (!e) return;
  e->calibrating = 1;
  e->calStartMs = nowMs;
  e->calAccum = 0;
  e->calSamples = 0;
}

static inline void showduino_sound_engine_note_playback_stop(ShowduinoSoundEngine *e, uint32_t nowMs) {
  if (!e) return;
  e->lastPlaybackStopMs = nowMs;
  e->inhibitUntil = nowMs + e->cfg.postPlaybackInhibitMs;
}

static inline void showduino_sound_engine_clear_stale(ShowduinoSoundEngine *e) {
  if (!e) return;
  e->pending.type = SHOWDUINO_SOUND_EVT_NONE;
  e->staleBlocked = 0;
}

static inline void showduino_sound_emit(ShowduinoSoundEngine *e, uint8_t type, uint32_t nowMs) {
  if (!e || type == SHOWDUINO_SOUND_EVT_NONE) return;
  e->lastEventId++;
  e->lastEventType = type;
  e->lastEventMs = nowMs;
  e->lastEventLevel = e->smoothed;
  e->pending.type = type;
  e->pending.id = e->lastEventId;
  e->pending.atMs = nowMs;
  e->pending.level = e->smoothed;
  e->pending.peak = e->instPeak;
  e->pending.noiseFloor = e->noiseFloor;
  e->pending.threshold = showduino_sound_fire_level(e);
  e->cooldownUntil = nowMs + e->cfg.cooldownMs;
  e->wasAbove = 1;
  if (type == SHOWDUINO_SOUND_EVT_SUSTAINED) e->sustainLatched = 1;
  if (type == SHOWDUINO_SOUND_EVT_QUIET) e->quietLatched = 1;
}

static inline int showduino_sound_engine_take_event(ShowduinoSoundEngine *e,
                                                    ShowduinoSoundEvent *out) {
  if (!e || e->pending.type == SHOWDUINO_SOUND_EVT_NONE) return 0;
  if (out) *out = e->pending;
  e->pending.type = SHOWDUINO_SOUND_EVT_NONE;
  return 1;
}

static inline void showduino_sound_engine_feed(ShowduinoSoundEngine *e,
                                               uint8_t rms, uint8_t peak,
                                               uint32_t nowMs, uint32_t dtMs,
                                               int playing, int emergency, int commsOk) {
  uint8_t fire, rearm, sustainFire;
  int allow;

  if (!e) return;
  if (dtMs == 0) dtMs = 20;
  if (dtMs > 200) dtMs = 200;

  e->instPeak = peak;
  e->shortRms = rms;
  e->smoothed = (uint8_t)(((unsigned)e->smoothed * 7u + (unsigned)rms * 3u) / 10u);

  if (e->calibrating) {
    e->calAccum += e->smoothed;
    e->calSamples++;
    if ((nowMs - e->calStartMs) >= e->cfg.calibrateMs && e->calSamples > 0) {
      e->noiseFloor = (uint8_t)(e->calAccum / e->calSamples);
      e->calibrated = 1;
      e->calibrating = 0;
    }
    e->armed = 0;
    e->prevPeak = peak;
    return;
  }

  if (e->cfg.autoNoiseFloor && e->calibrated && e->smoothed + 4 < e->noiseFloor) {
    /* Slow downward adapt only. Never chase a screaming room upward. */
    if ((nowMs % 800u) < dtMs && e->noiseFloor > 0) e->noiseFloor--;
  } else if (e->cfg.autoNoiseFloor && !e->calibrated) {
    if (e->noiseFloor == 0) e->noiseFloor = e->smoothed;
    else e->noiseFloor = (uint8_t)(((unsigned)e->noiseFloor * 31u + (unsigned)e->smoothed) / 32u);
  }

  allow = e->cfg.enabled && e->cfg.mode != SHOWDUINO_SOUND_MODE_OFF &&
          e->inputReady && !e->inputFault && !emergency && commsOk &&
          !e->calibrating;
  if (playing && !e->cfg.triggerWhilePlaying) allow = 0;
  if (e->inhibitUntil && (int32_t)(nowMs - e->inhibitUntil) < 0) allow = 0;
  if (e->cooldownUntil && (int32_t)(nowMs - e->cooldownUntil) < 0) allow = 0;
  if (!commsOk) {
    e->staleBlocked = 1;
    e->pending.type = SHOWDUINO_SOUND_EVT_NONE;
    e->aboveMs = 0;
    e->sustainMs = 0;
    e->quietMs = 0;
    e->wasAbove = 1;
    allow = 0;
  } else if (e->staleBlocked) {
    e->staleBlocked = 0;
    e->pending.type = SHOWDUINO_SOUND_EVT_NONE;
    e->aboveMs = 0;
    e->sustainMs = 0;
    e->quietMs = 0;
    e->wasAbove = 1;
  }
  if (emergency) {
    e->pending.type = SHOWDUINO_SOUND_EVT_NONE;
    e->aboveMs = 0;
    e->sustainMs = 0;
    e->quietMs = 0;
    e->wasAbove = 1;
    e->armed = 0;
    e->prevPeak = peak;
    return;
  }

  e->armed = allow ? 1 : 0;
  fire = showduino_sound_fire_level(e);
  rearm = showduino_sound_rearm_level(e);
  sustainFire = e->cfg.autoNoiseFloor ? fire : e->cfg.sustainedThreshold;

  if (e->smoothed < rearm) {
    e->wasAbove = 0;
    if (e->smoothed + 2 < sustainFire) e->sustainLatched = 0;
  }
  if (e->smoothed > (uint8_t)(e->cfg.quietThreshold + e->cfg.hysteresis)) {
    e->quietLatched = 0;
  }

  if (e->smoothed >= fire) e->aboveMs += dtMs;
  else e->aboveMs = 0;

  if (e->smoothed >= sustainFire) e->sustainMs += dtMs;
  else e->sustainMs = 0;

  if (e->cfg.quietEnabled && e->smoothed <= e->cfg.quietThreshold) e->quietMs += dtMs;
  else e->quietMs = 0;

  if (allow) {
    if (showduino_sound_mode_has_transient(e->cfg.mode)) {
      uint8_t need = showduino_sound_transient_delta(&e->cfg);
      int rise = (int)peak - (int)e->noiseFloor;
      int jump = (int)peak - (int)e->prevPeak;
      if (rise >= (int)need && jump >= (int)(need / 2) && peak >= fire) {
        showduino_sound_emit(e, SHOWDUINO_SOUND_EVT_TRANSIENT, nowMs);
        e->prevPeak = peak;
        return;
      }
    }
    if (showduino_sound_mode_has_level(e->cfg.mode) && !e->wasAbove &&
        e->aboveMs >= e->cfg.minimumDurationMs) {
      showduino_sound_emit(e, SHOWDUINO_SOUND_EVT_LEVEL, nowMs);
      e->prevPeak = peak;
      return;
    }
    if (showduino_sound_mode_has_sustained(e->cfg.mode) && !e->sustainLatched &&
        e->sustainMs >= e->cfg.sustainedDurationMs) {
      showduino_sound_emit(e, SHOWDUINO_SOUND_EVT_SUSTAINED, nowMs);
      e->prevPeak = peak;
      return;
    }
    if (e->cfg.quietEnabled && !e->quietLatched && e->quietMs >= e->cfg.quietDurationMs) {
      showduino_sound_emit(e, SHOWDUINO_SOUND_EVT_QUIET, nowMs);
      e->prevPeak = peak;
      return;
    }
  }

  e->prevPeak = peak;
}

static inline uint32_t showduino_sound_cooldown_remaining(const ShowduinoSoundEngine *e,
                                                          uint32_t nowMs) {
  if (!e || !e->cooldownUntil) return 0;
  if ((int32_t)(e->cooldownUntil - nowMs) <= 0) return 0;
  return e->cooldownUntil - nowMs;
}

static inline const char *showduino_sound_json_section(const char *json) {
  const char *p = showduino_audio_json_after_key(json, "soundInput");
  if (!p) return NULL;
  if (*p != '{') return NULL;
  return p;
}

static inline ShowduinoAudioCfgStatus showduino_sound_config_parse(const char *json,
                                                                  ShowduinoSoundInputConfig *out) {
  ShowduinoSoundInputConfig tmp;
  const char *sec;
  const char *p;
  if (!json || !out) return SHOWDUINO_AUDIO_CFG_BAD;
  showduino_sound_config_defaults(&tmp);
  sec = showduino_sound_json_section(json);
  if (!sec) {
    *out = tmp;
    return SHOWDUINO_AUDIO_CFG_OK;
  }

  p = showduino_audio_json_after_key(sec, "enabled");
  if (p) {
    if (!strncmp(p, "true", 4)) tmp.enabled = 1;
    else if (!strncmp(p, "false", 5)) tmp.enabled = 0;
    else {
      int v = 0;
      if (!showduino_audio_parse_u32(p, 1, &v) || (v != 0 && v != 1)) {
        return SHOWDUINO_AUDIO_CFG_BAD;
      }
      tmp.enabled = (uint8_t)v;
    }
  }
  p = showduino_audio_json_after_key(sec, "mode");
  if (p) {
    char name[20];
    size_t i = 0;
    if (*p != '"') return SHOWDUINO_AUDIO_CFG_BAD;
    p++;
    while (*p && *p != '"' && i + 1 < sizeof(name)) name[i++] = *p++;
    name[i] = '\0';
    if (*p != '"' || !showduino_sound_mode_from_name(name, &tmp.mode)) {
      return SHOWDUINO_AUDIO_CFG_BAD;
    }
  }
  p = showduino_audio_json_after_key(sec, "threshold");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 3, &v) || v > 100) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.threshold = (uint8_t)v;
  }
  p = showduino_audio_json_after_key(sec, "thresholdAboveNoiseFloor");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 3, &v) || v > 100) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.thresholdAboveNoiseFloor = (uint8_t)v;
  }
  p = showduino_audio_json_after_key(sec, "minimumDurationMs");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 5, &v) || v < 20 || v > 5000) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.minimumDurationMs = (uint16_t)v;
  }
  p = showduino_audio_json_after_key(sec, "sustainedDurationMs");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 5, &v) || v < 200 || v > 30000) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.sustainedDurationMs = (uint16_t)v;
  }
  p = showduino_audio_json_after_key(sec, "quietDurationMs");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 5, &v) || v < 200 || v > 60000) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.quietDurationMs = (uint16_t)v;
  }
  p = showduino_audio_json_after_key(sec, "cooldownMs");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 5, &v) || v < 200 || v > 30000) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.cooldownMs = (uint16_t)v;
  }
  p = showduino_audio_json_after_key(sec, "hysteresis");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 3, &v) || v > 50) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.hysteresis = (uint8_t)v;
  }
  p = showduino_audio_json_after_key(sec, "postPlaybackInhibitMs");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 5, &v) || v > 10000) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.postPlaybackInhibitMs = (uint16_t)v;
  }
  p = showduino_audio_json_after_key(sec, "triggerWhilePlaying");
  if (p) {
    if (!strncmp(p, "true", 4)) tmp.triggerWhilePlaying = 1;
    else if (!strncmp(p, "false", 5)) tmp.triggerWhilePlaying = 0;
    else {
      int v = 0;
      if (!showduino_audio_parse_u32(p, 1, &v) || (v != 0 && v != 1)) {
        return SHOWDUINO_AUDIO_CFG_BAD;
      }
      tmp.triggerWhilePlaying = (uint8_t)v;
    }
  }
  p = showduino_audio_json_after_key(sec, "autoNoiseFloor");
  if (p) {
    if (!strncmp(p, "true", 4)) tmp.autoNoiseFloor = 1;
    else if (!strncmp(p, "false", 5)) tmp.autoNoiseFloor = 0;
    else {
      int v = 0;
      if (!showduino_audio_parse_u32(p, 1, &v) || (v != 0 && v != 1)) {
        return SHOWDUINO_AUDIO_CFG_BAD;
      }
      tmp.autoNoiseFloor = (uint8_t)v;
    }
  }
  p = showduino_audio_json_after_key(sec, "quietEnabled");
  if (p) {
    if (!strncmp(p, "true", 4)) tmp.quietEnabled = 1;
    else if (!strncmp(p, "false", 5)) tmp.quietEnabled = 0;
    else {
      int v = 0;
      if (!showduino_audio_parse_u32(p, 1, &v) || (v != 0 && v != 1)) {
        return SHOWDUINO_AUDIO_CFG_BAD;
      }
      tmp.quietEnabled = (uint8_t)v;
    }
  }
  p = showduino_audio_json_after_key(sec, "sensitivity");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 2, &v) || v < 1 || v > 10) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.sensitivity = (uint8_t)v;
  }
  p = showduino_audio_json_after_key(sec, "sustainedThreshold");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 3, &v) || v > 100) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.sustainedThreshold = (uint8_t)v;
  }
  *out = tmp;
  return SHOWDUINO_AUDIO_CFG_OK;
}

static inline int showduino_sound_format_status(char *out, size_t n,
                                                const char *readyTok,
                                                uint8_t level, uint8_t peak,
                                                uint8_t floor, uint8_t th,
                                                int armed, uint32_t coolMs,
                                                const char *evtWire, int cal) {
  if (!out || n == 0) return 0;
  return snprintf(out, n, "SOUND:STATUS:%s,L=%u,P=%u,F=%u,T=%u,A=%d,C=%lu,E=%s,K=%d",
                  readyTok ? readyTok : "OFF",
                  (unsigned)level, (unsigned)peak, (unsigned)floor, (unsigned)th,
                  armed ? 1 : 0, (unsigned long)coolMs,
                  evtWire ? evtWire : "NONE", cal ? 1 : 0);
}

static inline int showduino_sound_format_trigger(char *out, size_t n,
                                                 const ShowduinoSoundEvent *ev) {
  if (!out || n == 0 || !ev) return 0;
  return snprintf(out, n, "SOUND:TRIGGER:%s,ID=%lu,L=%u,P=%u,F=%u,TH=%u",
                  showduino_sound_event_name(ev->type),
                  (unsigned long)ev->id,
                  (unsigned)ev->level, (unsigned)ev->peak,
                  (unsigned)ev->noiseFloor, (unsigned)ev->threshold);
}

#ifdef __cplusplus
}
#endif

#endif
