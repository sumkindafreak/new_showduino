#ifndef SHOWDUINO_DIRECTOR_AUTH_H
#define SHOWDUINO_DIRECTOR_AUTH_H

/*
 * MISSION 001K: end-to-end authenticated operator clear, Director -> P4.
 *
 * Wire (fits the 95-character desk packet and the S3 -> P4 UART line):
 *   DCLR:<op>:<dirId>:<episode8>:<txn16>:<counter8>:<tag32>
 *     op      REQ (clear request, txn = 16 zeros) | CFM (clear confirm)
 *     dirId   DIR-<1..4 digits>
 *     episode P4 emergency episode the operator is clearing (never 0)
 *     txn     P4-issued per-request transaction nonce (CFM only)
 *     counter Director message counter, strictly increasing, persisted on both ends
 *     tag     HMAC-SHA256 truncated to 128 bits over the canonical string
 *
 * Canonical string (domain-separated from station AUTH|v1 and CMD|v1):
 *   DIRCLR|v1|<dirId>|<op>|<episode8>|<txn16>|<counter8>
 * The P4 verifies DCLR only with the Director key, which is a separate record from
 * the station keys and is refused if it equals any station key.
 *
 * The Comms S3 relays these lines and holds no key: it can drop or delay them, but
 * cannot create, alter or replay one that the P4 accepts.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "showduino_station_auth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_DIR_ID_MAX       8          /* "DIR-9999" */
#define SHOWDUINO_DIR_LINE_MAX     95
#define SHOWDUINO_DIR_TXN_ZERO     "0000000000000000"

typedef enum ShowduinoDirOp {
  SHOWDUINO_DIR_OP_NONE = 0,
  SHOWDUINO_DIR_OP_REQ = 1,
  SHOWDUINO_DIR_OP_CFM = 2
} ShowduinoDirOp;

typedef struct ShowduinoDirMsg {
  int op;
  char id[SHOWDUINO_DIR_ID_MAX + 1];
  uint32_t episode;
  char txnHex[SHOWDUINO_AUTH_NONCE_HEX + 1];
  uint32_t counter;
  uint8_t tag[SHOWDUINO_AUTH_TAG_BYTES];
} ShowduinoDirMsg;

static inline int showduino_dir_id_ok(const char *id) {
  if (!id || strncmp(id, "DIR-", 4) != 0) return 0;
  const char *p = id + 4;
  int n = 0;
  while (*p >= '0' && *p <= '9' && n < 4) { ++p; ++n; }
  return n >= 1 && *p == '\0';
}

static inline const char *showduino_dir_op_name(int op) {
  return op == SHOWDUINO_DIR_OP_REQ ? "REQ" : op == SHOWDUINO_DIR_OP_CFM ? "CFM" : "";
}

static inline int showduino_dir_canon(char *out, size_t n, const char *id, int op,
                                      uint32_t episode, const char *txnHex, uint32_t counter) {
  if (!out || !id || !txnHex || !showduino_dir_op_name(op)[0]) return 0;
  const int w = snprintf(out, n, "DIRCLR|v1|%s|%s|%08lx|%s|%08lx", id, showduino_dir_op_name(op),
                         (unsigned long)episode, txnHex, (unsigned long)counter);
  return w > 0 && (size_t)w < n;
}

/* Director side. txnHex must be SHOWDUINO_DIR_TXN_ZERO for REQ. */
static inline int showduino_dir_format(int op, const char *id, uint32_t episode,
                                       const char *txnHex, uint32_t counter,
                                       const uint8_t *key, size_t keyLen, ShowduinoHmacFn hmac,
                                       char *out, size_t n) {
  char canon[96];
  uint8_t tag[SHOWDUINO_AUTH_TAG_BYTES];
  char tagHex[SHOWDUINO_AUTH_TAG_HEX + 1];
  if (!showduino_dir_id_ok(id) || episode == 0 || counter == 0 || !txnHex ||
      strlen(txnHex) != SHOWDUINO_AUTH_NONCE_HEX) {
    return 0;
  }
  if (!showduino_dir_canon(canon, sizeof(canon), id, op, episode, txnHex, counter)) return 0;
  if (!showduino_auth_tag(hmac, key, keyLen, canon, tag)) return 0;
  showduino_auth_hex_encode(tag, sizeof(tag), tagHex);
  memset(tag, 0, sizeof(tag));
  const int w = snprintf(out, n, "DCLR:%s:%s:%08lx:%s:%08lx:%s", showduino_dir_op_name(op), id,
                         (unsigned long)episode, txnHex, (unsigned long)counter, tagHex);
  return w > 0 && (size_t)w < n && (size_t)w <= SHOWDUINO_DIR_LINE_MAX;
}

