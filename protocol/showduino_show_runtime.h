#ifndef SHOWDUINO_SHOW_RUNTIME_H
#define SHOWDUINO_SHOW_RUNTIME_H

/*
 * Showduino OS Stage 6 — shared ShowRuntime (single source of truth schema).
 * Stage Engine owns the authoritative instance; Director mirrors read-only.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ShowState {
  SHOW_STATE_BOOTING = 0,
  SHOW_STATE_IDLE,
  SHOW_STATE_SHOW_LOADED,
  SHOW_STATE_RUNNING,
  SHOW_STATE_PAUSED,
  SHOW_STATE_EMERGENCY_STOP,
  SHOW_STATE_FINISHED,
  SHOW_STATE_ERROR
} ShowState;

typedef struct ShowRuntime {
  ShowState state;
  char showName[64];
  uint32_t elapsedMs;
  uint32_t remainingMs;
  uint32_t totalDurationMs;
  uint32_t currentCue;
  uint32_t totalCues;
  uint8_t loaded;
  uint8_t running;
  uint8_t paused;
  uint8_t emergency;
  uint8_t finished;
  uint8_t aborted;
  uint8_t stageConnected;
  uint32_t stateEnteredMs;
  char lastError[96];
  uint32_t revision; /* bumps on every authoritative change */
} ShowRuntime;

#define SHOW_RUNTIME_WIRE_PREFIX   "SHOW:RUNTIME:"
#define SHOW_STATE_WIRE_PREFIX     "SHOW:STATE:"
#define SHOW_FINISHED_WIRE         "SHOW:FINISHED"
#define SHOW_STATE_QUERY           "SHOW:STATE?"

static inline void showRuntimeClear(ShowRuntime *rt) {
  if (!rt) return;
  memset(rt, 0, sizeof(*rt));
  rt->state = SHOW_STATE_IDLE;
}

static inline void showRuntimeSyncFlags(ShowRuntime *rt) {
  if (!rt) return;
  rt->running = (rt->state == SHOW_STATE_RUNNING) ? 1 : 0;
  rt->paused = (rt->state == SHOW_STATE_PAUSED) ? 1 : 0;
  rt->emergency = (rt->state == SHOW_STATE_EMERGENCY_STOP) ? 1 : 0;
  rt->finished = (rt->state == SHOW_STATE_FINISHED) ? 1 : 0;
  rt->loaded = (rt->state == SHOW_STATE_SHOW_LOADED ||
                rt->state == SHOW_STATE_RUNNING ||
                rt->state == SHOW_STATE_PAUSED ||
                rt->state == SHOW_STATE_EMERGENCY_STOP ||
                rt->state == SHOW_STATE_FINISHED) ? 1 : 0;
}

static inline const char *showStateName(ShowState s) {
  switch (s) {
    case SHOW_STATE_BOOTING:         return "BOOTING";
    case SHOW_STATE_IDLE:            return "IDLE";
    case SHOW_STATE_SHOW_LOADED:     return "SHOW_LOADED";
    case SHOW_STATE_RUNNING:         return "RUNNING";
    case SHOW_STATE_PAUSED:          return "PAUSED";
    case SHOW_STATE_EMERGENCY_STOP:  return "EMERGENCY_STOP";
    case SHOW_STATE_FINISHED:        return "FINISHED";
    case SHOW_STATE_ERROR:           return "ERROR";
    default:                         return "ERROR";
  }
}

static inline ShowState showStateFromName(const char *name) {
  if (!name) return SHOW_STATE_ERROR;
  if (strcmp(name, "BOOTING") == 0) return SHOW_STATE_BOOTING;
  if (strcmp(name, "IDLE") == 0) return SHOW_STATE_IDLE;
  if (strcmp(name, "SHOW_LOADED") == 0) return SHOW_STATE_SHOW_LOADED;
  if (strcmp(name, "RUNNING") == 0) return SHOW_STATE_RUNNING;
  if (strcmp(name, "PAUSED") == 0) return SHOW_STATE_PAUSED;
  if (strcmp(name, "EMERGENCY_STOP") == 0) return SHOW_STATE_EMERGENCY_STOP;
  if (strcmp(name, "FINISHED") == 0) return SHOW_STATE_FINISHED;
  if (strcmp(name, "ERROR") == 0) return SHOW_STATE_ERROR;
  /* Legacy STATE:SHOW tokens */
  if (strcmp(name, "PLAYING") == 0) return SHOW_STATE_RUNNING;
  if (strcmp(name, "EMERGENCY") == 0) return SHOW_STATE_EMERGENCY_STOP;
  if (strcmp(name, "FAULT") == 0) return SHOW_STATE_ERROR;
  return SHOW_STATE_ERROR;
}

