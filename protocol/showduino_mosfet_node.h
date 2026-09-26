#ifndef SHOWDUINO_MOSFET_NODE_H
#define SHOWDUINO_MOSFET_NODE_H

/*
 * Host-testable MOSFET Node identity, routing, command class and gates.
 * No Arduino / LEDC / ESP-NOW side effects.
 *
 * One physical ESP32_MOS_X4 peer owns four powered outputs (OUT1–OUT4).
 * Fail-safe is ALWAYS all outputs OFF; never restore after authority loss.
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

#define SHOWDUINO_MOSFET_NODE_TYPE        "MOSFET"
#define SHOWDUINO_MOSFET_NODE_NAME        "MOSFET Node"
#define SHOWDUINO_MOSFET_PROTOCOL         "1.0"
#define SHOWDUINO_MOSFET_ID_MAX           12
#define SHOWDUINO_MOSFET_NAME_MAX         20
#define SHOWDUINO_MOSFET_OUT_NAME_MAX     20
#define SHOWDUINO_MOSFET_NODE_MAX_NODES   8
#define SHOWDUINO_MOSFET_OUT_COUNT        4
#define SHOWDUINO_MOSFET_LEVEL_MAX        100u
#define SHOWDUINO_MOSFET_DURATION_MAX_MS  600000u
#define SHOWDUINO_MOSFET_LOCAL_TEST_MS    5000u
#define SHOWDUINO_MOSFET_CAPS \
  "4CH,PWM,PULSE,FADE,OWN,ESPNOW,SOFTAP,EMERGENCY,ALL_OFF"

/* Software pin map for ESP32_MOS_X4 / 303E32NMOS4 — hardware unverified. */
#define SHOWDUINO_MOSFET_OUT1_GPIO_DEFAULT 16
#define SHOWDUINO_MOSFET_OUT2_GPIO_DEFAULT 17
#define SHOWDUINO_MOSFET_OUT3_GPIO_DEFAULT 26
#define SHOWDUINO_MOSFET_OUT4_GPIO_DEFAULT 27
#define SHOWDUINO_MOSFET_STATUS_LED_GPIO_DEFAULT 23
#define SHOWDUINO_MOSFET_GPIO_VERIFIED_DEFAULT 0

typedef enum ShowduinoMosfetNodeState {
  SHOWDUINO_MOSFET_ST_UNKNOWN = 0,
  SHOWDUINO_MOSFET_ST_BOOTING,
  SHOWDUINO_MOSFET_ST_SEARCHING,
  SHOWDUINO_MOSFET_ST_SAFE,
  SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED,
  SHOWDUINO_MOSFET_ST_EMERGENCY,
  SHOWDUINO_MOSFET_ST_FAULT
} ShowduinoMosfetNodeState;

typedef enum ShowduinoMosfetCmd {
  SHOWDUINO_MOSFET_CMD_NONE = 0,
  SHOWDUINO_MOSFET_CMD_STATUS,
  SHOWDUINO_MOSFET_CMD_CAPS,
  SHOWDUINO_MOSFET_CMD_ALL_OFF,
  SHOWDUINO_MOSFET_CMD_OUT_OFF,
  SHOWDUINO_MOSFET_CMD_OUT_ON,
  SHOWDUINO_MOSFET_CMD_OUT_LEVEL,
  SHOWDUINO_MOSFET_CMD_OUT_PULSE,
  SHOWDUINO_MOSFET_CMD_OUT_FADE,
  SHOWDUINO_MOSFET_CMD_OUT_NAME,
  SHOWDUINO_MOSFET_CMD_ID,
  SHOWDUINO_MOSFET_CMD_NAME,
  SHOWDUINO_MOSFET_CMD_OWN_GRANT,
  SHOWDUINO_MOSFET_CMD_EMERGENCY_STOP,
  SHOWDUINO_MOSFET_CMD_EMERGENCY_CLEAR,
  SHOWDUINO_MOSFET_CMD_TEST
} ShowduinoMosfetCmd;

