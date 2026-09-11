#ifndef SHOWDUINO_PIXEL_NODE_H
#define SHOWDUINO_PIXEL_NODE_H

/*
 * Host-testable C3 Pixel Node identity, command class, and ownership gates.
 * No Arduino, NeoPixel, OLED, or ESP-NOW side effects.
 *
 * The Pixel Node is the remote equivalent of the P4 GPIO23 Show Pixel Line.
 * Same vocabulary: PIXEL LINE → SEGMENTS → EFFECTS → PARAMETERS.
 * Shared FX enum lives in showduino_pixel_fx.h.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "showduino_node_ownership.h"
#include "showduino_legacy_strings.h"
#include "showduino_protocol_version.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_PIXEL_NODE_TYPE        "PIXEL"
#define SHOWDUINO_PIXEL_NODE_NAME        "C3 Pixel Node"
#define SHOWDUINO_PIXEL_PROTOCOL         "1.0"
#define SHOWDUINO_PIXEL_ID_MAX           12
#define SHOWDUINO_PIXEL_NAME_MAX         20
#define SHOWDUINO_PIXEL_NODE_MAX_NODES   8
#define SHOWDUINO_PIXEL_NODE_MAX_PIXELS  512
#define SHOWDUINO_PIXEL_DEFAULT_SUGGEST  100
#define SHOWDUINO_PIXEL_MAX_SEGMENTS     16
#define SHOWDUINO_PIXEL_LOCATE_MS        4000u
#define SHOWDUINO_PIXEL_COMMS_TIMEOUT_MS 5000u
#define SHOWDUINO_PIXEL_FRAME_MS         20u
#define SHOWDUINO_PIXEL_CAPS \
  "LINE,SEGMENTS,SHOWDUINO_FX,COUNT,INIT,LOCATE,OLED,ESPNOW,EMERGENCY,STANDALONE,OWN"

typedef enum ShowduinoPixelNodeState {
  SHOWDUINO_PIXEL_ST_UNKNOWN = 0,
  SHOWDUINO_PIXEL_ST_BOOTING,
  SHOWDUINO_PIXEL_ST_SEARCHING,
  SHOWDUINO_PIXEL_ST_STANDALONE,
  SHOWDUINO_PIXEL_ST_SHOW_CONTROLLED,
  SHOWDUINO_PIXEL_ST_UNINIT,
  SHOWDUINO_PIXEL_ST_EMERGENCY,
  SHOWDUINO_PIXEL_ST_LOCATE,
  SHOWDUINO_PIXEL_ST_FAULT
} ShowduinoPixelNodeState;

typedef enum ShowduinoPixelCmd {
  SHOWDUINO_PIXEL_CMD_NONE = 0,
  SHOWDUINO_PIXEL_CMD_STATUS,
  SHOWDUINO_PIXEL_CMD_COUNT,
  SHOWDUINO_PIXEL_CMD_INIT,
  SHOWDUINO_PIXEL_CMD_TEST,
  SHOWDUINO_PIXEL_CMD_TEST_STOP,
  SHOWDUINO_PIXEL_CMD_OFF,
  SHOWDUINO_PIXEL_CMD_BLACKOUT,
  SHOWDUINO_PIXEL_CMD_SOLID,
  SHOWDUINO_PIXEL_CMD_BRIGHTNESS,
  SHOWDUINO_PIXEL_CMD_SEGMENT,
  SHOWDUINO_PIXEL_CMD_LOCATE,
  SHOWDUINO_PIXEL_CMD_ID,
  SHOWDUINO_PIXEL_CMD_NAME,
  SHOWDUINO_PIXEL_CMD_OWN_GRANT,
  SHOWDUINO_PIXEL_CMD_EMERGENCY_STOP,
  SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR
} ShowduinoPixelCmd;

typedef enum ShowduinoPixelFail {
  SHOWDUINO_PIXEL_FAIL_NONE = 0,
  SHOWDUINO_PIXEL_FAIL_EMERGENCY,
  SHOWDUINO_PIXEL_FAIL_BAD_COMMAND,
  SHOWDUINO_PIXEL_FAIL_BAD_ID,
  SHOWDUINO_PIXEL_FAIL_BAD_COUNT,
  SHOWDUINO_PIXEL_FAIL_NOT_INITIALISED,
  SHOWDUINO_PIXEL_FAIL_COMMS_TIMEOUT,
  SHOWDUINO_PIXEL_FAIL_SHOW_CONTROLLED,
  SHOWDUINO_PIXEL_FAIL_NOT_OWNER,
  SHOWDUINO_PIXEL_FAIL_OFFLINE
} ShowduinoPixelFail;

typedef struct ShowduinoPixelAnnounce {
  char mac[18];
  char firmware[12];
  char state[20];
  char id[SHOWDUINO_PIXEL_ID_MAX + 1];
  char name[SHOWDUINO_PIXEL_NAME_MAX + 1];
  uint16_t pixelCount;
  uint8_t initialised;
  uint8_t segments;
} ShowduinoPixelAnnounce;

typedef struct ShowduinoPixelRoute {
  char id[SHOWDUINO_PIXEL_ID_MAX + 1];
  uint32_t sequence;
  const char *command;
} ShowduinoPixelRoute;

static inline const char *showduino_pixel_state_name(ShowduinoPixelNodeState st) {
  switch (st) {
    case SHOWDUINO_PIXEL_ST_BOOTING: return "BOOTING";
    case SHOWDUINO_PIXEL_ST_SEARCHING: return "SEARCHING";
    case SHOWDUINO_PIXEL_ST_STANDALONE: return "STANDALONE";
    case SHOWDUINO_PIXEL_ST_SHOW_CONTROLLED: return "SHOW_CONTROLLED";
    case SHOWDUINO_PIXEL_ST_UNINIT: return "UNINIT";
    case SHOWDUINO_PIXEL_ST_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_PIXEL_ST_LOCATE: return "LOCATE";
    case SHOWDUINO_PIXEL_ST_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

static inline ShowduinoPixelNodeState showduino_pixel_state_from_owner(
    ShowduinoNodeOwnerMode m) {
  switch (m) {
    case SHOWDUINO_OWNER_BOOTING: return SHOWDUINO_PIXEL_ST_BOOTING;
    case SHOWDUINO_OWNER_SEARCHING: return SHOWDUINO_PIXEL_ST_SEARCHING;
    case SHOWDUINO_OWNER_STANDALONE: return SHOWDUINO_PIXEL_ST_STANDALONE;
    case SHOWDUINO_OWNER_SHOW_CONTROLLED: return SHOWDUINO_PIXEL_ST_SHOW_CONTROLLED;
    case SHOWDUINO_OWNER_EMERGENCY: return SHOWDUINO_PIXEL_ST_EMERGENCY;
    case SHOWDUINO_OWNER_FAULT: return SHOWDUINO_PIXEL_ST_FAULT;
    default: return SHOWDUINO_PIXEL_ST_UNKNOWN;
  }
}

static inline const char *showduino_pixel_fail_name(ShowduinoPixelFail f) {
  switch (f) {
    case SHOWDUINO_PIXEL_FAIL_NONE: return "NONE";
    case SHOWDUINO_PIXEL_FAIL_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_PIXEL_FAIL_BAD_COMMAND: return "BAD_COMMAND";
    case SHOWDUINO_PIXEL_FAIL_BAD_ID: return "BAD_ID";
    case SHOWDUINO_PIXEL_FAIL_BAD_COUNT: return "BAD_COUNT";
    case SHOWDUINO_PIXEL_FAIL_NOT_INITIALISED: return "NOT_INITIALISED";
    case SHOWDUINO_PIXEL_FAIL_COMMS_TIMEOUT: return "COMMS_TIMEOUT";
    case SHOWDUINO_PIXEL_FAIL_SHOW_CONTROLLED: return "SHOW_CONTROLLED";
    case SHOWDUINO_PIXEL_FAIL_NOT_OWNER: return "NOT_OWNER";
    case SHOWDUINO_PIXEL_FAIL_OFFLINE: return "OFFLINE";
    default: return "FAULT";
  }
}

/* Logical IDs: LED-01 style. 2–12 chars, start with letter, A-Z a-z 0-9 _ - */
static inline int showduino_pixel_id_ok(const char *id) {
  size_t n;
  const char *p;
  if (!id || !id[0]) return 0;
  n = strlen(id);
  if (n < 2 || n > SHOWDUINO_PIXEL_ID_MAX) return 0;
  if (!isalpha((unsigned char)id[0])) return 0;
  for (p = id; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '_' || c == '-')) return 0;
  }
  return 1;
}

