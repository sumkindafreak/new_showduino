#ifndef SHOWDUINO_STATE_WIRE_H
#define SHOWDUINO_STATE_WIRE_H

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "showduino_message_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Stage 3 authoritative / lifecycle wire tokens (colon-text v1) ---- */
#define SHOWDUINO_WIRE_SNAPSHOT_BEGIN     "SNAPSHOT:BEGIN"
#define SHOWDUINO_WIRE_SNAPSHOT_END       "SNAPSHOT:END"

#define SHOWDUINO_WIRE_STATE_SHOW_PREFIX  "STATE:SHOW:"
#define SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX "STATE:EMERGENCY:"
#define SHOWDUINO_WIRE_STATE_NODE_RELAY_PREFIX "STATE:NODE:RELAY:"
#define SHOWDUINO_WIRE_STATE_RELAY_PREFIX "STATE:RELAY:"

#define SHOWDUINO_WIRE_ACCEPTED_RELAY_PREFIX "ACCEPTED:RELAY:"
#define SHOWDUINO_WIRE_REJECTED_RELAY_PREFIX "REJECTED:RELAY:"
#define SHOWDUINO_WIRE_FAILED_RELAY_PREFIX  "FAILED:RELAY:"

#define SHOWDUINO_WIRE_UNSUPPORTED_PREFIX "UNSUPPORTED:"
#define SHOWDUINO_WIRE_NOT_IMPLEMENTED_PREFIX "NOT_IMPLEMENTED:"
#define SHOWDUINO_WIRE_NODE_UNAVAILABLE_PREFIX "NODE_UNAVAILABLE:"

#define SHOWDUINO_WIRE_SHOW_IDLE       "IDLE"
#define SHOWDUINO_WIRE_SHOW_PLAYING    "PLAYING"
#define SHOWDUINO_WIRE_SHOW_PAUSED     "PAUSED"
#define SHOWDUINO_WIRE_SHOW_STOPPING   "STOPPING"
#define SHOWDUINO_WIRE_SHOW_FAULT      "FAULT"
#define SHOWDUINO_WIRE_SHOW_EMERGENCY  "EMERGENCY"

#define SHOWDUINO_WIRE_EMERGENCY_ACTIVE "ACTIVE"
#define SHOWDUINO_WIRE_EMERGENCY_CLEAR  "CLEAR"

#define SHOWDUINO_WIRE_NODE_UNKNOWN "UNKNOWN"
#define SHOWDUINO_WIRE_NODE_ONLINE  "ONLINE"
#define SHOWDUINO_WIRE_NODE_OFFLINE "OFFLINE"
#define SHOWDUINO_WIRE_NODE_FAULT   "FAULT"

#define SHOWDUINO_WIRE_RELAY_UNKNOWN "UNKNOWN"
#define SHOWDUINO_WIRE_RELAY_OFF     "OFF"
#define SHOWDUINO_WIRE_RELAY_ON      "ON"
#define SHOWDUINO_WIRE_RELAY_FAULT   "FAULT"

typedef enum ShowduinoShowRuntimeWire {
  SHOWDUINO_SHOW_WIRE_IDLE = 0,
  SHOWDUINO_SHOW_WIRE_PLAYING,
  SHOWDUINO_SHOW_WIRE_PAUSED,
  SHOWDUINO_SHOW_WIRE_STOPPING,
  SHOWDUINO_SHOW_WIRE_FAULT,
  SHOWDUINO_SHOW_WIRE_EMERGENCY,
  SHOWDUINO_SHOW_WIRE_INVALID = -1
} ShowduinoShowRuntimeWire;

typedef enum ShowduinoEmergencyWire {
  SHOWDUINO_EMERGENCY_WIRE_CLEAR = 0,
  SHOWDUINO_EMERGENCY_WIRE_ACTIVE = 1,
  SHOWDUINO_EMERGENCY_WIRE_INVALID = -1
} ShowduinoEmergencyWire;

typedef enum ShowduinoNodeAvailWire {
  SHOWDUINO_NODE_WIRE_UNKNOWN = 0,
  SHOWDUINO_NODE_WIRE_ONLINE,
  SHOWDUINO_NODE_WIRE_OFFLINE,
  SHOWDUINO_NODE_WIRE_FAULT,
  SHOWDUINO_NODE_WIRE_INVALID = -1
} ShowduinoNodeAvailWire;

typedef enum ShowduinoRelayKnowledgeWire {
  SHOWDUINO_RELAY_WIRE_UNKNOWN = 0,
  SHOWDUINO_RELAY_WIRE_OFF,
  SHOWDUINO_RELAY_WIRE_ON,
  SHOWDUINO_RELAY_WIRE_FAULT,
  SHOWDUINO_RELAY_WIRE_INVALID = -1
} ShowduinoRelayKnowledgeWire;

