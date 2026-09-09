#ifndef PAGE_04_NODES_H
#define PAGE_04_NODES_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Page 04 - Nodes
 *
 * Fabric inventory. Planned specialist roles stay visible when absent.
 * Live Audio Node state comes from STATE:NODE:AUDIO: (P4 -> Director).
 * Visual language is the Director standard: transparent header, BACK,
 * title underline, raised cards, corner ticks, status dots.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE04_CMD_BACK  "PAGE04:BACK"
#define PAGE04_CMD_AUDIO "PAGE04:AUDIO"

typedef enum Page04Role {
  PAGE04_ROLE_AUDIO = 0,
  PAGE04_ROLE_LAMP,
  PAGE04_ROLE_MOSFET,
  PAGE04_ROLE_NEOPIXEL,
  PAGE04_ROLE_DMX,
  PAGE04_ROLE_STAGE,
  PAGE04_ROLE_COUNT
} Page04Role;

typedef void (*page04_command_fn)(const char *command);

void page_04_nodes_create(lv_obj_t *parent, page04_command_fn command_cb);
void page_04_nodes_destroy(void);
bool page_04_nodes_is_active(void);
void page_04_nodes_apply_theme(void);

void page_04_nodes_set_summary(const char *text);
void page_04_nodes_set_card(Page04Role role, bool present,
                            const char *status, const char *detail,
                            uint32_t status_color);

#ifdef __cplusplus
}
#endif

#endif
