#ifndef SHOWDUINO_AUDIO_NODE_H
#define SHOWDUINO_AUDIO_NODE_H

/*
 * Host-testable Audio Node command, path, state, and lifecycle rules.
 * No Arduino, I2S, SD, or ESP-NOW side effects.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_AUDIO_NODE_TYPE          "AUDIO"
#define SHOWDUINO_AUDIO_NODE_NAME          "Audio Node"
#define SHOWDUINO_AUDIO_ROOT               "/showduino/audio"
#define SHOWDUINO_AUDIO_PATH_MAX           79
#define SHOWDUINO_AUDIO_REL_MAX            63
#define SHOWDUINO_AUDIO_VOLUME_MAX         100
#define SHOWDUINO_AUDIO_COMMS_TIMEOUT_MS   5000u
#define SHOWDUINO_AUDIO_TEST_FILE          "system-test.wav"
#define SHOWDUINO_AUDIO_CAPS \
  "WAV,PLAY,LOOP,STOP,VOL,PAUSE,RESUME,FADE,DUCK,SPK,HP,LINE,INV,MIC,RECORD"
#define SHOWDUINO_AUDIO_INV_PER_PAGE       6
#define SHOWDUINO_AUDIO_INV_MAX            48
#define SHOWDUINO_AUDIO_FADE_MAX_MS        15000
#define SHOWDUINO_AUDIO_PROTOCOL           "1.2"

typedef enum ShowduinoAudioNodeState {
  SHOWDUINO_AUDIO_ST_UNKNOWN = 0,
  SHOWDUINO_AUDIO_ST_OFFLINE,
  SHOWDUINO_AUDIO_ST_BOOTING,
  SHOWDUINO_AUDIO_ST_IDLE,
  SHOWDUINO_AUDIO_ST_LOADING,
  SHOWDUINO_AUDIO_ST_PLAYING,
  SHOWDUINO_AUDIO_ST_LOOPING,
  SHOWDUINO_AUDIO_ST_PAUSED,
  SHOWDUINO_AUDIO_ST_STOPPING,
  SHOWDUINO_AUDIO_ST_EMERGENCY,
  SHOWDUINO_AUDIO_ST_FAULT,
  SHOWDUINO_AUDIO_ST_NO_STORAGE
} ShowduinoAudioNodeState;

typedef enum ShowduinoAudioCmd {
  SHOWDUINO_AUDIO_CMD_NONE = 0,
  SHOWDUINO_AUDIO_CMD_PLAY,
  SHOWDUINO_AUDIO_CMD_LOOP,
  SHOWDUINO_AUDIO_CMD_STOP,
  SHOWDUINO_AUDIO_CMD_PAUSE,
  SHOWDUINO_AUDIO_CMD_RESUME,
  SHOWDUINO_AUDIO_CMD_VOLUME,
  SHOWDUINO_AUDIO_CMD_STATUS,
  SHOWDUINO_AUDIO_CMD_TEST,
  SHOWDUINO_AUDIO_CMD_DUCK,
  SHOWDUINO_AUDIO_CMD_UNDUCK,
  SHOWDUINO_AUDIO_CMD_INVENTORY,
  SHOWDUINO_AUDIO_CMD_EMERGENCY_STOP,
  SHOWDUINO_AUDIO_CMD_EMERGENCY_CLEAR,
  SHOWDUINO_AUDIO_CMD_LOCAL_REJECT
} ShowduinoAudioCmd;

typedef enum ShowduinoAudioPriority {
  SHOWDUINO_AUDIO_PRI_AMBIENCE = 0,
  SHOWDUINO_AUDIO_PRI_DIALOGUE = 1,
  SHOWDUINO_AUDIO_PRI_SFX = 2
} ShowduinoAudioPriority;

typedef enum ShowduinoAudioPathStatus {
  SHOWDUINO_AUDIO_PATH_OK = 0,
  SHOWDUINO_AUDIO_PATH_EMPTY,
  SHOWDUINO_AUDIO_PATH_TOO_LONG,
  SHOWDUINO_AUDIO_PATH_TRAVERSAL,
  SHOWDUINO_AUDIO_PATH_BAD_CHAR,
  SHOWDUINO_AUDIO_PATH_BAD_EXT,
  SHOWDUINO_AUDIO_PATH_OUTSIDE_ROOT
} ShowduinoAudioPathStatus;

typedef enum ShowduinoAudioLifecycle {
  SHOWDUINO_AUDIO_LIFE_NONE = 0,
  SHOWDUINO_AUDIO_LIFE_ACCEPTED,
  SHOWDUINO_AUDIO_LIFE_REJECTED,
  SHOWDUINO_AUDIO_LIFE_STARTED,
  SHOWDUINO_AUDIO_LIFE_COMPLETED,
  SHOWDUINO_AUDIO_LIFE_FAILED
} ShowduinoAudioLifecycle;

typedef enum ShowduinoAudioFail {
  SHOWDUINO_AUDIO_FAIL_NONE = 0,
  SHOWDUINO_AUDIO_FAIL_BAD_COMMAND,
  SHOWDUINO_AUDIO_FAIL_BAD_PATH,
  SHOWDUINO_AUDIO_FAIL_FILE_NOT_FOUND,
  SHOWDUINO_AUDIO_FAIL_UNSUPPORTED,
  SHOWDUINO_AUDIO_FAIL_EMERGENCY,
  SHOWDUINO_AUDIO_FAIL_FAULT,
  SHOWDUINO_AUDIO_FAIL_OFFLINE,
  SHOWDUINO_AUDIO_FAIL_CODEC,
  SHOWDUINO_AUDIO_FAIL_STORAGE,
  SHOWDUINO_AUDIO_FAIL_NO_STORAGE,
  SHOWDUINO_AUDIO_FAIL_CONFIG_FAULT,
  SHOWDUINO_AUDIO_FAIL_COMMS_TIMEOUT
} ShowduinoAudioFail;

typedef enum ShowduinoAudioButton {
  SHOWDUINO_AUDIO_BTN_NONE = 0,
  SHOWDUINO_AUDIO_BTN_PLAY,
  SHOWDUINO_AUDIO_BTN_VOL_UP,
  SHOWDUINO_AUDIO_BTN_VOL_DOWN,
  SHOWDUINO_AUDIO_BTN_MODE,
  SHOWDUINO_AUDIO_BTN_REC,
  SHOWDUINO_AUDIO_BTN_SET,
  SHOWDUINO_AUDIO_BTN_PREV,
  SHOWDUINO_AUDIO_BTN_NEXT
} ShowduinoAudioButton;

typedef enum ShowduinoAudioCfgStatus {
  SHOWDUINO_AUDIO_CFG_OK = 0,
  SHOWDUINO_AUDIO_CFG_BAD
} ShowduinoAudioCfgStatus;

typedef struct ShowduinoAudioConfig {
  uint8_t formatVersion;
  uint8_t volume;
  uint8_t startupVolume;
  uint8_t duckVolume;
  uint16_t commsTimeoutMs;
  uint16_t fadeDefaultMs;
  char output[12];
} ShowduinoAudioConfig;

static inline const char *showduino_audio_state_name(ShowduinoAudioNodeState st) {
  switch (st) {
    case SHOWDUINO_AUDIO_ST_UNKNOWN: return "UNKNOWN";
    case SHOWDUINO_AUDIO_ST_OFFLINE: return "OFFLINE";
    case SHOWDUINO_AUDIO_ST_BOOTING: return "BOOTING";
    case SHOWDUINO_AUDIO_ST_IDLE: return "IDLE";
    case SHOWDUINO_AUDIO_ST_LOADING: return "LOADING";
    case SHOWDUINO_AUDIO_ST_PLAYING: return "PLAYING";
    case SHOWDUINO_AUDIO_ST_LOOPING: return "LOOPING";
    case SHOWDUINO_AUDIO_ST_PAUSED: return "PAUSED";
    case SHOWDUINO_AUDIO_ST_STOPPING: return "STOPPING";
    case SHOWDUINO_AUDIO_ST_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_AUDIO_ST_FAULT: return "FAULT";
    case SHOWDUINO_AUDIO_ST_NO_STORAGE: return "NO_STORAGE";
    default: return "UNKNOWN";
  }
}

static inline const char *showduino_audio_fail_name(ShowduinoAudioFail f) {
  switch (f) {
    case SHOWDUINO_AUDIO_FAIL_NONE: return "NONE";
    case SHOWDUINO_AUDIO_FAIL_BAD_COMMAND: return "BAD_COMMAND";
    case SHOWDUINO_AUDIO_FAIL_BAD_PATH: return "BAD_PATH";
    case SHOWDUINO_AUDIO_FAIL_FILE_NOT_FOUND: return "FILE_NOT_FOUND";
    case SHOWDUINO_AUDIO_FAIL_UNSUPPORTED: return "UNSUPPORTED";
    case SHOWDUINO_AUDIO_FAIL_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_AUDIO_FAIL_FAULT: return "FAULT";
    case SHOWDUINO_AUDIO_FAIL_OFFLINE: return "OFFLINE";
    case SHOWDUINO_AUDIO_FAIL_CODEC: return "CODEC";
    case SHOWDUINO_AUDIO_FAIL_STORAGE: return "STORAGE";
    case SHOWDUINO_AUDIO_FAIL_NO_STORAGE: return "NO_STORAGE";
    case SHOWDUINO_AUDIO_FAIL_CONFIG_FAULT: return "CONFIG_FAULT";
    case SHOWDUINO_AUDIO_FAIL_COMMS_TIMEOUT: return "COMMS_TIMEOUT";
    default: return "FAILED";
  }
}

static inline int showduino_audio_volume_ok(int v) {
  return v >= 0 && v <= SHOWDUINO_AUDIO_VOLUME_MAX;
}

static inline int showduino_audio_clamp_volume(int v) {
  if (v < 0) return 0;
  if (v > SHOWDUINO_AUDIO_VOLUME_MAX) return SHOWDUINO_AUDIO_VOLUME_MAX;
  return v;
}

static inline int showduino_audio_has_wav_ext(const char *path) {
  size_t n;
  if (!path) return 0;
  n = strlen(path);
  if (n < 4) return 0;
  return path[n - 4] == '.' &&
         (path[n - 3] == 'w' || path[n - 3] == 'W') &&
         (path[n - 2] == 'a' || path[n - 2] == 'A') &&
         (path[n - 1] == 'v' || path[n - 1] == 'V');
}

static inline ShowduinoAudioPathStatus showduino_audio_path_check(const char *rel) {
  size_t i, n;
  if (!rel || !rel[0]) return SHOWDUINO_AUDIO_PATH_EMPTY;
  n = strlen(rel);
  if (n > SHOWDUINO_AUDIO_REL_MAX) return SHOWDUINO_AUDIO_PATH_TOO_LONG;
  if (strstr(rel, "..") || strchr(rel, '\\') || strstr(rel, "//")) {
    return SHOWDUINO_AUDIO_PATH_TRAVERSAL;
  }
  for (i = 0; i < n; i++) {
    const char c = rel[i];
    const int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                   c == '.' || c == '/';
    if (!ok) return SHOWDUINO_AUDIO_PATH_BAD_CHAR;
  }
  if (!showduino_audio_has_wav_ext(rel)) return SHOWDUINO_AUDIO_PATH_BAD_EXT;
  return SHOWDUINO_AUDIO_PATH_OK;
}

static inline ShowduinoAudioPathStatus showduino_audio_resolve_path(const char *rel,
                                                                   char *out,
                                                                   size_t outLen) {
  const char *use = rel;
  size_t rootLen;
  size_t useLen;
  ShowduinoAudioPathStatus st;
  if (!out || outLen < 8) return SHOWDUINO_AUDIO_PATH_TOO_LONG;
  out[0] = '\0';
  if (!rel || !rel[0]) return SHOWDUINO_AUDIO_PATH_EMPTY;

  if (rel[0] == '/') {
    rootLen = strlen(SHOWDUINO_AUDIO_ROOT);
    if (strncmp(rel, SHOWDUINO_AUDIO_ROOT, rootLen) != 0) return SHOWDUINO_AUDIO_PATH_OUTSIDE_ROOT;
    if (rel[rootLen] != '/' && rel[rootLen] != '\0') return SHOWDUINO_AUDIO_PATH_OUTSIDE_ROOT;
    use = (rel[rootLen] == '/') ? rel + rootLen + 1 : "";
    if (!use[0]) return SHOWDUINO_AUDIO_PATH_EMPTY;
  }

  st = showduino_audio_path_check(use);
  if (st != SHOWDUINO_AUDIO_PATH_OK) return st;

  useLen = strlen(use);
  rootLen = strlen(SHOWDUINO_AUDIO_ROOT);
  if (rootLen + 1 + useLen + 1 > outLen || rootLen + 1 + useLen > SHOWDUINO_AUDIO_PATH_MAX) {
    return SHOWDUINO_AUDIO_PATH_TOO_LONG;
  }
  memcpy(out, SHOWDUINO_AUDIO_ROOT, rootLen);
  out[rootLen] = '/';
  memcpy(out + rootLen + 1, use, useLen + 1);
  return SHOWDUINO_AUDIO_PATH_OK;
}

static inline int showduino_audio_starts(const char *cmd, const char *pfx) {
  size_t n;
  if (!cmd || !pfx) return 0;
  n = strlen(pfx);
  return strncmp(cmd, pfx, n) == 0;
}

static inline int showduino_audio_parse_u32(const char *p, int maxDigits, int *out) {
  int v = 0;
  int digits = 0;
  if (!p || !out) return 0;
  while (*p >= '0' && *p <= '9') {
    v = v * 10 + (*p - '0');
    p++;
    digits++;
    if (digits > maxDigits) return 0;
  }
  if (digits == 0) return 0;
  if (*p != '\0' && *p != ':' && *p != ',' && *p != '}' &&
      *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
    return 0;
  }
  *out = v;
  return 1;
}

static inline void showduino_audio_strip_opts(char *arg, int *fadeMs, int *priOut) {
  char *fade;
  char *pri;
  char *cut;
  if (!arg) return;
  fade = strstr(arg, ":FADE=");
  pri = strstr(arg, ":PRI=");
  if (fade && fadeMs) {
    int ms = 0;
    if (showduino_audio_parse_u32(fade + 6, 5, &ms) &&
        ms >= 0 && ms <= SHOWDUINO_AUDIO_FADE_MAX_MS) {
      *fadeMs = ms;
    }
  }
  if (pri && priOut) {
    const char *v = pri + 5;
    if (strncmp(v, "SFX", 3) == 0) *priOut = SHOWDUINO_AUDIO_PRI_SFX;
    else if (strncmp(v, "DIALOGUE", 8) == 0) *priOut = SHOWDUINO_AUDIO_PRI_DIALOGUE;
    else if (strncmp(v, "AMBIENCE", 8) == 0) *priOut = SHOWDUINO_AUDIO_PRI_AMBIENCE;
  }
  cut = NULL;
  if (fade && (!pri || fade < pri)) cut = fade;
  else if (pri) cut = pri;
  if (cut) *cut = '\0';
}

static inline ShowduinoAudioPriority showduino_audio_priority_from_path(const char *rel) {
  if (!rel) return SHOWDUINO_AUDIO_PRI_AMBIENCE;
  if (strncmp(rel, "effects/", 8) == 0 || strncmp(rel, "stingers/", 9) == 0) {
    return SHOWDUINO_AUDIO_PRI_SFX;
  }
  if (strncmp(rel, "dialogue/", 9) == 0) return SHOWDUINO_AUDIO_PRI_DIALOGUE;
  return SHOWDUINO_AUDIO_PRI_AMBIENCE;
}

static inline int showduino_audio_can_preempt(ShowduinoAudioPriority cur,
                                              ShowduinoAudioPriority incoming) {
  return incoming >= cur;
}

static inline uint8_t showduino_audio_fade_level(uint8_t from, uint8_t to,
                                                 uint32_t elapsedMs, uint32_t durMs) {
  int32_t span;
  int32_t v;
  if (durMs == 0 || elapsedMs >= durMs) return to;
  span = (int32_t)to - (int32_t)from;
  v = (int32_t)from + (span * (int32_t)elapsedMs) / (int32_t)durMs;
  if (v < 0) return 0;
  if (v > 100) return 100;
  return (uint8_t)v;
}

static inline int showduino_audio_inventory_slice(uint16_t total, uint16_t page,
                                                  uint16_t *start, uint16_t *count) {
  uint16_t s;
  if (!start || !count) return 0;
  s = (uint16_t)(page * SHOWDUINO_AUDIO_INV_PER_PAGE);
  if (total == 0 || s >= total) {
    *start = 0;
    *count = 0;
    return 0;
  }
  *start = s;
  *count = (uint16_t)(total - s);
  if (*count > SHOWDUINO_AUDIO_INV_PER_PAGE) *count = SHOWDUINO_AUDIO_INV_PER_PAGE;
  return 1;
}

static inline const char *showduino_audio_wire_token(ShowduinoAudioNodeState st) {
  switch (st) {
    case SHOWDUINO_AUDIO_ST_OFFLINE: return "OFFLINE";
    case SHOWDUINO_AUDIO_ST_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_AUDIO_ST_FAULT:
    case SHOWDUINO_AUDIO_ST_NO_STORAGE: return "FAULT";
    case SHOWDUINO_AUDIO_ST_PLAYING:
    case SHOWDUINO_AUDIO_ST_LOADING: return "PLAYING";
    case SHOWDUINO_AUDIO_ST_LOOPING: return "LOOPING";
    case SHOWDUINO_AUDIO_ST_PAUSED: return "PAUSED";
    default: return "ONLINE";
  }
}

static inline ShowduinoAudioCmd showduino_audio_parse_command_ex(const char *cmd,
                                                                char *arg,
                                                                size_t argLen,
                                                                int *volumeOut,
                                                                int *fadeMsOut,
                                                                int *priOut) {
  if (arg && argLen) arg[0] = '\0';
  if (volumeOut) *volumeOut = -1;
  if (fadeMsOut) *fadeMsOut = -1;
  if (priOut) *priOut = -1;
  if (!cmd || !cmd[0]) return SHOWDUINO_AUDIO_CMD_NONE;

  if (showduino_audio_starts(cmd, "AUDIO:LOCAL:")) return SHOWDUINO_AUDIO_CMD_LOCAL_REJECT;

  if (strcmp(cmd, "EMERGENCY:STOP") == 0) return SHOWDUINO_AUDIO_CMD_EMERGENCY_STOP;
  if (strcmp(cmd, "EMERGENCY:CLEAR") == 0) return SHOWDUINO_AUDIO_CMD_EMERGENCY_CLEAR;
  if (strcmp(cmd, "AUDIO:NODE:STOP") == 0 || strcmp(cmd, "AUDIO:STOP") == 0) {
    return SHOWDUINO_AUDIO_CMD_STOP;
  }
  if (showduino_audio_starts(cmd, "AUDIO:NODE:STOP:FADE=")) {
    if (fadeMsOut) {
      int ms = 0;
      if (showduino_audio_parse_u32(cmd + 21, 5, &ms) &&
          ms >= 0 && ms <= SHOWDUINO_AUDIO_FADE_MAX_MS) {
        *fadeMsOut = ms;
      }
    }
    return SHOWDUINO_AUDIO_CMD_STOP;
  }
  if (strcmp(cmd, "AUDIO:NODE:PAUSE") == 0) return SHOWDUINO_AUDIO_CMD_PAUSE;
  if (strcmp(cmd, "AUDIO:NODE:RESUME") == 0) return SHOWDUINO_AUDIO_CMD_RESUME;
  if (strcmp(cmd, "AUDIO:NODE:STATUS") == 0 || strcmp(cmd, "AUDIO:STATUS") == 0) {
    return SHOWDUINO_AUDIO_CMD_STATUS;
  }
  if (strcmp(cmd, "AUDIO:NODE:DUCK") == 0) return SHOWDUINO_AUDIO_CMD_DUCK;
  if (strcmp(cmd, "AUDIO:NODE:UNDUCK") == 0) return SHOWDUINO_AUDIO_CMD_UNDUCK;
  if (strcmp(cmd, "AUDIO:NODE:INVENTORY") == 0) {
    if (arg && argLen > 1) {
      arg[0] = '0';
      arg[1] = '\0';
    }
    return SHOWDUINO_AUDIO_CMD_INVENTORY;
  }
  if (showduino_audio_starts(cmd, "AUDIO:NODE:INVENTORY:")) {
    const char *p = cmd + 21;
    if (arg && argLen) {
      size_t n = strlen(p);
      if (n + 1 < argLen) memcpy(arg, p, n + 1);
    }
    return SHOWDUINO_AUDIO_CMD_INVENTORY;
  }
  if (strcmp(cmd, "AUDIO:NODE:TEST") == 0 || strcmp(cmd, "AUDIO:TEST") == 0) {
    if (arg && argLen) {
      const char *t = SHOWDUINO_AUDIO_TEST_FILE;
      size_t n = strlen(t);
      if (n + 1 < argLen) memcpy(arg, t, n + 1);
    }
    return SHOWDUINO_AUDIO_CMD_TEST;
  }

  if (showduino_audio_starts(cmd, "AUDIO:NODE:PLAY:")) {
    const char *p = cmd + 16;
    if (arg && argLen) {
      size_t n = strlen(p);
      if (n + 1 >= argLen) return SHOWDUINO_AUDIO_CMD_NONE;
      memcpy(arg, p, n + 1);
      showduino_audio_strip_opts(arg, fadeMsOut, priOut);
    }
    return SHOWDUINO_AUDIO_CMD_PLAY;
  }
  if (showduino_audio_starts(cmd, "AUDIO:PLAY:")) {
    const char *p = cmd + 11;
    if (arg && argLen) {
      size_t n = strlen(p);
      if (n + 1 >= argLen) return SHOWDUINO_AUDIO_CMD_NONE;
      memcpy(arg, p, n + 1);
      showduino_audio_strip_opts(arg, fadeMsOut, priOut);
    }
    return SHOWDUINO_AUDIO_CMD_PLAY;
  }
  if (showduino_audio_starts(cmd, "AUDIO:NODE:LOOP:")) {
    const char *p = cmd + 16;
    if (arg && argLen) {
      size_t n = strlen(p);
      if (n + 1 >= argLen) return SHOWDUINO_AUDIO_CMD_NONE;
      memcpy(arg, p, n + 1);
      showduino_audio_strip_opts(arg, fadeMsOut, priOut);
    }
    return SHOWDUINO_AUDIO_CMD_LOOP;
  }
  if (showduino_audio_starts(cmd, "AUDIO:NODE:VOLUME:") ||
      showduino_audio_starts(cmd, "AUDIO:VOLUME:")) {
    const char *p = strchr(cmd + 12, ':');
    int v = 0;
    int digits = 0;
    if (!p) return SHOWDUINO_AUDIO_CMD_NONE;
    p++;
    if (*p == '\0') return SHOWDUINO_AUDIO_CMD_NONE;
    while (*p >= '0' && *p <= '9') {
      v = v * 10 + (*p - '0');
      p++;
      digits++;
      if (digits > 3) return SHOWDUINO_AUDIO_CMD_NONE;
    }
    if (*p != '\0' || digits == 0) return SHOWDUINO_AUDIO_CMD_NONE;
    if (!showduino_audio_volume_ok(v)) return SHOWDUINO_AUDIO_CMD_NONE;
    if (volumeOut) *volumeOut = v;
    return SHOWDUINO_AUDIO_CMD_VOLUME;
  }
  return SHOWDUINO_AUDIO_CMD_NONE;
}

static inline ShowduinoAudioCmd showduino_audio_parse_command(const char *cmd,
                                                             char *arg,
                                                             size_t argLen,
                                                             int *volumeOut) {
  return showduino_audio_parse_command_ex(cmd, arg, argLen, volumeOut, NULL, NULL);
}

static inline ShowduinoAudioFail showduino_audio_can_accept(ShowduinoAudioNodeState st,
                                                            ShowduinoAudioCmd cmd) {
  if (cmd == SHOWDUINO_AUDIO_CMD_LOCAL_REJECT) return SHOWDUINO_AUDIO_FAIL_UNSUPPORTED;
  if (cmd == SHOWDUINO_AUDIO_CMD_NONE) return SHOWDUINO_AUDIO_FAIL_BAD_COMMAND;
  if (cmd == SHOWDUINO_AUDIO_CMD_EMERGENCY_STOP ||
      cmd == SHOWDUINO_AUDIO_CMD_EMERGENCY_CLEAR ||
      cmd == SHOWDUINO_AUDIO_CMD_STATUS ||
      cmd == SHOWDUINO_AUDIO_CMD_STOP ||
      cmd == SHOWDUINO_AUDIO_CMD_INVENTORY) {
    return SHOWDUINO_AUDIO_FAIL_NONE;
  }
  if (st == SHOWDUINO_AUDIO_ST_EMERGENCY) return SHOWDUINO_AUDIO_FAIL_EMERGENCY;
  if (st == SHOWDUINO_AUDIO_ST_FAULT) return SHOWDUINO_AUDIO_FAIL_FAULT;
  if (st == SHOWDUINO_AUDIO_ST_NO_STORAGE) return SHOWDUINO_AUDIO_FAIL_NO_STORAGE;
  if (st == SHOWDUINO_AUDIO_ST_OFFLINE) return SHOWDUINO_AUDIO_FAIL_OFFLINE;
  if (cmd == SHOWDUINO_AUDIO_CMD_RESUME && st != SHOWDUINO_AUDIO_ST_PAUSED) {
    return SHOWDUINO_AUDIO_FAIL_BAD_COMMAND;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_PAUSE &&
      st != SHOWDUINO_AUDIO_ST_PLAYING &&
      st != SHOWDUINO_AUDIO_ST_LOOPING) {
    return SHOWDUINO_AUDIO_FAIL_BAD_COMMAND;
  }
  return SHOWDUINO_AUDIO_FAIL_NONE;
}

static inline ShowduinoAudioNodeState showduino_audio_next_state(ShowduinoAudioNodeState st,
                                                                ShowduinoAudioLifecycle life,
                                                                ShowduinoAudioCmd cmd) {
  if (cmd == SHOWDUINO_AUDIO_CMD_EMERGENCY_STOP) return SHOWDUINO_AUDIO_ST_EMERGENCY;
  if (cmd == SHOWDUINO_AUDIO_CMD_EMERGENCY_CLEAR) {
    if (st == SHOWDUINO_AUDIO_ST_FAULT) return SHOWDUINO_AUDIO_ST_FAULT;
    if (st == SHOWDUINO_AUDIO_ST_NO_STORAGE) return SHOWDUINO_AUDIO_ST_NO_STORAGE;
    return SHOWDUINO_AUDIO_ST_IDLE;
  }
  if (st == SHOWDUINO_AUDIO_ST_EMERGENCY || st == SHOWDUINO_AUDIO_ST_FAULT ||
      st == SHOWDUINO_AUDIO_ST_NO_STORAGE) {
    return st;
  }
  if (life == SHOWDUINO_AUDIO_LIFE_STARTED) {
    return (cmd == SHOWDUINO_AUDIO_CMD_LOOP) ? SHOWDUINO_AUDIO_ST_LOOPING
                                             : SHOWDUINO_AUDIO_ST_PLAYING;
  }
  if (life == SHOWDUINO_AUDIO_LIFE_COMPLETED || life == SHOWDUINO_AUDIO_LIFE_FAILED) {
    return SHOWDUINO_AUDIO_ST_IDLE;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_PAUSE) return SHOWDUINO_AUDIO_ST_PAUSED;
  if (cmd == SHOWDUINO_AUDIO_CMD_RESUME) return SHOWDUINO_AUDIO_ST_PLAYING;
  if (cmd == SHOWDUINO_AUDIO_CMD_STOP) return SHOWDUINO_AUDIO_ST_IDLE;
  return st;
}

/* Comms-loss policy: stop show audio; return IDLE. Do not resume. */
static inline ShowduinoAudioNodeState showduino_audio_on_comms_timeout(ShowduinoAudioNodeState st) {
  if (st == SHOWDUINO_AUDIO_ST_PLAYING || st == SHOWDUINO_AUDIO_ST_LOOPING ||
      st == SHOWDUINO_AUDIO_ST_PAUSED || st == SHOWDUINO_AUDIO_ST_LOADING ||
      st == SHOWDUINO_AUDIO_ST_STOPPING) {
    return SHOWDUINO_AUDIO_ST_IDLE;
  }
  return st;
}

