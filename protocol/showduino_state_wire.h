#ifndef SHOWDUINO_STATE_WIRE_H
#define SHOWDUINO_STATE_WIRE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
#define SHOWDUINO_WIRE_STATE_NODE_AUDIO_PREFIX "STATE:NODE:AUDIO:"
#define SHOWDUINO_WIRE_STATE_NODE_LAMP_PREFIX  "STATE:NODE:LAMP:"
#define SHOWDUINO_WIRE_STATE_NODE_PIXEL_PREFIX "STATE:NODE:PIXEL:"
#define SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_PREFIX "STATE:NODE:EMERGENCY:"
#define SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX "STATE:EMERGENCY:SOURCE:"
#define SHOWDUINO_WIRE_STATE_SAFETY_PREFIX "STATE:SAFETY:"
#define SHOWDUINO_WIRE_STATE_RELAY_PREFIX "STATE:RELAY:"
/* Reserved for a future Director Ethernet / E1.31 status page. Not published yet. */
#define SHOWDUINO_WIRE_STATE_ETHERNET_PREFIX "STATE:ETHERNET:"
#define SHOWDUINO_WIRE_STATE_E131_PREFIX     "STATE:E131:"
#define SHOWDUINO_WIRE_ETHERNET_ONLINE       "ONLINE"
#define SHOWDUINO_WIRE_ETHERNET_OFFLINE      "OFFLINE"
#define SHOWDUINO_WIRE_E131_ONLINE           "ONLINE"
#define SHOWDUINO_WIRE_E131_STALE            "STALE"
#define SHOWDUINO_WIRE_E131_OFFLINE          "OFFLINE"
#define SHOWDUINO_WIRE_E131_UNAVAILABLE      "UNAVAILABLE"
/* Reserved for a future Director storage status page. Not required for safety. */
#define SHOWDUINO_WIRE_STATE_STORAGE_PREFIX  "STATE:STORAGE:"
#define SHOWDUINO_WIRE_STORAGE_ONLINE        "ONLINE"
#define SHOWDUINO_WIRE_STORAGE_DEGRADED      "DEGRADED"
#define SHOWDUINO_WIRE_STORAGE_READ_ONLY     "READ_ONLY"
#define SHOWDUINO_WIRE_STORAGE_OFFLINE       "OFFLINE"
#define SHOWDUINO_WIRE_STORAGE_FAULT         "FAULT"

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

/* Lamp Node presence for Director capability gating (replaces Relay Node slot). */
typedef enum ShowduinoLampNodeWire {
  SHOWDUINO_LAMP_NODE_WIRE_OFFLINE = 0,
  SHOWDUINO_LAMP_NODE_WIRE_ONLINE,
  SHOWDUINO_LAMP_NODE_WIRE_ACTIVE,
  SHOWDUINO_LAMP_NODE_WIRE_FAULT,
  SHOWDUINO_LAMP_NODE_WIRE_EMERGENCY,
  SHOWDUINO_LAMP_NODE_WIRE_INVALID = -1
} ShowduinoLampNodeWire;

#define SHOWDUINO_WIRE_STATE_NODE_LAMP_DETAIL_PREFIX "STATE:NODE:LAMP:D:"
#define SHOWDUINO_WIRE_STATE_NODE_LAMP_SENSOR_PREFIX "STATE:NODE:LAMP:S:"

typedef struct ShowduinoLampDetailWire {
  char state[16];
  char fx[24];
  uint8_t brightness;
  char mac[18];
  char firmware[12];
} ShowduinoLampDetailWire;

/* Compact live sensor snapshot for Director commissioning.
 * Wire: STATE:NODE:LAMP:S:<btn>:<mic>:<blow>:<motion>:<light>:<volt_mv>:<audio>
 * btn: RELEASED|PRESSED  blow: IDLE|PUFF|BLOW|DETECTED
 * motion: CLEAR|DETECTED|UNCONF  mic/light: 0-100 or -  volt_mv: integer or -
 */
typedef struct ShowduinoLampSensorWire {
  char button[12];
  char mic[8];
  char blow[12];
  char motion[12];
  char light[8];
  char voltage[12];
  char audio[16];
} ShowduinoLampSensorWire;

