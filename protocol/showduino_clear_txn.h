#ifndef SHOWDUINO_CLEAR_TXN_H
#define SHOWDUINO_CLEAR_TXN_H

/*
 * MISSION 001K: P4 emergency-clear transaction (shared by firmware and host tests).
 *
 *   IDLE --(authenticated Director REQ, or local USB maintenance)--> OPEN
 *        P4 draws a fresh transaction nonce, bound to the current episode.
 *        The Director must echo the nonce in its signed CFM.
 *   OPEN --(authenticated CFM for this nonce, within SHOWDUINO_CLEAR_TXN_OPEN_MS)--> VERIFY
 *        The P4 records the next challenge sequence and challenges every station
 *        now. Only answers to challenges issued AFTER the confirm count; proofs
 *        cached from before the confirm never do.
 *   VERIFY --(every provisioned station proves RELEASED, full clear decision OK)--> ACCEPT
 *   VERIFY --(any ASSERT, any fresh PRESSED, episode change, storage or overflow
 *             fault)--> REJECT immediately
 *   VERIFY --(not all proven within SHOWDUINO_CLEAR_VERIFY_WINDOW_MS)--> REJECT
 *
 * Boundary (see docs/emergency-director-auth-001K.md section 6): every station
 * message travels through the Comms S3. A compromised S3 can suppress an ASSERT,
 * and the station's later answers, so the P4 cannot prove that no station is pressed
 * at the instant of clear. This transaction bounds the exposure to presses made
 * after a station's post-confirm RELEASED answer and before the clear is applied
 * (at most SHOWDUINO_CLEAR_VERIFY_WINDOW_MS), and only when the S3 is actively hostile.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "showduino_station_model.h"
#include "showduino_station_auth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_CLEAR_TXN_OPEN_MS        12000u  /* matches SHOWDUINO_ESTOP_CLEAR_REQUEST_TIMEOUT_MS */
#define SHOWDUINO_CLEAR_VERIFY_WINDOW_MS   1000u
#define SHOWDUINO_CLEAR_VERIFY_RECHAL_MS   250u

typedef enum ShowduinoClearTxnState {
  SHOWDUINO_TXN_IDLE = 0,
  SHOWDUINO_TXN_OPEN = 1,
  SHOWDUINO_TXN_VERIFY = 2
} ShowduinoClearTxnState;

typedef enum ShowduinoClearAuthority {
  SHOWDUINO_CLEAR_BY_NONE = 0,
  SHOWDUINO_CLEAR_BY_DIRECTOR = 1,
  SHOWDUINO_CLEAR_BY_LOCAL_USB = 2
} ShowduinoClearAuthority;

typedef enum ShowduinoClearTxnResult {
  SHOWDUINO_TXN_PENDING = 0,
  SHOWDUINO_TXN_ACCEPT = 1,
  SHOWDUINO_TXN_REJECT_NOT_OPEN = 2,
  SHOWDUINO_TXN_REJECT_EXPIRED = 3,
  SHOWDUINO_TXN_REJECT_EPISODE = 4,
  SHOWDUINO_TXN_REJECT_SUPERSEDED = 5,
  SHOWDUINO_TXN_REJECT_PRESSED = 6,
  SHOWDUINO_TXN_REJECT_UNVERIFIED = 7,
  SHOWDUINO_TXN_REJECT_BLOCKED = 8
} ShowduinoClearTxnResult;

typedef struct ShowduinoClearTxn {
  uint8_t state;
  uint8_t authority;
  uint32_t episode;
  char nonceHex[SHOWDUINO_AUTH_NONCE_HEX + 1];
  uint32_t openedMs;
  uint32_t verifyStartMs;
  uint32_t verifySeq;       /* first challenge sequence whose answers count */
  uint32_t lastChalMs;
  int blockDecision;        /* ShowduinoClearDecision behind a BLOCKED / UNVERIFIED result */
  char blockId[SHOWDUINO_BLOCKER_ID_CHARS + 1];
} ShowduinoClearTxn;

static inline const char *showduino_clear_txn_result_name(int r) {
  switch (r) {
    case SHOWDUINO_TXN_PENDING: return "PENDING";
    case SHOWDUINO_TXN_ACCEPT: return "OK";
    case SHOWDUINO_TXN_REJECT_NOT_OPEN: return "NO_REQUEST";
    case SHOWDUINO_TXN_REJECT_EXPIRED: return "TIMEOUT";
    case SHOWDUINO_TXN_REJECT_EPISODE: return "WRONG_EPISODE";
    case SHOWDUINO_TXN_REJECT_SUPERSEDED: return "SUPERSEDED";
    case SHOWDUINO_TXN_REJECT_PRESSED: return "STATION_PRESSED";
    case SHOWDUINO_TXN_REJECT_UNVERIFIED: return "STATION_UNVERIFIED";
    case SHOWDUINO_TXN_REJECT_BLOCKED: return "STATION_BLOCKED";
    default: return "REJECTED";
  }
}

