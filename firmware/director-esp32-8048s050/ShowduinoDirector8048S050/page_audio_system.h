#ifndef PAGE_AUDIO_SYSTEM_H
#define PAGE_AUDIO_SYSTEM_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * P4 Audio System - Settings child page (not the Audio Node page).
 * Visual language matches Diagnostics / Nodes.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*page_audio_system_command_fn)(const char *command);

void page_audio_system_create(lv_obj_t *parent, page_audio_system_command_fn command_cb);
void page_audio_system_destroy(void);
bool page_audio_system_is_active(void);
void page_audio_system_apply_theme(void);

void page_audio_system_set_header(const char *status, uint32_t color);
void page_audio_system_set_local_status(const char *text);
void page_audio_system_set_local_detail(const char *text);
void page_audio_system_set_nodes(const char *text);
void page_audio_system_set_routing(const char *text);
void page_audio_system_set_command_status(const char *text);

#ifdef __cplusplus
}
#endif

#endif
