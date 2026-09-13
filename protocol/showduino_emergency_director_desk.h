#ifndef SHOWDUINO_EMERGENCY_DIRECTOR_DESK_H
#define SHOWDUINO_EMERGENCY_DIRECTOR_DESK_H

/*
 * Director Emergency-station strip/sheet presentation.
 * Host-testable. No LVGL or radio side effects.
 *
 * ASSERT ONLY. This desk never formats a wireless emergency-clear command.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "showduino_emergency_node.h"
#include "showduino_state_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_ESTOP_DESK_TITLE          "EMERGENCY STATIONS"
#define SHOWDUINO_ESTOP_DESK_STRIP_H        44
#define SHOWDUINO_ESTOP_DESK_CARD_H         118
#define SHOWDUINO_ESTOP_DESK_DOCK_Y         402
#define SHOWDUINO_ESTOP_DESK_GRID_Y         56
#define SHOWDUINO_ESTOP_DESK_MAX_STATIONS   SHOWDUINO_EMERGENCY_NODE_MAX_NODES

typedef struct ShowduinoEmergencyDeskStation {
  uint8_t used;
  uint8_t online;
  uint8_t input_open;
  uint8_t latched;
  uint8_t acked;
  char id[16];
  char name[20];
  char state[20];
  char line[48];
} ShowduinoEmergencyDeskStation;

typedef struct ShowduinoEmergencyDirectorSheet {
  char title[24];
  char strip[40];
  char summary[48];
  char warning[64];
  char banner[160];
  char refresh_cmd[24];
  uint8_t seen;
  uint8_t online;
  uint8_t offline;
  uint8_t asserting;
  uint8_t faults;
  uint8_t safety_fault;
  uint8_t global_emergency;
  uint8_t has_clear_emergency;
  uint8_t has_simulate_emergency;
  ShowduinoEmergencyDeskStation stations[SHOWDUINO_ESTOP_DESK_MAX_STATIONS];
} ShowduinoEmergencyDirectorSheet;

static inline void showduino_emergency_desk_reset(ShowduinoEmergencyDirectorSheet *s) {
  if (!s) return;
  memset(s, 0, sizeof(*s));
  strncpy(s->title, SHOWDUINO_ESTOP_DESK_TITLE, sizeof(s->title) - 1);
  strncpy(s->refresh_cmd, "ESTOP:STATUS", sizeof(s->refresh_cmd) - 1);
  strncpy(s->strip, "E-STOP  0 ONLINE", sizeof(s->strip) - 1);
  strncpy(s->summary, "0 ONLINE", sizeof(s->summary) - 1);
}

static inline const char *showduino_emergency_desk_station_status(
    const ShowduinoEmergencyDeskStation *st) {
  if (!st || !st->used) return "NOT DETECTED";
  if (!st->online) return "OFFLINE";
  if (st->latched) return "LATCHED";
  if (st->input_open) return "OPEN";
  return "READY";
}

static inline void showduino_emergency_desk_rebuild(ShowduinoEmergencyDirectorSheet *s) {
  uint8_t i;
  if (!s) return;
  s->seen = 0;
  s->online = 0;
  s->offline = 0;
  s->asserting = 0;
  s->faults = 0;
  s->has_clear_emergency = 0;
  s->has_simulate_emergency = 0;
  strncpy(s->refresh_cmd, "ESTOP:STATUS", sizeof(s->refresh_cmd) - 1);
  strncpy(s->title, SHOWDUINO_ESTOP_DESK_TITLE, sizeof(s->title) - 1);
  for (i = 0; i < SHOWDUINO_ESTOP_DESK_MAX_STATIONS; i++) {
    ShowduinoEmergencyDeskStation *st = &s->stations[i];
    if (!st->used || !st->id[0]) continue;
    s->seen++;
    if (st->online) s->online++;
    else {
      s->offline++;
      s->faults++;
    }
    if (st->latched) s->asserting++;
    snprintf(st->line, sizeof(st->line), "%s  %s  %s",
             st->id,
             st->name[0] ? st->name : "-",
             showduino_emergency_desk_station_status(st));
  }
  s->safety_fault = (s->offline > 0) ? 1 : 0;
  if (s->seen == 0) {
    snprintf(s->strip, sizeof(s->strip), "E-STOP  0 ONLINE");
    snprintf(s->summary, sizeof(s->summary), "0 ONLINE");
  } else {
    snprintf(s->strip, sizeof(s->strip), "E-STOP  %u / %u ONLINE",
             (unsigned)s->online, (unsigned)s->seen);
    snprintf(s->summary, sizeof(s->summary), "%u ONLINE  %u FAULTS",
             (unsigned)s->online, (unsigned)s->faults);
  }
  if (s->safety_fault && !s->global_emergency) {
    snprintf(s->warning, sizeof(s->warning),
             "SAFETY NODE FAULT  %u EMERGENCY STATION OFFLINE",
             (unsigned)s->offline);
    snprintf(s->banner, sizeof(s->banner),
             "Offline Emergency Node is a safety-station fault, not global emergency.");
  } else if (s->global_emergency) {
    s->warning[0] = 0;
    strncpy(s->banner, "Global emergency is shown on the emergency screen, not this sheet.",
            sizeof(s->banner) - 1);
  } else {
    s->warning[0] = 0;
    strncpy(s->banner, "Stations assert only. This sheet cannot clear Showduino emergency.",
            sizeof(s->banner) - 1);
  }
}

static inline void showduino_emergency_desk_apply_detail(
    ShowduinoEmergencyDirectorSheet *s, const ShowduinoEmergencyDetailWire *d) {
  if (!s || !d) return;
  s->online = d->online;
  s->seen = d->seen;
  s->asserting = d->asserting;
  s->offline = d->offline;
  if (d->firstId[0]) {
    ShowduinoEmergencyDeskStation *st = &s->stations[0];
    if (!st->used || !st->id[0]) {
      st->used = 1;
      strncpy(st->id, d->firstId, sizeof(st->id) - 1);
      strncpy(st->name, d->firstName, sizeof(st->name) - 1);
      strncpy(st->state, d->firstState, sizeof(st->state) - 1);
      st->online = (d->online > 0) ? 1 : 0;
      st->latched = d->asserting ? 1 : 0;
    }
  }
  showduino_emergency_desk_rebuild(s);
}

static inline void showduino_emergency_desk_apply_station(
    ShowduinoEmergencyDirectorSheet *s, const ShowduinoEmergencyStationWire *st) {
  ShowduinoEmergencyDeskStation *dst;
  uint8_t i;
  if (!s || !st || !st->id[0]) return;
  dst = NULL;
  if (st->slot < SHOWDUINO_ESTOP_DESK_MAX_STATIONS &&
      (!s->stations[st->slot].used ||
       showduino_emergency_id_equal(s->stations[st->slot].id, st->id))) {
    dst = &s->stations[st->slot];
  }
  if (!dst) {
    for (i = 0; i < SHOWDUINO_ESTOP_DESK_MAX_STATIONS; i++) {
      if (s->stations[i].used && showduino_emergency_id_equal(s->stations[i].id, st->id)) {
        dst = &s->stations[i];
        break;
      }
    }
  }
  if (!dst) {
    for (i = 0; i < SHOWDUINO_ESTOP_DESK_MAX_STATIONS; i++) {
      if (!s->stations[i].used) {
        dst = &s->stations[i];
        break;
      }
    }
  }
  if (!dst) return;
  dst->used = 1;
  dst->online = st->online;
  dst->input_open = st->input_open;
  dst->latched = st->latched;
  dst->acked = st->acked;
  strncpy(dst->id, st->id, sizeof(dst->id) - 1);
  if (st->name[0]) strncpy(dst->name, st->name, sizeof(dst->name) - 1);
  if (st->state[0]) strncpy(dst->state, st->state, sizeof(dst->state) - 1);
  showduino_emergency_desk_rebuild(s);
}

static inline int showduino_emergency_desk_layout_fits_800x480(void) {
  const int cards_bottom =
      SHOWDUINO_ESTOP_DESK_GRID_Y + SHOWDUINO_ESTOP_DESK_STRIP_H + 8 +
      (2 * SHOWDUINO_ESTOP_DESK_CARD_H) + 8;
  return cards_bottom < SHOWDUINO_ESTOP_DESK_DOCK_Y;
}

static inline int showduino_emergency_desk_has_clear_control(
    const ShowduinoEmergencyDirectorSheet *s) {
  return s && s->has_clear_emergency ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_EMERGENCY_DIRECTOR_DESK_H */
