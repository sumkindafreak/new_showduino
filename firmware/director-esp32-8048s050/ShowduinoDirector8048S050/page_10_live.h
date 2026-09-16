#ifndef PAGE_10_LIVE_H
#define PAGE_10_LIVE_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Page 10 - Live show desk.
 * Visual language matches Diagnostics / Nodes. Transport commands unchanged.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE10_CMD_BACK "PAGE10:BACK"

typedef void (*page10_command_fn)(const char *command);

void page_10_live_create(lv_obj_t *parent, page10_command_fn command_cb);
void page_10_live_destroy(void);
bool page_10_live_is_active(void);
void page_10_live_apply_theme(void);

void page_10_live_set_header(const char *word, uint32_t color);
void page_10_live_set_cue(const char *text);
void page_10_live_set_elapsed(const char *text);
void page_10_live_set_remain(const char *text);
void page_10_live_set_pending(const char *text, uint32_t color);
void page_10_live_set_progress(uint8_t pct);
void page_10_live_set_emergency(bool active);

#ifdef __cplusplus
}
#endif

#endif
