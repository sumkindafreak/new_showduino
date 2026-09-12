#ifndef SHOWDUINO_LAMP_MOTION_H
#define SHOWDUINO_LAMP_MOTION_H

/*
 * Host-testable digital motion input for the S3 Lamp Node.
 *
 * PHYSICAL INPUT → debounce/edge → MOTION_ACTIVE / MOTION_CLEAR
 *     → authority check → optional carbide event
 *
 * Motion is never hardwired to IGNITE. Theatrical mapping is configuration.
 * Do not treat this as a generic Showduino input router.
 */

#include <string.h>
#include "showduino_carbide_lamp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_MOTION_DEFAULT_DEBOUNCE_MS  50u
#define SHOWDUINO_MOTION_DEFAULT_COOLDOWN_MS  3000u
#define SHOWDUINO_MOTION_DEFAULT_ENABLED      0
#define SHOWDUINO_MOTION_DEFAULT_ACTIVE_LOW   0
#define SHOWDUINO_MOTION_COOLDOWN_MAX_MS      60000u

typedef enum ShowduinoMotionAction {
  SHOWDUINO_MOTION_ACT_DISABLED = 0,
  SHOWDUINO_MOTION_ACT_IGNITE = 1,
  SHOWDUINO_MOTION_ACT_FLARE = 2,
  SHOWDUINO_MOTION_ACT_UNSTABLE = 3
} ShowduinoMotionAction;

typedef enum ShowduinoMotionEvent {
  SHOWDUINO_MOTION_EV_NONE = 0,
  SHOWDUINO_MOTION_EV_ACTIVE = 1,
  SHOWDUINO_MOTION_EV_CLEAR = 2
} ShowduinoMotionEvent;

typedef enum ShowduinoMotionDecision {
  SHOWDUINO_MOTION_DECIDE_NONE = 0,
  SHOWDUINO_MOTION_DECIDE_TELEMETRY,
  SHOWDUINO_MOTION_DECIDE_IGNITE,
  SHOWDUINO_MOTION_DECIDE_FLARE,
  SHOWDUINO_MOTION_DECIDE_UNSTABLE,
  SHOWDUINO_MOTION_DECIDE_BLOCKED_OWNER,
  SHOWDUINO_MOTION_DECIDE_BLOCKED_EMERGENCY,
  SHOWDUINO_MOTION_DECIDE_BLOCKED_UNCONFIRMED
} ShowduinoMotionDecision;

typedef struct ShowduinoMotionDetector {
  uint8_t rawHigh;
  uint8_t logical;
  uint8_t stable;
  uint8_t lastRaw;
  uint8_t armed;
  uint8_t seeded;
  uint32_t debounceMs;
  uint32_t cooldownMs;
  uint32_t lastChangeMs;
  uint32_t activeSinceMs;
  uint32_t lastActiveMs;
  uint32_t nowMs;
} ShowduinoMotionDetector;

static inline const char *showduino_motion_action_name(ShowduinoMotionAction a) {
  switch (a) {
    case SHOWDUINO_MOTION_ACT_IGNITE: return "IGNITE";
    case SHOWDUINO_MOTION_ACT_FLARE: return "FLARE";
    case SHOWDUINO_MOTION_ACT_UNSTABLE: return "UNSTABLE";
    default: return "DISABLED";
  }
}

static inline ShowduinoMotionAction showduino_motion_action_from_name(const char *s) {
  if (!s || !s[0]) return SHOWDUINO_MOTION_ACT_DISABLED;
  if (strcmp(s, "IGNITE") == 0) return SHOWDUINO_MOTION_ACT_IGNITE;
  if (strcmp(s, "FLARE") == 0) return SHOWDUINO_MOTION_ACT_FLARE;
  if (strcmp(s, "UNSTABLE") == 0) return SHOWDUINO_MOTION_ACT_UNSTABLE;
  return SHOWDUINO_MOTION_ACT_DISABLED;
}

static inline ShowduinoMotionAction showduino_motion_action_from_u8(uint8_t v) {
  if (v == (uint8_t)SHOWDUINO_MOTION_ACT_IGNITE) return SHOWDUINO_MOTION_ACT_IGNITE;
  if (v == (uint8_t)SHOWDUINO_MOTION_ACT_FLARE) return SHOWDUINO_MOTION_ACT_FLARE;
  if (v == (uint8_t)SHOWDUINO_MOTION_ACT_UNSTABLE) return SHOWDUINO_MOTION_ACT_UNSTABLE;
  return SHOWDUINO_MOTION_ACT_DISABLED;
}