static inline int showduino_dir_is_line(const char *line) {
  return line && strncmp(line, "DCLR:", 5) == 0;
}

/* Strict parse: fixed field widths, hex only. */
static inline int showduino_dir_parse(const char *line, ShowduinoDirMsg *m) {
  if (!line || !m || strncmp(line, "DCLR:", 5) != 0) return 0;
  memset(m, 0, sizeof(*m));
  const char *p = line + 5;
  if (!strncmp(p, "REQ:", 4)) m->op = SHOWDUINO_DIR_OP_REQ;
  else if (!strncmp(p, "CFM:", 4)) m->op = SHOWDUINO_DIR_OP_CFM;
  else return 0;
  p += 4;
  const char *c = strchr(p, ':');
  if (!c || (size_t)(c - p) > SHOWDUINO_DIR_ID_MAX || c == p) return 0;
  memcpy(m->id, p, (size_t)(c - p));
  m->id[c - p] = '\0';
  if (!showduino_dir_id_ok(m->id)) return 0;
  p = c + 1;
  /* E8:T16:C8:TAG32 */
  if (strlen(p) != 8 + 1 + 16 + 1 + 8 + 1 + SHOWDUINO_AUTH_TAG_HEX) return 0;
  if (p[8] != ':' || p[25] != ':' || p[34] != ':') return 0;
  char e[9], t[17], k[9];
  memcpy(e, p, 8); e[8] = '\0';
  memcpy(t, p + 9, 16); t[16] = '\0';
  memcpy(k, p + 26, 8); k[8] = '\0';
  if (!showduino_auth_parse_u32hex(e, &m->episode)) return 0;
  if (!showduino_auth_parse_u32hex(k, &m->counter)) return 0;
  uint8_t tmp[SHOWDUINO_AUTH_NONCE_BYTES];
  if (!showduino_auth_hex_decode(t, sizeof(tmp), tmp)) return 0;
  memcpy(m->txnHex, t, sizeof(t));
  if (m->op == SHOWDUINO_DIR_OP_REQ && strcmp(m->txnHex, SHOWDUINO_DIR_TXN_ZERO) != 0) return 0;
  return showduino_auth_hex_decode(p + 35, SHOWDUINO_AUTH_TAG_BYTES, m->tag);
}

static inline int showduino_dir_verify_tag(const ShowduinoDirMsg *m, const uint8_t *key,
                                           size_t keyLen, ShowduinoHmacFn hmac) {
  char canon[96];
  uint8_t expect[SHOWDUINO_AUTH_TAG_BYTES];
  if (!m || !key || keyLen != SHOWDUINO_AUTH_KEY_BYTES) return 0;
  if (!showduino_dir_canon(canon, sizeof(canon), m->id, m->op, m->episode, m->txnHex, m->counter)) {
    return 0;
  }
  if (!showduino_auth_tag(hmac, key, keyLen, canon, expect)) return 0;
  const int ok = showduino_auth_ct_equal(expect, m->tag, SHOWDUINO_AUTH_TAG_BYTES);
  memset(expect, 0, sizeof(expect));
  return ok;
}

/* ---------- P4 acceptance policy ---------- */

/* The P4's Director credential (one enrolled Director). */
typedef struct ShowduinoDirKeyRec {
  uint8_t present;
  char id[SHOWDUINO_DIR_ID_MAX + 1];
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  /* Enrolled while an emergency was latched: may not clear until one clear has been
   * accepted through another authority (USB maintenance). Persisted. */
  uint8_t restricted;
  uint32_t lastCounter;    /* persisted; 0 = none accepted */
} ShowduinoDirKeyRec;

