#ifndef PAGE_04_NODES_H
#define PAGE_04_NODES_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../../protocol/showduino_lamp_director_desk.h"

/**
 * Page 04 - Nodes
 *
 * Fabric inventory. Touch a role card to expand a settings sheet for that
 * node. Planned specialist roles stay visible when absent. Live Audio / Lamp
 * / Pixel state comes from P4 wire reports. The Director only displays and
 * submits operator requests.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE04_CMD_BACK            "PAGE04:BACK"
#define PAGE04_CMD_CLOSE           "PAGE04:CLOSE"
#define PAGE04_CMD_AUDIO           "PAGE04:AUDIO"
#define PAGE04_CMD_STATUS          "PAGE04:STATUS"
#define PAGE04_CMD_AUDIO_TEST      "PAGE04:AUDIO:TEST"
#define PAGE04_CMD_AUDIO_STOP      "PAGE04:AUDIO:STOP"
#define PAGE04_CMD_LAMP_IGNITE     "PAGE04:LAMP:IGNITE"
#define PAGE04_CMD_LAMP_EXTINGUISH "PAGE04:LAMP:EXTINGUISH"
#define PAGE04_CMD_LAMP_FLARE      "PAGE04:LAMP:FLARE"
#define PAGE04_CMD_LAMP_STATUS     "PAGE04:LAMP:STATUS"

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
void page_04_nodes_set_lock(bool emergency);
void page_04_nodes_close_sheet(void);
bool page_04_nodes_sheet_open(void);
void page_04_nodes_set_lamp_sheet(const ShowduinoLampDirectorSheet *model);

#ifdef __cplusplus
}
#endif

#endif
