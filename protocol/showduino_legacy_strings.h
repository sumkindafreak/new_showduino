#ifndef SHOWDUINO_LEGACY_STRINGS_H
#define SHOWDUINO_LEGACY_STRINGS_H

#include <string.h>
#include "showduino_message_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Link / system (colon-text on wire today) ---- */
#define SHOWDUINO_LEGACY_HELLO            "HELLO"
#define SHOWDUINO_LEGACY_HEARTBEAT        "HEARTBEAT"
#define SHOWDUINO_LEGACY_ACK_HEARTBEAT    "ACK:HEARTBEAT"
#define SHOWDUINO_LEGACY_READY            "READY"
#define SHOWDUINO_LEGACY_STATUS_REQUEST   "STATUS:REQUEST"
#define SHOWDUINO_LEGACY_TIME_REQUEST     "TIME:REQUEST"
#define SHOWDUINO_LEGACY_TIME_PREFIX      "TIME:"
#define SHOWDUINO_LEGACY_SHOWDUINO_STAGE  "SHOWDUINO_STAGE_ENGINE" /* legacy identity string */

/* ---- Show ---- */
#define SHOWDUINO_LEGACY_SHOW_START       "SHOW:START"
#define SHOWDUINO_LEGACY_SHOW_STOP        "SHOW:STOP"
#define SHOWDUINO_LEGACY_SHOW_RUNNING     "SHOW:RUNNING"
#define SHOWDUINO_LEGACY_SHOW_STOPPED     "SHOW:STOPPED"
#define SHOWDUINO_LEGACY_ACK_SHOW_START   "ACK:SHOW:START"
#define SHOWDUINO_LEGACY_ACK_SHOW_STOP    "ACK:SHOW:STOP"
#define SHOWDUINO_LEGACY_STOP_ALL         "STOP:ALL"

/* ---- Emergency ---- */
#define SHOWDUINO_LEGACY_EMERGENCY_STOP   "EMERGENCY:STOP"
#define SHOWDUINO_LEGACY_EMERGENCY_CLEAR  "EMERGENCY:CLEAR"
#define SHOWDUINO_LEGACY_STATUS_ELOCKED   "STATUS:EMERGENCY_LOCKED"
#define SHOWDUINO_LEGACY_STATUS_ECLEARED  "STATUS:EMERGENCY_CLEARED"

/* ---- Relay ---- */
#define SHOWDUINO_LEGACY_RELAY_ALL_OFF    "RELAY:ALL:OFF"
#define SHOWDUINO_LEGACY_ACK_RELAY_PREFIX "ACK:RELAY:"
#define SHOWDUINO_LEGACY_OK_RELAY_PREFIX  "OK:RELAY:"

/* ---- Transport envelopes (not permanent app vocabulary) ---- */
#define SHOWDUINO_LEGACY_ROUTE_PREFIX     "ROUTE:"
#define SHOWDUINO_LEGACY_ROUTE_RELAY      "ROUTE:RELAY:"
#define SHOWDUINO_LEGACY_ROUTE_PIXEL      "ROUTE:PIXEL:"
#define SHOWDUINO_LEGACY_ROUTE_AUDIO      "ROUTE:AUDIO:"
#define SHOWDUINO_LEGACY_NODE_PREFIX      "NODE:"
#define SHOWDUINO_LEGACY_ACK_PREFIX       "ACK:"
#define SHOWDUINO_LEGACY_ERR_PREFIX       "ERR:"
#define SHOWDUINO_LEGACY_STATUS_PREFIX    "STATUS:"

/* Node type token used inside node packets / ROUTE */
#define SHOWDUINO_LEGACY_NODETYPE_RELAY   "RELAY"
#define SHOWDUINO_LEGACY_NODETYPE_PIXEL   "PIXEL"
#define SHOWDUINO_LEGACY_NODETYPE_AUDIO   "AUDIO"

/* ---- Audio (colon-text in desk/node command[]; no PCM over ESP-NOW) ---- */
#define SHOWDUINO_LEGACY_AUDIO_PREFIX           "AUDIO:"
#define SHOWDUINO_LEGACY_AUDIO_LOCAL_PLAY       "AUDIO:LOCAL:PLAY"
#define SHOWDUINO_LEGACY_AUDIO_LOCAL_PAUSE      "AUDIO:LOCAL:PAUSE"
#define SHOWDUINO_LEGACY_AUDIO_LOCAL_STOP       "AUDIO:LOCAL:STOP"
#define SHOWDUINO_LEGACY_AUDIO_LOCAL_VOLUME     "AUDIO:LOCAL:VOLUME"
#define SHOWDUINO_LEGACY_AUDIO_LOCAL_MUTE       "AUDIO:LOCAL:MUTE"
#define SHOWDUINO_LEGACY_AUDIO_LOCAL_TEST       "AUDIO:LOCAL:TEST"
#define SHOWDUINO_LEGACY_AUDIO_NODE_PLAY        "AUDIO:NODE:PLAY"
#define SHOWDUINO_LEGACY_AUDIO_NODE_PAUSE       "AUDIO:NODE:PAUSE"
#define SHOWDUINO_LEGACY_AUDIO_NODE_STOP        "AUDIO:NODE:STOP"
#define SHOWDUINO_LEGACY_AUDIO_NODE_VOLUME      "AUDIO:NODE:VOLUME"
#define SHOWDUINO_LEGACY_AUDIO_NODE_MUTE        "AUDIO:NODE:MUTE"
#define SHOWDUINO_LEGACY_AUDIO_NODE_TEST        "AUDIO:NODE:TEST"
#define SHOWDUINO_LEGACY_AUDIO_NODE_STATUS_REQ  "AUDIO:NODE:STATUS"