static inline int showduino_audio_button_allowed(ShowduinoAudioNodeState st,
                                                 int showControlled,
                                                 ShowduinoAudioButton btn) {
  if (st == SHOWDUINO_AUDIO_ST_EMERGENCY) return 0;
  if (btn == SHOWDUINO_AUDIO_BTN_MODE || btn == SHOWDUINO_AUDIO_BTN_SET) return 0;
  if (btn == SHOWDUINO_AUDIO_BTN_VOL_UP || btn == SHOWDUINO_AUDIO_BTN_VOL_DOWN) return 1;
  if (btn == SHOWDUINO_AUDIO_BTN_PLAY || btn == SHOWDUINO_AUDIO_BTN_PREV ||
      btn == SHOWDUINO_AUDIO_BTN_NEXT) {
    if (showControlled) return 0;
    return st == SHOWDUINO_AUDIO_ST_IDLE || st == SHOWDUINO_AUDIO_ST_PLAYING ||
           st == SHOWDUINO_AUDIO_ST_LOOPING || st == SHOWDUINO_AUDIO_ST_PAUSED;
  }
  return 0;
}

static inline int showduino_audio_wav_pcm_ok(uint16_t audioFormat,
                                             uint16_t channels,
                                             uint32_t sampleRate,
                                             uint16_t bits) {
  if (audioFormat != 1) return 0; /* PCM only */
  if (channels != 1 && channels != 2) return 0;
  if (bits != 16) return 0;
  if (sampleRate != 8000u && sampleRate != 16000u && sampleRate != 22050u &&
      sampleRate != 32000u && sampleRate != 44100u && sampleRate != 48000u) {
    return 0;
  }
  return 1;
}