typedef enum ShowduinoMosfetFail {
  SHOWDUINO_MOSFET_FAIL_NONE = 0,
  SHOWDUINO_MOSFET_FAIL_EMERGENCY,
  SHOWDUINO_MOSFET_FAIL_BAD_COMMAND,
  SHOWDUINO_MOSFET_FAIL_BAD_ID,
  SHOWDUINO_MOSFET_FAIL_BAD_CHANNEL,
  SHOWDUINO_MOSFET_FAIL_BAD_LEVEL,
  SHOWDUINO_MOSFET_FAIL_BAD_DURATION,
  SHOWDUINO_MOSFET_FAIL_SHOW_CONTROLLED,
  SHOWDUINO_MOSFET_FAIL_NOT_OWNER,
  SHOWDUINO_MOSFET_FAIL_OFFLINE
} ShowduinoMosfetFail;

typedef struct ShowduinoMosfetAnnounce {
  char mac[18];
  char firmware[12];
  char state[20];
  char id[SHOWDUINO_MOSFET_ID_MAX + 1];
  char name[SHOWDUINO_MOSFET_NAME_MAX + 1];
} ShowduinoMosfetAnnounce;

typedef struct ShowduinoMosfetRoute {
  char id[SHOWDUINO_MOSFET_ID_MAX + 1];
  uint32_t sequence;
  const char *command;
} ShowduinoMosfetRoute;

typedef struct ShowduinoMosfetOutCmd {
  ShowduinoMosfetCmd cmd;
  uint8_t channel; /* 1–4 */
  uint8_t level;   /* 0–100 */
  uint32_t durationMs;
  char name[SHOWDUINO_MOSFET_OUT_NAME_MAX + 1];
} ShowduinoMosfetOutCmd;

