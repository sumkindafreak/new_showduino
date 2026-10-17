#ifndef SHOWDUINO_STATION_AUTH_H
#define SHOWDUINO_STATION_AUTH_H

/*
 * MISSION 001J: application-layer authentication for Emergency Node traffic.
 *
 * Pure logic. No Arduino, NVS, ESP-NOW or RNG calls here. The caller injects:
 *   - ShowduinoHmacFn: HMAC-SHA256 (mbedtls on firmware, OpenSSL in host tests).
 *   - ShowduinoRandFn: hardware random bytes (esp_random on firmware).
 *   - a persist callback for the counter block (NVS on firmware).
 *
 * Authority:
 *   - Only the P4 can authorise a clear. Stations never clear.
 *   - A station's physical state (IN, L, A) is trusted only inside an AUTH
 *     response that carries a tag made with that station's key, and that answers
 *     a challenge issued by this P4 boot, for the current episode, in time.
 *   - The S3 holds no keys. Any relayed or forged line without a valid tag is
 *     rejected. It cannot change global state, only the ASSERT path, which is
 *     fail-safe (it latches, never clears).
 *
 * Wire formats (all ASCII, all within the 96-byte ESP-NOW command limit):
 *   CHAL:<E8>:<N16>                         P4 -> stations (broadcast, no tag)
 *   AUTH:<ID>:<E8>:<N16>:<IN>:<L>:<A>:<T32> station -> P4
 *   CMD:<ID>:<C8>:<OP>:<E8>:<T32>           P4 -> station (OP = ACK | GCL)
 * E = episode id (0 when idle). N = 64-bit nonce. C = monotonic command counter.
 * T = HMAC-SHA256 truncated to 128 bits, lower-case hex.
 *   AUTH tag: HMAC(K_ID, "AUTH|v1|ID|E|N|IN|L|A")
 *   CMD  tag: HMAC(K_ID, "CMD|v1|ID|C|OP|E")
 *
 * Identity is the key, not the MAC. A MAC is not authentication.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "showduino_emergency_node.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_AUTH_KEY_BYTES      32
#define SHOWDUINO_AUTH_TAG_BYTES      16   /* 128-bit truncation of HMAC-SHA256 */
#define SHOWDUINO_AUTH_TAG_HEX        32
#define SHOWDUINO_AUTH_NONCE_BYTES    8
#define SHOWDUINO_AUTH_NONCE_HEX      16
#define SHOWDUINO_AUTH_KEY_HEX        64
#define SHOWDUINO_AUTH_WINDOW_MS      1500u
#define SHOWDUINO_AUTH_CHAL_SLOTS     4
#define SHOWDUINO_AUTH_COUNTER_BLOCK  256u
#define SHOWDUINO_AUTH_LINE_MAX       95   /* ESP-NOW command field: 96 bytes incl. NUL */

/* HMAC-SHA256 over (key, msg). Writes 32 bytes. Returns 0 on success. */
typedef int (*ShowduinoHmacFn)(const uint8_t *key, size_t keyLen,
                               const uint8_t *msg, size_t msgLen, uint8_t out[32]);
/* Fills n random bytes from a hardware source. Returns 0 on success. */
typedef int (*ShowduinoRandFn)(uint8_t *out, size_t n);
/* Persists a counter-block limit. Returns 1 only when the value is verified stored. */
typedef int (*ShowduinoPersistFn)(void *ctx, uint32_t value);

/* ---------- hex helpers (strict) ---------- */

static inline void showduino_auth_hex_encode(const uint8_t *in, size_t n, char *out) {
  static const char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < n; ++i) {
    out[2 * i] = kHex[(in[i] >> 4) & 0x0F];
    out[2 * i + 1] = kHex[in[i] & 0x0F];
  }
  out[2 * n] = '\0';
}

