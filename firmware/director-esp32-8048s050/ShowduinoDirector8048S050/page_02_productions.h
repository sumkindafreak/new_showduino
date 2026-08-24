#ifndef PAGE_02_PRODUCTIONS_H
#define PAGE_02_PRODUCTIONS_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Page 02 — Productions (SD library)
 *
 * Lists packages from ShowManager on the Director SD card.
 * Load uploads the timeline to Stage. Run requests SHOW:RUN.
 * Open shows the existing details/transport page. No local mock entries.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE02_CMD_BACK       "PAGE02:BACK"
#define PAGE02_CMD_OPEN       "PAGE02:OPEN"
#define PAGE02_CMD_LOAD       "PAGE02:LOAD"
#define PAGE02_CMD_RUN        "PAGE02:RUN"

#ifndef PAGE02_MAX_PRODUCTIONS
#define PAGE02_MAX_PRODUCTIONS 16
#endif

typedef void (*page02_command_fn)(const char *command);

/** One production row in the Page 02 local view-model. */
typedef struct {
  char id[32];
  char name[48];
  char description[64];
  char modified[24];
  bool used;
} Page02ProductionEntry;

void page_02_productions_create(lv_obj_t *parent, page02_command_fn command_cb);
void page_02_productions_destroy(void);
bool page_02_productions_is_active(void);

/** Re-apply theme accent to Page 02 objects. */
void page_02_productions_apply_theme(void);

/** Selected entry helpers (nullptr / empty if none). */
const char *page_02_productions_selected_id(void);
const char *page_02_productions_selected_name(void);
int page_02_productions_count(void);
void page_02_productions_set_entries(const Page02ProductionEntry *entries, int count);

#ifdef __cplusplus
}
#endif

#endif