static inline const char *showduino_mosfet_state_name(ShowduinoMosfetNodeState st) {
  switch (st) {
    case SHOWDUINO_MOSFET_ST_BOOTING: return "BOOTING";
    case SHOWDUINO_MOSFET_ST_SEARCHING: return "SEARCHING";
    case SHOWDUINO_MOSFET_ST_SAFE: return "SAFE";
    case SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED: return "SHOW_CONTROLLED";
    case SHOWDUINO_MOSFET_ST_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_MOSFET_ST_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

static inline ShowduinoMosfetNodeState showduino_mosfet_state_from_owner(
    ShowduinoNodeOwnerMode m) {
  switch (m) {
    case SHOWDUINO_OWNER_BOOTING: return SHOWDUINO_MOSFET_ST_BOOTING;
    case SHOWDUINO_OWNER_SEARCHING: return SHOWDUINO_MOSFET_ST_SEARCHING;
    case SHOWDUINO_OWNER_STANDALONE: return SHOWDUINO_MOSFET_ST_SAFE;
    case SHOWDUINO_OWNER_SHOW_CONTROLLED: return SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED;
    case SHOWDUINO_OWNER_EMERGENCY: return SHOWDUINO_MOSFET_ST_EMERGENCY;
    case SHOWDUINO_OWNER_FAULT: return SHOWDUINO_MOSFET_ST_FAULT;
    default: return SHOWDUINO_MOSFET_ST_UNKNOWN;
  }
}

static inline const char *showduino_mosfet_fail_name(ShowduinoMosfetFail f) {
  switch (f) {
    case SHOWDUINO_MOSFET_FAIL_NONE: return "NONE";
    case SHOWDUINO_MOSFET_FAIL_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_MOSFET_FAIL_BAD_COMMAND: return "BAD_COMMAND";
    case SHOWDUINO_MOSFET_FAIL_BAD_ID: return "BAD_ID";
    case SHOWDUINO_MOSFET_FAIL_BAD_CHANNEL: return "BAD_CHANNEL";
    case SHOWDUINO_MOSFET_FAIL_BAD_LEVEL: return "BAD_LEVEL";
    case SHOWDUINO_MOSFET_FAIL_BAD_DURATION: return "BAD_DURATION";
    case SHOWDUINO_MOSFET_FAIL_SHOW_CONTROLLED: return "SHOW_CONTROLLED";
    case SHOWDUINO_MOSFET_FAIL_NOT_OWNER: return "NOT_OWNER";
    case SHOWDUINO_MOSFET_FAIL_OFFLINE: return "OFFLINE";
    default: return "FAULT";
  }
}

/* Logical IDs: MOSFET-<digits>, e.g. MOSFET-1 / MOSFET-01 / MOSFET-008 */
static inline int showduino_mosfet_id_ok(const char *id) {
  size_t n;
  size_t i;
  unsigned long v;
  char *end = NULL;
  if (!id || !id[0]) return 0;
  n = strlen(id);
  if (n < 8 || n > SHOWDUINO_MOSFET_ID_MAX) return 0;
  if (strncmp(id, "MOSFET-", 7) != 0 && strncmp(id, "mosfet-", 7) != 0) return 0;
  for (i = 7; i < n; ++i) {
    if (!isdigit((unsigned char)id[i])) return 0;
  }
  v = strtoul(id + 7, &end, 10);
  if (!end || *end || v < 1ul || v > (unsigned long)SHOWDUINO_MOSFET_NODE_MAX_NODES) {
    return 0;
  }
  return 1;
}

static inline int showduino_mosfet_format_id(unsigned num, char *out, size_t n) {
  if (!out || n < 10 || num < 1 || num > SHOWDUINO_MOSFET_NODE_MAX_NODES) return 0;
  snprintf(out, n, "MOSFET-%02u", num);
  return 1;
}

static inline int showduino_mosfet_id_equal(const char *a, const char *b) {
  if (!a || !b) return 0;
  while (*a && *b) {
    char ca = (char)toupper((unsigned char)*a++);
    char cb = (char)toupper((unsigned char)*b++);
    if (ca != cb) return 0;
  }
  return *a == 0 && *b == 0;
}

static inline int showduino_mosfet_name_ok(const char *name) {
  size_t n;
  const char *p;
  if (!name || !name[0]) return 0;
  n = strlen(name);
  if (n > SHOWDUINO_MOSFET_NAME_MAX) return 0;
  for (p = name; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c < 32 || c > 126) return 0;
    if (c == ':' || c == '"' || c == '\\') return 0;
  }
  return 1;
}

static inline int showduino_mosfet_channel_ok(unsigned ch) {
  return ch >= 1u && ch <= (unsigned)SHOWDUINO_MOSFET_OUT_COUNT;
}

static inline int showduino_mosfet_level_ok(unsigned level) {
  return level <= SHOWDUINO_MOSFET_LEVEL_MAX;
}

static inline int showduino_mosfet_duration_ok(uint32_t ms) {
  return ms >= 1u && ms <= SHOWDUINO_MOSFET_DURATION_MAX_MS;
}

/* MOSFET:NODE:<id>:<rest> → rest */
static inline const char *showduino_mosfet_strip_node_prefix(const char *cmd,
                                                             char *idOut,
                                                             size_t idLen) {
  const char *p;
  const char *colon;
  size_t n;
  if (!cmd) return NULL;
  if (strncmp(cmd, "MOSFET:NODE:", 12) != 0) return cmd;
  p = cmd + 12;
  colon = strchr(p, ':');
  if (!colon || colon == p) return NULL;
  n = (size_t)(colon - p);
  if (n > SHOWDUINO_MOSFET_ID_MAX) return NULL;
  if (idOut && idLen) {
    if (n >= idLen) n = idLen - 1;
    memcpy(idOut, p, n);
    idOut[n] = '\0';
    if (!showduino_mosfet_id_ok(idOut)) return NULL;
  }
  return colon + 1;
}

static inline int showduino_mosfet_parse_route(const char *line,
                                               ShowduinoMosfetRoute *out) {
  const char *p;
  const char *colon;
  size_t n;
  char *end = NULL;
  if (!line || !out) return 0;
  memset(out, 0, sizeof(*out));
  if (strncmp(line, "ROUTE:MOSFET:", 13) != 0) return 0;
  p = line + 13;
  colon = strchr(p, ':');
  if (!colon || colon == p) return 0;
  n = (size_t)(colon - p);
  if (n > SHOWDUINO_MOSFET_ID_MAX) return 0;
  memcpy(out->id, p, n);
  out->id[n] = '\0';
  if (!showduino_mosfet_id_ok(out->id)) return 0;
  p = colon + 1;
  colon = strchr(p, ':');
  if (!colon || colon == p) return 0;
  out->sequence = (uint32_t)strtoul(p, &end, 10);
  if (!end || end != colon) return 0;
  out->command = colon + 1;
  return out->command[0] ? 1 : 0;
}

static inline int showduino_mosfet_cmd_theatrical(ShowduinoMosfetCmd cmd) {
  switch (cmd) {
    case SHOWDUINO_MOSFET_CMD_OUT_ON:
    case SHOWDUINO_MOSFET_CMD_OUT_LEVEL:
    case SHOWDUINO_MOSFET_CMD_OUT_PULSE:
    case SHOWDUINO_MOSFET_CMD_OUT_FADE:
    case SHOWDUINO_MOSFET_CMD_TEST:
      return 1;
    default:
      return 0;
  }
}

static inline int showduino_mosfet_cmd_always_ok_in_emergency(ShowduinoMosfetCmd cmd) {
  return cmd == SHOWDUINO_MOSFET_CMD_STATUS ||
         cmd == SHOWDUINO_MOSFET_CMD_CAPS ||
         cmd == SHOWDUINO_MOSFET_CMD_ALL_OFF ||
         cmd == SHOWDUINO_MOSFET_CMD_OUT_OFF ||
         cmd == SHOWDUINO_MOSFET_CMD_EMERGENCY_STOP ||
         cmd == SHOWDUINO_MOSFET_CMD_EMERGENCY_CLEAR;
}

static inline ShowduinoMosfetCmd showduino_mosfet_classify_command(const char *raw) {
  char id[SHOWDUINO_MOSFET_ID_MAX + 1];
  const char *cmd;
  if (!raw || !raw[0]) return SHOWDUINO_MOSFET_CMD_NONE;
  if (strcmp(raw, "EMERGENCY:STOP") == 0) return SHOWDUINO_MOSFET_CMD_EMERGENCY_STOP;
  if (strcmp(raw, "EMERGENCY:CLEAR") == 0) return SHOWDUINO_MOSFET_CMD_EMERGENCY_CLEAR;
  if (strcmp(raw, "OWN:GRANT") == 0 ||
      strcmp(raw, "MOSFET:OWN:GRANT") == 0 ||
      strcmp(raw, "MOSFET:NODE:OWN:GRANT") == 0) {
    return SHOWDUINO_MOSFET_CMD_OWN_GRANT;
  }
  cmd = showduino_mosfet_strip_node_prefix(raw, id, sizeof(id));
  if (!cmd) return SHOWDUINO_MOSFET_CMD_NONE;
  if (strcmp(cmd, "OWN:GRANT") == 0 || strcmp(cmd, "MOSFET:OWN:GRANT") == 0) {
    return SHOWDUINO_MOSFET_CMD_OWN_GRANT;
  }
  if (strncmp(cmd, "MOSFET:", 7) == 0) cmd += 7;
  if (strcmp(cmd, "STATUS") == 0) return SHOWDUINO_MOSFET_CMD_STATUS;
  if (strcmp(cmd, "CAPS") == 0) return SHOWDUINO_MOSFET_CMD_CAPS;
  if (strcmp(cmd, "ALL:OFF") == 0) return SHOWDUINO_MOSFET_CMD_ALL_OFF;
  if (strcmp(cmd, "TEST") == 0) return SHOWDUINO_MOSFET_CMD_TEST;
  if (strncmp(cmd, "ID:", 3) == 0) return SHOWDUINO_MOSFET_CMD_ID;
  if (strncmp(cmd, "NAME:", 5) == 0) return SHOWDUINO_MOSFET_CMD_NAME;
  if (strncmp(cmd, "OUT:", 4) == 0) {
    const char *p = cmd + 4;
    unsigned ch = 0;
    while (*p >= '0' && *p <= '9') {
      ch = ch * 10u + (unsigned)(*p - '0');
      ++p;
    }
    if (*p != ':' || !showduino_mosfet_channel_ok(ch)) return SHOWDUINO_MOSFET_CMD_NONE;
    ++p;
    if (strcmp(p, "OFF") == 0) return SHOWDUINO_MOSFET_CMD_OUT_OFF;
    if (strcmp(p, "ON") == 0) return SHOWDUINO_MOSFET_CMD_OUT_ON;
    if (strncmp(p, "LEVEL:", 6) == 0) return SHOWDUINO_MOSFET_CMD_OUT_LEVEL;
    if (strncmp(p, "PULSE:", 6) == 0) return SHOWDUINO_MOSFET_CMD_OUT_PULSE;
    if (strncmp(p, "FADE:", 5) == 0) return SHOWDUINO_MOSFET_CMD_OUT_FADE;
    if (strncmp(p, "NAME:", 5) == 0) return SHOWDUINO_MOSFET_CMD_OUT_NAME;
  }
  return SHOWDUINO_MOSFET_CMD_NONE;
}

static inline int showduino_mosfet_parse_out_command(const char *raw,
                                                     ShowduinoMosfetOutCmd *out) {
  char id[SHOWDUINO_MOSFET_ID_MAX + 1];
  const char *cmd;
  const char *p;
  unsigned ch = 0;
  unsigned long level = 0;
  unsigned long dur = 0;
  char *end = NULL;
  if (!raw || !out) return 0;
  memset(out, 0, sizeof(*out));
  out->cmd = showduino_mosfet_classify_command(raw);
  cmd = showduino_mosfet_strip_node_prefix(raw, id, sizeof(id));
  if (!cmd) return 0;
  if (strncmp(cmd, "MOSFET:", 7) == 0) cmd += 7;
  if (out->cmd == SHOWDUINO_MOSFET_CMD_ALL_OFF ||
      out->cmd == SHOWDUINO_MOSFET_CMD_STATUS ||
      out->cmd == SHOWDUINO_MOSFET_CMD_CAPS ||
      out->cmd == SHOWDUINO_MOSFET_CMD_OWN_GRANT ||
      out->cmd == SHOWDUINO_MOSFET_CMD_EMERGENCY_STOP ||
      out->cmd == SHOWDUINO_MOSFET_CMD_EMERGENCY_CLEAR ||
      out->cmd == SHOWDUINO_MOSFET_CMD_ID ||
      out->cmd == SHOWDUINO_MOSFET_CMD_NAME ||
      out->cmd == SHOWDUINO_MOSFET_CMD_TEST) {
    return 1;
  }
  if (strncmp(cmd, "OUT:", 4) != 0) return 0;
  p = cmd + 4;
  while (*p >= '0' && *p <= '9') {
    ch = ch * 10u + (unsigned)(*p - '0');
    ++p;
  }
  if (*p != ':' || !showduino_mosfet_channel_ok(ch)) return 0;
  out->channel = (uint8_t)ch;
  ++p;
  if (strcmp(p, "OFF") == 0) {
    out->cmd = SHOWDUINO_MOSFET_CMD_OUT_OFF;
    out->level = 0;
    return 1;
  }
  if (strcmp(p, "ON") == 0) {
    out->cmd = SHOWDUINO_MOSFET_CMD_OUT_ON;
    out->level = 100;
    return 1;
  }
  if (strncmp(p, "LEVEL:", 6) == 0) {
    level = strtoul(p + 6, &end, 10);
    if (!end || *end || !showduino_mosfet_level_ok((unsigned)level)) return 0;
    out->cmd = SHOWDUINO_MOSFET_CMD_OUT_LEVEL;
    out->level = (uint8_t)level;
    return 1;
  }
  if (strncmp(p, "PULSE:", 6) == 0) {
    level = strtoul(p + 6, &end, 10);
    if (!end || *end != ':' || !showduino_mosfet_level_ok((unsigned)level)) return 0;
    dur = strtoul(end + 1, &end, 10);
    if (!end || *end || !showduino_mosfet_duration_ok((uint32_t)dur)) return 0;
    out->cmd = SHOWDUINO_MOSFET_CMD_OUT_PULSE;
    out->level = (uint8_t)level;
    out->durationMs = (uint32_t)dur;
    return 1;
  }
  if (strncmp(p, "FADE:", 5) == 0) {
    level = strtoul(p + 5, &end, 10);
    if (!end || *end != ':' || !showduino_mosfet_level_ok((unsigned)level)) return 0;
    dur = strtoul(end + 1, &end, 10);
    if (!end || *end || !showduino_mosfet_duration_ok((uint32_t)dur)) return 0;
    out->cmd = SHOWDUINO_MOSFET_CMD_OUT_FADE;
    out->level = (uint8_t)level;
    out->durationMs = (uint32_t)dur;
    return 1;
  }
  if (strncmp(p, "NAME:", 5) == 0) {
    if (!showduino_mosfet_name_ok(p + 5)) return 0;
    out->cmd = SHOWDUINO_MOSFET_CMD_OUT_NAME;
    strncpy(out->name, p + 5, sizeof(out->name) - 1);
    return 1;
  }
  return 0;
}

static inline ShowduinoMosfetFail showduino_mosfet_can_accept(
    ShowduinoMosfetNodeState st, ShowduinoMosfetCmd cmd, ShowduinoCmdOrigin origin) {
  if (cmd == SHOWDUINO_MOSFET_CMD_NONE) return SHOWDUINO_MOSFET_FAIL_BAD_COMMAND;
  if (cmd == SHOWDUINO_MOSFET_CMD_EMERGENCY_STOP) return SHOWDUINO_MOSFET_FAIL_NONE;
  if (cmd == SHOWDUINO_MOSFET_CMD_STATUS || cmd == SHOWDUINO_MOSFET_CMD_CAPS) {
    return SHOWDUINO_MOSFET_FAIL_NONE;
  }
  if (cmd == SHOWDUINO_MOSFET_CMD_OWN_GRANT) {
    return origin == SHOWDUINO_CMD_ORIGIN_SHOW ? SHOWDUINO_MOSFET_FAIL_NONE
                                              : SHOWDUINO_MOSFET_FAIL_NOT_OWNER;
  }
  if (st == SHOWDUINO_MOSFET_ST_EMERGENCY) {
    if (cmd == SHOWDUINO_MOSFET_CMD_EMERGENCY_CLEAR) {
      return origin == SHOWDUINO_CMD_ORIGIN_SHOW ? SHOWDUINO_MOSFET_FAIL_NONE
                                                 : SHOWDUINO_MOSFET_FAIL_EMERGENCY;
    }
    if (showduino_mosfet_cmd_always_ok_in_emergency(cmd)) {
      return SHOWDUINO_MOSFET_FAIL_NONE;
    }
    return SHOWDUINO_MOSFET_FAIL_EMERGENCY;
  }
  if (cmd == SHOWDUINO_MOSFET_CMD_EMERGENCY_CLEAR) {
    return origin == SHOWDUINO_CMD_ORIGIN_SHOW ? SHOWDUINO_MOSFET_FAIL_NONE
                                               : SHOWDUINO_MOSFET_FAIL_SHOW_CONTROLLED;
  }
  if (cmd == SHOWDUINO_MOSFET_CMD_ID || cmd == SHOWDUINO_MOSFET_CMD_NAME ||
      cmd == SHOWDUINO_MOSFET_CMD_OUT_NAME) {
    if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) return SHOWDUINO_MOSFET_FAIL_NONE;
    if (st == SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED) {
      return SHOWDUINO_MOSFET_FAIL_SHOW_CONTROLLED;
    }
    return SHOWDUINO_MOSFET_FAIL_NONE;
  }
  if (cmd == SHOWDUINO_MOSFET_CMD_ALL_OFF || cmd == SHOWDUINO_MOSFET_CMD_OUT_OFF) {
    return SHOWDUINO_MOSFET_FAIL_NONE;
  }
  if (showduino_mosfet_cmd_theatrical(cmd)) {
    if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) {
      return (st == SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED)
                 ? SHOWDUINO_MOSFET_FAIL_NONE
                 : SHOWDUINO_MOSFET_FAIL_NOT_OWNER;
    }
    if (st == SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED) {
      return SHOWDUINO_MOSFET_FAIL_SHOW_CONTROLLED;
    }
    return SHOWDUINO_MOSFET_FAIL_NONE;
  }
  return SHOWDUINO_MOSFET_FAIL_NONE;
}

