#ifndef SHOWDUINO_E131_H
#define SHOWDUINO_E131_H

/*
 * Host-testable E1.31 / sACN data-packet parser.
 * No Arduino, sockets, or show-control side effects.
 *
 * This is observation-only. A valid packet never starts a production,
 * changes outputs, or becomes authoritative Showduino state.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_E131_UDP_PORT           5568
#define SHOWDUINO_E131_HEADER_BYTES       126
#define SHOWDUINO_E131_MAX_SLOTS          512
#define SHOWDUINO_E131_MAX_PACKET         (SHOWDUINO_E131_HEADER_BYTES + SHOWDUINO_E131_MAX_SLOTS)
#define SHOWDUINO_E131_MIN_PACKET         SHOWDUINO_E131_HEADER_BYTES
#define SHOWDUINO_E131_ACN_ID             "ASC-E1.17\0\0\0"
#define SHOWDUINO_E131_ACN_ID_LEN         12
#define SHOWDUINO_E131_ROOT_VECTOR        0x00000004u
#define SHOWDUINO_E131_FRAMING_VECTOR     0x00000002u
#define SHOWDUINO_E131_DMP_VECTOR         0x02u
#define SHOWDUINO_E131_DMP_TYPE           0xa1u
#define SHOWDUINO_E131_START_CODE_DMX     0x00u
#define SHOWDUINO_E131_UNIVERSE_MIN       1
#define SHOWDUINO_E131_UNIVERSE_MAX       63999
#define SHOWDUINO_E131_FLAGS_NIBBLE       0x7u

#define SHOWDUINO_E131_OFF_PREAMBLE       0
#define SHOWDUINO_E131_OFF_POSTAMBLE      2
#define SHOWDUINO_E131_OFF_ACN_ID         4
#define SHOWDUINO_E131_OFF_ROOT_FLAGS     16
#define SHOWDUINO_E131_OFF_ROOT_VECTOR    18
#define SHOWDUINO_E131_OFF_CID            22
#define SHOWDUINO_E131_OFF_FRAMING_FLAGS  38
#define SHOWDUINO_E131_OFF_FRAMING_VECTOR 40
#define SHOWDUINO_E131_OFF_SOURCE_NAME    44
#define SHOWDUINO_E131_OFF_PRIORITY       108
#define SHOWDUINO_E131_OFF_SYNC           109
#define SHOWDUINO_E131_OFF_SEQUENCE       111
#define SHOWDUINO_E131_OFF_OPTIONS        112
#define SHOWDUINO_E131_OFF_UNIVERSE       113
#define SHOWDUINO_E131_OFF_DMP_FLAGS      115
#define SHOWDUINO_E131_OFF_DMP_VECTOR     117
#define SHOWDUINO_E131_OFF_DMP_TYPE       118
#define SHOWDUINO_E131_OFF_FIRST_ADDR     119
#define SHOWDUINO_E131_OFF_INCREMENT      121
#define SHOWDUINO_E131_OFF_PROP_COUNT     123
#define SHOWDUINO_E131_OFF_START_CODE     125
#define SHOWDUINO_E131_OFF_SLOTS          126
#define SHOWDUINO_E131_SOURCE_NAME_LEN    64

typedef enum ShowduinoE131Status {
  SHOWDUINO_E131_OK = 0,
  SHOWDUINO_E131_NULL,
  SHOWDUINO_E131_TRUNCATED,
  SHOWDUINO_E131_TOO_LARGE,
  SHOWDUINO_E131_BAD_PREAMBLE,
  SHOWDUINO_E131_BAD_IDENTIFIER,
  SHOWDUINO_E131_BAD_ROOT_VECTOR,
  SHOWDUINO_E131_BAD_FRAMING_VECTOR,
  SHOWDUINO_E131_BAD_DMP_VECTOR,
  SHOWDUINO_E131_BAD_DMP_TYPE,
  SHOWDUINO_E131_BAD_ADDRESS,
  SHOWDUINO_E131_BAD_INCREMENT,
  SHOWDUINO_E131_BAD_PROPERTY_COUNT,
  SHOWDUINO_E131_BAD_LENGTH,
  SHOWDUINO_E131_WRONG_UNIVERSE,
  SHOWDUINO_E131_UNSUPPORTED_START_CODE,
  SHOWDUINO_E131_DUPLICATE_SEQUENCE,
  SHOWDUINO_E131_OLD_SEQUENCE
} ShowduinoE131Status;

typedef struct ShowduinoE131View {
  uint8_t cid[16];
  char sourceName[SHOWDUINO_E131_SOURCE_NAME_LEN + 1];
  uint8_t priority;
  uint8_t sequence;
  uint16_t universe;
  uint16_t syncAddress;
  uint8_t options;
  uint8_t startCode;
  uint16_t slotCount;
  const uint8_t *slots;
} ShowduinoE131View;

static inline uint16_t showduino_e131_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t showduino_e131_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint16_t showduino_e131_pdu_length(const uint8_t *p) {
  return (uint16_t)(((uint16_t)(p[0] & 0x0f) << 8) | (uint16_t)p[1]);
}

static inline int showduino_e131_flags_ok(const uint8_t *p) {
  return ((p[0] >> 4) & 0x0f) == SHOWDUINO_E131_FLAGS_NIBBLE;
}

static inline int showduino_e131_universe_valid(uint16_t universe) {
  return universe >= SHOWDUINO_E131_UNIVERSE_MIN &&
         universe <= SHOWDUINO_E131_UNIVERSE_MAX;
}

/* E1.31 IPv4 multicast: 239.255.(universe >> 8).(universe & 0xff) */
static inline void showduino_e131_multicast_ipv4(uint16_t universe, uint8_t out[4]) {
  if (!out) return;
  out[0] = 239;
  out[1] = 255;
  out[2] = (uint8_t)(universe >> 8);
  out[3] = (uint8_t)(universe & 0xff);
}

