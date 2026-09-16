#ifndef PAGE_LOGS_H
#define PAGE_LOGS_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * System Logs - Settings child page.
 * Visual language matches Diagnostics / Nodes.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*page_logs_command_fn)(const char *command);

void page_logs_create(lv_obj_t *parent, page_logs_command_fn command_cb);
void page_logs_destroy(void);
bool page_logs_is_active(void);
void page_logs_apply_theme(void);

void page_logs_set_header(const char *status, uint32_t color);
void page_logs_set_count(const char *text);
void page_logs_set_newest(const char *text);
void page_logs_set_filter(const char *text);
void page_logs_set_body(const char *text);

lv_obj_t *page_logs_scroll(void);
lv_obj_t *page_logs_body_label(void);

#ifdef __cplusplus
}
#endif

#endif
