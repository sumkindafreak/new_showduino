#ifndef PAGE_01_HOME_H
#define PAGE_01_HOME_H

#include <lvgl.h>
#include "showduino_capabilities.h"

/**
 * Page 01 — Showduino Home
 *
 * LVGL draws every live label, icon, button, node state, production name,
 * clock, warning, and theme accent. No BMP chrome.
 *
 * Hierarchy (approved production UI):
 *   RUN SHOW hero → Productions → Nodes / Settings / Diagnostics
 *   Cue Library and Outputs remain in the layout but are not tappable.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Set to 1 before compile to draw tile hit outlines + numbers and log coords. */
#ifndef SHOWDUINO_PAGE01_ALIGNMENT_DEBUG
#define SHOWDUINO_PAGE01_ALIGNMENT_DEBUG 0
#endif

/** Navigation command strings emitted by Home tiles. */
#define PAGE01_CMD_PRODUCTIONS   "HOME:PRODUCTIONS"
#define PAGE01_CMD_RUN_SHOW      "HOME:RUN_SHOW"
#define PAGE01_CMD_CUE_LIBRARY   "HOME:CUE_LIBRARY"
#define PAGE01_CMD_NODES         "HOME:NODES"
#define PAGE01_CMD_OUTPUTS       "HOME:OUTPUTS"
#define PAGE01_CMD_SETTINGS      "HOME:SETTINGS"
#define PAGE01_CMD_DIAGNOSTICS   "HOME:DIAGNOSTICS"

typedef void (*page01_command_fn)(const char *command);

/**
 * Build Page 01 into an existing transparent parent panel.
 * Parent is owned by DisplayManager — do not delete the parent here.
 */
void page_01_home_create(lv_obj_t *parent, page01_command_fn command_cb);

/**
 * Tear down LVGL children created by Page 01 and clear theme registrations.
 * Safe to call when the parent panel is about to be discarded.
 */
void page_01_home_destroy(void);

/** True after a successful create and before destroy. */
bool page_01_home_is_active(void);

/** Apply / refresh capability gating on tiles and footer indicators. */
void page_01_home_set_capabilities(const ShowduinoCapabilities *caps);

/** Copy of the capabilities currently applied to the page. */
ShowduinoCapabilities page_01_home_get_capabilities(void);

/**
 * Header / hero update helpers — static defaults until real data is wired.
 * Pass nullptr to leave a field unchanged (except production empty → NO PRODUCTION).
 */
void page_01_home_set_production(const char *name);
void page_01_home_set_link_text(const char *text);
void page_01_home_set_clock_text(const char *text);

/**
 * Footer placeholder updates — keep separate from create so live data
 * can replace these later without rebuilding the page.
 */
void page_01_home_set_footer_sue(const char *text);
void page_01_home_set_footer_p4(const char *text);
void page_01_home_set_footer_relay(const char *text);
void page_01_home_set_footer_mosfet(const char *text);
void page_01_home_set_footer_neopixel(const char *text);
void page_01_home_set_footer_audio(const char *text);
void page_01_home_set_footer_dmx(const char *text);
void page_01_home_set_footer_notify(const char *text);

/** Re-apply the current theme accent to Page 01 objects. */
void page_01_home_apply_theme(void);

#ifdef __cplusplus
}
#endif

#endif
