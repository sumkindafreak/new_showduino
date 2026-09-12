#ifndef SHOWDUINO_LAMP_DIRECTOR_DESK_H
#define SHOWDUINO_LAMP_DIRECTOR_DESK_H

/*
 * Director Lamp-sheet presentation and command formatting.
 * Host-testable. No LVGL, Jewel, Fermion, mic, or ESP-NOW side effects.
 *
 * Director requests. P4 is authoritative. Lamp S3 performs the flame.
 * This is not a protocol revision — it formats existing LAMP:NODE: syntax
 * and maps already-published STATE:NODE:LAMP / STATE:NODE:LAMP:D: fields.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "showduino_lamp_node.h"
#include "showduino_carbide_lamp.h"
#include "showduino_state_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_LAMP_DESK_UNKNOWN        "--"
#define SHOWDUINO_LAMP_DESK_TITLE          "LAMP NODE"
#define PAGE04_NAV_ROLE_AUDIO              0
#define PAGE04_NAV_ROLE_LAMP               1
#define PAGE04_NAV_ROLE_MOSFET             2
#define PAGE04_NAV_ROLE_NEOPIXEL           3
#define PAGE04_NAV_ROLE_DMX                4
#define PAGE04_NAV_ROLE_STAGE              5
#define PAGE04_NAV_ROLE_COUNT              6

typedef enum ShowduinoLampDeskVerb {
  SHOWDUINO_LAMP_DESK_CMD_IGNITE = 0,
  SHOWDUINO_LAMP_DESK_CMD_EXTINGUISH,
  SHOWDUINO_LAMP_DESK_CMD_FLARE,
  SHOWDUINO_LAMP_DESK_CMD_STATUS
} ShowduinoLampDeskVerb;

typedef struct ShowduinoLampDirectorInput {
  int present;
  int offline;
  int emergency;
  int detail_valid;
  char logical_id[16];
  char friendly[24];
  ShowduinoLampDetailWire detail;
} ShowduinoLampDirectorInput;

typedef struct ShowduinoLampDirectorSheet {
  char title[16];
  char logical_id[16];
  char friendly[24];
  char presence_line[40];
  char state[20];
  char motion[12];
  char blow[12];
  char audio[16];
  char jewel_health[12];
  char audio_health[12];
  char motion_health[12];
  char mic_health[12];
  char voltage_health[12];
  char banner[160];
  uint8_t online;
  uint8_t emergency;
  uint8_t ignite_enabled;
  uint8_t extinguish_enabled;
  uint8_t flare_enabled;
  uint8_t refresh_enabled;
  uint8_t has_clear_emergency;
  uint8_t has_jewel_control;
  uint8_t has_audio_control;
  uint8_t has_espnow_direct;
  char ignite_cmd[48];
  char extinguish_cmd[48];
  char flare_cmd[48];
  char refresh_cmd[48];
} ShowduinoLampDirectorSheet;

typedef enum Page04NavPlace {
  PAGE04_NAV_HOME = 0,
  PAGE04_NAV_GRID,
  PAGE04_NAV_SHEET
} Page04NavPlace;

typedef struct Page04Nav {
  Page04NavPlace place;
  int role;
} Page04Nav;

typedef struct Page04SheetAction {
  char label[16];
  char cmd[32];
  uint8_t enabled;
  uint8_t shown;
} Page04SheetAction;

static inline const char *showduino_lamp_desk_verb_tail(ShowduinoLampDeskVerb verb) {
  switch (verb) {
    case SHOWDUINO_LAMP_DESK_CMD_IGNITE: return "IGNITE";
    case SHOWDUINO_LAMP_DESK_CMD_EXTINGUISH: return "EXTINGUISH";
    case SHOWDUINO_LAMP_DESK_CMD_FLARE: return "FX:FLARE";
    case SHOWDUINO_LAMP_DESK_CMD_STATUS: return "STATUS";
    default: return "STATUS";
  }
}

static inline void showduino_lamp_director_select_id(const char *in, char *out, size_t n) {
  if (!out || n == 0) return;
  out[0] = 0;
  if (in && showduino_lamp_id_ok(in)) {
    strncpy(out, in, n - 1);
    out[n - 1] = 0;
    return;
  }
  strncpy(out, SHOWDUINO_CARBIDE_LOGICAL_DEFAULT, n - 1);
  out[n - 1] = 0;
}

static inline int showduino_lamp_director_format_cmd(const char *logicalId,
                                                     ShowduinoLampDeskVerb verb,
                                                     char *out, size_t n) {
  const char *tail;
  if (!out || n < 16) return 0;
  tail = showduino_lamp_desk_verb_tail(verb);
  if (logicalId && showduino_lamp_id_ok(logicalId)) {
    snprintf(out, n, "LAMP:NODE:%s:%s", logicalId, tail);
  } else {
    snprintf(out, n, "LAMP:NODE:%s", tail);
  }
  return 1;
}

static inline int showduino_lamp_desk_fx_is_carbide(const char *fx) {
  if (!fx || !fx[0] || strcmp(fx, "-") == 0) return 0;
  return strcmp(fx, "STRIKING") == 0 ||
         strcmp(fx, "IGNITING") == 0 ||
         strcmp(fx, "BURNING") == 0 ||
         strcmp(fx, "LOW_FLAME") == 0 ||
         strcmp(fx, "UNSTABLE") == 0 ||
         strcmp(fx, "FLARE") == 0 ||
         strcmp(fx, "EXTINGUISHING") == 0;
}

static inline int showduino_lamp_desk_fx_is_flame(const char *fx) {
  if (!fx) return 0;
  return strcmp(fx, "BURNING") == 0 ||
         strcmp(fx, "LOW_FLAME") == 0 ||
         strcmp(fx, "UNSTABLE") == 0 ||
         strcmp(fx, "FLARE") == 0;
}

static inline int showduino_lamp_desk_health_is_fake_ok(const char *v) {
  return v && (strcmp(v, "OK") == 0 || strcmp(v, "GOOD") == 0);
}

static inline void showduino_lamp_director_build_sheet(
    const ShowduinoLampDirectorInput *in,
    ShowduinoLampDirectorSheet *out) {
  char id[16];
  int live;
  int flare;
  const char *fx;
  const char *own;

  if (!out) return;
  memset(out, 0, sizeof(*out));
  strncpy(out->title, SHOWDUINO_LAMP_DESK_TITLE, sizeof(out->title) - 1);
  strncpy(out->motion, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->motion) - 1);
  strncpy(out->blow, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->blow) - 1);
  strncpy(out->audio, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->audio) - 1);
  strncpy(out->jewel_health, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->jewel_health) - 1);
  strncpy(out->audio_health, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->audio_health) - 1);
  strncpy(out->motion_health, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->motion_health) - 1);
  strncpy(out->mic_health, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->mic_health) - 1);
  strncpy(out->voltage_health, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->voltage_health) - 1);
  out->refresh_enabled = 1;
  out->has_clear_emergency = 0;
  out->has_jewel_control = 0;
  out->has_audio_control = 0;
  out->has_espnow_direct = 0;

  showduino_lamp_director_select_id(in ? in->logical_id : 0, id, sizeof(id));
  strncpy(out->logical_id, id, sizeof(out->logical_id) - 1);
  if (in && in->friendly[0]) {
    strncpy(out->friendly, in->friendly, sizeof(out->friendly) - 1);
  }

  out->emergency = (uint8_t)(in && in->emergency);
  out->online = (uint8_t)(in && in->present && !in->offline);
  live = out->online && !out->emergency;

  if (!out->online) {
    strncpy(out->presence_line, "OFFLINE", sizeof(out->presence_line) - 1);
    strncpy(out->state, "OFFLINE", sizeof(out->state) - 1);
    strncpy(out->banner, "No compatible Lamp Node detected.", sizeof(out->banner) - 1);
  } else if (out->emergency) {
    strncpy(out->presence_line, "EMERGENCY", sizeof(out->presence_line) - 1);
    strncpy(out->state, "EMERGENCY", sizeof(out->state) - 1);
    strncpy(out->banner,
            "SYSTEM EMERGENCY ACTIVE\n"
            "LAMP CONTROLS LOCKED BY SHOW ENGINE",
            sizeof(out->banner) - 1);
  } else {
    own = 0;
    if (in && in->detail_valid) {
      if (strcmp(in->detail.state, "SHOW_CONTROLLED") == 0) {
        own = "ONLINE • SHOWDUINO OWNED";
      } else if (strcmp(in->detail.state, "STANDALONE") == 0) {
        own = "ONLINE • STANDALONE";
      } else if (strcmp(in->detail.state, "SEARCHING") == 0) {
        own = "ONLINE • SEARCHING";
      } else if (strcmp(in->detail.state, "FAULT") == 0) {
        own = "FAULT";
      }
    }
    strncpy(out->presence_line, own ? own : "ONLINE", sizeof(out->presence_line) - 1);

    if (in && in->detail_valid) {
      fx = in->detail.fx;
      if (showduino_lamp_desk_fx_is_carbide(fx)) {
        strncpy(out->state, fx, sizeof(out->state) - 1);
      } else {
        strncpy(out->state, "OFF", sizeof(out->state) - 1);
      }
    } else {
      strncpy(out->state, SHOWDUINO_LAMP_DESK_UNKNOWN, sizeof(out->state) - 1);
    }
  }

  out->ignite_enabled = (uint8_t)live;
  out->extinguish_enabled = (uint8_t)live;
  flare = live;
  if (live && in && in->detail_valid) {
    fx = in->detail.fx;
    if (showduino_lamp_desk_fx_is_carbide(fx) ||
        !fx[0] || strcmp(fx, "-") == 0) {
      flare = showduino_lamp_desk_fx_is_flame(fx);
    }
  }
  out->flare_enabled = (uint8_t)flare;

  showduino_lamp_director_format_cmd(id, SHOWDUINO_LAMP_DESK_CMD_IGNITE,
                                     out->ignite_cmd, sizeof(out->ignite_cmd));
  showduino_lamp_director_format_cmd(id, SHOWDUINO_LAMP_DESK_CMD_EXTINGUISH,
                                     out->extinguish_cmd, sizeof(out->extinguish_cmd));
  showduino_lamp_director_format_cmd(id, SHOWDUINO_LAMP_DESK_CMD_FLARE,
                                     out->flare_cmd, sizeof(out->flare_cmd));
  showduino_lamp_director_format_cmd(id, SHOWDUINO_LAMP_DESK_CMD_STATUS,
                                     out->refresh_cmd, sizeof(out->refresh_cmd));
}

static inline void page04_nav_reset(Page04Nav *n) {
  if (!n) return;
  n->place = PAGE04_NAV_GRID;
  n->role = -1;
}

static inline void page04_nav_open_sheet(Page04Nav *n, int role) {
  if (!n) return;
  n->place = PAGE04_NAV_SHEET;
  n->role = role;
}

static inline void page04_nav_close_sheet(Page04Nav *n) {
  if (!n) return;
  n->place = PAGE04_NAV_GRID;
  n->role = -1;
}

static inline int page04_nav_back(Page04Nav *n) {
  if (!n) return 1;
  if (n->place == PAGE04_NAV_SHEET) {
    page04_nav_close_sheet(n);
    return 0;
  }
  n->place = PAGE04_NAV_HOME;
  n->role = -1;
  return 1;
}

static inline void page04_sheet_clear_actions(Page04SheetAction out[4]) {
  int i;
  if (!out) return;
  for (i = 0; i < 4; i++) {
    memset(&out[i], 0, sizeof(out[i]));
  }
}

static inline void page04_sheet_set_action(Page04SheetAction *a, const char *label,
                                           const char *cmd, int enabled) {
  if (!a) return;
  memset(a, 0, sizeof(*a));
  if (label) strncpy(a->label, label, sizeof(a->label) - 1);
  if (cmd) strncpy(a->cmd, cmd, sizeof(a->cmd) - 1);
  a->enabled = enabled ? 1 : 0;
  a->shown = 1;
}

static inline void page04_sheet_fill_actions(int role, int present, int emergency,
                                             Page04SheetAction out[4]) {
  const int live = present && !emergency;
  page04_sheet_clear_actions(out);
  if (role == PAGE04_NAV_ROLE_AUDIO) {
    page04_sheet_set_action(&out[0], "AUDIO DESK", "PAGE04:AUDIO", 1);
    page04_sheet_set_action(&out[1], "TEST", "PAGE04:AUDIO:TEST", live);
    page04_sheet_set_action(&out[2], "STOP", "PAGE04:AUDIO:STOP", live);
    page04_sheet_set_action(&out[3], "REFRESH", "PAGE04:STATUS", 1);
    return;
  }
  if (role == PAGE04_NAV_ROLE_LAMP) {
    page04_sheet_set_action(&out[0], "IGNITE", "PAGE04:LAMP:IGNITE", live);
    page04_sheet_set_action(&out[1], "EXTINGUISH", "PAGE04:LAMP:EXTINGUISH", live);
    page04_sheet_set_action(&out[2], "FLARE", "PAGE04:LAMP:FLARE", live);
    page04_sheet_set_action(&out[3], "REFRESH", "PAGE04:LAMP:STATUS", 1);
    return;
  }
  if (role == PAGE04_NAV_ROLE_NEOPIXEL) {
    page04_sheet_set_action(&out[0], "REFRESH", "PAGE04:STATUS", 1);
    return;
  }
  if (role == PAGE04_NAV_ROLE_STAGE) {
    page04_sheet_set_action(&out[0], "REFRESH", "PAGE04:STATUS", 1);
    return;
  }
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_LAMP_DIRECTOR_DESK_H */