/*
 * ANNOUNCE:<mac>:<fw>:<state>:ID=<id>:N=<name>
 */
static inline int showduino_mosfet_parse_announce(const char *line,
                                                   ShowduinoMosfetAnnounce *out) {
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
  out->firmware[n] = '\0';
  p = colon + 1;
  colon = strchr(p, ':');
  if (!colon) return 0;
  n = (size_t)(colon - p);
  if (n >= sizeof(out->state)) n = sizeof(out->state) - 1;
  memcpy(out->state, p, n);
  out->state[n] = '\0';
  p = colon + 1;
  if (strncmp(p, "ID=", 3) != 0) return 0;
  p += 3;
  colon = strchr(p, ':');
  if (!colon) return 0;
  n = (size_t)(colon - p);
  if (n > SHOWDUINO_MOSFET_ID_MAX) return 0;
  memcpy(out->id, p, n);
  out->id[n] = '\0';
  if (!showduino_mosfet_id_ok(out->id)) return 0;
  p = colon + 1;
  if (strncmp(p, "N=", 2) != 0) return 0;
  p += 2;
  n = strlen(p);
  if (n > SHOWDUINO_MOSFET_NAME_MAX) n = SHOWDUINO_MOSFET_NAME_MAX;
  memcpy(out->name, p, n);
  out->name[n] = '\0';
  return 1;
}

