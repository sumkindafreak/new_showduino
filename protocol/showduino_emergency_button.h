#ifndef SHOWDUINO_EMERGENCY_BUTTON_H
#define SHOWDUINO_EMERGENCY_BUTTON_H

/*
 * Host-testable main Showduino Emergency button state.
 *
 * Physical GPIO debounce stays in P4 firmware. This header models the
 * post-debounce press / 8-second Locate hold / pending-clear authorisation.
 *
 * The physical button never clears Emergency.
 */

#include <stdint.h>
#include <string.h>

#ifndef SHOWDUINO_ESTOP_LOCATE_HOLD_MS
#define SHOWDUINO_ESTOP_LOCATE_HOLD_MS 8000UL
#endif

#ifndef SHOWDUINO_ESTOP_CLEAR_REQUEST_TIMEOUT_MS
#define SHOWDUINO_ESTOP_CLEAR_REQUEST_TIMEOUT_MS 12000UL
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
  SHOWDUINO_ESTOP_CLEAR_OK = 0,
  SHOWDUINO_ESTOP_CLEAR_ERR_NOT_LATCHED = 1,
  SHOWDUINO_ESTOP_CLEAR_ERR_BUTTON_ACTIVE = 2,
  SHOWDUINO_ESTOP_CLEAR_ERR_NO_REQUEST = 3,
  SHOWDUINO_ESTOP_CLEAR_ERR_TIMEOUT = 4,
  SHOWDUINO_ESTOP_CLEAR_ERR_SUPERSEDED = 5
};

typedef struct ShowduinoEstopHoldState {
  uint8_t pressed;
  uint8_t locateHoldFired;
  uint32_t pressStartedMs;
} ShowduinoEstopHoldState;

typedef struct ShowduinoEstopHoldEvents {
  uint8_t pressBegan;
  uint8_t released;
  uint8_t locateRequested;
} ShowduinoEstopHoldEvents;

typedef struct ShowduinoEstopClearAuth {
  uint8_t pending;
  uint32_t untilMs;
  uint32_t assertionSeqAtRequest;
} ShowduinoEstopClearAuth;

static inline void showduino_estop_hold_reset(ShowduinoEstopHoldState *s) {
  if (!s) return;
  memset(s, 0, sizeof(*s));
}

static inline void showduino_estop_hold_on_press(ShowduinoEstopHoldState *s,
                                                 uint32_t nowMs,
                                                 ShowduinoEstopHoldEvents *ev) {
  if (!s) return;
  s->pressed = 1;
  s->pressStartedMs = nowMs;
  s->locateHoldFired = 0;
  if (ev) ev->pressBegan = 1;
}

static inline void showduino_estop_hold_on_release(ShowduinoEstopHoldState *s,
                                                   ShowduinoEstopHoldEvents *ev) {
  if (!s) return;
  s->pressed = 0;
  s->pressStartedMs = 0;
  s->locateHoldFired = 0;
  if (ev) ev->released = 1;
}

static inline void showduino_estop_hold_tick(ShowduinoEstopHoldState *s,
                                             uint32_t nowMs,
                                             ShowduinoEstopHoldEvents *ev) {
  if (!s || !s->pressed || s->locateHoldFired) return;
  if ((nowMs - s->pressStartedMs) >= SHOWDUINO_ESTOP_LOCATE_HOLD_MS) {
    s->locateHoldFired = 1;
    if (ev) ev->locateRequested = 1;
  }
}

static inline int showduino_estop_hold_locate_active(const ShowduinoEstopHoldState *s) {
  return (s && s->pressed && !s->locateHoldFired) ? 1 : 0;
}

static inline void showduino_estop_clear_cancel(ShowduinoEstopClearAuth *a) {
  if (!a) return;
  a->pending = 0;
  a->untilMs = 0;
  a->assertionSeqAtRequest = 0;
}

static inline void showduino_estop_clear_reset(ShowduinoEstopClearAuth *a) {
  showduino_estop_clear_cancel(a);
}

static inline int showduino_estop_clear_pending_valid(const ShowduinoEstopClearAuth *a,
                                                      uint32_t nowMs) {
  if (!a || !a->pending) return 0;
  return ((int32_t)(nowMs - a->untilMs) < 0) ? 1 : 0;
}

static inline int showduino_estop_clear_begin(ShowduinoEstopClearAuth *a,
                                              uint32_t nowMs,
                                              uint8_t emergencyLatched,
                                              uint8_t buttonPressed,
                                              uint32_t assertionSeq) {
  if (!a) return SHOWDUINO_ESTOP_CLEAR_ERR_NO_REQUEST;
  if (!emergencyLatched) return SHOWDUINO_ESTOP_CLEAR_ERR_NOT_LATCHED;
  if (buttonPressed) return SHOWDUINO_ESTOP_CLEAR_ERR_BUTTON_ACTIVE;
  a->pending = 1;
  a->untilMs = nowMs + SHOWDUINO_ESTOP_CLEAR_REQUEST_TIMEOUT_MS;
  a->assertionSeqAtRequest = assertionSeq;
  return SHOWDUINO_ESTOP_CLEAR_OK;
}

static inline int showduino_estop_clear_confirm(ShowduinoEstopClearAuth *a,
                                                uint32_t nowMs,
                                                uint8_t emergencyLatched,
                                                uint8_t buttonPressed,
                                                uint32_t assertionSeq) {
  if (!emergencyLatched) {
    showduino_estop_clear_cancel(a);
    return SHOWDUINO_ESTOP_CLEAR_ERR_NOT_LATCHED;
  }
  if (!a || !a->pending) return SHOWDUINO_ESTOP_CLEAR_ERR_NO_REQUEST;
  if (a->assertionSeqAtRequest != assertionSeq) {
    showduino_estop_clear_cancel(a);
    return SHOWDUINO_ESTOP_CLEAR_ERR_SUPERSEDED;
  }
  if (!showduino_estop_clear_pending_valid(a, nowMs)) {
    showduino_estop_clear_cancel(a);
    return SHOWDUINO_ESTOP_CLEAR_ERR_TIMEOUT;
  }
  if (buttonPressed) return SHOWDUINO_ESTOP_CLEAR_ERR_BUTTON_ACTIVE;
  showduino_estop_clear_cancel(a);
  return SHOWDUINO_ESTOP_CLEAR_OK;
}

static inline int showduino_estop_clear_tick(ShowduinoEstopClearAuth *a, uint32_t nowMs) {
  if (!a || !a->pending) return 0;
  if ((int32_t)(nowMs - a->untilMs) >= 0) {
    showduino_estop_clear_cancel(a);
    return 1;
  }
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_EMERGENCY_BUTTON_H */