static inline int showduino_pixel_name_ok(const char *name) {
  size_t n;
  const char *p;
  if (!name || !name[0]) return 0;
  n = strlen(name);
  if (n > SHOWDUINO_PIXEL_NAME_MAX) return 0;
  for (p = name; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c < 32 || c > 126) return 0;
    if (c == ':' || c == '"' || c == '\\') return 0;
  }
  return 1;
}

static inline int showduino_pixel_count_ok(uint32_t count, uint16_t maxPixels) {
  return count >= 1u && count <= (uint32_t)maxPixels;
}

static inline int showduino_pixel_id_equal(const char *a, const char *b) {
  if (!a || !b) return 0;
  while (*a && *b) {
    char ca = (char)toupper((unsigned char)*a++);
    char cb = (char)toupper((unsigned char)*b++);
    if (ca != cb) return 0;
  }
  return *a == 0 && *b == 0;
}

/* PIXEL:NODE:<id>:<rest> → rest, copies id. Returns rest or NULL. */
static inline const char *showduino_pixel_strip_node_prefix(const char *cmd,
                                                            char *idOut, size_t idLen) {
  const char *p;
  const char *colon;
  size_t n;
  if (!cmd) return NULL;
  if (strncmp(cmd, "PIXEL:NODE:", 11) != 0) return cmd;
  p = cmd + 11;
  colon = strchr(p, ':');
  if (!colon || colon == p) return NULL;
  n = (size_t)(colon - p);
  if (n > SHOWDUINO_PIXEL_ID_MAX) return NULL;
  if (idOut && idLen) {
    if (n >= idLen) n = idLen - 1;
    memcpy(idOut, p, n);
    idOut[n] = '\0';
    if (!showduino_pixel_id_ok(idOut)) return NULL;
  }
  return colon + 1;
}