/*
 * Map a legacy colon command (request side) to a catalog message type.
 * Returns SHOWDUINO_MSG_UNKNOWN if not recognized.
 */
static inline ShowduinoMessageType showduino_legacy_map_command(const char *cmd) {
  if (!cmd || !cmd[0]) return SHOWDUINO_MSG_UNKNOWN;

  if (strcmp(cmd, SHOWDUINO_LEGACY_HELLO) == 0) return SHOWDUINO_MSG_HELLO;
  if (strcmp(cmd, SHOWDUINO_LEGACY_HEARTBEAT) == 0) return SHOWDUINO_MSG_HEARTBEAT;
  if (strcmp(cmd, SHOWDUINO_LEGACY_STATUS_REQUEST) == 0) return SHOWDUINO_MSG_STATUS_REQUEST;
  if (strcmp(cmd, SHOWDUINO_LEGACY_TIME_REQUEST) == 0) return SHOWDUINO_MSG_STATUS_REQUEST;
  if (strcmp(cmd, SHOWDUINO_LEGACY_SHOW_START) == 0) return SHOWDUINO_MSG_SHOW_START_REQUEST;
  if (strcmp(cmd, SHOWDUINO_LEGACY_SHOW_STOP) == 0) return SHOWDUINO_MSG_SHOW_STOP_REQUEST;
  if (strcmp(cmd, SHOWDUINO_LEGACY_STOP_ALL) == 0) return SHOWDUINO_MSG_SHOW_STOP_REQUEST;
  if (strcmp(cmd, SHOWDUINO_LEGACY_EMERGENCY_STOP) == 0) {
    return SHOWDUINO_MSG_EMERGENCY_ACTIVATE_REQUEST;
  }
  if (strcmp(cmd, SHOWDUINO_LEGACY_EMERGENCY_CLEAR) == 0) {
    return SHOWDUINO_MSG_EMERGENCY_CLEAR_REQUEST;
  }
  if (strcmp(cmd, SHOWDUINO_LEGACY_ACK_HEARTBEAT) == 0) return SHOWDUINO_MSG_HEARTBEAT_ACK;

  /* RELAY:n:ON / OFF / TOGGLE / PULSE — coarse classify */
  if (strncmp(cmd, "RELAY:", 6) == 0) {
    if (strstr(cmd, ":TOGGLE") != 0) return SHOWDUINO_MSG_RELAY_TOGGLE_DEPRECATED;
    if (strstr(cmd, ":ON") != 0 || strstr(cmd, ":OFF") != 0 ||
        strstr(cmd, ":PULSE:") != 0 || strcmp(cmd, SHOWDUINO_LEGACY_RELAY_ALL_OFF) == 0) {
      return SHOWDUINO_MSG_RELAY_SET_REQUEST;
    }
  }

  return SHOWDUINO_MSG_UNKNOWN;
}

/*
 * Parse RELAY:<ch>:ON|OFF into channel + state.
 * Returns 0 on success, -1 on failure. Does not accept TOGGLE.
 */
static inline int showduino_legacy_parse_relay_set(
    const char *cmd,
    int *channelOut,
    ShowduinoRelayState *stateOut) {
  int ch = 0;
  const char *p;
  if (!cmd || !channelOut || !stateOut) return -1;
  if (strncmp(cmd, "RELAY:", 6) != 0) return -1;
  p = cmd + 6;
  while (*p >= '0' && *p <= '9') {
    ch = ch * 10 + (*p - '0');
    p++;
  }
  if (ch < 1 || ch > 8) return -1;
  if (*p != ':') return -1;
  p++;
  if (strcmp(p, "ON") == 0) {
    *channelOut = ch;
    *stateOut = SHOWDUINO_RELAY_ON;
    return 0;
  }
  if (strcmp(p, "OFF") == 0) {
    *channelOut = ch;
    *stateOut = SHOWDUINO_RELAY_OFF;
    return 0;
  }
  return -1;
}

static inline int showduino_legacy_is_toggle(const char *cmd) {
  return cmd && strstr(cmd, ":TOGGLE") != 0 && strncmp(cmd, "RELAY:", 6) == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_LEGACY_STRINGS_H */
