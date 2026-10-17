#ifndef SHOWDUINO_STATION_RESPONDER_H
#define SHOWDUINO_STATION_RESPONDER_H

/*
 * MISSION 001J: Emergency Node (C3) side of station authentication.
 *
 * The C3 firmware and the host tests both use these functions, so the tests run
 * the production policy.
 *
 *   CHAL  (unauthenticated, from the P4 via the S3 relay)
 *         -> AUTH reporting this station's physical state, bound to the nonce and
 *            episode in the challenge, tagged with this station's key.
 *            A forged CHAL only yields an AUTH for a nonce the P4 never issued,
 *            which the P4 rejects.
 *   CMD   (tagged by the P4 with this station's key)
 *         ACK: accepted only while latched. Records the P4 episode.
 *         GCL: accepted only while latched AND for the recorded episode.
 *         Both need a counter strictly above the last accepted one, and the new
 *         counter is persisted BEFORE the command is applied. Persistence failure
 *         rejects the command (the station stays latched: fail safe).
 *
 * Nothing in this file can latch, unlatch, or rearm the station by itself; the
 * caller applies the returned operation to the real state machine.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "showduino_emergency_node.h"
#include "showduino_station_auth.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShowduinoStationAuthState {
  uint8_t provisioned;              /* 1 = key holds a provisioned secret */
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  uint32_t lastCmdCounter;          /* persisted; 0 = none accepted */
  uint32_t latchedEpisode;          /* RAM; learned from an authenticated ACK */
  uint8_t prevLatched;
} ShowduinoStationAuthState;

typedef enum ShowduinoResponderCmd {
  SHOWDUINO_RESP_REJECT_FORMAT = -6,
  SHOWDUINO_RESP_REJECT_NOT_PROVISIONED = -5,
  SHOWDUINO_RESP_REJECT_IDENTITY = -4,
  SHOWDUINO_RESP_REJECT_TAG = -3,
  SHOWDUINO_RESP_REJECT_REPLAY = -2,
  SHOWDUINO_RESP_REJECT_STATE = -1,     /* not latched, or wrong episode */
  SHOWDUINO_RESP_REJECT_PERSIST = 0,
  SHOWDUINO_RESP_ACK = 1,
  SHOWDUINO_RESP_GCL = 2
} ShowduinoResponderCmd;

/* Boot. `storedCounter` is the persisted last accepted counter (0 if none). */
static inline void showduino_responder_boot(ShowduinoStationAuthState *st, const uint8_t *key,
                                            int keyValid, uint32_t storedCounter,
                                            int latchedNow) {
  if (!st) return;
  memset(st, 0, sizeof(*st));
  if (key && keyValid) {
    memcpy(st->key, key, SHOWDUINO_AUTH_KEY_BYTES);
    st->provisioned = 1;
  }
  st->lastCmdCounter = storedCounter;
  st->prevLatched = latchedNow ? 1 : 0;
}

/* Call on every service pass with the real machine's latched flag. A fresh latch
 * forgets the episode, so a GCL held back from an earlier episode cannot apply. */
static inline void showduino_responder_track_latch(ShowduinoStationAuthState *st, int latchedNow) {
  if (!st) return;
  const uint8_t l = latchedNow ? 1 : 0;
  if (l != st->prevLatched) st->latchedEpisode = 0;
  st->prevLatched = l;
}

/* Builds the AUTH answer for a CHAL line. Returns 1 when `out` holds an AUTH line. */
static inline int showduino_responder_answer_chal(const ShowduinoStationAuthState *st,
                                                  const char *id, int inputPressed, int latched,
                                                  const char *line, ShowduinoHmacFn hmac,
                                                  char *out, size_t n) {
  ShowduinoAuthChalMsg ch;
  if (!st || !st->provisioned || !id || !id[0] || !hmac) return 0;
  if (!showduino_auth_parse_chal(line, &ch)) return 0;
  const int acked = (latched && st->latchedEpisode != 0 && st->latchedEpisode == ch.episode) ? 1 : 0;
  return showduino_auth_format_auth(id, ch.episode, ch.nonceHex, inputPressed ? 1 : 0,
                                    latched ? 1 : 0, acked, st->key, SHOWDUINO_AUTH_KEY_BYTES,
                                    hmac, out, n);
}

/* Verifies and accepts a CMD line. Returns a ShowduinoResponderCmd. On ACK/GCL the
 * counter is already persisted and recorded; the caller applies the operation. */
static inline int showduino_responder_accept_cmd(ShowduinoStationAuthState *st, const char *id,
                                                 int latched, const char *line,
                                                 ShowduinoHmacFn hmac, ShowduinoPersistFn persist,
                                                 void *ctx, uint32_t *episodeOut) {
  ShowduinoCmdMsg m;
  if (!st || !id || !hmac || !persist) return SHOWDUINO_RESP_REJECT_FORMAT;
  if (!st->provisioned) return SHOWDUINO_RESP_REJECT_NOT_PROVISIONED;
  if (!showduino_auth_parse_cmd(line, &m)) return SHOWDUINO_RESP_REJECT_FORMAT;
  if (!showduino_emergency_id_equal(m.id, id)) return SHOWDUINO_RESP_REJECT_IDENTITY;
  if (!showduino_auth_verify_cmd(&m, st->key, SHOWDUINO_AUTH_KEY_BYTES, hmac)) {
    return SHOWDUINO_RESP_REJECT_TAG;
  }
  if (!showduino_cmd_counter_is_fresh(st->lastCmdCounter, m.counter)) {
    return SHOWDUINO_RESP_REJECT_REPLAY;
  }
  const int isAck = strcmp(m.op, "ACK") == 0;
  if (!latched) return SHOWDUINO_RESP_REJECT_STATE;
  if (isAck) {
    if (m.episode == 0) return SHOWDUINO_RESP_REJECT_STATE;
  } else if (!showduino_auth_gcl_matches_episode(st->latchedEpisode, m.episode)) {
    return SHOWDUINO_RESP_REJECT_STATE;
  }
  /* Persist first. A command is never applied without a durable counter. */
  if (!persist(ctx, m.counter)) return SHOWDUINO_RESP_REJECT_PERSIST;
  st->lastCmdCounter = m.counter;
  if (episodeOut) *episodeOut = m.episode;
  if (isAck) {
    st->latchedEpisode = m.episode;
    return SHOWDUINO_RESP_ACK;
  }
  return SHOWDUINO_RESP_GCL;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_STATION_RESPONDER_H */
