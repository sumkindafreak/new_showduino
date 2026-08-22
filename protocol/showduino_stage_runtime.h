#ifndef SHOWDUINO_STAGE_RUNTIME_H
#define SHOWDUINO_STAGE_RUNTIME_H

/*
 * Showduino Stage Runtime protocol (SRv1) — C3 ↔ P4 UART.
 *
 * Separate from the GET-only WEB/ / WEBR: Studio tunnel.
 * Line-oriented ASCII framing, CRC16 over payload, no dynamic allocation
 * required by callers (fixed buffers).
 *
 * Frame:
 *   SRv1:<TYPE>:<REQ_ID>:<LEN>:<CRC16>\n
 *   <LEN bytes payload>\n
 *
 * CRC16: CCITT-FALSE (poly 0x1021, init 0xFFFF, no final XOR).
 * CRC field is 4 uppercase hex digits.
 *
 * Slice-1 types: SNAP_REQ, SNAP_RSP, ERROR
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_SRV1_PREFIX           "SRv1:"
#define SHOWDUINO_SRV1_PREFIX_LEN       5

#define SHOWDUINO_SRV1_TYPE_SNAP_REQ    "SNAP_REQ"
#define SHOWDUINO_SRV1_TYPE_SNAP_RSP    "SNAP_RSP"
#define SHOWDUINO_SRV1_TYPE_ERROR       "ERROR"

#define SHOWDUINO_SRV1_PAYLOAD_MAX      1024u
#define SHOWDUINO_SRV1_REQ_ID_MAX       16u
#define SHOWDUINO_SRV1_TYPE_MAX         16u
#define SHOWDUINO_SRV1_HEADER_MAX       64u
#define SHOWDUINO_SRV1_FRAME_MAX        (SHOWDUINO_SRV1_HEADER_MAX + SHOWDUINO_SRV1_PAYLOAD_MAX + 2u)

#define SHOWDUINO_SRV1_SNAP_PERIOD_MS   3000u
#define SHOWDUINO_SRV1_SNAP_TIMEOUT_MS  2500u
#define SHOWDUINO_SRV1_STALE_MS         10000u

typedef enum ShowduinoSrv1Type {
  SHOWDUINO_SRV1_UNKNOWN = 0,
  SHOWDUINO_SRV1_SNAP_REQ,
  SHOWDUINO_SRV1_SNAP_RSP,
  SHOWDUINO_SRV1_ERROR
} ShowduinoSrv1Type;

typedef struct ShowduinoSrv1Header {
  ShowduinoSrv1Type type;
  char typeName[SHOWDUINO_SRV1_TYPE_MAX];
  char reqId[SHOWDUINO_SRV1_REQ_ID_MAX];
  uint16_t payloadLen;
  uint16_t crc16;
} ShowduinoSrv1Header;

static inline uint16_t showduino_srv1_crc16(const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint16_t crc = 0xFFFFu;
  size_t i;
  int b;
  if (!p && len) return 0;
  for (i = 0; i < len; i++) {
    crc ^= (uint16_t)p[i] << 8;
    for (b = 0; b < 8; b++) {
      if (crc & 0x8000u) crc = (uint16_t)((crc << 1) ^ 0x1021u);
      else crc = (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static inline ShowduinoSrv1Type showduino_srv1_type_from_name(const char *name) {
  if (!name) return SHOWDUINO_SRV1_UNKNOWN;
  if (strcmp(name, SHOWDUINO_SRV1_TYPE_SNAP_REQ) == 0) return SHOWDUINO_SRV1_SNAP_REQ;
  if (strcmp(name, SHOWDUINO_SRV1_TYPE_SNAP_RSP) == 0) return SHOWDUINO_SRV1_SNAP_RSP;
  if (strcmp(name, SHOWDUINO_SRV1_TYPE_ERROR) == 0) return SHOWDUINO_SRV1_ERROR;
  return SHOWDUINO_SRV1_UNKNOWN;
}

static inline const char *showduino_srv1_type_name(ShowduinoSrv1Type t) {
  switch (t) {
    case SHOWDUINO_SRV1_SNAP_REQ: return SHOWDUINO_SRV1_TYPE_SNAP_REQ;
    case SHOWDUINO_SRV1_SNAP_RSP: return SHOWDUINO_SRV1_TYPE_SNAP_RSP;
    case SHOWDUINO_SRV1_ERROR:    return SHOWDUINO_SRV1_TYPE_ERROR;
    default:                      return "UNKNOWN";
  }
}

static inline int showduino_srv1_is_prefix_line(const char *line) {
  return line && strncmp(line, SHOWDUINO_SRV1_PREFIX, SHOWDUINO_SRV1_PREFIX_LEN) == 0;
}

static inline int showduino_srv1_parse_header(const char *line, ShowduinoSrv1Header *out) {
  char typeBuf[SHOWDUINO_SRV1_TYPE_MAX];
  char reqBuf[SHOWDUINO_SRV1_REQ_ID_MAX];
  unsigned len = 0;
  unsigned crc = 0;
  int n;

  if (!line || !out) return 0;
  memset(out, 0, sizeof(*out));

  if (!showduino_srv1_is_prefix_line(line)) return 0;

  n = sscanf(line + SHOWDUINO_SRV1_PREFIX_LEN,
             "%15[^:]:%15[^:]:%u:%x",
             typeBuf, reqBuf, &len, &crc);
  if (n != 4) return 0;
  if (len > SHOWDUINO_SRV1_PAYLOAD_MAX) return 0;
  if (crc > 0xFFFFu) return 0;

  strncpy(out->typeName, typeBuf, sizeof(out->typeName) - 1);
  strncpy(out->reqId, reqBuf, sizeof(out->reqId) - 1);
  out->payloadLen = (uint16_t)len;
  out->crc16 = (uint16_t)crc;
  out->type = showduino_srv1_type_from_name(out->typeName);
  if (out->type == SHOWDUINO_SRV1_UNKNOWN) return 0;
  return 1;
}

static inline int showduino_srv1_payload_ok(const ShowduinoSrv1Header *hdr,
                                           const void *payload) {
  uint16_t got;
  if (!hdr) return 0;
  if (hdr->payloadLen == 0) return hdr->crc16 == 0;
  if (!payload) return 0;
  got = showduino_srv1_crc16(payload, hdr->payloadLen);
  return got == hdr->crc16;
}

static inline int showduino_srv1_format_header(char *out, size_t outCap,
                                              ShowduinoSrv1Type type,
                                              const char *reqId,
                                              uint16_t payloadLen,
                                              uint16_t crc16) {
  int n;
  if (!out || outCap < 8 || !reqId || reqId[0] == '\0') return -1;
  if (payloadLen > SHOWDUINO_SRV1_PAYLOAD_MAX) return -1;
  if (type == SHOWDUINO_SRV1_UNKNOWN) return -1;
  n = snprintf(out, outCap, "%s%s:%s:%u:%04X",
               SHOWDUINO_SRV1_PREFIX, showduino_srv1_type_name(type), reqId,
               (unsigned)payloadLen, (unsigned)crc16);
  if (n < 0 || (size_t)n >= outCap) return -1;
  return n;
}

static inline int showduino_srv1_encode_frame(char *outBuf, size_t outCap,
                                             ShowduinoSrv1Type type,
                                             const char *reqId,
                                             const void *payload,
                                             uint16_t payloadLen) {
  char header[SHOWDUINO_SRV1_HEADER_MAX];
  uint16_t crc;
  int hlen;
  size_t total;

  if (!outBuf || !reqId) return -1;
  if (payloadLen > SHOWDUINO_SRV1_PAYLOAD_MAX) return -1;
  if (payloadLen > 0 && !payload) return -1;

  crc = showduino_srv1_crc16(payload, payloadLen);
  hlen = showduino_srv1_format_header(header, sizeof(header), type, reqId, payloadLen, crc);
  if (hlen < 0) return -1;

  total = (size_t)hlen + 1u + (size_t)payloadLen + 1u;
  if (total > outCap) return -1;

  memcpy(outBuf, header, (size_t)hlen);
  outBuf[hlen] = '\n';
  if (payloadLen > 0) {
    memcpy(outBuf + hlen + 1, payload, payloadLen);
  }
  outBuf[hlen + 1 + payloadLen] = '\n';
  return (int)total;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_STAGE_RUNTIME_H */
