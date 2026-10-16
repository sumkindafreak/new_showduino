#ifndef SHOWDUINO_STATION_BLOCKERS_H
#define SHOWDUINO_STATION_BLOCKERS_H

/*
 * Wireless Emergency Node clear-blocker policy and persistence codec.
 *
 * A station whose last valid report says input_open=1 (button pressed) blocks
 * emergency clear. It keeps blocking across disconnection, restart, timeout and
 * P4 reboot. Only a fresh valid report with input_open=0 from the same station
 * identity (ID and stored MAC) releases it.
 *
 * Persisted format (one NVS string, written whole, read back and verified):
 *   "B1;" then zero or more "ESTOP-NN,<mac>;" entries, then "#XXXX"
 *   where XXXX is CRC-16/CCITT-FALSE over everything before '#'.
 * A missing mac is written as "-".
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SHOWDUINO_BLOCKER_MAX        8
#define SHOWDUINO_BLOCKER_ID_CHARS   12
#define SHOWDUINO_BLOCKER_MAC_CHARS  17
#define SHOWDUINO_BLOCKER_NVS_KEY    "estop_blk"
#define SHOWDUINO_BLOCKER_BUF        512

typedef struct {
  char id[SHOWDUINO_BLOCKER_ID_CHARS + 1];
  char mac[SHOWDUINO_BLOCKER_MAC_CHARS + 1];
} ShowduinoBlocker;

typedef enum {
  SHOWDUINO_BLOCKER_OK = 0,
  SHOWDUINO_BLOCKER_ABSENT = 1,
  SHOWDUINO_BLOCKER_CORRUPT = 2,
  SHOWDUINO_BLOCKER_UNREADABLE = 3
} ShowduinoBlockerStatus;

static inline uint16_t showduino_blocker_crc16(const char *s, size_t n) {
  uint16_t crc = 0xFFFFu;
  size_t i;
  uint8_t bit;
  for (i = 0; i < n; i++) {
    crc ^= (uint16_t)((uint8_t)s[i]) << 8;
    for (bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static inline int showduino_blocker_id_ok(const char *id, size_t n) {
  size_t i;
  if (!id || n < 8 || n > SHOWDUINO_BLOCKER_ID_CHARS) return 0;
  if (strncmp(id, "ESTOP-", 6) != 0) return 0;
  for (i = 6; i < n; i++) {
    if (id[i] < '0' || id[i] > '9') return 0;
  }
  return 1;
}

/* Returns the stored length, or -1 when the set cannot be represented. */
static inline int showduino_blocker_encode(const ShowduinoBlocker *b, uint8_t n,
                                           char *out, size_t cap) {
  size_t len, i;
  int k;
  if (!out || !b || n > SHOWDUINO_BLOCKER_MAX || cap < 16) return -1;
  k = snprintf(out, cap, "B1;");
  if (k < 0 || (size_t)k >= cap) return -1;
  len = (size_t)k;
  for (i = 0; i < n; i++) {
    const char *mac = b[i].mac[0] ? b[i].mac : "-";
    if (!showduino_blocker_id_ok(b[i].id, strlen(b[i].id))) return -1;
    k = snprintf(out + len, cap - len, "%s,%s;", b[i].id, mac);
    if (k < 0 || (size_t)k >= cap - len) return -1;
    len += (size_t)k;
  }
  k = snprintf(out + len, cap - len, "#%04X", (unsigned)showduino_blocker_crc16(out, len));
  if (k < 0 || (size_t)k >= cap - len) return -1;
  return (int)(len + (size_t)k);
}

/* Parses a stored string. Any malformation, bad checksum or count overflow is
 * CORRUPT. The caller must treat CORRUPT as "unknown", never as "released". */
