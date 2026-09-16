#ifndef PAGE_06_DIAGNOSTICS_H
#define PAGE_06_DIAGNOSTICS_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Page 06 - Diagnostics dashboard.
 * Visual language matches Page 04 Nodes / Page 05 Audio Node.
 * Presentation only. The P4 remains authoritative.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE06_CMD_BACK           "PAGE06:BACK"
#define PAGE06_CMD_CLOSE          "PAGE06:CLOSE"
#define PAGE06_CMD_REFRESH        "PAGE06:REFRESH"
#define PAGE06_CMD_STAGE_STATUS   "PAGE06:STAGE_STATUS"
#define PAGE06_CMD_SD_STATUS      "PAGE06:SD_STATUS"
#define PAGE06_CMD_BACKUP         "PAGE06:BACKUP"
#define PAGE06_CMD_REPAIR         "PAGE06:REPAIR"
#define PAGE06_CMD_LOGS           "PAGE06:LOGS"
#define PAGE06_CMD_TOOLS          "PAGE06:TOOLS"

typedef enum Page06CardId {
  PAGE06_CARD_DIRECTOR = 0,
  PAGE06_CARD_COMMS,
  PAGE06_CARD_P4,
  PAGE06_CARD_SAFETY,
  PAGE06_CARD_STORAGE,
  PAGE06_CARD_AUDIO,
  PAGE06_CARD_PIXELS,
  PAGE06_CARD_NODES,
  PAGE06_CARD_COUNT
} Page06CardId;

typedef void (*page06_command_fn)(const char *command);

void page_06_diagnostics_create(lv_obj_t *parent, page06_command_fn command_cb);
void page_06_diagnostics_destroy(void);
bool page_06_diagnostics_is_active(void);
void page_06_diagnostics_apply_theme(void);

void page_06_diagnostics_set_health(const char *word, const char *strip, uint32_t color);
void page_06_diagnostics_set_card(Page06CardId id, bool present,
                                  const char *status, const char *detail,
                                  uint32_t status_color);
void page_06_diagnostics_set_sheet_body(Page06CardId id, const char *body);
void page_06_diagnostics_close_sheet(void);
bool page_06_diagnostics_sheet_open(void);

#ifdef __cplusplus
}
#endif

#endif
