#ifndef SHOWDUINO_DIRECTOR_UI_MOTION_H
#define SHOWDUINO_DIRECTOR_UI_MOTION_H

#include <Arduino.h>
#include <lvgl.h>

/**
 * Central, restrained Director UI motion policy.
 * Pages must not invent their own durations or scatter LVGL animations.
 *
 * ON      - short polished transitions (boot exit, page load)
 * REDUCED - faster / fewer transitions
 * OFF     - instant screen changes
 *
 * Emergency always cancels cosmetic motion.
 */
enum DirectorUiAnimMode : uint8_t {
  DIRECTOR_UI_ANIM_OFF = 0,
  DIRECTOR_UI_ANIM_REDUCED = 1,
  DIRECTOR_UI_ANIM_ON = 2
};

#ifndef DIRECTOR_UI_PAGE_MS
#define DIRECTOR_UI_PAGE_MS 160
#endif
#ifndef DIRECTOR_UI_PAGE_REDUCED_MS
#define DIRECTOR_UI_PAGE_REDUCED_MS 80
#endif
#ifndef DIRECTOR_UI_BOOT_EXIT_MS
#define DIRECTOR_UI_BOOT_EXIT_MS 220
#endif
#ifndef DIRECTOR_UI_BOOT_EXIT_REDUCED_MS
#define DIRECTOR_UI_BOOT_EXIT_REDUCED_MS 120
#endif
#ifndef DIRECTOR_AMBIENT_FADE_MS
#define DIRECTOR_AMBIENT_FADE_MS 400
#endif
#ifndef DIRECTOR_AMBIENT_BOOT_FADE_MS
#define DIRECTOR_AMBIENT_BOOT_FADE_MS 1400
#endif

inline DirectorUiAnimMode &directorUiMotionModeRef() {
  static DirectorUiAnimMode mode = DIRECTOR_UI_ANIM_ON;
  return mode;
}

inline bool &directorUiMotionEmergencyRef() {
  static bool emergency = false;
  return emergency;
}

inline void directorUiMotionSetMode(DirectorUiAnimMode mode) {
  directorUiMotionModeRef() = mode;
}

inline DirectorUiAnimMode directorUiMotionMode() {
  return directorUiMotionModeRef();
}

inline void directorUiMotionSetEmergency(bool emergency) {
  directorUiMotionEmergencyRef() = emergency;
}

inline bool directorUiMotionAllowed() {
  if (directorUiMotionEmergencyRef()) return false;
  return directorUiMotionModeRef() != DIRECTOR_UI_ANIM_OFF;
}

inline uint16_t directorUiMotionPageMs() {
  if (!directorUiMotionAllowed()) return 0;
  if (directorUiMotionModeRef() == DIRECTOR_UI_ANIM_REDUCED) return DIRECTOR_UI_PAGE_REDUCED_MS;
  return DIRECTOR_UI_PAGE_MS;
}

inline uint16_t directorUiMotionBootExitMs() {
  if (!directorUiMotionAllowed()) return 0;
  if (directorUiMotionModeRef() == DIRECTOR_UI_ANIM_REDUCED) return DIRECTOR_UI_BOOT_EXIT_REDUCED_MS;
  return DIRECTOR_UI_BOOT_EXIT_MS;
}

inline const char *directorUiMotionModeName() {
  switch (directorUiMotionModeRef()) {
    case DIRECTOR_UI_ANIM_OFF: return "OFF";
    case DIRECTOR_UI_ANIM_REDUCED: return "REDUCED";
    default: return "ON";
  }
}

inline DirectorUiAnimMode directorUiMotionNextMode(DirectorUiAnimMode cur) {
  return (DirectorUiAnimMode)(((uint8_t)cur + 1) % 3);
}

/**
 * Load an application screen.
 *
 * Boot exit MUST be an immediate swap. A fade starts with the desk
 * transparent while LVGL still reports the boot screen as active.
 * Deleting that object (or flushing the half-switched RGB frame)
 * freezes the last READY pixels on this panel. Do not put the fade
 * back without a physical retest of the READY hand-off.
 */
inline void directorUiMotionLoadScreen(lv_obj_t *screen, bool leavingBoot) {
  if (!screen) return;
  lv_obj_t *prev = lv_screen_active();
  if (prev == screen) return;

  if (leavingBoot) {
    lv_screen_load(screen);
    if (prev && prev != screen && lv_obj_is_valid(prev)) {
      lv_obj_delete(prev);
    }
    return;
  }

  const uint16_t ms = directorUiMotionPageMs();
  if (ms > 0) {
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_ON, ms, 0, false);
    return;
  }

  lv_screen_load(screen);
}

#endif