static inline int showduino_pixel_cmd_requires_init(ShowduinoPixelCmd cmd) {
  switch (cmd) {
    case SHOWDUINO_PIXEL_CMD_TEST:
    case SHOWDUINO_PIXEL_CMD_TEST_STOP:
    case SHOWDUINO_PIXEL_CMD_OFF:
    case SHOWDUINO_PIXEL_CMD_BLACKOUT:
    case SHOWDUINO_PIXEL_CMD_SOLID:
    case SHOWDUINO_PIXEL_CMD_BRIGHTNESS:
    case SHOWDUINO_PIXEL_CMD_SEGMENT:
    case SHOWDUINO_PIXEL_CMD_LOCATE:
      return 1;
    default:
      return 0;
  }
}

static inline int showduino_pixel_cmd_theatrical(ShowduinoPixelCmd cmd) {
  return showduino_pixel_cmd_requires_init(cmd) ||
         cmd == SHOWDUINO_PIXEL_CMD_COUNT ||
         cmd == SHOWDUINO_PIXEL_CMD_INIT;
}

static inline ShowduinoPixelCmd showduino_pixel_classify_command(const char *raw) {
  char id[SHOWDUINO_PIXEL_ID_MAX + 1];
  const char *cmd;
  if (!raw || !raw[0]) return SHOWDUINO_PIXEL_CMD_NONE;
  if (strcmp(raw, "EMERGENCY:STOP") == 0) return SHOWDUINO_PIXEL_CMD_EMERGENCY_STOP;
  if (strcmp(raw, "EMERGENCY:CLEAR") == 0) return SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR;
  if (strcmp(raw, "OWN:GRANT") == 0 ||
      strcmp(raw, "PIXEL:OWN:GRANT") == 0 ||
      strcmp(raw, "PIXEL:NODE:OWN:GRANT") == 0) {
    return SHOWDUINO_PIXEL_CMD_OWN_GRANT;
  }
  cmd = showduino_pixel_strip_node_prefix(raw, id, sizeof(id));
  if (!cmd) return SHOWDUINO_PIXEL_CMD_NONE;
  if (strcmp(cmd, "OWN:GRANT") == 0 || strcmp(cmd, "PIXEL:OWN:GRANT") == 0) {
    return SHOWDUINO_PIXEL_CMD_OWN_GRANT;
  }
  if (strncmp(cmd, "PIXEL:", 6) == 0) cmd += 6;
  if (strcmp(cmd, "STATUS") == 0) return SHOWDUINO_PIXEL_CMD_STATUS;
  if (strcmp(cmd, "INIT") == 0) return SHOWDUINO_PIXEL_CMD_INIT;
  if (strcmp(cmd, "TEST") == 0) return SHOWDUINO_PIXEL_CMD_TEST;
  if (strcmp(cmd, "TEST:STOP") == 0) return SHOWDUINO_PIXEL_CMD_TEST_STOP;
  if (strcmp(cmd, "OFF") == 0) return SHOWDUINO_PIXEL_CMD_OFF;
  if (strcmp(cmd, "BLACKOUT") == 0) return SHOWDUINO_PIXEL_CMD_BLACKOUT;
  if (strcmp(cmd, "LOCATE") == 0) return SHOWDUINO_PIXEL_CMD_LOCATE;
  if (strncmp(cmd, "COUNT:", 6) == 0) return SHOWDUINO_PIXEL_CMD_COUNT;
  if (strncmp(cmd, "SOLID:", 6) == 0) return SHOWDUINO_PIXEL_CMD_SOLID;
  if (strncmp(cmd, "BRIGHTNESS:", 11) == 0) return SHOWDUINO_PIXEL_CMD_BRIGHTNESS;
  if (strncmp(cmd, "SEGMENT:", 8) == 0) return SHOWDUINO_PIXEL_CMD_SEGMENT;
  if (strncmp(cmd, "ID:", 3) == 0) return SHOWDUINO_PIXEL_CMD_ID;
  if (strncmp(cmd, "NAME:", 5) == 0) return SHOWDUINO_PIXEL_CMD_NAME;
  return SHOWDUINO_PIXEL_CMD_NONE;
}

