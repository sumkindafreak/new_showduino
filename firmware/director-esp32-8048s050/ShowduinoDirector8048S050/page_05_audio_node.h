#ifndef PAGE_05_AUDIO_NODE_H
#define PAGE_05_AUDIO_NODE_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>
#include "DirectorAudioNodeControl.h"

/**
 * Page 05 - Audio Node operator control.
 * Requests travel Director -> Comms -> P4 -> Audio Node.
 * Visual language matches Page 04 / Home / unlock.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE05_CMD_BACK      "PAGE05:BACK"
#define PAGE05_CMD_PLAY      "PAGE05:PLAY"
#define PAGE05_CMD_LOOP      "PAGE05:LOOP"
#define PAGE05_CMD_PAUSE     "PAGE05:PAUSE"
#define PAGE05_CMD_RESUME    "PAGE05:RESUME"
#define PAGE05_CMD_STOP      "PAGE05:STOP"
#define PAGE05_CMD_VOL_DOWN  "PAGE05:VOL-"
#define PAGE05_CMD_VOL_UP    "PAGE05:VOL+"
#define PAGE05_CMD_SELECT    "PAGE05:SELECT"
#define PAGE05_CMD_TEST      "PAGE05:TEST"
#define PAGE05_CMD_DETAILS   "PAGE05:DETAILS"
#define PAGE05_CMD_REFRESH   "PAGE05:REFRESH"
#define PAGE05_CMD_INV_NEXT  "PAGE05:INV_NEXT"
#define PAGE05_CMD_CLOSE     "PAGE05:CLOSE"
#define PAGE05_CMD_CALIBRATE "PAGE05:SOUND:CALIBRATE"
#define PAGE05_CMD_SND_EN    "PAGE05:SOUND:ENABLE"
#define PAGE05_CMD_SND_DIS   "PAGE05:SOUND:DISABLE"
#define PAGE05_CMD_TH_UP     "PAGE05:SOUND:TH+"
#define PAGE05_CMD_TH_DN     "PAGE05:SOUND:TH-"
#define PAGE05_CMD_SND_TEST  "PAGE05:SOUND:TEST"
#define PAGE05_CMD_AMBIENCE  "PAGE05:AMBIENCE"
#define PAGE05_CMD_AMB_PLAY  "PAGE05:AMB:PLAY"
#define PAGE05_CMD_AMB_LOOP  "PAGE05:AMB:LOOP"
#define PAGE05_CMD_AMB_STOP  "PAGE05:AMB:STOP"
#define PAGE05_CMD_AMB_VOL_DN "PAGE05:AMB:VOL-"
#define PAGE05_CMD_AMB_VOL_UP "PAGE05:AMB:VOL+"
#define PAGE05_CMD_AMB_STATUS "PAGE05:AMB:STATUS"
#define PAGE05_CMD_AMB_CLOSE "PAGE05:AMB:CLOSE"

/* Finish-line convention: P4-local PCM5102A ambience asset. */
#define PAGE05_AMBIENCE_DEFAULT_PATH "/showduino/audio/ambience.wav"

typedef void (*page05_command_fn)(const char *command);

void page_05_audio_node_create(lv_obj_t *parent, page05_command_fn command_cb);
void page_05_audio_node_destroy(void);
bool page_05_audio_node_is_active(void);
void page_05_audio_node_apply_theme(void);
void page_05_audio_node_set_model(const DirectorAudioNodeControl *model);
void page_05_audio_node_show_details(bool show);
void page_05_audio_node_show_select(bool show);
void page_05_audio_node_show_ambience(bool show);
void page_05_audio_node_apply_ambience_line(const char *line);
uint8_t page_05_audio_node_ambience_volume(void);
const char *page_05_audio_node_selected_asset(void);

#ifdef __cplusplus
}
#endif

#endif