typedef enum ShowduinoDirResult {
  SHOWDUINO_DIR_OK = 0,
  SHOWDUINO_DIR_REJECT_FORMAT = 1,
  SHOWDUINO_DIR_REJECT_NO_KEY = 2,
  SHOWDUINO_DIR_REJECT_IDENTITY = 3,
  SHOWDUINO_DIR_REJECT_TAG = 4,
  SHOWDUINO_DIR_REJECT_EPISODE = 5,
  SHOWDUINO_DIR_REJECT_RESTRICTED = 6,
  SHOWDUINO_DIR_REJECT_REPLAY = 7,
  SHOWDUINO_DIR_REJECT_TXN = 8,
  SHOWDUINO_DIR_REJECT_PERSIST = 9,
  SHOWDUINO_DIR_REJECT_SOURCE = 10
} ShowduinoDirResult;

static inline const char *showduino_dir_result_name(int r) {
  switch (r) {
    case SHOWDUINO_DIR_OK: return "OK";
    case SHOWDUINO_DIR_REJECT_FORMAT: return "AUTH_FORMAT";
    case SHOWDUINO_DIR_REJECT_NO_KEY: return "DIRECTOR_NOT_ENROLLED";
    case SHOWDUINO_DIR_REJECT_IDENTITY: return "DIRECTOR_UNKNOWN";
    case SHOWDUINO_DIR_REJECT_TAG: return "AUTH_FAILED";
    case SHOWDUINO_DIR_REJECT_EPISODE: return "WRONG_EPISODE";
    case SHOWDUINO_DIR_REJECT_RESTRICTED: return "DIRECTOR_RESTRICTED";
    case SHOWDUINO_DIR_REJECT_REPLAY: return "REPLAY";
    case SHOWDUINO_DIR_REJECT_TXN: return "STALE_TRANSACTION";
    case SHOWDUINO_DIR_REJECT_PERSIST: return "AUTH_STORAGE";
    case SHOWDUINO_DIR_REJECT_SOURCE: return "AUTH_SOURCE";
    default: return "AUTH_FAILED";
  }
}

/* Verifies one DCLR line against the enrolled Director.
 *   episodeNow    the P4's current episode (0 when not latched or not durable)
 *   openTxnHex    the open transaction nonce for CFM, or NULL / "" when none
 *   persist       stores the new lastCounter; must verify the write
 * On SHOWDUINO_DIR_OK the counter is already persisted and recorded. The caller then
 * performs the operation. No state changes on any rejection. */
static inline int showduino_dir_accept(ShowduinoDirKeyRec *rec, const char *line,
                                       uint32_t episodeNow, const char *openTxnHex,
                                       ShowduinoHmacFn hmac, ShowduinoPersistFn persist,
                                       void *ctx, ShowduinoDirMsg *out) {
  ShowduinoDirMsg m;
  if (!showduino_dir_parse(line, &m)) return SHOWDUINO_DIR_REJECT_FORMAT;
  if (!rec || !rec->present) return SHOWDUINO_DIR_REJECT_NO_KEY;
  if (strcmp(rec->id, m.id) != 0) return SHOWDUINO_DIR_REJECT_IDENTITY;
  if (!showduino_dir_verify_tag(&m, rec->key, sizeof(rec->key), hmac)) {
    return SHOWDUINO_DIR_REJECT_TAG;
  }
  if (episodeNow == 0 || m.episode != episodeNow) return SHOWDUINO_DIR_REJECT_EPISODE;
  if (rec->restricted) return SHOWDUINO_DIR_REJECT_RESTRICTED;
  if (!showduino_cmd_counter_is_fresh(rec->lastCounter, m.counter)) {
    return SHOWDUINO_DIR_REJECT_REPLAY;
  }
  if (m.op == SHOWDUINO_DIR_OP_CFM) {
    if (!openTxnHex || strlen(openTxnHex) != SHOWDUINO_AUTH_NONCE_HEX ||
        strcmp(openTxnHex, SHOWDUINO_DIR_TXN_ZERO) == 0 || strcmp(openTxnHex, m.txnHex) != 0) {
      return SHOWDUINO_DIR_REJECT_TXN;
    }
  }
  if (!persist || !persist(ctx, m.counter)) return SHOWDUINO_DIR_REJECT_PERSIST;
  rec->lastCounter = m.counter;
  if (out) *out = m;
  return SHOWDUINO_DIR_OK;
}

/* ---------- P4 -> Director notices (unauthenticated; the S3 may alter them, which
 * only makes the Director's signed message fail verification) ---------- */

