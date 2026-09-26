#ifndef SHOWDUINO_WEB_TUNNEL_H
#define SHOWDUINO_WEB_TUNNEL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Showduino Studio — HTTP tunnel over Communications Engine ↔ P4 UART.
 *
 * Does NOT modify ESP-NOW desk/node packet sizes or colon-text show commands.
 * Distinct prefix so line-oriented parsers can ignore tunnel traffic until framed.
 *
 * Request (Comms → P4, newline-terminated ASCII):
 *   WEB/GET/api/system
 *   WEB/GET/api/devices
 *   WEB/GET/api/logs
 *   WEB/POST/api/command/SHOW:START
 *
 * Body-bearing requests (preferred for JSON command envelopes):
 *   WEB/BODY:<len>:<METHOD>:<path>\n
 *   <len bytes>
 *   See protocol/showduino_deploy.h for deploy chunk helpers.
 *
 * (Legacy double-slash WEB/GET//api/... is also accepted on P4.)
 *
 * Response (P4 → Comms):
 *   WEBR:<status>:<bodyLen>[:<mime>]\n
 *   <bodyLen bytes — length-delimited, may contain newlines>
 *
 * Canonical host: Communications S3 SoftAP serves PROGMEM Studio assets and
 * proxies HTTP /api routes to the P4 over this UART tunnel. P4 remains API authority.
 * One in-flight tunnel exchange at a time on the Comms HTTP server.
 * Bodies are length-prefixed up to BODY_MAX (~2 s @ 115200 baud worst case).
 * Receivers must pump UART / emergency service while waiting; do not block the
 * show engine on browser connectivity. Onboard C6 is unused reserved hardware.
 *
 * Browser command contract (SoftAP same-origin):
 *   POST /api/command
 *   Content-Type: application/json
 *   Body: { "cmd": "SHOW:START", "requestId": "<optional opaque id>" }
 *   Also accepted (Studio compatibility):
 *     { "category":"show","action":"start" }
 *     { "category":"emergency","action":"stop" }
 *     { "cmd":"...", "source","destination","priority","payload" } — extra
 *       fields are ignored for transport; authoritative wire verb is `cmd`
 *       after normalizeShowCommand mapping.
 *
 * Response (authoritative fields from P4; Comms may add transport errors):
 *   {
 *     "ok": true|false,
 *     "cmd": "SHOW:START",
 *     "requestId": "...",
 *     "lifecycle": "accepted"|"rejected"|"duplicate",
 *     "duplicate": false,
 *     "replies": "<P4 desk text>",
 *     "showState": "...",
 *     "emergencyActive": false,
 *     "note": "..."
 *   }
 * lifecycle "accepted" means the Show Engine validated and dispatched the
 * request. It is NOT physical completion. Confirmed state comes from
 * GET /api/show and GET /api/system polling.
 *
 * Remote emergency CLEAR is not exposed on the browser surface by default.
 */

#define SHOWDUINO_WEB_TUNNEL_REQ_PREFIX "WEB/"
#define SHOWDUINO_WEB_TUNNEL_RESP_PREFIX "WEBR:"

#define SHOWDUINO_WEB_TUNNEL_BODY_MAX 24576u

#define SHOWDUINO_WEB_REQID_MAX 40u
#define SHOWDUINO_WEB_DUP_SLOTS 16u

typedef struct ShowduinoWebrHeader {
  int status;
  size_t bodyLen;
  char mime[48];
  int truncated; /* 1 if bodyLen was capped to BODY_MAX */
} ShowduinoWebrHeader;

#ifdef __cplusplus
extern "C" {
#endif

/* Parse WEBR:<status>:<bodyLen>[:<mime>]. Returns 1 on success. */
static inline int showduino_webr_parse_header(const char *line,
                                             ShowduinoWebrHeader *out) {
  size_t i;
  const char *p;
  int status;
  size_t bodyLen;
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WEB_TUNNEL_RESP_PREFIX,
              strlen(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX)) != 0) {
    return 0;
  }
  p = line + strlen(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX);
  status = 0;
  if (*p < '0' || *p > '9') return 0;
  while (*p >= '0' && *p <= '9') {
    status = status * 10 + (*p - '0');
    p++;
  }
  if (*p != ':') return 0;
  p++;
  bodyLen = 0;
  if (*p < '0' || *p > '9') return 0;
  while (*p >= '0' && *p <= '9') {
    bodyLen = bodyLen * 10u + (size_t)(*p - '0');
    p++;
  }
  out->status = status;
  out->truncated = 0;
  out->mime[0] = '\0';
  if (*p == ':') {
    p++;
    i = 0;
    while (*p && *p != '\r' && *p != '\n' && i + 1 < sizeof(out->mime)) {
      out->mime[i++] = *p++;
    }
    out->mime[i] = '\0';
  } else if (*p != '\0' && *p != '\r' && *p != '\n') {
    return 0;
  }
  if (bodyLen > SHOWDUINO_WEB_TUNNEL_BODY_MAX) {
    bodyLen = SHOWDUINO_WEB_TUNNEL_BODY_MAX;
    out->truncated = 1;
  }
  out->bodyLen = bodyLen;
  return 1;
}

