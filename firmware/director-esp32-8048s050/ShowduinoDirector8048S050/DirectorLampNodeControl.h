#ifndef SHOWDUINO_DIRECTOR_LAMP_NODE_CONTROL_H
#define SHOWDUINO_DIRECTOR_LAMP_NODE_CONTROL_H

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../../protocol/showduino_lamp_director_desk.h"
#include "../../../protocol/showduino_state_wire.h"

#define DIRECTOR_LAMP_PENDING_MS 5000u

typedef enum DirectorLampPending {
  DIRECTOR_LAMP_PEND_NONE = 0,
  DIRECTOR_LAMP_PEND_IGNITE,
  DIRECTOR_LAMP_PEND_EXTINGUISH,
  DIRECTOR_LAMP_PEND_JEWEL,
  DIRECTOR_LAMP_PEND_FLAME,
  DIRECTOR_LAMP_PEND_OFF,
  DIRECTOR_LAMP_PEND_AUDIO,
  DIRECTOR_LAMP_PEND_STATUS
} DirectorLampPending;

typedef struct DirectorLampNodeControl {
  bool seen;
  bool online;
  bool emergency;
  bool detailValid;
  bool sensorsValid;
  char logicalId[16];
  char presence[40];
  char state[20];
  char owner[20];
  char button[12];
  char mic[8];
  char blow[12];
  char motion[12];
  char light[8];
  char voltage[12];
  char audio[16];
  char feedback[48];
  char lastErrorText[40];
  uint32_t pendingSinceMs;
  uint32_t pendingSeq;
  DirectorLampPending pending;
  ShowduinoLampDirectorSheet sheet;
} DirectorLampNodeControl;

static inline void director_lamp_node_clear(DirectorLampNodeControl *m) {
  if (!m) return;
  memset(m, 0, sizeof(*m));
  strncpy(m->state, "OFFLINE", sizeof(m->state) - 1);
  strncpy(m->presence, "OFFLINE", sizeof(m->presence) - 1);
  strncpy(m->logicalId, SHOWDUINO_CARBIDE_LOGICAL_DEFAULT, sizeof(m->logicalId) - 1);
  strncpy(m->button, "--", sizeof(m->button) - 1);
  strncpy(m->mic, "--", sizeof(m->mic) - 1);
  strncpy(m->blow, "--", sizeof(m->blow) - 1);
  strncpy(m->motion, "--", sizeof(m->motion) - 1);
  strncpy(m->light, "--", sizeof(m->light) - 1);
  strncpy(m->voltage, "--", sizeof(m->voltage) - 1);
  strncpy(m->audio, "--", sizeof(m->audio) - 1);
}

static inline void director_lamp_clear_pending(DirectorLampNodeControl *m,
                                               const char *feedback) {
  if (!m) return;
  m->pending = DIRECTOR_LAMP_PEND_NONE;
  m->pendingSinceMs = 0;
  m->pendingSeq = 0;
  if (feedback) {
    strncpy(m->feedback, feedback, sizeof(m->feedback) - 1);
    m->feedback[sizeof(m->feedback) - 1] = '\0';
  }
}

static inline void director_lamp_set_pending(DirectorLampNodeControl *m,
                                             DirectorLampPending p,
                                             uint32_t nowMs) {
  if (!m) return;
  m->pending = p;
  m->pendingSinceMs = nowMs;
  m->feedback[0] = '\0';
  m->lastErrorText[0] = '\0';
}

static inline bool director_lamp_pending_timeout(DirectorLampNodeControl *m,
                                                 uint32_t nowMs) {
  if (!m || m->pending == DIRECTOR_LAMP_PEND_NONE) return false;
  if ((nowMs - m->pendingSinceMs) < DIRECTOR_LAMP_PENDING_MS) return false;
  director_lamp_clear_pending(m, "COMMAND TIMEOUT");
  strncpy(m->lastErrorText, "TIMEOUT", sizeof(m->lastErrorText) - 1);
  return true;
}

static inline void director_lamp_apply_sheet(DirectorLampNodeControl *m,
                                             const ShowduinoLampDirectorSheet *sh) {
  if (!m || !sh) return;
  m->sheet = *sh;
  m->online = sh->online != 0;
  m->emergency = sh->emergency != 0;
  strncpy(m->logicalId, sh->logical_id, sizeof(m->logicalId) - 1);
  strncpy(m->presence, sh->presence_line, sizeof(m->presence) - 1);
  strncpy(m->state, sh->state, sizeof(m->state) - 1);
  strncpy(m->button, sh->button, sizeof(m->button) - 1);
  strncpy(m->mic, sh->mic_health, sizeof(m->mic) - 1);
  strncpy(m->blow, sh->blow, sizeof(m->blow) - 1);
  strncpy(m->motion, sh->motion, sizeof(m->motion) - 1);
  strncpy(m->light, sh->light, sizeof(m->light) - 1);
  strncpy(m->voltage, sh->voltage, sizeof(m->voltage) - 1);
  strncpy(m->audio, sh->audio, sizeof(m->audio) - 1);
  if (m->online) m->seen = true;
}

static inline void director_lamp_note_accepted(DirectorLampNodeControl *m) {
  if (!m) return;
  if (m->pending != DIRECTOR_LAMP_PEND_NONE) {
    director_lamp_clear_pending(m, "ACCEPTED");
  }
}

static inline void director_lamp_reconcile_from_state(DirectorLampNodeControl *m) {
  if (!m || m->pending == DIRECTOR_LAMP_PEND_NONE) return;
  if (m->pending == DIRECTOR_LAMP_PEND_IGNITE || m->pending == DIRECTOR_LAMP_PEND_FLAME) {
    if (showduino_lamp_desk_fx_is_carbide(m->state) && strcmp(m->state, "EXTINGUISHING") != 0) {
      director_lamp_clear_pending(m, "CONFIRMED");
    }
  } else if (m->pending == DIRECTOR_LAMP_PEND_EXTINGUISH ||
             m->pending == DIRECTOR_LAMP_PEND_OFF) {
    if (!strcmp(m->state, "OFF") || !strcmp(m->state, "OFFLINE")) {
      director_lamp_clear_pending(m, "CONFIRMED");
    }
  } else if (m->pending == DIRECTOR_LAMP_PEND_STATUS ||
             m->pending == DIRECTOR_LAMP_PEND_JEWEL ||
             m->pending == DIRECTOR_LAMP_PEND_AUDIO) {
    /* ACCEPTED wire or timeout clears these. */
  }
}

static inline void director_lamp_note_failed(DirectorLampNodeControl *m,
                                             const char *reason) {
  if (!m) return;
  director_lamp_clear_pending(m, "FAILED");
  if (reason && reason[0]) {
    strncpy(m->lastErrorText, reason, sizeof(m->lastErrorText) - 1);
  }
}

#endif