static inline ShowduinoLampNodeWire showduino_parse_state_node_lamp(const char *line) {
  const size_t prefixLen = sizeof(SHOWDUINO_WIRE_STATE_NODE_LAMP_PREFIX) - 1;
  if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_NODE_LAMP_PREFIX, prefixLen) != 0) {
    return SHOWDUINO_LAMP_NODE_WIRE_INVALID;
  }
  const char *v = line + prefixLen;
  if (v[0] && v[1] == ':' && (v[0] == 'D' || v[0] == 'S')) {
    return SHOWDUINO_LAMP_NODE_WIRE_INVALID;
  }
  if (strcmp(v, "OFFLINE") == 0) return SHOWDUINO_LAMP_NODE_WIRE_OFFLINE;
  if (strcmp(v, "ONLINE") == 0) return SHOWDUINO_LAMP_NODE_WIRE_ONLINE;
  if (strcmp(v, "ACTIVE") == 0) return SHOWDUINO_LAMP_NODE_WIRE_ACTIVE;
  if (strcmp(v, "FAULT") == 0) return SHOWDUINO_LAMP_NODE_WIRE_FAULT;
  if (strcmp(v, "EMERGENCY") == 0) return SHOWDUINO_LAMP_NODE_WIRE_EMERGENCY;
  /* Ownership-oriented names from the node still mean "present". */
  if (strcmp(v, "STANDALONE") == 0 || strcmp(v, "SHOW_CONTROLLED") == 0 ||
      strcmp(v, "SEARCHING") == 0 || strcmp(v, "IDLE") == 0) {
    return SHOWDUINO_LAMP_NODE_WIRE_ONLINE;
  }
  return SHOWDUINO_LAMP_NODE_WIRE_INVALID;
}

static inline ShowduinoNodeAvailWire showduino_lamp_wire_to_avail(ShowduinoLampNodeWire w) {
  if (w == SHOWDUINO_LAMP_NODE_WIRE_ONLINE || w == SHOWDUINO_LAMP_NODE_WIRE_ACTIVE) {
    return SHOWDUINO_NODE_WIRE_ONLINE;
  }
  if (w == SHOWDUINO_LAMP_NODE_WIRE_FAULT || w == SHOWDUINO_LAMP_NODE_WIRE_EMERGENCY) {
    return SHOWDUINO_NODE_WIRE_FAULT;
  }
  if (w == SHOWDUINO_LAMP_NODE_WIRE_OFFLINE) return SHOWDUINO_NODE_WIRE_OFFLINE;
  return SHOWDUINO_NODE_WIRE_INVALID;
}

#define SHOWDUINO_WIRE_STATE_NODE_PIXEL_DETAIL_PREFIX "STATE:NODE:PIXEL:D:"

typedef enum ShowduinoPixelNodeWire {
  SHOWDUINO_PIXEL_NODE_WIRE_OFFLINE = 0,
  SHOWDUINO_PIXEL_NODE_WIRE_ONLINE,
  SHOWDUINO_PIXEL_NODE_WIRE_FAULT,
  SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY,
  SHOWDUINO_PIXEL_NODE_WIRE_INVALID = -1
} ShowduinoPixelNodeWire;

typedef struct ShowduinoPixelDetailWire {
  uint8_t online;
  uint8_t seen;
  char firstId[16];
  char firstState[20];
} ShowduinoPixelDetailWire;

static inline ShowduinoPixelNodeWire showduino_parse_state_node_pixel(const char *line) {
  const size_t prefixLen = sizeof(SHOWDUINO_WIRE_STATE_NODE_PIXEL_PREFIX) - 1;
  if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_NODE_PIXEL_PREFIX, prefixLen) != 0) {
    return SHOWDUINO_PIXEL_NODE_WIRE_INVALID;
  }
  const char *v = line + prefixLen;
  if (v[0] && v[1] == ':' && v[0] == 'D') return SHOWDUINO_PIXEL_NODE_WIRE_INVALID;
  if (strcmp(v, "OFFLINE") == 0) return SHOWDUINO_PIXEL_NODE_WIRE_OFFLINE;
  if (strcmp(v, "ONLINE") == 0 || strcmp(v, "ACTIVE") == 0) return SHOWDUINO_PIXEL_NODE_WIRE_ONLINE;
  if (strcmp(v, "FAULT") == 0) return SHOWDUINO_PIXEL_NODE_WIRE_FAULT;
  if (strcmp(v, "EMERGENCY") == 0) return SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY;
  return SHOWDUINO_PIXEL_NODE_WIRE_INVALID;
}

