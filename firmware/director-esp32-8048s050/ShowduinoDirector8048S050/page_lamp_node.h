#ifndef PAGE_LAMP_NODE_H
#define PAGE_LAMP_NODE_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>
#include "DirectorLampNodeControl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_LAMP_CMD_BACK          "PAGE_LAMP:BACK"
#define PAGE_LAMP_CMD_IGNITE        "PAGE_LAMP:IGNITE"
#define PAGE_LAMP_CMD_EXTINGUISH    "PAGE_LAMP:EXTINGUISH"
#define PAGE_LAMP_CMD_JEWEL_TEST    "PAGE_LAMP:JEWEL"
#define PAGE_LAMP_CMD_FLAME_TEST    "PAGE_LAMP:FLAME"
#define PAGE_LAMP_CMD_OFF           "PAGE_LAMP:OFF"
#define PAGE_LAMP_CMD_AUDIO_FLICK   "PAGE_LAMP:AUDIO:FLICK"
#define PAGE_LAMP_CMD_AUDIO_IGNITE  "PAGE_LAMP:AUDIO:IGNITION"
#define PAGE_LAMP_CMD_AUDIO_LOOP    "PAGE_LAMP:AUDIO:LOOP"
#define PAGE_LAMP_CMD_AUDIO_STOP    "PAGE_LAMP:AUDIO:STOP"
#define PAGE_LAMP_CMD_REFRESH       "PAGE_LAMP:REFRESH"

typedef void (*page_lamp_command_fn)(const char *command);

void page_lamp_node_create(lv_obj_t *parent, page_lamp_command_fn command_cb);
void page_lamp_node_destroy(void);
bool page_lamp_node_is_active(void);
void page_lamp_node_apply_theme(void);
void page_lamp_node_set_model(const DirectorLampNodeControl *model);

#ifdef __cplusplus
}
#endif

#endif