static inline void showduino_audio_config_defaults(ShowduinoAudioConfig *c) {
  if (!c) return;
  memset(c, 0, sizeof(*c));
  c->formatVersion = 1;
  c->volume = 80;
  c->startupVolume = 80;
  c->duckVolume = 30;
  c->commsTimeoutMs = SHOWDUINO_AUDIO_COMMS_TIMEOUT_MS;
  c->fadeDefaultMs = 0;
  memcpy(c->output, "SPEAKER", 8);
}

static inline int showduino_audio_output_ok(const char *s) {
  if (!s) return 0;
  return strcmp(s, "SPEAKER") == 0 || strcmp(s, "HEADPHONE") == 0 ||
         strcmp(s, "LINE") == 0 || strcmp(s, "AUTO") == 0;
}

static inline const char *showduino_audio_json_after_key(const char *json, const char *key) {
  char needle[40];
  size_t n;
  const char *p;
  if (!json || !key) return NULL;
  n = strlen(key);
  if (n + 3 >= sizeof(needle)) return NULL;
  needle[0] = '"';
  memcpy(needle + 1, key, n);
  needle[n + 1] = '"';
  needle[n + 2] = '\0';
  p = strstr(json, needle);
  if (!p) return NULL;
  p = strchr(p + n + 2, ':');
  if (!p) return NULL;
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  return p;
}