static inline void showduino_clear_txn_abort(ShowduinoClearTxn *t) {
  if (!t) return;
  memset(t, 0, sizeof(*t));
}

/* Opens a transaction. Replaces any earlier one (an earlier nonce can then never be
 * confirmed). Returns 1 and fills t->nonceHex. */
static inline int showduino_clear_txn_open(ShowduinoClearTxn *t, int authority, uint32_t episode,
                                           uint32_t nowMs, ShowduinoRandFn rnd) {
  uint8_t nonce[SHOWDUINO_AUTH_NONCE_BYTES];
  if (!t || !rnd || episode == 0) return 0;
  showduino_clear_txn_abort(t);
  if (rnd(nonce, sizeof(nonce)) != 0) return 0;
  int nonzero = 0;
  for (size_t i = 0; i < sizeof(nonce); ++i) nonzero |= nonce[i];
  if (!nonzero) return 0;
  showduino_auth_hex_encode(nonce, sizeof(nonce), t->nonceHex);
  memset(nonce, 0, sizeof(nonce));
  t->state = SHOWDUINO_TXN_OPEN;
  t->authority = (uint8_t)authority;
  t->episode = episode;
  t->openedMs = nowMs;
  return 1;
}

/* The open nonce, or NULL. Only an OPEN transaction can be confirmed. */
static inline const char *showduino_clear_txn_open_nonce(const ShowduinoClearTxn *t) {
  return (t && t->state == SHOWDUINO_TXN_OPEN) ? t->nonceHex : 0;
}

/* OPEN -> VERIFY. firstSeq = showduino_auth_chal_next_seq() taken before the P4
 * issues the verification challenge. */
static inline int showduino_clear_txn_begin_verify(ShowduinoClearTxn *t, uint32_t episodeNow,
                                                   uint32_t nowMs, uint32_t firstSeq) {
  if (!t || t->state != SHOWDUINO_TXN_OPEN) return SHOWDUINO_TXN_REJECT_NOT_OPEN;
  if ((uint32_t)(nowMs - t->openedMs) > SHOWDUINO_CLEAR_TXN_OPEN_MS) {
    showduino_clear_txn_abort(t);
    return SHOWDUINO_TXN_REJECT_EXPIRED;
  }
  if (episodeNow == 0 || episodeNow != t->episode) {
    showduino_clear_txn_abort(t);
    return SHOWDUINO_TXN_REJECT_EPISODE;
  }
  t->state = SHOWDUINO_TXN_VERIFY;
  t->verifyStartMs = nowMs;
  t->lastChalMs = nowMs;
  t->verifySeq = firstSeq;
  return SHOWDUINO_TXN_PENDING;
}

/* Any ASSERT (any ID, recorded or not), a new emergency trigger, or the physical
 * input: the operator's decision was made on stale information. */
static inline int showduino_clear_txn_on_assert(ShowduinoClearTxn *t) {
  if (!t || t->state == SHOWDUINO_TXN_IDLE) return 0;
  showduino_clear_txn_abort(t);
  return SHOWDUINO_TXN_REJECT_SUPERSEDED;
}

/* 1 when the verifying P4 should issue another challenge (lost frames). */
static inline int showduino_clear_txn_want_rechallenge(ShowduinoClearTxn *t, uint32_t nowMs) {
  if (!t || t->state != SHOWDUINO_TXN_VERIFY) return 0;
  if ((uint32_t)(nowMs - t->lastChalMs) < SHOWDUINO_CLEAR_VERIFY_RECHAL_MS) return 0;
  t->lastChalMs = nowMs;
  return 1;
}

static inline void showduino_clear_txn_note_block(ShowduinoClearTxn *t, const ShowduinoStationModel *m,
                                                  const char (*ids)[SHOWDUINO_BLOCKER_ID_CHARS + 1],
                                                  int n, int idCount, uint32_t since) {
  t->blockId[0] = '\0';
  for (int k = 0; k < idCount; ++k) {
    const int s = showduino_station_model_find_id(m, n, ids[k]);
    if (s < 0 || !m[s].reportKnown || !m[s].online || m[s].inputOpen ||
        m[s].proofChalSeq == 0 || (int32_t)(m[s].proofChalSeq - since) < 0) {
      showduino_station_copy(t->blockId, sizeof(t->blockId), ids[k]);
      return;
    }
  }
  for (int i = 0; i < n; ++i) {
    if (showduino_station_model_blocks_clear(&m[i])) {
      showduino_station_copy(t->blockId, sizeof(t->blockId), m[i].id);
      return;
    }
  }
}

/* Evaluates a VERIFY transaction. Call on every station event and from the loop.
 * On any result other than PENDING the transaction is closed (single use). */