static inline const char *showduino_motion_polarity_name(uint8_t activeLow) {
  return activeLow ? "ACTIVE LOW" : "ACTIVE HIGH";
}

static inline const char *showduino_motion_decision_name(ShowduinoMotionDecision d) {
  switch (d) {
    case SHOWDUINO_MOTION_DECIDE_TELEMETRY: return "TELEMETRY";
    case SHOWDUINO_MOTION_DECIDE_IGNITE: return "IGNITE";
    case SHOWDUINO_MOTION_DECIDE_FLARE: return "FLARE";
    case SHOWDUINO_MOTION_DECIDE_UNSTABLE: return "UNSTABLE";
    case SHOWDUINO_MOTION_DECIDE_BLOCKED_OWNER: return "BLOCKED_OWNER";
    case SHOWDUINO_MOTION_DECIDE_BLOCKED_EMERGENCY: return "BLOCKED_EMERGENCY";
    case SHOWDUINO_MOTION_DECIDE_BLOCKED_UNCONFIRMED: return "BLOCKED_UNCONFIRMED";
    default: return "NONE";
  }
}

static inline uint8_t showduino_motion_logical(uint8_t rawHigh, uint8_t activeLow) {
  const uint8_t high = rawHigh ? 1u : 0u;
  return activeLow ? (uint8_t)!high : high;
}

static inline void showduino_motion_reset(ShowduinoMotionDetector *d,
                                          uint32_t debounceMs,
                                          uint32_t cooldownMs) {
  if (!d) return;
  memset(d, 0, sizeof(*d));
  d->debounceMs = debounceMs ? debounceMs : SHOWDUINO_MOTION_DEFAULT_DEBOUNCE_MS;
  if (cooldownMs > SHOWDUINO_MOTION_COOLDOWN_MAX_MS) {
    cooldownMs = SHOWDUINO_MOTION_COOLDOWN_MAX_MS;
  }
  d->cooldownMs = cooldownMs;
  d->armed = 1;
}

static inline uint32_t showduino_motion_active_ms(const ShowduinoMotionDetector *d) {
  if (!d || !d->stable) return 0;
  if (d->nowMs < d->activeSinceMs) return 0;
  return d->nowMs - d->activeSinceMs;
}

static inline int showduino_motion_cooldown_hold(const ShowduinoMotionDetector *d) {
  if (!d || d->lastActiveMs == 0 || d->cooldownMs == 0) return 0;
  return (d->nowMs - d->lastActiveMs) < d->cooldownMs;
}

static inline uint32_t showduino_motion_cooldown_remain_ms(const ShowduinoMotionDetector *d) {
  if (!showduino_motion_cooldown_hold(d)) return 0;
  return d->cooldownMs - (d->nowMs - d->lastActiveMs);
}

static inline ShowduinoMotionEvent showduino_motion_feed(
    ShowduinoMotionDetector *d,
    uint8_t rawHigh,
    uint8_t activeLow,
    uint32_t nowMs) {
  uint8_t raw;
  uint8_t logical;
  if (!d) return SHOWDUINO_MOTION_EV_NONE;
  d->nowMs = nowMs;
  raw = rawHigh ? 1u : 0u;
  logical = showduino_motion_logical(raw, activeLow);
  d->rawHigh = raw;
  d->logical = logical;

  if (!d->seeded) {
    d->seeded = 1;
    d->lastRaw = raw;
    d->stable = logical;
    d->lastChangeMs = nowMs;
    /* Already-active at start must clear before the first MOTION_ACTIVE. */
    d->armed = logical ? 0u : 1u;
    if (logical) d->activeSinceMs = nowMs;
    return SHOWDUINO_MOTION_EV_NONE;
  }

  if (raw != d->lastRaw) {
    d->lastRaw = raw;
    d->lastChangeMs = nowMs;
  }
  if ((nowMs - d->lastChangeMs) < d->debounceMs) return SHOWDUINO_MOTION_EV_NONE;
  if (logical == d->stable) return SHOWDUINO_MOTION_EV_NONE;

  d->stable = logical;
  if (logical) {
    d->activeSinceMs = nowMs;
    if (!d->armed) return SHOWDUINO_MOTION_EV_NONE;
    if (showduino_motion_cooldown_hold(d)) {
      d->armed = 0;
      return SHOWDUINO_MOTION_EV_NONE;
    }
    d->armed = 0;
    d->lastActiveMs = nowMs;
    return SHOWDUINO_MOTION_EV_ACTIVE;
  }

  d->armed = 1;
  return SHOWDUINO_MOTION_EV_CLEAR;
}