static inline ShowduinoNodeAvailWire showduino_pixel_wire_to_avail(ShowduinoPixelNodeWire w) {
  if (w == SHOWDUINO_PIXEL_NODE_WIRE_ONLINE) return SHOWDUINO_NODE_WIRE_ONLINE;
  if (w == SHOWDUINO_PIXEL_NODE_WIRE_FAULT || w == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY) {
    return SHOWDUINO_NODE_WIRE_FAULT;
  }
  if (w == SHOWDUINO_PIXEL_NODE_WIRE_OFFLINE) return SHOWDUINO_NODE_WIRE_OFFLINE;
  return SHOWDUINO_NODE_WIRE_INVALID;
}

static inline int showduino_parse_state_node_pixel_detail(const char *line,
                                                         ShowduinoPixelDetailWire *out) {
  if (!line || !out) return 0;
  const size_t prefixLen = sizeof(SHOWDUINO_WIRE_STATE_NODE_PIXEL_DETAIL_PREFIX) - 1;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_NODE_PIXEL_DETAIL_PREFIX, prefixLen) != 0) return 0;
  memset(out, 0, sizeof(*out));
  const char *p = line + prefixLen;
  out->online = (uint8_t)strtoul(p, NULL, 10);
  const char *c1 = strchr(p, ':');
  if (!c1) return 1;
  out->seen = (uint8_t)strtoul(c1 + 1, NULL, 10);
  const char *c2 = strchr(c1 + 1, ':');
  if (!c2) return 1;
  const char *c3 = strchr(c2 + 1, ':');
  size_t n = c3 ? (size_t)(c3 - (c2 + 1)) : strlen(c2 + 1);
  if (n >= sizeof(out->firstId)) n = sizeof(out->firstId) - 1;
  memcpy(out->firstId, c2 + 1, n);
  out->firstId[n] = '\0';
  if (c3) strncpy(out->firstState, c3 + 1, sizeof(out->firstState) - 1);
  return 1;
}

static inline int showduino_parse_state_node_lamp_detail(const char *line,
                                                        ShowduinoLampDetailWire *out) {
  const char *p;
  const char *next;
  size_t n;
  int bri = 0;
  int digits = 0;
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_NODE_LAMP_DETAIL_PREFIX,
              sizeof(SHOWDUINO_WIRE_STATE_NODE_LAMP_DETAIL_PREFIX) - 1) != 0) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  p = line + (sizeof(SHOWDUINO_WIRE_STATE_NODE_LAMP_DETAIL_PREFIX) - 1);
  next = strchr(p, ':');
  if (!next) return 0;
  n = (size_t)(next - p);
  if (n >= sizeof(out->state)) n = sizeof(out->state) - 1;
  memcpy(out->state, p, n);
  p = next + 1;
  next = strchr(p, ':');
  if (!next) return 0;
  n = (size_t)(next - p);
  if (n >= sizeof(out->fx)) n = sizeof(out->fx) - 1;
  memcpy(out->fx, p, n);
  p = next + 1;
  while (*p >= '0' && *p <= '9') {
    bri = bri * 10 + (*p - '0');
    p++;
    digits++;
    if (digits > 3) return 0;
  }
  if (digits == 0 || *p != ':' || bri > 100) return 0;
  out->brightness = (uint8_t)bri;
  p++;
  next = strchr(p, ':');
  if (!next) {
    n = strlen(p);
    if (n >= sizeof(out->mac)) n = sizeof(out->mac) - 1;
    memcpy(out->mac, p, n);
    return 1;
  }
  n = (size_t)(next - p);
  if (n >= sizeof(out->mac)) n = sizeof(out->mac) - 1;
  memcpy(out->mac, p, n);
  p = next + 1;
  n = strlen(p);
  if (n >= sizeof(out->firmware)) n = sizeof(out->firmware) - 1;
  memcpy(out->firmware, p, n);
  return 1;
}