static inline ShowduinoPixelFail showduino_pixel_gate_init(
    int initialised, ShowduinoPixelCmd cmd) {
  if (!initialised && showduino_pixel_cmd_requires_init(cmd)) {
    return SHOWDUINO_PIXEL_FAIL_NOT_INITIALISED;
  }
  return SHOWDUINO_PIXEL_FAIL_NONE;
}

static inline ShowduinoPixelFail showduino_pixel_can_accept_ex(
    ShowduinoPixelNodeState st, ShowduinoPixelCmd cmd, ShowduinoCmdOrigin origin,
    int initialised) {
  ShowduinoPixelFail initFail;
  if (cmd == SHOWDUINO_PIXEL_CMD_NONE) return SHOWDUINO_PIXEL_FAIL_BAD_COMMAND;
  if (cmd == SHOWDUINO_PIXEL_CMD_EMERGENCY_STOP) return SHOWDUINO_PIXEL_FAIL_NONE;
  if (cmd == SHOWDUINO_PIXEL_CMD_STATUS) return SHOWDUINO_PIXEL_FAIL_NONE;
  if (cmd == SHOWDUINO_PIXEL_CMD_OWN_GRANT) {
    return origin == SHOWDUINO_CMD_ORIGIN_SHOW ? SHOWDUINO_PIXEL_FAIL_NONE
                                               : SHOWDUINO_PIXEL_FAIL_NOT_OWNER;
  }
  if (st == SHOWDUINO_PIXEL_ST_EMERGENCY) {
    if (cmd == SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR) {
      return origin == SHOWDUINO_CMD_ORIGIN_SHOW ? SHOWDUINO_PIXEL_FAIL_NONE
                                                 : SHOWDUINO_PIXEL_FAIL_EMERGENCY;
    }
    return SHOWDUINO_PIXEL_FAIL_EMERGENCY;
  }
  if (cmd == SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR) {
    if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) return SHOWDUINO_PIXEL_FAIL_NONE;
    return SHOWDUINO_PIXEL_FAIL_SHOW_CONTROLLED;
  }
  if (cmd == SHOWDUINO_PIXEL_CMD_ID || cmd == SHOWDUINO_PIXEL_CMD_NAME ||
      cmd == SHOWDUINO_PIXEL_CMD_COUNT || cmd == SHOWDUINO_PIXEL_CMD_INIT) {
    if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) return SHOWDUINO_PIXEL_FAIL_NONE;
    if (st == SHOWDUINO_PIXEL_ST_SHOW_CONTROLLED) {
      return SHOWDUINO_PIXEL_FAIL_SHOW_CONTROLLED;
    }
    return SHOWDUINO_PIXEL_FAIL_NONE;
  }
  initFail = showduino_pixel_gate_init(initialised, cmd);
  if (initFail != SHOWDUINO_PIXEL_FAIL_NONE) return initFail;
  if (showduino_pixel_cmd_theatrical(cmd)) {
    if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) {
      return (st == SHOWDUINO_PIXEL_ST_SHOW_CONTROLLED)
                 ? SHOWDUINO_PIXEL_FAIL_NONE
                 : SHOWDUINO_PIXEL_FAIL_NOT_OWNER;
    }
    if (st == SHOWDUINO_PIXEL_ST_SHOW_CONTROLLED) {
      return SHOWDUINO_PIXEL_FAIL_SHOW_CONTROLLED;
    }
    return SHOWDUINO_PIXEL_FAIL_NONE;
  }
  return SHOWDUINO_PIXEL_FAIL_NONE;
}

