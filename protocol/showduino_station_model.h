#ifndef SHOWDUINO_STATION_MODEL_H
#define SHOWDUINO_STATION_MODEL_H

/*
 * P4 interpretation of wireless Emergency Node messages (MISSION 001H).
 *
 * Three conditions are kept separate:
 *   1. Physical button: set ONLY by an identity-verified ANNOUNCE (IN=).
 *   2. Station local latch: the announced L= value (announcedLatched).
 *   3. P4 global emergency: any ASSERT, or an announced latched station, asks
 *      for the global latch. This never changes the physical button state.
 *
 * A bare ASSERT carries no button state. The C3 keeps re-sending it while it is
 * locally latched after a release, so it must not decide the button state.
 * Until an ANNOUNCE is verified, or when the last verified report was NOT
 * latched, an ASSERT is treated as PRESSED (conservative). A verified ANNOUNCE
 * then confirms or corrects that state.
 *
 * Clear blocking uses shared rules: any station that is PRESSED, has no
 * verified report this session, or is OFFLINE blocks clear.
 *
 * MISSION 001I freshness rule: a bare ASSERT never sets online. Only a fresh,
 * identity-validated full ANNOUNCE can make a station online again, so a
 * disconnected station stays blocking after an ASSERT alone.
 *
 * Persistence rule: a station that took part in the current global-latch
 * episode (asserted) stays in the stored blocker set until an accepted Director
 * clear ends the episode. This holds even when it was released before it
 * disconnected, so a reboot cannot drop it from the clear conditions.
 *
 * Identity (ID and MAC) is continuity, NOT authentication. ESP-NOW frames are
 * unsigned. The wireless-authentication problem remains a release blocker.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "showduino_emergency_node.h"
#include "showduino_station_blockers.h"

typedef struct ShowduinoStationModel {
  uint8_t used;
  uint8_t online;           /* heard within the comms timeout */
  uint8_t reportKnown;      /* an identity-verified ANNOUNCE was accepted */
  uint8_t inputOpen;        /* 1 = PRESSED. Only a verified ANNOUNCE can set 0. */
  uint8_t announcedLatched; /* L= from the last verified ANNOUNCE */
  uint8_t asserted;         /* in the current global-latch episode (see header note) */
  char id[SHOWDUINO_BLOCKER_ID_CHARS + 1];
  char mac[SHOWDUINO_BLOCKER_MAC_CHARS + 1];
} ShowduinoStationModel;

typedef enum ShowduinoStationLineKind {
  SHOWDUINO_STATION_LINE_NONE = 0,
  SHOWDUINO_STATION_LINE_ANNOUNCE,
  SHOWDUINO_STATION_LINE_ASSERT,
  SHOWDUINO_STATION_LINE_STATUS
} ShowduinoStationLineKind;

typedef struct ShowduinoStationResult {
  int kind;                 /* ShowduinoStationLineKind */
  int slot;                 /* table index, or -1 */
  int accepted;             /* 1 = applied; 0 = rejected, nothing changed */
  int globalLatch;          /* 1 = ask the P4 global latch to engage */
  char ackId[SHOWDUINO_BLOCKER_ID_CHARS + 1];
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1];
} ShowduinoStationResult;

static inline void showduino_station_copy(char *dst, size_t cap, const char *src) {
  if (!dst || cap == 0) return;
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}

/* A stored blocker restored at boot: PRESSED, unseen, offline, not yet verified. */
static inline void showduino_station_model_restore(ShowduinoStationModel *m,
                                                   const char *id, const char *mac) {
  if (!m) return;
  memset(m, 0, sizeof(*m));
  m->used = 1;
  m->inputOpen = 1;
  m->asserted = 1; /* a stored blocker always belongs to an active episode */
  showduino_station_copy(m->id, sizeof(m->id), id);
  showduino_station_copy(m->mac, sizeof(m->mac), mac);
}