static inline int showduino_parse_state_node_lamp_sensors(const char *line,
                                                          ShowduinoLampSensorWire *out) {
  char *next;
  size_t n;
  char *fields[7];
  char buf[96];
  int i;
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_NODE_LAMP_SENSOR_PREFIX,
              sizeof(SHOWDUINO_WIRE_STATE_NODE_LAMP_SENSOR_PREFIX) - 1) != 0) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  {
    const char *p = line + (sizeof(SHOWDUINO_WIRE_STATE_NODE_LAMP_SENSOR_PREFIX) - 1);
    n = strlen(p);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, p, n);
    buf[n] = 0;
  }
  for (i = 0; i < 7; i++) fields[i] = NULL;
  fields[0] = buf;
  for (i = 1; i < 7; i++) {
    next = strchr(fields[i - 1], ':');
    if (!next) break;
    *next = 0;
    fields[i] = next + 1;
  }
  if (fields[0]) {
    n = strlen(fields[0]);
    if (n >= sizeof(out->button)) n = sizeof(out->button) - 1;
    memcpy(out->button, fields[0], n);
  }
  if (fields[1]) {
    n = strlen(fields[1]);
    if (n >= sizeof(out->mic)) n = sizeof(out->mic) - 1;
    memcpy(out->mic, fields[1], n);
  }
  if (fields[2]) {
    n = strlen(fields[2]);
    if (n >= sizeof(out->blow)) n = sizeof(out->blow) - 1;
    memcpy(out->blow, fields[2], n);
  }
  if (fields[3]) {
    n = strlen(fields[3]);
    if (n >= sizeof(out->motion)) n = sizeof(out->motion) - 1;
    memcpy(out->motion, fields[3], n);
  }
  if (fields[4]) {
    n = strlen(fields[4]);
    if (n >= sizeof(out->light)) n = sizeof(out->light) - 1;
    memcpy(out->light, fields[4], n);
  }
  if (fields[5]) {
    n = strlen(fields[5]);
    if (n >= sizeof(out->voltage)) n = sizeof(out->voltage) - 1;
    memcpy(out->voltage, fields[5], n);
  }
  if (fields[6]) {
    n = strlen(fields[6]);
    if (n >= sizeof(out->audio)) n = sizeof(out->audio) - 1;
    memcpy(out->audio, fields[6], n);
  }
  return 1;
}

typedef enum ShowduinoAudioNodeWire {
  SHOWDUINO_AUDIO_NODE_WIRE_OFFLINE = 0,
  SHOWDUINO_AUDIO_NODE_WIRE_ONLINE,
  SHOWDUINO_AUDIO_NODE_WIRE_PLAYING,
  SHOWDUINO_AUDIO_NODE_WIRE_LOOPING,
  SHOWDUINO_AUDIO_NODE_WIRE_PAUSED,
  SHOWDUINO_AUDIO_NODE_WIRE_FAULT,
  SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY,
  SHOWDUINO_AUDIO_NODE_WIRE_INVALID = -1
} ShowduinoAudioNodeWire;

#define SHOWDUINO_WIRE_STATE_NODE_AUDIO_DETAIL_PREFIX "STATE:NODE:AUDIO:D:"
#define SHOWDUINO_WIRE_STATE_NODE_AUDIO_INV_PREFIX    "STATE:NODE:AUDIO:I:"
#define SHOWDUINO_WIRE_STATE_NODE_AUDIO_CAPS_PREFIX   "STATE:NODE:AUDIO:C:"
#define SHOWDUINO_WIRE_STATE_NODE_AUDIO_META_PREFIX   "STATE:NODE:AUDIO:N:"
#define SHOWDUINO_WIRE_STATE_NODE_AUDIO_SOUND_PREFIX  "STATE:NODE:AUDIO:S:"
#define SHOWDUINO_LOGICAL_INPUT_SOUND                 "AUDIO_NODE_SOUND_TRIGGER"
#define SHOWDUINO_AUDIO_DETAIL_ASSET_MAX 63
#define SHOWDUINO_AUDIO_INV_WIRE_MAX 3
#define SHOWDUINO_AUDIO_INV_NAME_MAX 63

typedef struct ShowduinoAudioDetailWire {
  char state[8];
  uint8_t volume;
  char storage[4];
  char codec[4];
  char fault[8];
  char asset[SHOWDUINO_AUDIO_DETAIL_ASSET_MAX + 1];
} ShowduinoAudioDetailWire;