/* Locate is always below emergency. */
static inline int showduino_pixel_locate_allowed(int emergencyActive, int initialised) {
  return !emergencyActive && initialised;
}

/*
 * ANNOUNCE:<mac>:<fw>:<state>:ID=<id>:N=<name>:P=<count>:I=<0|1>[:S=<segs>]
 * MAC is AA:BB:CC:DD:EE:FF (17 chars).
 */
static inline int showduino_pixel_parse_announce(const char *line,
                                                 ShowduinoPixelAnnounce *out) {
  const char *p;
  const char *colon;
  size_t n;
  if (!line || !out) return 0;
  memset(out, 0, sizeof(*out));
  if (strncmp(line, "ANNOUNCE:", 9) != 0) return 0;
  p = line + 9;
  if (strlen(p) < 17) return 0;
  memcpy(out->mac, p, 17);
  out->mac[17] = '\0';
  p += 17;
  if (*p != ':') return 0;
  p++;
  colon = strchr(p, ':');
  if (!colon) return 0;
  n = (size_t)(colon - p);
  if (n >= sizeof(out->firmware)) n = sizeof(out->firmware) - 1;
  memcpy(out->firmware, p, n);
  p = colon + 1;
  colon = strchr(p, ':');
  if (!colon) {
    strncpy(out->state, p, sizeof(out->state) - 1);
    return 1;
  }
  n = (size_t)(colon - p);
  if (n >= sizeof(out->state)) n = sizeof(out->state) - 1;
  memcpy(out->state, p, n);
  p = colon + 1;
  while (p && *p) {
    if (!strncmp(p, "ID=", 3)) {
      p += 3;
      colon = strchr(p, ':');
      n = colon ? (size_t)(colon - p) : strlen(p);
      if (n > SHOWDUINO_PIXEL_ID_MAX) n = SHOWDUINO_PIXEL_ID_MAX;
      memcpy(out->id, p, n);
      out->id[n] = '\0';
      p = colon ? colon + 1 : NULL;
    } else if (!strncmp(p, "N=", 2)) {
      p += 2;
      colon = strchr(p, ':');
      n = colon ? (size_t)(colon - p) : strlen(p);
      if (n > SHOWDUINO_PIXEL_NAME_MAX) n = SHOWDUINO_PIXEL_NAME_MAX;
      memcpy(out->name, p, n);
      out->name[n] = '\0';
      p = colon ? colon + 1 : NULL;
    } else if (!strncmp(p, "P=", 2)) {
      out->pixelCount = (uint16_t)strtoul(p + 2, NULL, 10);
      colon = strchr(p, ':');
      p = colon ? colon + 1 : NULL;
    } else if (!strncmp(p, "I=", 2)) {
      out->initialised = (uint8_t)(p[2] == '1' ? 1 : 0);
      colon = strchr(p, ':');
      p = colon ? colon + 1 : NULL;
    } else if (!strncmp(p, "S=", 2)) {
      out->segments = (uint8_t)strtoul(p + 2, NULL, 10);
      colon = strchr(p, ':');
      p = colon ? colon + 1 : NULL;
    } else {
      colon = strchr(p, ':');
      p = colon ? colon + 1 : NULL;
    }
  }
  return 1;
}