static inline const char *showduino_e131_status_name(int status) {
  switch ((ShowduinoE131Status)status) {
    case SHOWDUINO_E131_OK: return "OK";
    case SHOWDUINO_E131_NULL: return "NULL";
    case SHOWDUINO_E131_TRUNCATED: return "TRUNCATED";
    case SHOWDUINO_E131_TOO_LARGE: return "TOO_LARGE";
    case SHOWDUINO_E131_BAD_PREAMBLE: return "BAD_PREAMBLE";
    case SHOWDUINO_E131_BAD_IDENTIFIER: return "BAD_IDENTIFIER";
    case SHOWDUINO_E131_BAD_ROOT_VECTOR: return "BAD_ROOT_VECTOR";
    case SHOWDUINO_E131_BAD_FRAMING_VECTOR: return "BAD_FRAMING_VECTOR";
    case SHOWDUINO_E131_BAD_DMP_VECTOR: return "BAD_DMP_VECTOR";
    case SHOWDUINO_E131_BAD_DMP_TYPE: return "BAD_DMP_TYPE";
    case SHOWDUINO_E131_BAD_ADDRESS: return "BAD_ADDRESS";
    case SHOWDUINO_E131_BAD_INCREMENT: return "BAD_INCREMENT";
    case SHOWDUINO_E131_BAD_PROPERTY_COUNT: return "BAD_PROPERTY_COUNT";
    case SHOWDUINO_E131_BAD_LENGTH: return "BAD_LENGTH";
    case SHOWDUINO_E131_WRONG_UNIVERSE: return "WRONG_UNIVERSE";
    case SHOWDUINO_E131_UNSUPPORTED_START_CODE: return "UNSUPPORTED_START_CODE";
    case SHOWDUINO_E131_DUPLICATE_SEQUENCE: return "DUPLICATE_SEQUENCE";
    case SHOWDUINO_E131_OLD_SEQUENCE: return "OLD_SEQUENCE";
    default: return "UNKNOWN";
  }
}

/*
 * Sequence progression with 8-bit wrap.
 * haveLast == 0 accepts the first packet.
 * Duplicate (d == 0) and recent-old (d < 0 && d > -20) are rejected.
 */
static inline int showduino_e131_sequence_check(uint8_t last, uint8_t now, int haveLast) {
  int8_t d;
  if (!haveLast) return SHOWDUINO_E131_OK;
  d = (int8_t)(now - last);
  if (d == 0) return SHOWDUINO_E131_DUPLICATE_SEQUENCE;
  if (d < 0 && d > -20) return SHOWDUINO_E131_OLD_SEQUENCE;
  return SHOWDUINO_E131_OK;
}

