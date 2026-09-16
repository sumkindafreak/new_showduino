#ifndef PAGE_08_SETTINGS_H
#define PAGE_08_SETTINGS_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Page 08 - Settings dashboard.
 * Visual language matches Page 04 Nodes / Page 06 Diagnostics.
 * Presentation only. Existing SETTINGS:* / STORAGE:* / SCREEN:* commands.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE08_CMD_BACK     "PAGE08:BACK"
#define PAGE08_CMD_CLOSE    "PAGE08:CLOSE"
#define PAGE08_CMD_AUDIO    "SCREEN:AUDIO"
#define PAGE08_CMD_LOGS     "SCREEN:LOGS"
#define PAGE08_CMD_TOUCH_CAL   "SCREEN:TOUCHCAL"
#define PAGE08_CMD_TOUCH_RESET "SCREEN:TOUCHRESET"

typedef enum Page08CardId {
  PAGE08_CARD_DISPLAY = 0,
  PAGE08_CARD_ATMOSPHERE,
  PAGE08_CARD_AUDIO,
  PAGE08_CARD_LOGS,
  PAGE08_CARD_STORAGE,
  PAGE08_CARD_SYSTEM,
  PAGE08_CARD_COUNT
} Page08CardId;

typedef void (*page08_command_fn)(const char *command);

void page_08_settings_create(lv_obj_t *parent, page08_command_fn command_cb);
void page_08_settings_destroy(void);
bool page_08_settings_is_active(void);
void page_08_settings_apply_theme(void);
void page_08_settings_close_sheet(void);

void page_08_settings_set_header(const char *status, uint32_t color);
void page_08_settings_set_strip(const char *title, const char *status, uint32_t color);
void page_08_settings_set_card(Page08CardId id, bool present,
                               const char *status, const char *detail,
                               uint32_t status_color);
void page_08_settings_set_sheet_body(Page08CardId id, const char *body);

#ifdef __cplusplus
}
#endif

#endif
