#ifndef SHOWDUINO_DIRECTOR_LOCATE_H
#define SHOWDUINO_DIRECTOR_LOCATE_H

/*
 * Host-testable Director Locate acknowledgement / first-touch consume state.
 *
 * Locate is independent of Emergency. Acknowledgement never clears Emergency.
 * There is no automatic timeout.
 */

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShowduinoDirectorLocateState {
  uint8_t active;
  uint8_t eatUntilRelease;
} ShowduinoDirectorLocateState;

static inline void showduino_director_locate_reset(ShowduinoDirectorLocateState *s) {
  if (!s) return;
  memset(s, 0, sizeof(*s));
}

/* Returns 1 if Locate newly started, 0 if already active. */
static inline int showduino_director_locate_start(ShowduinoDirectorLocateState *s) {
  if (!s) return 0;
  if (s->active) return 0;
  s->active = 1;
  return 1;
}

static inline int showduino_director_locate_active(const ShowduinoDirectorLocateState *s) {
  return (s && s->active) ? 1 : 0;
}

/*
 * Process one touch sample.
 * Returns 1 if the sample (and the rest of this press/release cycle) must
 * not reach underlying LVGL controls.
 * *acknowledged is set to 1 only on the first consuming press that ends Locate.
 */
static inline int showduino_director_locate_on_touch(ShowduinoDirectorLocateState *s,
                                                     int pressed,
                                                     int *acknowledged) {
  if (acknowledged) *acknowledged = 0;
  if (!s) return 0;

  if (s->eatUntilRelease) {
    if (!pressed) s->eatUntilRelease = 0;
    return 1;
  }

  if (s->active && pressed) {
    s->active = 0;
    s->eatUntilRelease = 1;
    if (acknowledged) *acknowledged = 1;
    return 1;
  }

  return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_DIRECTOR_LOCATE_H */