static inline ShowduinoAudioCfgStatus showduino_audio_config_parse(const char *json,
                                                                  size_t len,
                                                                  ShowduinoAudioConfig *out) {
  ShowduinoAudioConfig tmp;
  const char *p;
  if (!json || !out || len == 0) return SHOWDUINO_AUDIO_CFG_BAD;
  showduino_audio_config_defaults(&tmp);

  p = showduino_audio_json_after_key(json, "formatVersion");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 3, &v) || v != 1) return SHOWDUINO_AUDIO_CFG_BAD;
    tmp.formatVersion = 1;
  }
  p = showduino_audio_json_after_key(json, "volume");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 3, &v) || !showduino_audio_volume_ok(v)) {
      return SHOWDUINO_AUDIO_CFG_BAD;
    }
    tmp.volume = (uint8_t)v;
  }
  p = showduino_audio_json_after_key(json, "startupVolume");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 3, &v) || !showduino_audio_volume_ok(v)) {
      return SHOWDUINO_AUDIO_CFG_BAD;
    }
    tmp.startupVolume = (uint8_t)v;
  }
  p = showduino_audio_json_after_key(json, "duckVolume");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 3, &v) || !showduino_audio_volume_ok(v)) {
      return SHOWDUINO_AUDIO_CFG_BAD;
    }
    tmp.duckVolume = (uint8_t)v;
  }
  p = showduino_audio_json_after_key(json, "commsTimeoutMs");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 5, &v) || v < 1000 || v > 30000) {
      return SHOWDUINO_AUDIO_CFG_BAD;
    }
    tmp.commsTimeoutMs = (uint16_t)v;
  }
  p = showduino_audio_json_after_key(json, "fadeDefaultMs");
  if (p) {
    int v = 0;
    if (!showduino_audio_parse_u32(p, 5, &v) || v < 0 || v > SHOWDUINO_AUDIO_FADE_MAX_MS) {
      return SHOWDUINO_AUDIO_CFG_BAD;
    }
    tmp.fadeDefaultMs = (uint16_t)v;
  }
  p = showduino_audio_json_after_key(json, "output");
  if (p) {
    char outv[12];
    size_t i = 0;
    if (*p != '"') return SHOWDUINO_AUDIO_CFG_BAD;
    p++;
    while (*p && *p != '"' && i + 1 < sizeof(outv)) {
      outv[i++] = *p++;
    }
    outv[i] = '\0';
    if (*p != '"' || !showduino_audio_output_ok(outv)) return SHOWDUINO_AUDIO_CFG_BAD;
    memcpy(tmp.output, outv, sizeof(tmp.output));
  }

  *out = tmp;
  return SHOWDUINO_AUDIO_CFG_OK;
}

#ifdef __cplusplus
}
#endif

#endif
