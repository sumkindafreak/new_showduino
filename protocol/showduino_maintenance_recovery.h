#ifndef SHOWDUINO_MAINTENANCE_RECOVERY_H
#define SHOWDUINO_MAINTENANCE_RECOVERY_H

/*
 * MISSION 001L: local maintenance recovery policy (shared by P4 firmware and tests).
 *
 * Two storage faults had no software exit:
 *   1. "Unknown boot state": the P4 restarted latched but the station blocker
 *      record (or the latch record itself) could not be read or was never
 *      verified. Every clear was refused forever.
 *   2. The command/episode counter record could not be read. Every clear was
 *      refused forever (no durable episode, no station command).
 *
 * Recovery here NEVER clears the emergency and NEVER shrinks the set of stations
 * that must prove RELEASED. It replaces an unknown state with a conservative known
 * one, after a person at the P4 USB port types an explicit confirmation stating the
 * physical inspection was done. The normal clear (Director or USB) must still pass
 * the full 001J/001K transaction afterwards.
 *
 *   Blockers: the new required set = every readable stored blocker UNION every
 *             provisioned station. Unprovisioned / legacy stations cannot be
 *             recovered automatically: they are covered only by the inspection.
 *   Counter : restart the counter at SHOWDUINO_RECOVER_COUNTER_BASE. Reusing a value
 *             can only make stations or the Director REJECT a command (they accept
 *             counters strictly above the last one they stored), never accept a
 *             replay. Re-provisioning removes that denial of service if it occurs.
 *
 * The Comms S3 can never reach these commands (USB console only, handled before
 * the command log line).
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "showduino_station_blockers.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_RECOVER_BLOCKERS_TOKEN "INSPECTED-ALL-STATIONS"
#define SHOWDUINO_RECOVER_COUNTER_TOKEN  "COUNTER-LOST"
#define SHOWDUINO_RECOVER_COUNTER_BASE   0xC0000000u

typedef enum ShowduinoRecoverResult {
  SHOWDUINO_RECOVER_OK = 0,
  SHOWDUINO_RECOVER_REFUSE_SOURCE = 1,     /* not the P4 USB console */
  SHOWDUINO_RECOVER_REFUSE_NOT_NEEDED = 2, /* the fault it repairs is not present */
  SHOWDUINO_RECOVER_REFUSE_TOKEN = 3,      /* confirmation text missing or wrong */
  SHOWDUINO_RECOVER_REFUSE_BUTTON = 4,     /* GPIO25 loop open */
  SHOWDUINO_RECOVER_REFUSE_NOT_LATCHED = 5,
  SHOWDUINO_RECOVER_REFUSE_OVERFLOW = 6    /* conservative set does not fit */
} ShowduinoRecoverResult;

static inline const char *showduino_recover_result_name(int r) {
  switch (r) {
    case SHOWDUINO_RECOVER_OK: return "OK";
    case SHOWDUINO_RECOVER_REFUSE_SOURCE: return "REFUSED_SOURCE";
    case SHOWDUINO_RECOVER_REFUSE_NOT_NEEDED: return "REFUSED_NOT_NEEDED";
    case SHOWDUINO_RECOVER_REFUSE_TOKEN: return "REFUSED_CONFIRMATION";
    case SHOWDUINO_RECOVER_REFUSE_BUTTON: return "REFUSED_BUTTON_ACTIVE";
    case SHOWDUINO_RECOVER_REFUSE_NOT_LATCHED: return "REFUSED_NOT_LATCHED";
    case SHOWDUINO_RECOVER_REFUSE_OVERFLOW: return "REFUSED_OVERFLOW";
    default: return "REFUSED";
  }
}

/* Gate for EMERGENCY:RECOVER:BLOCKERS:<token>. */
static inline int showduino_recover_blockers_allowed(int fromLocalUsb, int latched, int bootUnknown,
                                                     int physicalPressed, const char *token) {
  if (!fromLocalUsb) return SHOWDUINO_RECOVER_REFUSE_SOURCE;
  if (!latched) return SHOWDUINO_RECOVER_REFUSE_NOT_LATCHED;
  if (!bootUnknown) return SHOWDUINO_RECOVER_REFUSE_NOT_NEEDED;
  if (!token || strcmp(token, SHOWDUINO_RECOVER_BLOCKERS_TOKEN) != 0) return SHOWDUINO_RECOVER_REFUSE_TOKEN;
  if (physicalPressed) return SHOWDUINO_RECOVER_REFUSE_BUTTON;
  return SHOWDUINO_RECOVER_OK;
}

/* Conservative required set: readable stored blockers UNION provisioned IDs.
 * Returns SHOWDUINO_RECOVER_OK or SHOWDUINO_RECOVER_REFUSE_OVERFLOW. Never drops an
 * entry to make room. */
static inline int showduino_recover_build_blockers(const ShowduinoBlocker *stored, int storedCount,
                                                   const char (*provIds)[SHOWDUINO_BLOCKER_ID_CHARS + 1],
                                                   int provCount, ShowduinoBlocker *out, int cap,
                                                   int *count) {
  if (!out || !count) return SHOWDUINO_RECOVER_REFUSE_OVERFLOW;
  *count = 0;
  for (int i = 0; stored && i < storedCount; ++i) {
    if (!stored[i].id[0]) continue;
    int dup = 0;
    for (int k = 0; k < *count; ++k) dup |= strcmp(out[k].id, stored[i].id) == 0;
    if (dup) continue;
    if (*count >= cap) { *count = 0; return SHOWDUINO_RECOVER_REFUSE_OVERFLOW; }
    out[*count] = stored[i];
    (*count)++;
  }
  for (int i = 0; provIds && i < provCount; ++i) {
    if (!provIds[i][0]) continue;
    int dup = 0;
    for (int k = 0; k < *count; ++k) dup |= strcmp(out[k].id, provIds[i]) == 0;
    if (dup) continue;
    if (*count >= cap) { *count = 0; return SHOWDUINO_RECOVER_REFUSE_OVERFLOW; }
    memset(&out[*count], 0, sizeof(out[*count]));
    strncpy(out[*count].id, provIds[i], sizeof(out[*count].id) - 1);
    (*count)++;
  }
  return SHOWDUINO_RECOVER_OK;
}

/* Gate for EMERGENCY:RECOVER:COUNTER:<token>. */
static inline int showduino_recover_counter_allowed(int fromLocalUsb, int counterUnreadable,
                                                    const char *token) {
  if (!fromLocalUsb) return SHOWDUINO_RECOVER_REFUSE_SOURCE;
  if (!counterUnreadable) return SHOWDUINO_RECOVER_REFUSE_NOT_NEEDED;
  if (!token || strcmp(token, SHOWDUINO_RECOVER_COUNTER_TOKEN) != 0) return SHOWDUINO_RECOVER_REFUSE_TOKEN;
  return SHOWDUINO_RECOVER_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_MAINTENANCE_RECOVERY_H */