static inline int showduino_motion_may_act(ShowduinoLampNodeState st) {
  if (st == SHOWDUINO_LAMP_ST_EMERGENCY) return 0;
  return showduino_lamp_local_authority(st);
}

static inline ShowduinoMotionDecision showduino_motion_decide_ex(
    ShowduinoMotionEvent ev,
    uint8_t enabled,
    ShowduinoMotionAction action,
    ShowduinoLampNodeState owner,
    ShowduinoCarbideState flame,
    uint8_t pinConfirmed) {
  if (ev != SHOWDUINO_MOTION_EV_ACTIVE) return SHOWDUINO_MOTION_DECIDE_NONE;
  if (owner == SHOWDUINO_LAMP_ST_EMERGENCY) {
    return SHOWDUINO_MOTION_DECIDE_BLOCKED_EMERGENCY;
  }
  if (!showduino_lamp_local_authority(owner)) {
    return SHOWDUINO_MOTION_DECIDE_BLOCKED_OWNER;
  }
  if (!enabled || action == SHOWDUINO_MOTION_ACT_DISABLED) {
    return SHOWDUINO_MOTION_DECIDE_TELEMETRY;
  }
  if (!pinConfirmed) return SHOWDUINO_MOTION_DECIDE_BLOCKED_UNCONFIRMED;
  if (action == SHOWDUINO_MOTION_ACT_IGNITE) {
    /* Blow-extinguish must not be an ignite window. Need OFF + a new edge. */
    if (flame == SHOWDUINO_CARBIDE_OFF) return SHOWDUINO_MOTION_DECIDE_IGNITE;
    return SHOWDUINO_MOTION_DECIDE_TELEMETRY;
  }
  if (!showduino_carbide_is_flame(flame)) return SHOWDUINO_MOTION_DECIDE_TELEMETRY;
  if (action == SHOWDUINO_MOTION_ACT_FLARE) return SHOWDUINO_MOTION_DECIDE_FLARE;
  if (action == SHOWDUINO_MOTION_ACT_UNSTABLE) return SHOWDUINO_MOTION_DECIDE_UNSTABLE;
  return SHOWDUINO_MOTION_DECIDE_TELEMETRY;
}

static inline ShowduinoMotionDecision showduino_motion_decide(
    ShowduinoMotionEvent ev,
    uint8_t enabled,
    ShowduinoMotionAction action,
    ShowduinoLampNodeState owner,
    ShowduinoCarbideState flame) {
  return showduino_motion_decide_ex(ev, enabled, action, owner, flame, 1);
}

static inline ShowduinoCarbideEvent showduino_motion_carbide_event(
    ShowduinoMotionDecision d) {
  switch (d) {
    case SHOWDUINO_MOTION_DECIDE_IGNITE: return SHOWDUINO_CARBIDE_EV_IGNITE;
    case SHOWDUINO_MOTION_DECIDE_FLARE: return SHOWDUINO_CARBIDE_EV_FLARE;
    case SHOWDUINO_MOTION_DECIDE_UNSTABLE: return SHOWDUINO_CARBIDE_EV_UNSTABLE;
    default: return SHOWDUINO_CARBIDE_EV_NONE;
  }
}

static inline const char *showduino_motion_protocol_command(ShowduinoMotionDecision d) {
  switch (d) {
    case SHOWDUINO_MOTION_DECIDE_IGNITE: return "LAMP:IGNITE";
    case SHOWDUINO_MOTION_DECIDE_FLARE: return "LAMP:FX:FLARE";
    case SHOWDUINO_MOTION_DECIDE_UNSTABLE: return "LAMP:FX:UNSTABLE";
    default: return "";
  }
}

#ifdef __cplusplus
}
#endif

#endif