static inline int showduino_mosfet_format_announce(char *out, size_t n,
                                                   const char *mac,
                                                   const char *fw,
                                                   const char *state,
                                                   const char *id,
                                                   const char *name) {
  if (!out || !n || !mac || !fw || !state || !id) return 0;
  if (!showduino_mosfet_id_ok(id)) return 0;
  snprintf(out, n, "ANNOUNCE:%s:%s:%s:ID=%s:N=%s",
           mac, fw, state, id, name && name[0] ? name : "MOSFET NODE");
  return 1;
}

/*
 * Compact status: MOSFET:STATUS:<id>:OWN=<0|1>:EM=<0|1>:O=<a>,<b>,<c>,<d>
 */
static inline int showduino_mosfet_format_status(char *out, size_t n,
                                                 const char *id,
                                                 int owned,
                                                 int emergency,
                                                 const uint8_t levels[4]) {
  if (!out || !n || !id || !levels || !showduino_mosfet_id_ok(id)) return 0;
  snprintf(out, n, "MOSFET:STATUS:%s:OWN=%u:EM=%u:O=%u,%u,%u,%u",
           id,
           owned ? 1u : 0u,
           emergency ? 1u : 0u,
           (unsigned)levels[0], (unsigned)levels[1],
           (unsigned)levels[2], (unsigned)levels[3]);
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_MOSFET_NODE_H */