/* STATE:EMERGENCY:EPISODE:<episode8>  (00000000 when not latched) */
static inline int showduino_dir_format_episode(uint32_t episode, char *out, size_t n) {
  const int w = snprintf(out, n, "STATE:EMERGENCY:EPISODE:%08lx", (unsigned long)episode);
  return w > 0 && (size_t)w < n;
}
static inline int showduino_dir_parse_episode(const char *line, uint32_t *episode) {
  if (!line || strncmp(line, "STATE:EMERGENCY:EPISODE:", 24) != 0 || strlen(line + 24) != 8) return 0;
  return showduino_auth_parse_u32hex(line + 24, episode);
}

/* EMERGENCY:CLEAR:TXN:<episode8>:<txn16> — sent after an accepted REQ. */
static inline int showduino_dir_format_txn_notice(uint32_t episode, const char *txnHex, char *out,
                                                  size_t n) {
  if (!txnHex || strlen(txnHex) != SHOWDUINO_AUTH_NONCE_HEX) return 0;
  const int w = snprintf(out, n, "EMERGENCY:CLEAR:TXN:%08lx:%s", (unsigned long)episode, txnHex);
  return w > 0 && (size_t)w < n;
}
static inline int showduino_dir_parse_txn_notice(const char *line, uint32_t *episode,
                                                 char txnHex[SHOWDUINO_AUTH_NONCE_HEX + 1]) {
  if (!line || strncmp(line, "EMERGENCY:CLEAR:TXN:", 20) != 0) return 0;
  const char *p = line + 20;
  if (strlen(p) != 8 + 1 + SHOWDUINO_AUTH_NONCE_HEX || p[8] != ':') return 0;
  char e[9];
  memcpy(e, p, 8); e[8] = '\0';
  uint8_t tmp[SHOWDUINO_AUTH_NONCE_BYTES];
  if (!showduino_auth_parse_u32hex(e, episode)) return 0;
  if (!showduino_auth_hex_decode(p + 9, sizeof(tmp), tmp)) return 0;
  memcpy(txnHex, p + 9, SHOWDUINO_AUTH_NONCE_HEX);
  txnHex[SHOWDUINO_AUTH_NONCE_HEX] = '\0';
  return 1;
}

/* ---------- Director side ---------- */

typedef struct ShowduinoDirSigner {
  uint8_t provisioned;
  char id[SHOWDUINO_DIR_ID_MAX + 1];
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  ShowduinoAuthCounter ctr;      /* persisted block limit, like the P4 command counter */
  uint32_t episode;              /* last STATE:EMERGENCY:EPISODE from the P4 */
  char txnHex[SHOWDUINO_AUTH_NONCE_HEX + 1];  /* last TXN notice; "" when none */
  uint32_t txnEpisode;
} ShowduinoDirSigner;

/* Signs a REQ for the episode the P4 last reported. Returns 0 (nothing signed) when
 * not provisioned, no episode is known, or the counter block cannot be persisted. */
static inline int showduino_dir_sign_req(ShowduinoDirSigner *s, ShowduinoHmacFn hmac,
                                         ShowduinoPersistFn persist, void *ctx,
                                         char *out, size_t n) {
  uint32_t c = 0;
  if (!s || !s->provisioned || s->episode == 0) return 0;
  if (!showduino_auth_counter_next(&s->ctr, persist, ctx, &c) || c == 0) return 0;
  s->txnHex[0] = '\0';
  return showduino_dir_format(SHOWDUINO_DIR_OP_REQ, s->id, s->episode, SHOWDUINO_DIR_TXN_ZERO, c,
                             s->key, sizeof(s->key), hmac, out, n);
}

/* Signs a CFM for the transaction the P4 issued. Single use on the Director too. */
static inline int showduino_dir_sign_cfm(ShowduinoDirSigner *s, ShowduinoHmacFn hmac,
                                         ShowduinoPersistFn persist, void *ctx,
                                         char *out, size_t n) {
  uint32_t c = 0;
  if (!s || !s->provisioned || strlen(s->txnHex) != SHOWDUINO_AUTH_NONCE_HEX || s->txnEpisode == 0) {
    return 0;
  }
  if (!showduino_auth_counter_next(&s->ctr, persist, ctx, &c) || c == 0) return 0;
  const int ok = showduino_dir_format(SHOWDUINO_DIR_OP_CFM, s->id, s->txnEpisode, s->txnHex, c,
                                      s->key, sizeof(s->key), hmac, out, n);
  s->txnHex[0] = '\0';
  return ok;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_DIRECTOR_AUTH_H */
