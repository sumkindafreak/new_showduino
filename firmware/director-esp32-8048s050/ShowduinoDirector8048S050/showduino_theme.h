#ifndef SHOWDUINO_THEME_H
#define SHOWDUINO_THEME_H

#include <lvgl.h>

/**
 * Central Showduino accent colour (LVGL only — never baked into BMPs).
 *
 * Call showduino_theme_set_accent() to match NeoPixel ambience later.
 * Registered objects (tile borders, header accents, footer indicators, icons)
 * are updated without touching background artwork.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** How a registered object uses the accent colour. */
typedef enum {
  SHOWDUINO_THEME_ROLE_BORDER = 0,   /* Normal + pressed/focused border */
  SHOWDUINO_THEME_ROLE_BORDER_PRESSED, /* Pressed / focused border only */
  SHOWDUINO_THEME_ROLE_TEXT,         /* Text / icon recolour */
  SHOWDUINO_THEME_ROLE_INDICATOR,    /* Small footer / status fill */
  SHOWDUINO_THEME_ROLE_HEADER_ACCENT /* Thin header accent bar */
} showduino_theme_role_t;

/** Initialise theme with the default lime operator accent. */
void showduino_theme_init(void);

/** Change the single configurable accent colour and refresh registered objects. */
void showduino_theme_set_accent(lv_color_t colour);

/** Current accent colour. */
lv_color_t showduino_theme_get_accent(void);

/**
 * Register an LVGL object so accent changes can update it.
 * Safe to call more than once for the same object (updates the role).
 * Objects must outlive the registration or be unregistered before delete.
 */
void showduino_theme_register(lv_obj_t *obj, showduino_theme_role_t role);

/** Remove one object from the theme registry (call before deleting it). */
void showduino_theme_unregister(lv_obj_t *obj);

/** Clear the entire registry (e.g. before rebuilding a page). */
void showduino_theme_clear_registry(void);

/** Re-apply the current accent to every registered object. */
void showduino_theme_apply(void);

/**
 * Hardware-test helpers — call explicitly only (Serial / debug menu).
 * Named colours: lime, purple, blue, red, amber, green.
 * Returns true if the name was recognised.
 */
bool showduino_theme_test_apply_named(const char *name);

/** Cycle to the next fixed test colour. Does nothing automatically. */
void showduino_theme_test_next(void);

#ifdef __cplusplus
}
#endif

#endif