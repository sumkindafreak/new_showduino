#ifndef SHOWDUINO_DEPLOY_H
#define SHOWDUINO_DEPLOY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/*
 * Studio → Communications S3 → UART → P4 production deploy framing.
 *
 * HTTP stays on the S3. The P4 remains the validator and store.
 * Chunks are small enough for ESP32 WebServer POST bodies and are sent
 * over UART as a length-prefixed WEB/BODY frame so heartbeat/log lines
 * cannot be mistaken for payload bytes.
 *
 * Request (S3 → P4):
 *   WEB/BODY:<len>:<METHOD>:<path>\n
 *   <len bytes>
 *
 * Response remains WEBR:<status>:<bodyLen>[:mime]\n + body.
 */

#define SHOWDUINO_DEPLOY_CHUNK_MAX 1536u
#define SHOWDUINO_SHDO_MAX_BYTES (48u * 1024u)
#define SHOWDUINO_DEPLOY_SESSION_TIMEOUT_MS 30000u
#define SHOWDUINO_DEPLOY_COMMIT_TIMEOUT_MS 15000u
#define SHOWDUINO_WEB_BODY_REQ_PREFIX "WEB/BODY:"
#define SHOWDUINO_DEPLOY_BEGIN_PATH "/api/productions/deploy/begin"
#define SHOWDUINO_DEPLOY_CHUNK_PATH "/api/productions/deploy/chunk"
#define SHOWDUINO_DEPLOY_COMMIT_PATH "/api/productions/deploy/commit"
#define SHOWDUINO_DEPLOY_ABORT_PATH "/api/productions/deploy/abort"
#define SHOWDUINO_DEPLOY_STATUS_PATH "/api/productions/deploy/status"

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t showduino_crc32_ieee(const void *data, size_t len) {
  const unsigned char *p = (const unsigned char *)data;
  uint32_t crc = 0xFFFFFFFFu;
  size_t i, b;
  for (i = 0; i < len; ++i) {
    crc ^= p[i];
    for (b = 0; b < 8; ++b) {
      uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

typedef struct ShowduinoWebBodyHeader {
  size_t length;
  char method[8];
  char path[96];
} ShowduinoWebBodyHeader;

static inline int showduino_web_body_parse_header(const char *line,
                                                  ShowduinoWebBodyHeader *out) {
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WEB_BODY_REQ_PREFIX,
              strlen(SHOWDUINO_WEB_BODY_REQ_PREFIX)) != 0) {
    return 0;
  }
  const char *p = line + strlen(SHOWDUINO_WEB_BODY_REQ_PREFIX);
  size_t length = 0;
  if (*p < '0' || *p > '9') return 0;
  while (*p >= '0' && *p <= '9') {
    length = length * 10u + (size_t)(*p - '0');
    if (length > 65535u) return 0;
    ++p;
  }
  if (*p != ':') return 0;
  ++p;
  size_t methodLen = 0;
  memset(out, 0, sizeof(*out));
  while (*p && *p != ':' && methodLen + 1 < sizeof(out->method)) {
    char c = (char)toupper((unsigned char)*p++);
    if (c < 'A' || c > 'Z') return 0;
    out->method[methodLen++] = c;
  }
  if (*p != ':' || methodLen == 0) return 0;
  ++p;
  if (*p != '/') return 0;
  size_t pathLen = 0;
  while (*p && *p != '\r' && *p != '\n' && pathLen + 1 < sizeof(out->path)) {
    out->path[pathLen++] = *p++;
  }
  if (pathLen == 0 || *p != '\0') return 0;
  out->length = length;
  return 1;
}

static inline int showduino_json_find_key(const char *json, const char *key,
                                          const char **valueOut) {
  if (!json || !key || !valueOut) return 0;
  char needle[48];
  int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
  if (n <= 0 || (size_t)n >= sizeof(needle)) return 0;
  const char *p = strstr(json, needle);
  if (!p) return 0;
  p += (size_t)n;
  while (*p && isspace((unsigned char)*p)) ++p;
  if (*p != ':') return 0;
  ++p;
  while (*p && isspace((unsigned char)*p)) ++p;
  *valueOut = p;
  return 1;
}

static inline int showduino_json_u32_field(const char *json, const char *key,
                                           uint32_t *out) {
  const char *p = NULL;
  if (!out || !showduino_json_find_key(json, key, &p)) return 0;
  if (*p < '0' || *p > '9') return 0;
  uint64_t value = 0;
  while (*p >= '0' && *p <= '9') {
    value = value * 10u + (uint64_t)(*p - '0');
    if (value > 0xFFFFFFFFull) return 0;
    ++p;
  }
  *out = (uint32_t)value;
  return 1;
}

static inline int showduino_json_string_field(const char *json, const char *key,
                                              char *out, size_t outLen) {
  const char *p = NULL;
  if (!out || outLen == 0 || !showduino_json_find_key(json, key, &p)) return 0;
  if (*p != '"') return 0;
  ++p;
  size_t used = 0;
  while (*p && *p != '"') {
    unsigned char c = (unsigned char)*p++;
    if (c == '\\') {
      if (!*p) return 0;
      c = (unsigned char)*p++;
      if (c == 'n' || c == 'r' || c == 't') return 0;
    }
    if (c < 0x20) return 0;
    if (used + 1 >= outLen) return 0;
    out[used++] = (char)c;
  }
  if (*p != '"') return 0;
  out[used] = '\0';
  return 1;
}

static inline int showduino_hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static inline int showduino_hex_decode(const char *hex, unsigned char *out,
                                       size_t outCap, size_t *outLen) {
  if (!hex || !out || !outLen) return 0;
  size_t n = strlen(hex);
  if (n % 2u != 0) return 0;
  size_t bytes = n / 2u;
  if (bytes > outCap) return 0;
  size_t i;
  for (i = 0; i < bytes; ++i) {
    int hi = showduino_hex_nibble(hex[i * 2u]);
    int lo = showduino_hex_nibble(hex[i * 2u + 1u]);
    if (hi < 0 || lo < 0) return 0;
    out[i] = (unsigned char)((hi << 4) | lo);
  }
  *outLen = bytes;
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_DEPLOY_H */