static inline ShowduinoAudioNodeWire showduino_parse_state_node_audio(const char *line) {
  const size_t prefixLen = sizeof(SHOWDUINO_WIRE_STATE_NODE_AUDIO_PREFIX) - 1;
  if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_NODE_AUDIO_PREFIX, prefixLen) != 0) {
    return SHOWDUINO_AUDIO_NODE_WIRE_INVALID;
  }
  const char *v = line + prefixLen;
  /* Extended Director lines share the prefix; coarse parser ignores them. */
  if (v[0] && v[1] == ':' &&
      (v[0] == 'D' || v[0] == 'I' || v[0] == 'C' || v[0] == 'N' || v[0] == 'S')) {
    return SHOWDUINO_AUDIO_NODE_WIRE_INVALID;
  }
  if (strcmp(v, "OFFLINE") == 0) return SHOWDUINO_AUDIO_NODE_WIRE_OFFLINE;
  if (strcmp(v, "ONLINE") == 0) return SHOWDUINO_AUDIO_NODE_WIRE_ONLINE;
  if (strcmp(v, "PLAYING") == 0) return SHOWDUINO_AUDIO_NODE_WIRE_PLAYING;
  if (strcmp(v, "LOOPING") == 0) return SHOWDUINO_AUDIO_NODE_WIRE_LOOPING;
  if (strcmp(v, "PAUSED") == 0) return SHOWDUINO_AUDIO_NODE_WIRE_PAUSED;
  if (strcmp(v, "FAULT") == 0) return SHOWDUINO_AUDIO_NODE_WIRE_FAULT;
  if (strcmp(v, "EMERGENCY") == 0) return SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY;
  return SHOWDUINO_AUDIO_NODE_WIRE_INVALID;
}

static inline int showduino_parse_state_node_audio_detail(const char *line,
                                                         ShowduinoAudioDetailWire *out) {
  const char *p;
  const char *next;
  size_t n;
  int vol = 0;
  int digits = 0;
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_NODE_AUDIO_DETAIL_PREFIX,
              sizeof(SHOWDUINO_WIRE_STATE_NODE_AUDIO_DETAIL_PREFIX) - 1) != 0) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  p = line + (sizeof(SHOWDUINO_WIRE_STATE_NODE_AUDIO_DETAIL_PREFIX) - 1);
  next = strchr(p, ':');
  if (!next) return 0;
  n = (size_t)(next - p);
  if (n >= sizeof(out->state)) n = sizeof(out->state) - 1;
  memcpy(out->state, p, n);
  p = next + 1;
  while (*p >= '0' && *p <= '9') {
    vol = vol * 10 + (*p - '0');
    p++;
    digits++;
    if (digits > 3) return 0;
  }
  if (digits == 0 || *p != ':' || vol > 100) return 0;
  out->volume = (uint8_t)vol;
  p++;
  next = strchr(p, ':');
  if (!next) return 0;
  n = (size_t)(next - p);
  if (n >= sizeof(out->storage)) n = sizeof(out->storage) - 1;
  memcpy(out->storage, p, n);
  p = next + 1;
  next = strchr(p, ':');
  if (!next) return 0;
  n = (size_t)(next - p);
  if (n >= sizeof(out->codec)) n = sizeof(out->codec) - 1;
  memcpy(out->codec, p, n);
  p = next + 1;
  next = strchr(p, ':');
  if (!next) return 0;
  n = (size_t)(next - p);
  if (n >= sizeof(out->fault)) n = sizeof(out->fault) - 1;
  memcpy(out->fault, p, n);
  p = next + 1;
  n = strlen(p);
  if (n >= sizeof(out->asset)) n = sizeof(out->asset) - 1;
  memcpy(out->asset, p, n);
  return 1;
}

typedef struct ShowduinoAudioSoundWire {
  char ready[6];
  uint8_t level;
  uint8_t peak;
  uint8_t floor;
  uint8_t threshold;
  uint8_t armed;
  uint16_t cooldown;
  char event[8];
  uint8_t calibrated;
} ShowduinoAudioSoundWire;

static inline int showduino_parse_kv_u32(const char *p, const char *key, uint32_t *out) {
  char needle[8];
  const char *f;
  if (!p || !key || !out) return 0;
  snprintf(needle, sizeof(needle), "%s=", key);
  f = strstr(p, needle);
  if (!f) return 0;
  *out = (uint32_t)strtoul(f + strlen(needle), NULL, 10);
  return 1;
}

