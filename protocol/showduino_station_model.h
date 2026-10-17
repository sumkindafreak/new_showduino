#ifndef SHOWDUINO_STATION_MODEL_H
#define SHOWDUINO_STATION_MODEL_H

/*
 * P4 interpretation of wireless Emergency Node messages.
 *
 * MISSION 001J: only an authenticated AUTH response can change a station's
 * physical state, its online status, or its verified-report status.
 *
 *   ASSERT   unauthenticated. Latches the P4 global emergency and records that
 *            the ID asserted in this episode. It never sets online, never sets
 *            RELEASED, and never satisfies clearance.
 *   ANNOUNCE display only. No state change.
 *   STATUS   display only. No state change.
 *   AUTH     verified with the station's key, bound to a challenge this boot
 *            issued for the current episode, within the freshness window, once
 *            per station. Only this path sets RELEASED/PRESSED and ONLINE.
 *
 * Clear blocking: a station blocks clear while it is PRESSED, unverified, or
 * OFFLINE. Persistence (see showduino_station_model_required) keeps every station
 * that asserted in the current episode until an accepted Director clear ends it.
 *
 * Identity is the key, not the MAC. The S3 relay holds no keys and is not an
 * authority for any field in this file.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "showduino_emergency_node.h"
#include "showduino_station_blockers.h"
#include "showduino_station_auth.h"

typedef struct ShowduinoStationModel {
  uint8_t used;
  uint8_t online;           /* a verified AUTH was accepted within the timeout */
  uint8_t reportKnown;      /* a verified AUTH was accepted in this boot */
  uint8_t inputOpen;        /* 1 = PRESSED. Only a verified AUTH can set 0. */
  uint8_t announcedLatched; /* L= from the last verified AUTH */
  uint8_t asserted;         /* in the current global-latch episode */
  uint8_t ackedReport;      /* A= from the last verified AUTH (station holds this episode) */
  uint32_t proofChalSeq;    /* sequence of the challenge the last verified AUTH answered (0 = none) */
  char id[SHOWDUINO_BLOCKER_ID_CHARS + 1];
  char mac[SHOWDUINO_BLOCKER_MAC_CHARS + 1]; /* display only */
} ShowduinoStationModel;

typedef enum ShowduinoStationLineKind {
  SHOWDUINO_STATION_LINE_NONE = 0,
  SHOWDUINO_STATION_LINE_ANNOUNCE,
  SHOWDUINO_STATION_LINE_ASSERT,
  SHOWDUINO_STATION_LINE_STATUS,
  SHOWDUINO_STATION_LINE_AUTH
} ShowduinoStationLineKind;

/* Authentication context for AUTH lines. Supplied by the P4 for each line. */
typedef struct ShowduinoStationAuthPolicy {
  ShowduinoHmacFn hmac;
  ShowduinoAuthChalTable *chal;
  uint32_t episode;   /* current episode on the P4, 0 when idle */
  uint32_t nowMs;
  /* Returns the key-table index (0..SHOWDUINO_EMERGENCY_NODE_MAX_NODES-1) and fills
   * the key, or -1 when the ID has no provisioned key. */
  int (*keyFor)(void *ctx, const char *id, uint8_t key[SHOWDUINO_AUTH_KEY_BYTES]);
  void *ctx;
} ShowduinoStationAuthPolicy;

typedef struct ShowduinoStationResult {
  int kind;                 /* ShowduinoStationLineKind */
  int slot;                 /* table index, or -1 */
  int accepted;             /* 1 = a state change was applied */
  int verified;             /* 1 = an AUTH response was verified and applied */
  int globalLatch;          /* 1 = ask the P4 global latch to engage */
  int tableFull;            /* 1 = a station message had no free slot (clear must be refused) */
  char ackId[SHOWDUINO_BLOCKER_ID_CHARS + 1];
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1];
} ShowduinoStationResult;

static inline void showduino_station_copy(char *dst, size_t cap, const char *src) {
  if (!dst || cap == 0) return;
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}

/* A stored blocker restored at boot: PRESSED, unseen, offline, not yet verified.
 * It belongs to an active episode, so it is asserted. */