static inline ShowduinoShowRuntimeWire showduino_parse_state_show(const char *line) {
  if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_SHOW_PREFIX, 11) != 0) {
    return SHOWDUINO_SHOW_WIRE_INVALID;
  }
  const char *v = line + 11;
  if (strcmp(v, SHOWDUINO_WIRE_SHOW_IDLE) == 0) return SHOWDUINO_SHOW_WIRE_IDLE;
  if (strcmp(v, SHOWDUINO_WIRE_SHOW_PLAYING) == 0) return SHOWDUINO_SHOW_WIRE_PLAYING;
  if (strcmp(v, SHOWDUINO_WIRE_SHOW_PAUSED) == 0) return SHOWDUINO_SHOW_WIRE_PAUSED;
  if (strcmp(v, SHOWDUINO_WIRE_SHOW_STOPPING) == 0) return SHOWDUINO_SHOW_WIRE_STOPPING;
  if (strcmp(v, SHOWDUINO_WIRE_SHOW_FAULT) == 0) return SHOWDUINO_SHOW_WIRE_FAULT;
  if (strcmp(v, SHOWDUINO_WIRE_SHOW_EMERGENCY) == 0) return SHOWDUINO_SHOW_WIRE_EMERGENCY;
  return SHOWDUINO_SHOW_WIRE_INVALID;
}

static inline ShowduinoEmergencyWire showduino_parse_state_emergency(const char *line) {
  if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX, 16) != 0) {
    return SHOWDUINO_EMERGENCY_WIRE_INVALID;
  }
  const char *v = line + 16;
  if (strcmp(v, SHOWDUINO_WIRE_EMERGENCY_ACTIVE) == 0) return SHOWDUINO_EMERGENCY_WIRE_ACTIVE;
  if (strcmp(v, SHOWDUINO_WIRE_EMERGENCY_CLEAR) == 0) return SHOWDUINO_EMERGENCY_WIRE_CLEAR;
  return SHOWDUINO_EMERGENCY_WIRE_INVALID;
}

static inline ShowduinoNodeAvailWire showduino_parse_state_node_relay(const char *line) {
  const size_t prefixLen = sizeof(SHOWDUINO_WIRE_STATE_NODE_RELAY_PREFIX) - 1; /* 17 */
  if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_NODE_RELAY_PREFIX, prefixLen) != 0) {
    return SHOWDUINO_NODE_WIRE_INVALID;
  }
  const char *v = line + prefixLen;
  if (strcmp(v, SHOWDUINO_WIRE_NODE_UNKNOWN) == 0) return SHOWDUINO_NODE_WIRE_UNKNOWN;
  if (strcmp(v, SHOWDUINO_WIRE_NODE_ONLINE) == 0) return SHOWDUINO_NODE_WIRE_ONLINE;
  if (strcmp(v, SHOWDUINO_WIRE_NODE_OFFLINE) == 0) return SHOWDUINO_NODE_WIRE_OFFLINE;
  if (strcmp(v, SHOWDUINO_WIRE_NODE_FAULT) == 0) return SHOWDUINO_NODE_WIRE_FAULT;
  return SHOWDUINO_NODE_WIRE_INVALID;
}

/*
 * Parse STATE:RELAY:<ch>:ON|OFF|UNKNOWN|FAULT
 * Returns 0 on success.
 */
static inline int showduino_parse_state_relay(
    const char *line,
    int *channelOut,
    ShowduinoRelayKnowledgeWire *stateOut) {
  int ch = 0;
  const char *p;
  if (!line || !channelOut || !stateOut) return -1;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_RELAY_PREFIX, 12) != 0) return -1;
  p = line + 12;
  while (*p >= '0' && *p <= '9') {
    ch = ch * 10 + (*p - '0');
    p++;
  }
  if (ch < 1 || ch > 8) return -1;
  if (*p != ':') return -1;
  p++;
  *channelOut = ch;
  if (strcmp(p, SHOWDUINO_WIRE_RELAY_ON) == 0) {
    *stateOut = SHOWDUINO_RELAY_WIRE_ON;
    return 0;
  }
  if (strcmp(p, SHOWDUINO_WIRE_RELAY_OFF) == 0) {
    *stateOut = SHOWDUINO_RELAY_WIRE_OFF;
    return 0;
  }
  if (strcmp(p, SHOWDUINO_WIRE_RELAY_UNKNOWN) == 0) {
    *stateOut = SHOWDUINO_RELAY_WIRE_UNKNOWN;
    return 0;
  }
  if (strcmp(p, SHOWDUINO_WIRE_RELAY_FAULT) == 0) {
    *stateOut = SHOWDUINO_RELAY_WIRE_FAULT;
    return 0;
  }
  return -1;
}

/* REJECTED:RELAY:<ch>:<REASON> or FAILED:RELAY:<ch>:<REASON> */
static inline int showduino_parse_relay_outcome(
    const char *line,
    const char *prefix,
    int *channelOut,
    char *reasonOut,
    size_t reasonCap) {
  int ch = 0;
  const char *p;
  size_t n;
  size_t plen;
  if (!line || !prefix || !channelOut) return -1;
  plen = strlen(prefix);
  if (strncmp(line, prefix, plen) != 0) return -1;
  p = line + plen;
  while (*p >= '0' && *p <= '9') {
    ch = ch * 10 + (*p - '0');
    p++;
  }
  if (ch < 1 || ch > 8) return -1;
  if (*p != ':') return -1;
  p++;
  *channelOut = ch;
  if (reasonOut && reasonCap > 0) {
    n = strlen(p);
    if (n >= reasonCap) n = reasonCap - 1;
    memcpy(reasonOut, p, n);
    reasonOut[n] = '\0';
  }
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_STATE_WIRE_H */