static inline int showduino_parse_state_node_audio_sound(const char *line,
                                                         ShowduinoAudioSoundWire *out) {
  const char *p;
  const char *comma;
  size_t n;
  uint32_t v = 0;
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_NODE_AUDIO_SOUND_PREFIX,
              sizeof(SHOWDUINO_WIRE_STATE_NODE_AUDIO_SOUND_PREFIX) - 1) != 0) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  p = line + (sizeof(SHOWDUINO_WIRE_STATE_NODE_AUDIO_SOUND_PREFIX) - 1);
  comma = strchr(p, ',');
  n = comma ? (size_t)(comma - p) : strlen(p);
  if (n >= sizeof(out->ready)) n = sizeof(out->ready) - 1;
  memcpy(out->ready, p, n);
  if (showduino_parse_kv_u32(p, "L", &v)) out->level = (uint8_t)v;
  if (showduino_parse_kv_u32(p, "P", &v)) out->peak = (uint8_t)v;
  if (showduino_parse_kv_u32(p, "F", &v)) out->floor = (uint8_t)v;
  if (showduino_parse_kv_u32(p, "T", &v)) out->threshold = (uint8_t)v;
  if (showduino_parse_kv_u32(p, "A", &v)) out->armed = (uint8_t)v;
  if (showduino_parse_kv_u32(p, "C", &v)) out->cooldown = (uint16_t)v;
  if (showduino_parse_kv_u32(p, "K", &v)) out->calibrated = (uint8_t)v;
  {
    const char *e = strstr(p, "E=");
    if (e) {
      e += 2;
      comma = strchr(e, ',');
      n = comma ? (size_t)(comma - e) : strlen(e);
      if (n >= sizeof(out->event)) n = sizeof(out->event) - 1;
      memcpy(out->event, e, n);
    } else {
      strncpy(out->event, "NONE", sizeof(out->event) - 1);
    }
  }
  return 1;
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

#define SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_DETAIL_PREFIX "STATE:NODE:EMERGENCY:D:"
#define SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_STATION_PREFIX "STATE:NODE:EMERGENCY:S:"
#define SHOWDUINO_WIRE_SAFETY_ESTOP_OK    "ESTOP:OK"
#define SHOWDUINO_WIRE_SAFETY_ESTOP_FAULT "ESTOP:FAULT"

typedef enum ShowduinoEmergencyNodeWire {
  SHOWDUINO_EMERGENCY_NODE_WIRE_OFFLINE = 0,
  SHOWDUINO_EMERGENCY_NODE_WIRE_ONLINE,
  SHOWDUINO_EMERGENCY_NODE_WIRE_ACTIVE,
  SHOWDUINO_EMERGENCY_NODE_WIRE_FAULT,
  SHOWDUINO_EMERGENCY_NODE_WIRE_INVALID = -1
} ShowduinoEmergencyNodeWire;

typedef struct ShowduinoEmergencyDetailWire {
  uint8_t online;
  uint8_t seen;
  uint8_t asserting;
  uint8_t offline;
  char firstId[16];
  char firstName[20];
  char firstState[20];
} ShowduinoEmergencyDetailWire;

typedef struct ShowduinoEmergencySourceWire {
  char kind[12];
  char id[16];
  char name[20];
} ShowduinoEmergencySourceWire;

typedef struct ShowduinoEmergencyStationWire {
  uint8_t slot;
  uint8_t online;
  uint8_t input_open;
  uint8_t latched;
  uint8_t acked;
  char id[16];
  char name[20];
  char state[20];
} ShowduinoEmergencyStationWire;

static inline ShowduinoEmergencyNodeWire showduino_parse_state_node_emergency(const char *line) {
  const size_t prefixLen = sizeof(SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_PREFIX) - 1;
  const char *v;
  if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_PREFIX, prefixLen) != 0) {
    return SHOWDUINO_EMERGENCY_NODE_WIRE_INVALID;
  }
  v = line + prefixLen;
  if (v[0] == 'D' && v[1] == ':') return SHOWDUINO_EMERGENCY_NODE_WIRE_INVALID;
  if (strcmp(v, "OFFLINE") == 0) return SHOWDUINO_EMERGENCY_NODE_WIRE_OFFLINE;
  if (strcmp(v, "ONLINE") == 0) return SHOWDUINO_EMERGENCY_NODE_WIRE_ONLINE;
  if (strcmp(v, "ACTIVE") == 0) return SHOWDUINO_EMERGENCY_NODE_WIRE_ACTIVE;
  if (strcmp(v, "FAULT") == 0) return SHOWDUINO_EMERGENCY_NODE_WIRE_FAULT;
  return SHOWDUINO_EMERGENCY_NODE_WIRE_INVALID;
}