/* Extract a JSON string field "key":"value" (first match). */
static inline int showduino_web_json_string_field(const char *json, const char *key,
                                                 char *out, size_t outLen) {
  char needle[48];
  const char *p;
  const char *q1;
  const char *q2;
  size_t n;
  size_t i;
  if (!json || !key || !out || outLen < 2) return 0;
  if (strlen(key) + 3 >= sizeof(needle)) return 0;
  needle[0] = '"';
  memcpy(needle + 1, key, strlen(key));
  needle[1 + strlen(key)] = '"';
  needle[2 + strlen(key)] = '\0';
  p = strstr(json, needle);
  if (!p) return 0;
  p = strchr(p + strlen(needle), ':');
  if (!p) return 0;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  if (*p != '"') return 0;
  q1 = p + 1;
  q2 = strchr(q1, '"');
  if (!q2 || q2 <= q1) return 0;
  n = (size_t)(q2 - q1);
  if (n >= outLen) n = outLen - 1;
  for (i = 0; i < n; i++) out[i] = q1[i];
  out[n] = '\0';
  return n > 0 ? 1 : 0;
}

typedef struct ShowduinoWebDupSlot {
  char id[SHOWDUINO_WEB_REQID_MAX];
  char resultJson[192];
  int httpStatus;
  uint8_t used;
} ShowduinoWebDupSlot;

typedef struct ShowduinoWebDupRing {
  ShowduinoWebDupSlot slots[SHOWDUINO_WEB_DUP_SLOTS];
  uint8_t next;
} ShowduinoWebDupRing;

static inline void showduino_web_dup_init(ShowduinoWebDupRing *ring) {
  size_t i;
  if (!ring) return;
  memset(ring, 0, sizeof(*ring));
  for (i = 0; i < SHOWDUINO_WEB_DUP_SLOTS; i++) ring->slots[i].used = 0;
  ring->next = 0;
}

static inline int showduino_web_dup_find(const ShowduinoWebDupRing *ring,
                                        const char *requestId,
                                        const char **jsonOut, int *statusOut) {
  size_t i;
  if (!ring || !requestId || !requestId[0]) return 0;
  for (i = 0; i < SHOWDUINO_WEB_DUP_SLOTS; i++) {
    if (!ring->slots[i].used) continue;
    if (strncmp(ring->slots[i].id, requestId, SHOWDUINO_WEB_REQID_MAX) == 0) {
      if (jsonOut) *jsonOut = ring->slots[i].resultJson;
      if (statusOut) *statusOut = ring->slots[i].httpStatus;
      return 1;
    }
  }
  return 0;
}

static inline void showduino_web_dup_remember(ShowduinoWebDupRing *ring,
                                             const char *requestId,
                                             int httpStatus,
                                             const char *resultJson) {
  ShowduinoWebDupSlot *slot;
  if (!ring || !requestId || !requestId[0] || !resultJson) return;
  slot = &ring->slots[ring->next % SHOWDUINO_WEB_DUP_SLOTS];
  ring->next = (uint8_t)((ring->next + 1u) % SHOWDUINO_WEB_DUP_SLOTS);
  memset(slot, 0, sizeof(*slot));
  strncpy(slot->id, requestId, SHOWDUINO_WEB_REQID_MAX - 1);
  strncpy(slot->resultJson, resultJson, sizeof(slot->resultJson) - 1);
  slot->httpStatus = httpStatus;
  slot->used = 1;
}

/* Validate requestId charset: A-Z a-z 0-9 _ - . : max REQID_MAX-1. */
static inline int showduino_web_request_id_ok(const char *id) {
  size_t i;
  if (!id || !id[0]) return 0;
  for (i = 0; id[i] && i < SHOWDUINO_WEB_REQID_MAX; i++) {
    const char c = id[i];
    const int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
                   c == ':';
    if (!ok) return 0;
  }
  return id[i] == '\0';
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_WEB_TUNNEL_H */