static inline ShowduinoBlockerStatus showduino_blocker_decode(const char *in,
                                                              ShowduinoBlocker *out,
                                                              uint8_t *count) {
  const char *hash, *p, *semi, *comma;
  size_t bodyLen, entLen, idLen, macLen;
  uint8_t n = 0;
  unsigned crcStored = 0;
  if (!in || !out || !count) return SHOWDUINO_BLOCKER_CORRUPT;
  *count = 0;
  if (strncmp(in, "B1;", 3) != 0) return SHOWDUINO_BLOCKER_CORRUPT;
  hash = strrchr(in, '#');
  if (!hash || strlen(hash) != 5) return SHOWDUINO_BLOCKER_CORRUPT;
  for (p = hash + 1; p < hash + 5; p++) {
    char c = *p;
    crcStored <<= 4;
    if (c >= '0' && c <= '9') crcStored |= (unsigned)(c - '0');
    else if (c >= 'A' && c <= 'F') crcStored |= (unsigned)(c - 'A' + 10);
    else return SHOWDUINO_BLOCKER_CORRUPT;
  }
  bodyLen = (size_t)(hash - in);
  if (showduino_blocker_crc16(in, bodyLen) != (uint16_t)crcStored) return SHOWDUINO_BLOCKER_CORRUPT;

  p = in + 3;
  while (p < hash) {
    semi = (const char *)memchr(p, ';', (size_t)(hash - p));
    if (!semi) return SHOWDUINO_BLOCKER_CORRUPT;
    entLen = (size_t)(semi - p);
    comma = (const char *)memchr(p, ',', entLen);
    if (!comma) return SHOWDUINO_BLOCKER_CORRUPT;
    idLen = (size_t)(comma - p);
    macLen = entLen - idLen - 1;
    if (!showduino_blocker_id_ok(p, idLen)) return SHOWDUINO_BLOCKER_CORRUPT;
    if (macLen == 1 && comma[1] == '-') macLen = 0;
    else if (macLen != SHOWDUINO_BLOCKER_MAC_CHARS) return SHOWDUINO_BLOCKER_CORRUPT;
    if (n >= SHOWDUINO_BLOCKER_MAX) return SHOWDUINO_BLOCKER_CORRUPT;
    memcpy(out[n].id, p, idLen);
    out[n].id[idLen] = '\0';
    if (macLen) memcpy(out[n].mac, comma + 1, macLen);
    out[n].mac[macLen] = '\0';
    n++;
    p = semi + 1;
  }
  *count = n;
  return SHOWDUINO_BLOCKER_OK;
}

static inline int showduino_blocker_sets_equal(const ShowduinoBlocker *a, uint8_t na,
                                               const ShowduinoBlocker *b, uint8_t nb) {
  uint8_t i;
  if (na != nb) return 0;
  for (i = 0; i < na; i++) {
    if (strcmp(a[i].id, b[i].id) != 0 || strcmp(a[i].mac, b[i].mac) != 0) return 0;
  }
  return 1;
}

/* Input state after one report from a station.
 *   - A pressed report always sets input_open=1.
 *   - A released report sets input_open=0 when the stored identity is the ID
 *     alone (no MAC yet) or the MAC matches. On a MAC mismatch the current state
 *     is kept, so a block cannot be released or created by a different station. */
static inline int showduino_station_apply_report(int currentOpen, int reportOpen,
                                                 const char *storedMac,
                                                 const char *reportMac) {
  if (reportOpen) return 1;
  /* No stored MAC: the ID alone identifies the station, so a released report
   * from it is accepted. A stored MAC must match exactly. */
  if (!storedMac || !storedMac[0]) return 0;
  /* Mismatched identity: keep the current state. Neither release nor new
   * block is applied from a report that does not match the station. */
  if (!reportMac || strcmp(storedMac, reportMac) != 0) return currentOpen ? 1 : 0;
  return 0;
}

/* Clear is refused when any station blocks it, when the boot-time restore is
 * unknown or incomplete, when a persistence write is unverified, or before the
 * station table has been restored and applied. */
static inline int showduino_station_clear_blocked_full(unsigned pressedStations,
                                                       int unknownBootState,
                                                       int persistDirty,
                                                       int booted) {
  if (!booted) return 1;
  return (pressedStations != 0u) || unknownBootState || persistDirty;
}

/* Persist only on a meaningful change, or when storage was never verified. */
static inline int showduino_station_persist_needed(int validInStorage, int dirty,
                                                   int setsEqual) {
  return !validInStorage || dirty || !setsEqual;
}

#endif