static inline void showduino_station_model_restore(ShowduinoStationModel *m,
                                                   const char *id, const char *mac) {
  if (!m) return;
  memset(m, 0, sizeof(*m));
  m->used = 1;
  m->inputOpen = 1;
  m->asserted = 1;
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

/* Display-only. Returns 0: an ANNOUNCE never changes station state (MISSION 001J). */
static inline int showduino_station_model_on_announce(ShowduinoStationModel *m,
                                                      const ShowduinoEmergencyAnnounce *an,
                                                      int *globalLatch) {
  (void)m; (void)an;
  if (globalLatch) *globalLatch = 0;
  return 0;
}

/* Verified AUTH. Called only after tag, nonce, episode, and freshness checks. */
static inline void showduino_station_model_on_verified(ShowduinoStationModel *m,
                                                       const char *id, int in, int latched,
                                                       int acked, uint32_t chalSeq) {
  if (!m) return;
  m->proofChalSeq = chalSeq;
  if (!m->id[0]) showduino_station_copy(m->id, sizeof(m->id), id);
  m->inputOpen = in ? 1 : 0;
  m->reportKnown = 1;
  m->online = 1;
  m->announcedLatched = latched ? 1 : 0;
  m->ackedReport = acked ? 1 : 0;
  if (latched) m->asserted = 1;
}

/* Bare ASSERT. Latches the global emergency and records the episode. It never sets
 * ONLINE, RELEASED, or reportKnown. */
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

/* 1 = this station must be in the stored blocker set. */
static inline int showduino_station_model_required(const ShowduinoStationModel *m) {
  if (!m || !m->used) return 0;
  return (m->asserted || showduino_station_model_blocks_clear(m)) ? 1 : 0;
}

/* Accepted Director clear: the episode is over. */
static inline void showduino_station_model_end_episode(ShowduinoStationModel *t, int n) {
  for (int i = 0; t && i < n; ++i) t[i].asserted = 0;
}

/* Builds the stored blocker set. Returns 0 when it cannot be built faithfully. */
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

/* Clear-time proof (MISSION 001J). A Director clear needs, for EVERY provisioned
 * station ID, a verified AUTH reporting RELEASED that answered a challenge whose
 * sequence is at or after sinceSeq (showduino_auth_chal_next_seq() at confirm).
 * Sequences, not times, so a challenge issued in the same millisecond as the
 * confirm but before it can never count. Returns how many provisioned IDs lack
 * that proof. A station that is silent, offline, dropped by the relay, PRESSED,
 * or only ever seen through bare ASSERT is counted. */
static inline int showduino_station_clear_proof_missing(const ShowduinoStationModel *t, int n,
                                                        const char (*ids)[SHOWDUINO_BLOCKER_ID_CHARS + 1],
                                                        int idCount, uint32_t sinceSeq) {
  int missing = 0;
  for (int k = 0; k < idCount; ++k) {
    const int slot = showduino_station_model_find_id(t, n, ids[k]);
    if (slot < 0) { missing++; continue; }
    const ShowduinoStationModel *m = &t[slot];
    if (!m->reportKnown || !m->online || m->inputOpen ||
        m->proofChalSeq == 0 || (int32_t)(m->proofChalSeq - sinceSeq) < 0) {
      missing++;
    }
  }
  return missing;
}

/* First challenge sequence a CONFIRM accepts as proof (MISSION 001J).
 *   requestSeq  showduino_auth_chal_next_seq() taken at CLEAR_REQUEST (0 = no request)
 *   lastSeq     sequence of the most recent challenge issued before the confirm
 * The proof must answer one of the two most recent challenges AND a challenge
 * issued after the request. With challenges every SHOWDUINO_STATION_CHAL_LATCHED_MS
 * this bounds the age of an accepted proof to about two challenge periods. */
#define SHOWDUINO_STATION_CHAL_LATCHED_MS 500u
#define SHOWDUINO_STATION_CHAL_IDLE_MS    1000u
static inline uint32_t showduino_station_proof_since(uint32_t requestSeq, uint32_t lastSeq) {
  const uint32_t recent = lastSeq > 1u ? lastSeq - 1u : 1u;
  if (requestSeq == 0u) return recent;
  return (int32_t)(requestSeq - recent) > 0 ? requestSeq : recent;
}

/* Director clear decision used by the P4 firmware and the host tests.
 *   ids/idCount  provisioned station IDs (every one must prove RELEASED)
 *   proofSinceSeq first challenge sequence that counts (taken at the confirm)
 * Returns a ShowduinoClearDecision; only SHOWDUINO_CLEAR_OK permits the clear. */
typedef enum ShowduinoClearDecision {
  SHOWDUINO_CLEAR_OK = 0,
  SHOWDUINO_CLEAR_BLOCK_STATION = 1,   /* a known station is PRESSED, unverified or offline */
  SHOWDUINO_CLEAR_BLOCK_STORAGE = 2,   /* blocker set, latch record or counter not durable */
  SHOWDUINO_CLEAR_BLOCK_PROOF = 3,     /* a provisioned station has no fresh RELEASED proof */
  SHOWDUINO_CLEAR_BLOCK_OVERFLOW = 4   /* station table overflowed in this episode */
} ShowduinoClearDecision;

static inline int showduino_station_clear_decision(const ShowduinoStationModel *t, int n,
                                                   const char (*ids)[SHOWDUINO_BLOCKER_ID_CHARS + 1],
                                                   int idCount, uint32_t proofSinceSeq,
                                                   int unknownBoot, int persistDirty,
                                                   int counterFault, int tableOverflow,
                                                   int booted) {
  if (!booted || unknownBoot || persistDirty || counterFault) return SHOWDUINO_CLEAR_BLOCK_STORAGE;
  if (tableOverflow) return SHOWDUINO_CLEAR_BLOCK_OVERFLOW;
  if (showduino_station_clear_blocked_full((unsigned)showduino_station_model_count_blocking(t, n),
                                           0, 0, 1)) {
    return SHOWDUINO_CLEAR_BLOCK_STATION;
  }
  if (showduino_station_clear_proof_missing(t, n, ids, idCount, proofSinceSeq) != 0) {
    return SHOWDUINO_CLEAR_BLOCK_PROOF;
  }
  return SHOWDUINO_CLEAR_OK;
}

/* Local maintenance (P4 USB console only, logged): forgets an UNPROVISIONED station
 * entry, for example a forged ASSERT ID or a legacy station being retired. A
 * provisioned ID is refused: it must prove RELEASED or be revoked first.
 * Returns 1 when an entry was removed. */
static inline int showduino_station_model_dismiss(ShowduinoStationModel *t, int n, const char *id,
                                                  int idIsProvisioned) {
  if (idIsProvisioned) return 0;
  const int slot = showduino_station_model_find_id(t, n, id);
  if (slot < 0) return 0;
  memset(&t[slot], 0, sizeof(t[slot]));
  return 1;
}

/* Interprets one wireless line from an Emergency Node against the station table.
 * Returns 1 when the line is a station message this model handles. The caller
 * performs the side effects. `auth` may be NULL, in which case AUTH is rejected. */
static inline int showduino_station_interpret_line(ShowduinoStationModel *t, int n,
                                                   const char *line,
                                                   const ShowduinoStationAuthPolicy *auth,
                                                   ShowduinoStationResult *res) {
  if (!t || !line || !res) return 0;
  memset(res, 0, sizeof(*res));
  res->slot = -1;

  if (!strncmp(line, "AUTH:", 5)) {
    res->kind = SHOWDUINO_STATION_LINE_AUTH;
    ShowduinoAuthMsg am;
    if (!auth || !auth->hmac || !auth->chal || !auth->keyFor) return 1;
    if (!showduino_auth_parse_auth(line, &am)) return 1;
    if (am.episode != auth->episode) return 1;
    uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
    memset(key, 0, sizeof(key));
    const int keyIdx = auth->keyFor(auth->ctx, am.id, key);
    if (keyIdx < 0) { memset(key, 0, sizeof(key)); return 1; }
    const int tagOk = showduino_auth_verify_auth(&am, key, sizeof(key), auth->hmac);
    memset(key, 0, sizeof(key));
    if (!tagOk) return 1;
    uint32_t chalSeq = 0;
    if (!showduino_auth_chal_consume_at(auth->chal, am.nonceHex, auth->episode, auth->nowMs,
                                        keyIdx, &chalSeq)) {
      return 1;
    }
    int slot = showduino_station_model_find_id(t, n, am.id);
    if (slot < 0) slot = showduino_station_model_alloc(t, n);
    if (slot < 0) {
      res->tableFull = 1;
      res->globalLatch = am.latched ? 1 : 0;
      return 1;
    }
    showduino_station_model_on_verified(&t[slot], am.id, am.in, am.latched, am.acked, chalSeq);
    res->slot = slot;
    res->accepted = 1;
    res->verified = 1;
    res->globalLatch = am.latched ? 1 : 0;
    showduino_station_copy(res->ackId, sizeof(res->ackId), t[slot].id);
    return 1;
  }

  if (!strncmp(line, "ANNOUNCE:", 9)) {
    /* Display only: no slot, no state. */
    res->kind = SHOWDUINO_STATION_LINE_ANNOUNCE;
    return 1;
  }

  if (showduino_emergency_is_assert(line)) {
    char id[SHOWDUINO_BLOCKER_ID_CHARS + 1] = "";
    char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1] = "";
    if (!showduino_emergency_parse_assert(line, id, sizeof(id), name, sizeof(name))) return 0;
    res->kind = SHOWDUINO_STATION_LINE_ASSERT;
    /* An ASSERT always latches, even with no free slot: a full table (for example
     * filled by forged IDs) must never suppress a real emergency. */
    res->globalLatch = 1;
    int slot = showduino_station_model_find_id(t, n, id);
    if (slot < 0) slot = showduino_station_model_alloc(t, n);
    if (slot < 0) { res->tableFull = 1; return 1; }
    res->slot = slot;
    int gl = 0;
    res->accepted = showduino_station_model_on_assert(&t[slot], id, &gl);
    if (res->accepted) {
      res->globalLatch = gl ? 1 : res->globalLatch;
      showduino_station_copy(res->ackId, sizeof(res->ackId), t[slot].id);
      showduino_station_copy(res->name, sizeof(res->name), name);
    }
    return 1;
  }

  if (!strncmp(line, "STATUS:", 7)) {
    /* Display only: no state, no latch request. */
    res->kind = SHOWDUINO_STATION_LINE_STATUS;
    return 1;
  }
  return 0;
}

#endif