static inline int showduino_auth_hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* Decodes exactly 2*n hex characters. Returns 1 on success. */
static inline int showduino_auth_hex_decode(const char *in, size_t n, uint8_t *out) {
  if (!in || !out) return 0;
  for (size_t i = 0; i < n; ++i) {
    const int hi = showduino_auth_hex_nibble(in[2 * i]);
    const int lo = showduino_auth_hex_nibble(in[2 * i + 1]);
    if (hi < 0 || lo < 0) return 0;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return 1;
}

/* Parses exactly 8 hex digits into a u32. Returns 1 on success. */
static inline int showduino_auth_parse_u32hex(const char *s, uint32_t *out) {
  uint8_t b[4];
  if (!s || !out || strlen(s) != 8) return 0;
  if (!showduino_auth_hex_decode(s, 4, b)) return 0;
  *out = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
  return 1;
}

static inline int showduino_auth_hex_is_lower_or_upper_len(const char *s, size_t n) {
  if (!s || strlen(s) != n) return 0;
  for (size_t i = 0; i < n; ++i) {
    if (showduino_auth_hex_nibble(s[i]) < 0) return 0;
  }
  return 1;
}

/* Constant-time comparison of two equal-length buffers. */
static inline int showduino_auth_ct_equal(const uint8_t *a, const uint8_t *b, size_t n) {
  uint8_t acc = 0;
  for (size_t i = 0; i < n; ++i) acc |= (uint8_t)(a[i] ^ b[i]);
  return acc == 0;
}

/* ---------- canonical messages and tags ---------- */

static inline int showduino_auth_canon_auth(char *out, size_t n, const char *id,
                                            uint32_t episode, const char *nonceHex,
                                            int in, int latched, int acked) {
  if (!out || !id || !nonceHex) return 0;
  const int w = snprintf(out, n, "AUTH|v1|%s|%08lx|%s|%d|%d|%d", id,
                         (unsigned long)episode, nonceHex, in ? 1 : 0,
                         latched ? 1 : 0, acked ? 1 : 0);
  return w > 0 && (size_t)w < n;
}

static inline int showduino_auth_canon_cmd(char *out, size_t n, const char *id,
                                           uint32_t counter, const char *op, uint32_t episode) {
  if (!out || !id || !op) return 0;
  const int w = snprintf(out, n, "CMD|v1|%s|%08lx|%s|%08lx", id,
                         (unsigned long)counter, op, (unsigned long)episode);
  return w > 0 && (size_t)w < n;
}

static inline int showduino_auth_tag(const ShowduinoHmacFn hmac, const uint8_t *key,
                                     size_t keyLen, const char *canon,
                                     uint8_t tag[SHOWDUINO_AUTH_TAG_BYTES]) {
  uint8_t full[32];
  if (!hmac || !key || !canon) return 0;
  if (hmac(key, keyLen, (const uint8_t *)canon, strlen(canon), full) != 0) return 0;
  memcpy(tag, full, SHOWDUINO_AUTH_TAG_BYTES);
  memset(full, 0, sizeof(full));
  return 1;
}

/* ---------- CHAL (P4 -> stations) ---------- */

static inline int showduino_auth_format_chal(uint32_t episode, const char *nonceHex,
                                             char *out, size_t n) {
  if (!out || !nonceHex || !showduino_auth_hex_is_lower_or_upper_len(nonceHex, SHOWDUINO_AUTH_NONCE_HEX)) {
    return 0;
  }
  const int w = snprintf(out, n, "CHAL:%08lx:%s", (unsigned long)episode, nonceHex);
  return w > 0 && (size_t)w < n && (size_t)w <= SHOWDUINO_AUTH_LINE_MAX;
}

typedef struct ShowduinoAuthChalMsg {
  uint32_t episode;
  char nonceHex[SHOWDUINO_AUTH_NONCE_HEX + 1];
} ShowduinoAuthChalMsg;

static inline int showduino_auth_parse_chal(const char *line, ShowduinoAuthChalMsg *out) {
  if (!line || !out || strncmp(line, "CHAL:", 5) != 0) return 0;
  const char *p = line + 5;
  if (strlen(p) != 8 + 1 + SHOWDUINO_AUTH_NONCE_HEX) return 0;
  if (p[8] != ':') return 0;
  char eb[9];
  memcpy(eb, p, 8);
  eb[8] = '\0';
  if (!showduino_auth_parse_u32hex(eb, &out->episode)) return 0;
  memcpy(out->nonceHex, p + 9, SHOWDUINO_AUTH_NONCE_HEX);
  out->nonceHex[SHOWDUINO_AUTH_NONCE_HEX] = '\0';
  return showduino_auth_hex_is_lower_or_upper_len(out->nonceHex, SHOWDUINO_AUTH_NONCE_HEX);
}

/* ---------- AUTH (station -> P4) ---------- */

typedef struct ShowduinoAuthMsg {
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  uint32_t episode;
  char nonceHex[SHOWDUINO_AUTH_NONCE_HEX + 1];
  int in;
  int latched;
  int acked;
  uint8_t tag[SHOWDUINO_AUTH_TAG_BYTES];
} ShowduinoAuthMsg;

/* Station side: builds an AUTH line from the challenge and its own state. */
static inline int showduino_auth_format_auth(const char *id, uint32_t episode,
                                             const char *nonceHex, int in, int latched, int acked,
                                             const uint8_t *key, size_t keyLen,
                                             const ShowduinoHmacFn hmac,
                                             char *out, size_t n) {
  char canon[160];
  uint8_t tag[SHOWDUINO_AUTH_TAG_BYTES];
  char tagHex[SHOWDUINO_AUTH_TAG_HEX + 1];
  if (!id || !id[0] || !out) return 0;
  if (!showduino_auth_canon_auth(canon, sizeof(canon), id, episode, nonceHex, in, latched, acked)) {
    return 0;
  }
  if (!showduino_auth_tag(hmac, key, keyLen, canon, tag)) return 0;
  showduino_auth_hex_encode(tag, SHOWDUINO_AUTH_TAG_BYTES, tagHex);
  const int w = snprintf(out, n, "AUTH:%s:%08lx:%s:%d:%d:%d:%s", id,
                         (unsigned long)episode, nonceHex, in ? 1 : 0,
                         latched ? 1 : 0, acked ? 1 : 0, tagHex);
  return w > 0 && (size_t)w < n && (size_t)w <= SHOWDUINO_AUTH_LINE_MAX;
}

/* Strict parse of an AUTH line. Does not verify the tag. */
static inline int showduino_auth_parse_auth(const char *line, ShowduinoAuthMsg *out) {
  if (!line || !out || strncmp(line, "AUTH:", 5) != 0) return 0;
  memset(out, 0, sizeof(*out));
  const char *p = line + 5;
  const char *c = strchr(p, ':');
  if (!c || c == p) return 0;
  const size_t idLen = (size_t)(c - p);
  if (idLen > SHOWDUINO_EMERGENCY_ID_MAX) return 0;
  memcpy(out->id, p, idLen);
  out->id[idLen] = '\0';
  p = c + 1;
  /* remaining: E8:N16:IN:L:A:T32 */
  /* E8 ':' N16 ':' IN ':' L ':' A ':' T32 = 8+1+16+7+32 = 64 */
  const size_t need = 8 + 1 + 16 + 7 + SHOWDUINO_AUTH_TAG_HEX;
  if (strlen(p) != need) return 0;
  char eb[9];
  memcpy(eb, p, 8);
  eb[8] = '\0';
  if (!showduino_auth_parse_u32hex(eb, &out->episode)) return 0;
  if (p[8] != ':') return 0;
  memcpy(out->nonceHex, p + 9, SHOWDUINO_AUTH_NONCE_HEX);
  out->nonceHex[SHOWDUINO_AUTH_NONCE_HEX] = '\0';
  if (!showduino_auth_hex_is_lower_or_upper_len(out->nonceHex, SHOWDUINO_AUTH_NONCE_HEX)) return 0;
  const char *q = p + 9 + SHOWDUINO_AUTH_NONCE_HEX;
  if (q[0] != ':' || q[2] != ':' || q[4] != ':' || q[6] != ':') return 0;
  if ((q[1] != '0' && q[1] != '1') || (q[3] != '0' && q[3] != '1') ||
      (q[5] != '0' && q[5] != '1')) {
    return 0;
  }
  out->in = q[1] == '1';
  out->latched = q[3] == '1';
  out->acked = q[5] == '1';
  if (!showduino_auth_hex_decode(q + 7, SHOWDUINO_AUTH_TAG_BYTES, out->tag)) return 0;
  return 1;
}

/* Verifies the tag of a parsed AUTH with the station key. Constant time. */
static inline int showduino_auth_verify_auth(const ShowduinoAuthMsg *m,
                                             const uint8_t *key, size_t keyLen,
                                             const ShowduinoHmacFn hmac) {
  char canon[160];
  uint8_t expect[SHOWDUINO_AUTH_TAG_BYTES];
  if (!m || !key || keyLen != SHOWDUINO_AUTH_KEY_BYTES) return 0;
  if (!showduino_auth_canon_auth(canon, sizeof(canon), m->id, m->episode, m->nonceHex,
                                 m->in, m->latched, m->acked)) {
    return 0;
  }
  if (!showduino_auth_tag(hmac, key, keyLen, canon, expect)) return 0;
  return showduino_auth_ct_equal(expect, m->tag, SHOWDUINO_AUTH_TAG_BYTES);
}

/* ---------- CMD (P4 -> station) ---------- */

typedef struct ShowduinoCmdMsg {
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  uint32_t counter;
  char op[4];       /* "ACK" or "GCL" */
  uint32_t episode;
  uint8_t tag[SHOWDUINO_AUTH_TAG_BYTES];
} ShowduinoCmdMsg;

static inline int showduino_auth_format_cmd(const char *id, uint32_t counter, const char *op,
                                            uint32_t episode, const uint8_t *key, size_t keyLen,
                                            const ShowduinoHmacFn hmac, char *out, size_t n) {
  char canon[120];
  uint8_t tag[SHOWDUINO_AUTH_TAG_BYTES];
  char tagHex[SHOWDUINO_AUTH_TAG_HEX + 1];
  if (!id || !op || !out) return 0;
  if (!showduino_auth_canon_cmd(canon, sizeof(canon), id, counter, op, episode)) return 0;
  if (!showduino_auth_tag(hmac, key, keyLen, canon, tag)) return 0;
  showduino_auth_hex_encode(tag, SHOWDUINO_AUTH_TAG_BYTES, tagHex);
  const int w = snprintf(out, n, "CMD:%s:%08lx:%s:%08lx:%s", id, (unsigned long)counter, op,
                         (unsigned long)episode, tagHex);
  return w > 0 && (size_t)w < n && (size_t)w <= SHOWDUINO_AUTH_LINE_MAX;
}

static inline int showduino_auth_parse_cmd(const char *line, ShowduinoCmdMsg *out) {
  if (!line || !out || strncmp(line, "CMD:", 4) != 0) return 0;
  memset(out, 0, sizeof(*out));
  const char *p = line + 4;
  const char *c1 = strchr(p, ':');
  if (!c1 || c1 == p) return 0;
  const size_t idLen = (size_t)(c1 - p);
  if (idLen > SHOWDUINO_EMERGENCY_ID_MAX) return 0;
  memcpy(out->id, p, idLen);
  out->id[idLen] = '\0';
  /* C8:OP3:E8:T32 */
  const char *q = c1 + 1;
  if (strlen(q) != 8 + 1 + 3 + 1 + 8 + 1 + SHOWDUINO_AUTH_TAG_HEX) return 0;
  char cb[9];
  memcpy(cb, q, 8);
  cb[8] = '\0';
  if (!showduino_auth_parse_u32hex(cb, &out->counter)) return 0;
  if (q[8] != ':') return 0;
  memcpy(out->op, q + 9, 3);
  out->op[3] = '\0';
  if (strcmp(out->op, "ACK") != 0 && strcmp(out->op, "GCL") != 0) return 0;
  if (q[12] != ':') return 0;
  char eb[9];
  memcpy(eb, q + 13, 8);
  eb[8] = '\0';
  if (!showduino_auth_parse_u32hex(eb, &out->episode)) return 0;
  if (q[21] != ':') return 0;
  return showduino_auth_hex_decode(q + 22, SHOWDUINO_AUTH_TAG_BYTES, out->tag);
}

static inline int showduino_auth_verify_cmd(const ShowduinoCmdMsg *m, const uint8_t *key,
                                            size_t keyLen, const ShowduinoHmacFn hmac) {
  char canon[120];
  uint8_t expect[SHOWDUINO_AUTH_TAG_BYTES];
  if (!m || !key || keyLen != SHOWDUINO_AUTH_KEY_BYTES) return 0;
  if (!showduino_auth_canon_cmd(canon, sizeof(canon), m->id, m->counter, m->op, m->episode)) {
    return 0;
  }
  if (!showduino_auth_tag(hmac, key, keyLen, canon, expect)) return 0;
  return showduino_auth_ct_equal(expect, m->tag, SHOWDUINO_AUTH_TAG_BYTES);
}

/* Station side: accepts a command only when its counter is strictly newer than the
 * last accepted one. The caller persists the new value BEFORE applying the command
 * and refuses the command if persistence is not verified. */
static inline int showduino_cmd_counter_is_fresh(uint32_t lastAccepted, uint32_t counter) {
  return counter > lastAccepted;
}

/* ---------- challenge table (P4, RAM, per boot) ---------- */

typedef struct ShowduinoAuthChalSlot {
  uint8_t used;
  uint8_t usedMask;          /* bit i = station slot i already answered this nonce */
  uint32_t episode;
  uint32_t issuedMs;
  uint32_t seq;              /* per-boot issue order; never reused within a boot */
  uint8_t nonce[SHOWDUINO_AUTH_NONCE_BYTES];
} ShowduinoAuthChalSlot;

typedef struct ShowduinoAuthChalTable {
  ShowduinoAuthChalSlot slot[SHOWDUINO_AUTH_CHAL_SLOTS];
  uint32_t lastSeq;          /* sequence of the most recent challenge; survives chal_reset */
} ShowduinoAuthChalTable;

/* Sequence the NEXT issued challenge will carry. A Director confirm records this
 * before issuing its proof challenge. */
static inline uint32_t showduino_auth_chal_next_seq(const ShowduinoAuthChalTable *t) {
  return t ? t->lastSeq + 1u : 0u;
}

/* Issues a fresh challenge. Returns 1 and fills nonceHex (17 bytes) on success. */
static inline int showduino_auth_chal_issue(ShowduinoAuthChalTable *t, uint32_t nowMs,
                                            uint32_t episode, ShowduinoRandFn rnd,
                                            char nonceHex[SHOWDUINO_AUTH_NONCE_HEX + 1]) {
  if (!t || !rnd || !nonceHex) return 0;
  int pick = -1;
  for (int i = 0; i < SHOWDUINO_AUTH_CHAL_SLOTS; ++i) {
    if (!t->slot[i].used) { pick = i; break; }
  }
  if (pick < 0) {
    /* Evict the oldest. Its answers are no longer accepted. */
    uint32_t oldestAge = 0;
    for (int i = 0; i < SHOWDUINO_AUTH_CHAL_SLOTS; ++i) {
      const uint32_t age = nowMs - t->slot[i].issuedMs;
      if (pick < 0 || age > oldestAge) { pick = i; oldestAge = age; }
    }
  }
  ShowduinoAuthChalSlot *s = &t->slot[pick];
  if (rnd(s->nonce, sizeof(s->nonce)) != 0) return 0;
  /* A nonce of all zero bytes is never valid. */
  int nonzero = 0;
  for (size_t i = 0; i < sizeof(s->nonce); ++i) nonzero |= s->nonce[i];
  if (!nonzero) return 0;
  s->used = 1;
  s->usedMask = 0;
  s->episode = episode;
  s->issuedMs = nowMs;
  s->seq = ++t->lastSeq;
  showduino_auth_hex_encode(s->nonce, sizeof(s->nonce), nonceHex);
  return 1;
}

/* Consumes an outstanding challenge for one station answer. Returns 1 only if:
 *   - the nonce is an outstanding challenge of this boot,
 *   - it is no older than SHOWDUINO_AUTH_WINDOW_MS,
 *   - its episode equals the current episode,
 *   - this station slot has not already answered it.
 * The station answer is then recorded so the same nonce cannot be used twice. */
static inline int showduino_auth_chal_consume_at(ShowduinoAuthChalTable *t, const char *nonceHex,
                                                 uint32_t episodeNow, uint32_t nowMs,
                                                 int stationSlot, uint32_t *seqOut) {
  uint8_t want[SHOWDUINO_AUTH_NONCE_BYTES];
  if (!t || !nonceHex || stationSlot < 0 || stationSlot >= 8) return 0; /* usedMask is 8 bits */
  if (!showduino_auth_hex_decode(nonceHex, SHOWDUINO_AUTH_NONCE_BYTES, want)) return 0;
  for (int i = 0; i < SHOWDUINO_AUTH_CHAL_SLOTS; ++i) {
    ShowduinoAuthChalSlot *s = &t->slot[i];
    if (!s->used || memcmp(s->nonce, want, sizeof(want)) != 0) continue;
    const uint32_t age = nowMs - s->issuedMs;
    if (age > SHOWDUINO_AUTH_WINDOW_MS) return 0;
    if (s->episode != episodeNow) return 0;
    const uint8_t bit = (uint8_t)(1u << stationSlot);
    if (s->usedMask & bit) return 0;
    s->usedMask |= bit;
    if (seqOut) *seqOut = s->seq;
    return 1;
  }
  return 0;
}

static inline int showduino_auth_chal_consume(ShowduinoAuthChalTable *t, const char *nonceHex,
                                              uint32_t episodeNow, uint32_t nowMs,
                                              int stationSlot) {
  return showduino_auth_chal_consume_at(t, nonceHex, episodeNow, nowMs, stationSlot, 0);
}

/* Clears every outstanding challenge. Used when the P4 changes episode and at boot. */
static inline void showduino_auth_chal_reset(ShowduinoAuthChalTable *t) {
  if (!t) return;
  const uint32_t keep = t->lastSeq;  /* a reset must never make an old proof look newer */
  memset(t, 0, sizeof(*t));
  t->lastSeq = keep;
}

/* ---------- nonce generator (P4) ----------
 * The ESP32-P4 has no radio, so esp_random() is only a true RNG while the SAR ADC
 * entropy source is enabled (ESP-IDF "Random Number Generation"). The P4 therefore
 * draws a 32-byte seed once per boot with that source enabled, then derives nonces
 * as HMAC-SHA256(seed, "SHOWDUINO-NONCE|v1|" || counter), truncated. Nonces are
 * unpredictable to anyone without the seed (the S3 never sees it) and never repeat
 * within a boot. Output is refused before seeding and after counter exhaustion. */
typedef struct ShowduinoNonceGen {
  uint8_t seed[32];
  uint32_t ctr;
  uint8_t seeded;
  ShowduinoHmacFn hmac;
} ShowduinoNonceGen;

static inline int showduino_nonce_gen_seed(ShowduinoNonceGen *g, const uint8_t seed[32],
                                           ShowduinoHmacFn hmac) {
  if (!g || !seed || !hmac) return 0;
  int nonzero = 0;
  for (int i = 0; i < 32; ++i) nonzero |= seed[i];
  memset(g, 0, sizeof(*g));
  if (!nonzero) return 0;
  memcpy(g->seed, seed, 32);
  g->hmac = hmac;
  g->seeded = 1;
  return 1;
}

static inline int showduino_nonce_gen_next(ShowduinoNonceGen *g, uint8_t *out, size_t n) {
  uint8_t full[32];
  char msg[40];
  if (!g || !g->seeded || !out || n == 0 || n > sizeof(full)) return -1;
  if (g->ctr == 0xFFFFFFFFu) return -1;
  const int w = snprintf(msg, sizeof(msg), "SHOWDUINO-NONCE|v1|%08lx", (unsigned long)g->ctr);
  if (w <= 0 || (size_t)w >= sizeof(msg)) return -1;
  if (g->hmac(g->seed, sizeof(g->seed), (const uint8_t *)msg, (size_t)w, full) != 0) return -1;
  g->ctr++;
  memcpy(out, full, n);
  memset(full, 0, sizeof(full));
  return 0;
}

/* ---------- counter block (P4 command counter and episode ids) ---------- */

typedef struct ShowduinoAuthCounter {
  uint32_t next;   /* next value to hand out */
  uint32_t limit;  /* first value NOT yet covered by a persisted block */
} ShowduinoAuthCounter;

/* Boot: resumes after the persisted limit. Values below it may have been used. */
static inline void showduino_auth_counter_boot(ShowduinoAuthCounter *c, uint32_t storedLimit) {
  if (!c) return;
  c->next = storedLimit;
  c->limit = storedLimit;
}

/* Hands out the next value. Persists a new block limit first when needed. Returns 0
 * (and hands out nothing) if the block cannot be persisted. */
static inline int showduino_auth_counter_next(ShowduinoAuthCounter *c, ShowduinoPersistFn persist,
                                              void *ctx, uint32_t *out) {
  if (!c || !out || !persist) return 0;
  if (c->next >= c->limit) {
    const uint32_t newLimit = c->next + SHOWDUINO_AUTH_COUNTER_BLOCK;
    if (newLimit < c->next) return 0; /* overflow: refuse */
    if (!persist(ctx, newLimit)) return 0;
    c->limit = newLimit;
  }
  *out = c->next++;
  return 1;
}

/* Episode-id learning on a station: a non-zero episode from CHAL or ACK is recorded. */
static inline void showduino_auth_learn_episode(uint32_t *current, uint32_t episode) {
  if (current && episode != 0) *current = episode;
}

/* Station side: a GCL is applied only for the episode this station latched under. */
static inline int showduino_auth_gcl_matches_episode(uint32_t latchedEpisode, uint32_t gclEpisode) {
  return latchedEpisode != 0 && latchedEpisode == gclEpisode;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_STATION_AUTH_H */