/* ROUTE:PIXEL:<id>:<seq>:<command> */
static inline int showduino_pixel_parse_route(const char *line,
                                              ShowduinoPixelRoute *out) {
  const char *p;
  const char *c1;
  const char *c2;
  size_t n;
  if (!line || !out) return 0;
  memset(out, 0, sizeof(*out));
  if (strncmp(line, SHOWDUINO_LEGACY_ROUTE_PIXEL,
              strlen(SHOWDUINO_LEGACY_ROUTE_PIXEL)) != 0) {
    return 0;
  }
  p = line + strlen(SHOWDUINO_LEGACY_ROUTE_PIXEL);
  c1 = strchr(p, ':');
  if (!c1 || c1 == p) return 0;
  n = (size_t)(c1 - p);
  if (n > SHOWDUINO_PIXEL_ID_MAX) return 0;
  memcpy(out->id, p, n);
  out->id[n] = '\0';
  if (!showduino_pixel_id_ok(out->id)) return 0;
  c2 = strchr(c1 + 1, ':');
  if (!c2) return 0;
  out->sequence = (uint32_t)strtoul(c1 + 1, NULL, 10);
  out->command = c2 + 1;
  return out->command[0] ? 1 : 0;
}

static inline int showduino_pixel_segment_id_ok(int id) {
  return id >= 0 && id < SHOWDUINO_PIXEL_MAX_SEGMENTS;
}

/* Overlap is allowed. Later slot IDs win. No reject. */
static inline int showduino_pixel_range_ok(uint16_t start, uint16_t count,
                                           uint16_t lineCount) {
  if (!lineCount || !count) return 0;
  if (start >= lineCount) return 0;
  if ((uint32_t)start + count > lineCount) return 0;
  return 1;
}

/* Fit ANNOUNCE into the 96-byte node command field. Friendly name truncated. */
static inline int showduino_pixel_format_announce(
    char *out, size_t n, const char *mac, const char *fw, const char *state,
    const char *id, const char *name, uint16_t pix, int init, uint8_t segs) {
  char nbuf[9];
  size_t i = 0;
  int w;
  if (!out || n < 16) return 0;
  if (!name) name = "";
  for (; i < 8 && name[i]; ++i) nbuf[i] = name[i];
  nbuf[i] = '\0';
  w = snprintf(out, n, "ANNOUNCE:%s:%s:%s:ID=%s:N=%s:P=%u:I=%u:S=%u",
               mac && mac[0] ? mac : "00:00:00:00:00:00",
               fw && fw[0] ? fw : "0.0.0",
               state && state[0] ? state : "UNKNOWN",
               id && id[0] ? id : "LED-00",
               nbuf,
               (unsigned)pix,
               init ? 1 : 0,
               (unsigned)segs);
  if (w <= 0 || (size_t)w >= n) return 0;
  if ((size_t)w >= SHOWDUINO_NODE_COMMAND_MAX) return 0;
  return 1;
}

/* P4 reply when a production targets a Pixel Node that is not online.
 * Timeline continues; this is not a crash/freeze path. */
static inline int showduino_pixel_format_offline(char *out, size_t n, const char *id) {
  if (!out || n < 8) return 0;
  return snprintf(out, n, "REJECTED:PIXEL:OFFLINE:%s",
                  (id && id[0]) ? id : "-") > 0;
}

/* PIXEL:NODE:<id>:<rest> → PIXEL:<rest> unless rest already PIXEL:/EMERGENCY:/OWN: */
static inline int showduino_pixel_inner_command(const char *raw, char *idOut,
                                                size_t idLen, char *out, size_t n) {
  const char *rest;
  if (!raw || !out || n < 8) return 0;
  rest = showduino_pixel_strip_node_prefix(raw, idOut, idLen);
  if (!rest || !rest[0]) return 0;
  if (!strncmp(rest, "PIXEL:", 6) || !strncmp(rest, "EMERGENCY:", 10) ||
      !strcmp(rest, "OWN:GRANT")) {
    if (strlen(rest) >= n) return 0;
    memcpy(out, rest, strlen(rest) + 1);
    return 1;
  }
  if (snprintf(out, n, "PIXEL:%s", rest) <= 0) return 0;
  if (strlen(out) >= n) return 0;
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_PIXEL_NODE_H */