/** Returns 1 if transition is legal. */
static inline int showRuntimeCanTransition(ShowState from, ShowState to) {
  if (from == to) return 1;
  switch (from) {
    case SHOW_STATE_BOOTING:
      return (to == SHOW_STATE_IDLE || to == SHOW_STATE_ERROR);
    case SHOW_STATE_IDLE:
      return (to == SHOW_STATE_SHOW_LOADED || to == SHOW_STATE_ERROR ||
              to == SHOW_STATE_EMERGENCY_STOP || to == SHOW_STATE_BOOTING);
    case SHOW_STATE_SHOW_LOADED:
      return (to == SHOW_STATE_RUNNING || to == SHOW_STATE_IDLE ||
              to == SHOW_STATE_ERROR || to == SHOW_STATE_EMERGENCY_STOP);
    case SHOW_STATE_RUNNING:
      return (to == SHOW_STATE_PAUSED || to == SHOW_STATE_EMERGENCY_STOP ||
              to == SHOW_STATE_FINISHED || to == SHOW_STATE_IDLE ||
              to == SHOW_STATE_ERROR);
    case SHOW_STATE_PAUSED:
      return (to == SHOW_STATE_RUNNING || to == SHOW_STATE_EMERGENCY_STOP ||
              to == SHOW_STATE_IDLE || to == SHOW_STATE_ERROR);
    case SHOW_STATE_EMERGENCY_STOP:
      return (to == SHOW_STATE_RUNNING || to == SHOW_STATE_PAUSED ||
              to == SHOW_STATE_IDLE || to == SHOW_STATE_ERROR);
    case SHOW_STATE_FINISHED:
      return (to == SHOW_STATE_IDLE || to == SHOW_STATE_SHOW_LOADED ||
              to == SHOW_STATE_ERROR);
    case SHOW_STATE_ERROR:
      return (to == SHOW_STATE_IDLE || to == SHOW_STATE_BOOTING);
    default:
      return 0;
  }
}

/**
 * Apply transition. Returns 1 on success.
 * nowMs used for stateEnteredMs.
 */
static inline int showRuntimeTransition(ShowRuntime *rt, ShowState to, uint32_t nowMs) {
  if (!rt) return 0;
  if (!showRuntimeCanTransition(rt->state, to)) return 0;
  if (rt->state != to) {
    rt->state = to;
    rt->stateEnteredMs = nowMs;
    rt->revision++;
  }
  showRuntimeSyncFlags(rt);
  return 1;
}

static inline void showRuntimeSetError(ShowRuntime *rt, const char *err, uint32_t nowMs) {
  if (!rt) return;
  if (err) {
    strncpy(rt->lastError, err, sizeof(rt->lastError) - 1);
    rt->lastError[sizeof(rt->lastError) - 1] = '\0';
  }
  showRuntimeTransition(rt, SHOW_STATE_ERROR, nowMs);
}

/** One-letter state codes — required so SHOW:RUNTIME fits ESP-NOW desk cmd (95). */
static inline char showStateWireCode(ShowState s) {
  switch (s) {
    case SHOW_STATE_BOOTING:        return 'B';
    case SHOW_STATE_IDLE:           return 'I';
    case SHOW_STATE_SHOW_LOADED:    return 'L';
    case SHOW_STATE_RUNNING:        return 'R';
    case SHOW_STATE_PAUSED:         return 'P';
    case SHOW_STATE_EMERGENCY_STOP: return 'E';
    case SHOW_STATE_FINISHED:       return 'F';
    case SHOW_STATE_ERROR:          return 'X';
    default:                        return 'X';
  }
}

static inline ShowState showStateFromWireToken(const char *tok) {
  if (!tok || !tok[0]) return SHOW_STATE_ERROR;
  if (tok[1] == '\0') {
    switch (tok[0]) {
      case 'B': return SHOW_STATE_BOOTING;
      case 'I': return SHOW_STATE_IDLE;
      case 'L': return SHOW_STATE_SHOW_LOADED;
      case 'R': return SHOW_STATE_RUNNING;
      case 'P': return SHOW_STATE_PAUSED;
      case 'E': return SHOW_STATE_EMERGENCY_STOP;
      case 'F': return SHOW_STATE_FINISHED;
      case 'X': return SHOW_STATE_ERROR;
      default: break;
    }
  }
  return showStateFromName(tok);
}

/**
 * Compact wire (must fit SHOWDUINO_DESK_COMMAND_MAX-1 = 95):
 * SHOW:RUNTIME:rev|S|name|elapsed|remain|total|cue|cues|flags
 * S = B|I|L|R|P|E|F|X   flags = loaded|running|paused|emergency|finished|aborted|stageConnected
 * lastError is not on this line — use SHOW:STATE:ERROR + keep name short.
 */