static inline int showduino_parse_state_node_emergency_detail(const char *line,
                                                             ShowduinoEmergencyDetailWire *out) {
  const char *p;
  const char *c;
  size_t n;
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_DETAIL_PREFIX,
              sizeof(SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_DETAIL_PREFIX) - 1) != 0) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  p = line + (sizeof(SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_DETAIL_PREFIX) - 1);
  out->online = (uint8_t)strtoul(p, NULL, 10);
  c = strchr(p, ':');
  if (!c) return 1;
  out->seen = (uint8_t)strtoul(c + 1, NULL, 10);
  c = strchr(c + 1, ':');
  if (!c) return 1;
  out->asserting = (uint8_t)strtoul(c + 1, NULL, 10);
  c = strchr(c + 1, ':');
  if (!c) return 1;
  out->offline = (uint8_t)strtoul(c + 1, NULL, 10);
  c = strchr(c + 1, ':');
  if (!c) return 1;
  p = c + 1;
  c = strchr(p, ':');
  n = c ? (size_t)(c - p) : strlen(p);
  if (n >= sizeof(out->firstId)) n = sizeof(out->firstId) - 1;
  memcpy(out->firstId, p, n);
  if (!c) return 1;
  p = c + 1;
  c = strchr(p, ':');
  n = c ? (size_t)(c - p) : strlen(p);
  if (n >= sizeof(out->firstName)) n = sizeof(out->firstName) - 1;
  memcpy(out->firstName, p, n);
  if (c) strncpy(out->firstState, c + 1, sizeof(out->firstState) - 1);
  return 1;
}

static inline int showduino_parse_state_emergency_source(const char *line,
                                                         ShowduinoEmergencySourceWire *out) {
  const char *p;
  const char *c;
  size_t n;
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX,
              sizeof(SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX) - 1) != 0) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  p = line + (sizeof(SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX) - 1);
  c = strchr(p, ':');
  n = c ? (size_t)(c - p) : strlen(p);
  if (n >= sizeof(out->kind)) n = sizeof(out->kind) - 1;
  memcpy(out->kind, p, n);
  if (!c) return 1;
  p = c + 1;
  c = strchr(p, ':');
  n = c ? (size_t)(c - p) : strlen(p);
  if (n >= sizeof(out->id)) n = sizeof(out->id) - 1;
  memcpy(out->id, p, n);
  if (c) strncpy(out->name, c + 1, sizeof(out->name) - 1);
  return 1;
}

static inline int showduino_parse_state_node_emergency_station(const char *line,
                                                               ShowduinoEmergencyStationWire *out) {
  const char *p;
  const char *c;
  size_t n;
  if (!line || !out) return 0;
  if (strncmp(line, SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_STATION_PREFIX,
              sizeof(SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_STATION_PREFIX) - 1) != 0) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  p = line + (sizeof(SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_STATION_PREFIX) - 1);
  out->slot = (uint8_t)strtoul(p, NULL, 10);
  c = strchr(p, ':');
  if (!c) return 1;
  p = c + 1;
  c = strchr(p, ':');
  n = c ? (size_t)(c - p) : strlen(p);
  if (n >= sizeof(out->id)) n = sizeof(out->id) - 1;
  memcpy(out->id, p, n);
  if (!c) return 1;
  p = c + 1;
  c = strchr(p, ':');
  n = c ? (size_t)(c - p) : strlen(p);
  if (n >= sizeof(out->name)) n = sizeof(out->name) - 1;
  memcpy(out->name, p, n);
  if (!c) return 1;
  out->online = (uint8_t)strtoul(c + 1, NULL, 10);
  c = strchr(c + 1, ':');
  if (!c) return 1;
  out->input_open = (uint8_t)strtoul(c + 1, NULL, 10);
  c = strchr(c + 1, ':');
  if (!c) return 1;
  out->latched = (uint8_t)strtoul(c + 1, NULL, 10);
  c = strchr(c + 1, ':');
  if (!c) return 1;
  out->acked = (uint8_t)strtoul(c + 1, NULL, 10);
  c = strchr(c + 1, ':');
  if (c) strncpy(out->state, c + 1, sizeof(out->state) - 1);
  return 1;
}

static inline int showduino_parse_state_safety_estop_fault(const char *line) {
  if (!line) return -1;
  if (strcmp(line, "STATE:SAFETY:ESTOP:FAULT") == 0) return 1;
  if (strcmp(line, "STATE:SAFETY:ESTOP:OK") == 0) return 0;
  return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_STATE_WIRE_H */