static inline int showduino_station_model_find_id(const ShowduinoStationModel *t, int n,
                                                  const char *id) {
  if (!t || !id || !id[0]) return -1;
  for (int i = 0; i < n; ++i) {
    if (t[i].used && t[i].id[0] && strcmp(t[i].id, id) == 0) return i;
  }
  return -1;
}

static inline int showduino_station_model_find_mac(const ShowduinoStationModel *t, int n,
                                                   const char *mac) {
  if (!t || !mac || !mac[0]) return -1;
  for (int i = 0; i < n; ++i) {
    if (t[i].used && t[i].mac[0] && strcmp(t[i].mac, mac) == 0) return i;
  }
  return -1;
}

static inline int showduino_station_model_alloc(ShowduinoStationModel *t, int n) {
  if (!t) return -1;
  for (int i = 0; i < n; ++i) {
    if (!t[i].used) {
      memset(&t[i], 0, sizeof(t[i]));
      t[i].used = 1;
      return i;
    }
  }
  return -1;
}

/* Full status report. Accepted only when the identity matches (see
 * showduino_station_apply_identified_report). Sets the button state. */
static inline int showduino_station_model_on_announce(ShowduinoStationModel *m,
                                                      const ShowduinoEmergencyAnnounce *an,
                                                      int *globalLatch) {
  if (!m || !an) return 0;
  int open = m->inputOpen ? 1 : 0;
  if (!showduino_station_apply_identified_report(m->id, sizeof(m->id), m->mac, sizeof(m->mac),
                                                 &open, an->id, an->mac,
                                                 an->input_open ? 1 : 0)) {
    return 0;
  }
  m->inputOpen = open ? 1 : 0;
  m->reportKnown = 1;
  m->online = 1;
  m->announcedLatched = an->latched ? 1 : 0;
  if (an->latched) m->asserted = 1;
  if (globalLatch) *globalLatch = an->latched ? 1 : 0;
  return 1;
}

/* Bare ASSERT. Never sets RELEASED and never sets ONLINE (MISSION 001I).
 * Only a fresh, identity-validated ANNOUNCE can restore a station to online. */
static inline int showduino_station_model_on_assert(ShowduinoStationModel *m,
                                                    const char *id, int *globalLatch) {
  if (!m || !id || !id[0]) return 0;
  if (m->id[0] && strcmp(m->id, id) != 0) return 0;
  if (!m->id[0]) showduino_station_copy(m->id, sizeof(m->id), id);
  m->asserted = 1;
  if (!m->reportKnown || !m->announcedLatched) m->inputOpen = 1;
  if (globalLatch) *globalLatch = 1;
  return 1;
}

static inline void showduino_station_model_on_timeout(ShowduinoStationModel *m) {
  if (m) m->online = 0;
}

/* 1 = this station blocks Director clear. */
static inline int showduino_station_model_blocks_clear(const ShowduinoStationModel *m) {
  if (!m || !m->used) return 0;
  return (m->inputOpen || !m->reportKnown || !m->online) ? 1 : 0;
}

static inline int showduino_station_model_count_blocking(const ShowduinoStationModel *t, int n) {
  int c = 0;
  for (int i = 0; t && i < n; ++i) c += showduino_station_model_blocks_clear(&t[i]);
  return c;
}

/* 1 = this station must be in the stored blocker set. Superset of blocks_clear:
 * an asserted station stays required after it is released and verified. */
static inline int showduino_station_model_required(const ShowduinoStationModel *m) {
  if (!m || !m->used) return 0;
  return (m->asserted || showduino_station_model_blocks_clear(m)) ? 1 : 0;
}

/* Accepted Director clear: the episode is over, so no station is required. */
static inline void showduino_station_model_end_episode(ShowduinoStationModel *t, int n) {
  for (int i = 0; t && i < n; ++i) t[i].asserted = 0;
}