static inline int showduino_clear_txn_evaluate(ShowduinoClearTxn *t, const ShowduinoStationModel *m,
                                               int n,
                                               const char (*ids)[SHOWDUINO_BLOCKER_ID_CHARS + 1],
                                               int idCount, uint32_t episodeNow, uint32_t nowMs,
                                               int unknownBoot, int persistDirty, int counterFault,
                                               int tableOverflow, int booted) {
  if (!t) return SHOWDUINO_TXN_REJECT_NOT_OPEN;
  if (t->state == SHOWDUINO_TXN_OPEN) {
    if ((uint32_t)(nowMs - t->openedMs) > SHOWDUINO_CLEAR_TXN_OPEN_MS) {
      showduino_clear_txn_abort(t);
      return SHOWDUINO_TXN_REJECT_EXPIRED;
    }
    return SHOWDUINO_TXN_PENDING;
  }
  if (t->state != SHOWDUINO_TXN_VERIFY) return SHOWDUINO_TXN_REJECT_NOT_OPEN;
  if (episodeNow == 0 || episodeNow != t->episode) {
    showduino_clear_txn_abort(t);
    return SHOWDUINO_TXN_REJECT_EPISODE;
  }
  /* A fresh authenticated PRESSED from any station ends the attempt at once. */
  for (int i = 0; i < n; ++i) {
    if (m[i].used && m[i].reportKnown && m[i].inputOpen && m[i].proofChalSeq != 0 &&
        (int32_t)(m[i].proofChalSeq - t->verifySeq) >= 0) {
      showduino_station_copy(t->blockId, sizeof(t->blockId), m[i].id);
      t->state = SHOWDUINO_TXN_IDLE;
      return SHOWDUINO_TXN_REJECT_PRESSED;
    }
  }
  const int d = showduino_station_clear_decision(m, n, ids, idCount, t->verifySeq, unknownBoot,
                                                 persistDirty, counterFault, tableOverflow, booted);
  t->blockDecision = d;
  if (d == SHOWDUINO_CLEAR_OK) {
    t->state = SHOWDUINO_TXN_IDLE;
    return SHOWDUINO_TXN_ACCEPT;
  }
  if (d == SHOWDUINO_CLEAR_BLOCK_STORAGE || d == SHOWDUINO_CLEAR_BLOCK_OVERFLOW) {
    t->state = SHOWDUINO_TXN_IDLE;
    return SHOWDUINO_TXN_REJECT_BLOCKED;
  }
  if ((uint32_t)(nowMs - t->verifyStartMs) > SHOWDUINO_CLEAR_VERIFY_WINDOW_MS) {
    showduino_clear_txn_note_block(t, m, ids, n, idCount, t->verifySeq);
    t->state = SHOWDUINO_TXN_IDLE;
    return SHOWDUINO_TXN_REJECT_UNVERIFIED;
  }
  return SHOWDUINO_TXN_PENDING;
}

/* ---------- entry-point policy (P4 dispatch) ----------
 * Where a clear-related line may come from, and what it may do.
 *   fromLocalUsb  1 only for the P4's own USB console (physical access).
 * Every other source (Comms UART: Director, S3, S3 web UI, P4 web API, show cues)
 * is untrusted text and can only clear through a valid Director signature. */
typedef enum ShowduinoClearEntry {
  SHOWDUINO_CLEAR_ENTRY_NONE = 0,          /* not a clear command */
  SHOWDUINO_CLEAR_ENTRY_DIRECTOR = 1,      /* authenticate as DCLR */
  SHOWDUINO_CLEAR_ENTRY_LOCAL = 2,         /* USB maintenance clear */
  SHOWDUINO_CLEAR_ENTRY_CANCEL = 3,        /* always allowed: only ever cancels */
  SHOWDUINO_CLEAR_ENTRY_REFUSE_AUTH = 4,   /* plain clear text from an untrusted source */
  SHOWDUINO_CLEAR_ENTRY_REFUSE_SOURCE = 5  /* right command, wrong source */
} ShowduinoClearEntry;

static inline int showduino_clear_entry(int fromLocalUsb, const char *cmd) {
  if (!cmd) return SHOWDUINO_CLEAR_ENTRY_NONE;
  const int isDclr = strncmp(cmd, "DCLR:", 5) == 0;
  const int isClear = strcmp(cmd, "EMERGENCY:CLEAR") == 0 || strcmp(cmd, "EMERGENCY:CLEAR_REQUEST") == 0;
  const int isConfirm = strcmp(cmd, "EMERGENCY:CLEAR_CONFIRM") == 0;
  const int isCancel = strcmp(cmd, "EMERGENCY:CLEAR_CANCEL") == 0;
  if (isCancel) return SHOWDUINO_CLEAR_ENTRY_CANCEL;
  if (fromLocalUsb) {
    if (strcmp(cmd, "EMERGENCY:CLEAR") == 0) return SHOWDUINO_CLEAR_ENTRY_LOCAL;
    if (isDclr || isConfirm || isClear) return SHOWDUINO_CLEAR_ENTRY_REFUSE_SOURCE;
    return SHOWDUINO_CLEAR_ENTRY_NONE;
  }
  if (isDclr) return SHOWDUINO_CLEAR_ENTRY_DIRECTOR;
  if (isClear || isConfirm) return SHOWDUINO_CLEAR_ENTRY_REFUSE_AUTH;
  return SHOWDUINO_CLEAR_ENTRY_NONE;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_CLEAR_TXN_H */