static inline int showRuntimeSerialize(const ShowRuntime *rt, char *out, size_t outLen) {
  if (!rt || !out || outLen < 24) return 0;
  unsigned flags = 0;
  if (rt->loaded) flags |= 1;
  if (rt->running) flags |= 2;
  if (rt->paused) flags |= 4;
  if (rt->emergency) flags |= 8;
  if (rt->finished) flags |= 16;
  if (rt->aborted) flags |= 32;
  if (rt->stageConnected) flags |= 64;

  char name[32];
  strncpy(name, rt->showName, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  for (char *p = name; *p; p++) if (*p == '|' || *p == '\n') *p = '_';

  /* Shrink name until the line fits the desk packet command field. */
  for (;;) {
    int n = snprintf(out, outLen, "%s%lu|%c|%s|%lu|%lu|%lu|%lu|%lu|%u",
                     SHOW_RUNTIME_WIRE_PREFIX,
                     (unsigned long)rt->revision,
                     showStateWireCode(rt->state),
                     name,
                     (unsigned long)rt->elapsedMs,
                     (unsigned long)rt->remainingMs,
                     (unsigned long)rt->totalDurationMs,
                     (unsigned long)rt->currentCue,
                     (unsigned long)rt->totalCues,
                     flags);
    if (n > 0 && (size_t)n < outLen && n < 96) return 1;
    size_t len = strlen(name);
    if (len <= 4) return 0;
    name[len / 2] = '\0';
  }
}

static inline int showRuntimeParse(const char *line, ShowRuntime *out) {
  if (!line || !out) return 0;
  const char *p = line;
  if (strncmp(p, SHOW_RUNTIME_WIRE_PREFIX, strlen(SHOW_RUNTIME_WIRE_PREFIX)) == 0) {
    p += strlen(SHOW_RUNTIME_WIRE_PREFIX);
  }

  ShowRuntime rt;
  showRuntimeClear(&rt);

  char buf[128];
  strncpy(buf, p, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *cursor = buf;
  int field = 0;
  while (cursor && field < 12) {
    char *tok = cursor;
    char *sep = strchr(cursor, '|');
    if (sep) {
      *sep = '\0';
      cursor = sep + 1;
    } else {
      cursor = NULL;
    }
    switch (field) {
      case 0: rt.revision = (uint32_t)strtoul(tok, NULL, 10); break;
      case 1: rt.state = showStateFromWireToken(tok); break;
      case 2: strncpy(rt.showName, tok, sizeof(rt.showName) - 1); break;
      case 3: rt.elapsedMs = (uint32_t)strtoul(tok, NULL, 10); break;
      case 4: rt.remainingMs = (uint32_t)strtoul(tok, NULL, 10); break;
      case 5: rt.totalDurationMs = (uint32_t)strtoul(tok, NULL, 10); break;
      case 6: rt.currentCue = (uint32_t)strtoul(tok, NULL, 10); break;
      case 7: rt.totalCues = (uint32_t)strtoul(tok, NULL, 10); break;
      case 8: {
        unsigned flags = (unsigned)strtoul(tok, NULL, 10);
        rt.loaded = (flags & 1) ? 1 : 0;
        rt.running = (flags & 2) ? 1 : 0;
        rt.paused = (flags & 4) ? 1 : 0;
        rt.emergency = (flags & 8) ? 1 : 0;
        rt.finished = (flags & 16) ? 1 : 0;
        rt.aborted = (flags & 32) ? 1 : 0;
        rt.stageConnected = (flags & 64) ? 1 : 0;
        break;
      }
      case 9: strncpy(rt.lastError, tok, sizeof(rt.lastError) - 1); break;
      default: break;
    }
    field++;
  }
  if (field < 8) return 0;
  showRuntimeSyncFlags(&rt);
  *out = rt;
  return 1;
}

/** SHOW:STATE:<NAME> helper — always short enough for desk packet. */
static inline int showStateSerialize(ShowState s, char *out, size_t outLen) {
  if (!out || outLen < 16) return 0;
  int n = snprintf(out, outLen, "%s%s", SHOW_STATE_WIRE_PREFIX, showStateName(s));
  return (n > 0 && (size_t)n < outLen) ? 1 : 0;
}

/** Map new ShowState → legacy STATE:SHOW token for Stage 3 widgets. */
static inline const char *showStateLegacyToken(ShowState s) {
  switch (s) {
    case SHOW_STATE_RUNNING: return "PLAYING";
    case SHOW_STATE_PAUSED: return "PAUSED";
    case SHOW_STATE_EMERGENCY_STOP: return "EMERGENCY";
    case SHOW_STATE_ERROR: return "FAULT";
    case SHOW_STATE_FINISHED:
    case SHOW_STATE_SHOW_LOADED:
    case SHOW_STATE_IDLE:
    case SHOW_STATE_BOOTING:
    default: return "IDLE";
  }
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_SHOW_RUNTIME_H */