static inline int showduino_e131_parse(const uint8_t *pkt, size_t len,
                                       uint16_t expectUniverse,
                                       ShowduinoE131View *out) {
  uint16_t rootLen;
  uint16_t framingLen;
  uint16_t dmpLen;
  uint16_t propCount;
  uint16_t universe;
  uint16_t slotCount;
  size_t i;

  if (out) memset(out, 0, sizeof(*out));
  if (!pkt) return SHOWDUINO_E131_NULL;
  if (len < SHOWDUINO_E131_MIN_PACKET) return SHOWDUINO_E131_TRUNCATED;
  if (len > SHOWDUINO_E131_MAX_PACKET) return SHOWDUINO_E131_TOO_LARGE;

  if (showduino_e131_be16(pkt + SHOWDUINO_E131_OFF_PREAMBLE) != 0x0010 ||
      showduino_e131_be16(pkt + SHOWDUINO_E131_OFF_POSTAMBLE) != 0x0000) {
    return SHOWDUINO_E131_BAD_PREAMBLE;
  }
  if (memcmp(pkt + SHOWDUINO_E131_OFF_ACN_ID, SHOWDUINO_E131_ACN_ID,
             SHOWDUINO_E131_ACN_ID_LEN) != 0) {
    return SHOWDUINO_E131_BAD_IDENTIFIER;
  }

  if (!showduino_e131_flags_ok(pkt + SHOWDUINO_E131_OFF_ROOT_FLAGS) ||
      !showduino_e131_flags_ok(pkt + SHOWDUINO_E131_OFF_FRAMING_FLAGS) ||
      !showduino_e131_flags_ok(pkt + SHOWDUINO_E131_OFF_DMP_FLAGS)) {
    return SHOWDUINO_E131_BAD_LENGTH;
  }

  rootLen = showduino_e131_pdu_length(pkt + SHOWDUINO_E131_OFF_ROOT_FLAGS);
  framingLen = showduino_e131_pdu_length(pkt + SHOWDUINO_E131_OFF_FRAMING_FLAGS);
  dmpLen = showduino_e131_pdu_length(pkt + SHOWDUINO_E131_OFF_DMP_FLAGS);
  if (rootLen != (uint16_t)(len - SHOWDUINO_E131_OFF_ROOT_FLAGS) ||
      framingLen != (uint16_t)(len - SHOWDUINO_E131_OFF_FRAMING_FLAGS) ||
      dmpLen != (uint16_t)(len - SHOWDUINO_E131_OFF_DMP_FLAGS)) {
    return SHOWDUINO_E131_BAD_LENGTH;
  }

  if (showduino_e131_be32(pkt + SHOWDUINO_E131_OFF_ROOT_VECTOR) !=
      SHOWDUINO_E131_ROOT_VECTOR) {
    return SHOWDUINO_E131_BAD_ROOT_VECTOR;
  }
  if (showduino_e131_be32(pkt + SHOWDUINO_E131_OFF_FRAMING_VECTOR) !=
      SHOWDUINO_E131_FRAMING_VECTOR) {
    return SHOWDUINO_E131_BAD_FRAMING_VECTOR;
  }
  if (pkt[SHOWDUINO_E131_OFF_DMP_VECTOR] != SHOWDUINO_E131_DMP_VECTOR) {
    return SHOWDUINO_E131_BAD_DMP_VECTOR;
  }
  if (pkt[SHOWDUINO_E131_OFF_DMP_TYPE] != SHOWDUINO_E131_DMP_TYPE) {
    return SHOWDUINO_E131_BAD_DMP_TYPE;
  }
  if (showduino_e131_be16(pkt + SHOWDUINO_E131_OFF_FIRST_ADDR) != 0) {
    return SHOWDUINO_E131_BAD_ADDRESS;
  }
  if (showduino_e131_be16(pkt + SHOWDUINO_E131_OFF_INCREMENT) != 1) {
    return SHOWDUINO_E131_BAD_INCREMENT;
  }

  propCount = showduino_e131_be16(pkt + SHOWDUINO_E131_OFF_PROP_COUNT);
  if (propCount < 1 || propCount > (SHOWDUINO_E131_MAX_SLOTS + 1)) {
    return SHOWDUINO_E131_BAD_PROPERTY_COUNT;
  }
  if ((size_t)SHOWDUINO_E131_OFF_START_CODE + (size_t)propCount != len) {
    return SHOWDUINO_E131_BAD_PROPERTY_COUNT;
  }

  universe = showduino_e131_be16(pkt + SHOWDUINO_E131_OFF_UNIVERSE);
  if (!showduino_e131_universe_valid(universe)) {
    return SHOWDUINO_E131_WRONG_UNIVERSE;
  }
  if (expectUniverse != 0 && universe != expectUniverse) {
    return SHOWDUINO_E131_WRONG_UNIVERSE;
  }

  slotCount = (uint16_t)(propCount - 1u);
  if (out) {
    memcpy(out->cid, pkt + SHOWDUINO_E131_OFF_CID, 16);
    memcpy(out->sourceName, pkt + SHOWDUINO_E131_OFF_SOURCE_NAME,
           SHOWDUINO_E131_SOURCE_NAME_LEN);
    out->sourceName[SHOWDUINO_E131_SOURCE_NAME_LEN] = '\0';
    for (i = 0; i < SHOWDUINO_E131_SOURCE_NAME_LEN; i++) {
      unsigned char ch = (unsigned char)out->sourceName[i];
      if (ch == 0) break;
      if (ch < 32 || ch > 126) out->sourceName[i] = '?';
    }
    out->priority = pkt[SHOWDUINO_E131_OFF_PRIORITY];
    out->sequence = pkt[SHOWDUINO_E131_OFF_SEQUENCE];
    out->universe = universe;
    out->syncAddress = showduino_e131_be16(pkt + SHOWDUINO_E131_OFF_SYNC);
    out->options = pkt[SHOWDUINO_E131_OFF_OPTIONS];
    out->startCode = pkt[SHOWDUINO_E131_OFF_START_CODE];
    out->slotCount = slotCount;
    out->slots = (slotCount > 0) ? (pkt + SHOWDUINO_E131_OFF_SLOTS) : NULL;
  }

  if (pkt[SHOWDUINO_E131_OFF_START_CODE] != SHOWDUINO_E131_START_CODE_DMX) {
    return SHOWDUINO_E131_UNSUPPORTED_START_CODE;
  }
  return SHOWDUINO_E131_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_E131_H */