/* Builds the stored blocker set. Returns 0 when it cannot be built faithfully (a
 * required station without an identity, or more entries than cap). The caller
 * must then treat storage as failed, so clear stays blocked. Never drops silently. */
static inline int showduino_station_model_collect_required(const ShowduinoStationModel *t, int n,
                                                           ShowduinoBlocker *out, int cap,
                                                           int *count) {
  if (!count) return 0;
  *count = 0;
  if (!t || !out) return 0;
  for (int i = 0; i < n; ++i) {
    if (!showduino_station_model_required(&t[i])) continue;
    if (!t[i].id[0]) return 0;
    if (*count >= cap) return 0;
    showduino_station_copy(out[*count].id, sizeof(out[*count].id), t[i].id);
    showduino_station_copy(out[*count].mac, sizeof(out[*count].mac), t[i].mac);
    (*count)++;
  }
  return 1;
}

/* Interprets one wireless line from an Emergency Node against the station table.
 * Returns 1 when the line is a station message this model handles. The caller
 * performs the side effects (persistence, global latch, ACK routing). */
static inline int showduino_station_interpret_line(ShowduinoStationModel *t, int n,
                                                   const char *line,
                                                   ShowduinoStationResult *res) {
  if (!t || !line || !res) return 0;
  memset(res, 0, sizeof(*res));
  res->slot = -1;

  if (!strncmp(line, "ANNOUNCE:", 9)) {
    ShowduinoEmergencyAnnounce an;
    if (!showduino_emergency_parse_announce(line, &an)) return 0;
    res->kind = SHOWDUINO_STATION_LINE_ANNOUNCE;
    int slot = showduino_station_model_find_id(t, n, an.id);
    if (slot < 0) slot = showduino_station_model_find_mac(t, n, an.mac);
    if (slot < 0) slot = showduino_station_model_alloc(t, n);
    if (slot < 0) return 1; /* table full: rejected */
    res->slot = slot;
    int gl = 0;
    res->accepted = showduino_station_model_on_announce(&t[slot], &an, &gl);
    if (res->accepted) {
      res->globalLatch = (gl && t[slot].id[0]) ? 1 : 0;
      showduino_station_copy(res->ackId, sizeof(res->ackId), t[slot].id);
      showduino_station_copy(res->name, sizeof(res->name), an.name);
    }
    return 1;
  }

  if (showduino_emergency_is_assert(line)) {
    char id[SHOWDUINO_BLOCKER_ID_CHARS + 1] = "";
    char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1] = "";
    if (!showduino_emergency_parse_assert(line, id, sizeof(id), name, sizeof(name))) return 0;
    res->kind = SHOWDUINO_STATION_LINE_ASSERT;
    int slot = showduino_station_model_find_id(t, n, id);
    if (slot < 0) slot = showduino_station_model_alloc(t, n);
    if (slot < 0) return 1;
    res->slot = slot;
    int gl = 0;
    res->accepted = showduino_station_model_on_assert(&t[slot], id, &gl);
    if (res->accepted) {
      res->globalLatch = gl;
      showduino_station_copy(res->ackId, sizeof(res->ackId), t[slot].id);
      showduino_station_copy(res->name, sizeof(res->name), name);
    }
    return 1;
  }

  if (!strncmp(line, "STATUS:", 7)) {
    /* STATUS carries no MAC and is not an identity-verified report. It can
     * request the global latch, but never sets the button state. */
    res->kind = SHOWDUINO_STATION_LINE_STATUS;
    int onlineCount = 0, only = -1;
    for (int i = 0; i < n; ++i) {
      if (t[i].used && t[i].online) { onlineCount++; only = i; }
    }
    if (onlineCount == 1 && only >= 0) {
      res->slot = only;
      res->accepted = 1;
      if (strstr(line, "L=1") && t[only].id[0]) {
        res->globalLatch = 1;
        showduino_station_copy(res->ackId, sizeof(res->ackId), t[only].id);
      }
    }
    return 1;
  }
  return 0;
}

#endif
