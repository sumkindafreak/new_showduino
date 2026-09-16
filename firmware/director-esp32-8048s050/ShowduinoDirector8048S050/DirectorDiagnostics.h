#ifndef SHOWDUINO_DIRECTOR_DIAGNOSTICS_H
#define SHOWDUINO_DIRECTOR_DIAGNOSTICS_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "ShowduinoOsPalette.h"
#include "../../../protocol/showduino_legacy_strings.h"

/**
 * Director-side retention of P4 HELLO / sendCapabilities() reports.
 * Display only. Does not invent values or change emergency authority.
 *
 * Captured lines:
 *   SHOWDUINO_STAGE_ENGINE
 *   FW:<version>
 *   PIXELS:READY | PIXELS:FAULT
 *   ETHERNET:ONLINE | ETHERNET:OFFLINE
 *   AUDIO:READY | AUDIO:FAULT
 *   SD:<state>
 *   TIME:READY | TIME:UNSYNCED
 *   READY
 *
 * DMX / E1.31 / INPUTS lines are ignored on purpose.
 */

enum DirectorDiagTri : uint8_t {
  DIRECTOR_DIAG_UNREPORTED = 0,
  DIRECTOR_DIAG_READY,
  DIRECTOR_DIAG_FAULT,
  DIRECTOR_DIAG_ONLINE,
  DIRECTOR_DIAG_OFFLINE,
  DIRECTOR_DIAG_UNSYNCED,
  DIRECTOR_DIAG_DEGRADED
};

enum DirectorDiagHealth : uint8_t {
  DIRECTOR_DIAG_HEALTH_READY = 0,
  DIRECTOR_DIAG_HEALTH_DEGRADED,
  DIRECTOR_DIAG_HEALTH_EMERGENCY,
  DIRECTOR_DIAG_HEALTH_STAGE_OFFLINE,
  DIRECTOR_DIAG_HEALTH_FAULT
};

enum DirectorDiagSafetySync : uint8_t {
  DIRECTOR_DIAG_SYNC_OK = 0,
  DIRECTOR_DIAG_SYNC_UNKNOWN,
  DIRECTOR_DIAG_SYNC_STATE_MISMATCH,
  DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT
};

struct DirectorDiagnosticsCaps {
  bool identitySeen;
  bool helloComplete;
  bool firmwareSeen;
  bool sdSeen;
  char firmware[24];
  char sdState[16];
  DirectorDiagTri pixels;
  DirectorDiagTri ethernet;
  DirectorDiagTri audio;
  DirectorDiagTri time;
};

static inline void director_diag_caps_clear(DirectorDiagnosticsCaps *c) {
  if (!c) return;
  memset(c, 0, sizeof(*c));
}

static inline bool director_diag_apply_line(DirectorDiagnosticsCaps *c, const char *line) {
  if (!c || !line || !line[0]) return false;

  if (strcmp(line, SHOWDUINO_LEGACY_SHOWDUINO_STAGE) == 0) {
    c->identitySeen = true;
    return true;
  }
  if (strncmp(line, "FW:", 3) == 0 && line[3] != '\0') {
    memset(c->firmware, 0, sizeof(c->firmware));
    strncpy(c->firmware, line + 3, sizeof(c->firmware) - 1);
    c->firmwareSeen = (c->firmware[0] != '\0');
    return true;
  }
  if (strcmp(line, "PIXELS:READY") == 0) {
    c->pixels = DIRECTOR_DIAG_READY;
    return true;
  }
  if (strcmp(line, "PIXELS:FAULT") == 0) {
    c->pixels = DIRECTOR_DIAG_FAULT;
    return true;
  }
  if (strcmp(line, "ETHERNET:ONLINE") == 0) {
    c->ethernet = DIRECTOR_DIAG_ONLINE;
    return true;
  }
  if (strcmp(line, "ETHERNET:OFFLINE") == 0) {
    c->ethernet = DIRECTOR_DIAG_OFFLINE;
    return true;
  }
  if (strcmp(line, "AUDIO:READY") == 0) {
    c->audio = DIRECTOR_DIAG_READY;
    return true;
  }
  if (strcmp(line, "AUDIO:FAULT") == 0) {
    c->audio = DIRECTOR_DIAG_FAULT;
    return true;
  }
  if (strcmp(line, "TIME:READY") == 0) {
    c->time = DIRECTOR_DIAG_READY;
    return true;
  }
  if (strcmp(line, "TIME:UNSYNCED") == 0) {
    c->time = DIRECTOR_DIAG_UNSYNCED;
    return true;
  }
  if (strncmp(line, "SD:", 3) == 0 && line[3] != '\0' &&
      strncmp(line, "SD:STATUS", 9) != 0) {
    memset(c->sdState, 0, sizeof(c->sdState));
    strncpy(c->sdState, line + 3, sizeof(c->sdState) - 1);
    c->sdSeen = (c->sdState[0] != '\0');
    return true;
  }
  if (strcmp(line, SHOWDUINO_LEGACY_READY) == 0) {
    if (c->identitySeen) c->helloComplete = true;
    return c->identitySeen;
  }
  /* Known HELLO companions that Diagnostics must not surface. */
  if (strncmp(line, "DMX:", 4) == 0 ||
      strncmp(line, "E131:", 5) == 0 ||
      strncmp(line, "INPUTS:", 7) == 0) {
    return true;
  }
  return false;
}

static inline const char *director_diag_health_word(DirectorDiagHealth h) {
  switch (h) {
    case DIRECTOR_DIAG_HEALTH_EMERGENCY: return "EMERGENCY";
    case DIRECTOR_DIAG_HEALTH_STAGE_OFFLINE: return "STAGE OFFLINE";
    case DIRECTOR_DIAG_HEALTH_FAULT: return "FAULT";
    case DIRECTOR_DIAG_HEALTH_DEGRADED: return "DEGRADED";
    default: return "SYSTEM READY";
  }
}

static inline uint32_t director_diag_health_color(DirectorDiagHealth h) {
  switch (h) {
    case DIRECTOR_DIAG_HEALTH_EMERGENCY:
    case DIRECTOR_DIAG_HEALTH_FAULT:
    case DIRECTOR_DIAG_HEALTH_STAGE_OFFLINE:
      return ShowduinoPalette::Danger;
    case DIRECTOR_DIAG_HEALTH_DEGRADED:
      return ShowduinoPalette::Warn;
    default:
      return ShowduinoPalette::Accent;
  }
}

static inline DirectorDiagTri director_diag_sd_tri(const DirectorDiagnosticsCaps *c) {
  if (!c || !c->sdSeen) return DIRECTOR_DIAG_UNREPORTED;
  if (strcmp(c->sdState, "ONLINE") == 0) return DIRECTOR_DIAG_ONLINE;
  if (strcmp(c->sdState, "DEGRADED") == 0 || strcmp(c->sdState, "READ_ONLY") == 0) {
    return DIRECTOR_DIAG_DEGRADED;
  }
  if (strcmp(c->sdState, "OFFLINE") == 0) return DIRECTOR_DIAG_OFFLINE;
  if (strcmp(c->sdState, "FAULT") == 0) return DIRECTOR_DIAG_FAULT;
  return DIRECTOR_DIAG_UNREPORTED;
}

static inline bool director_diag_sd_fault(const DirectorDiagnosticsCaps *c) {
  const DirectorDiagTri t = director_diag_sd_tri(c);
  return t == DIRECTOR_DIAG_FAULT || t == DIRECTOR_DIAG_OFFLINE;
}

static inline void director_diag_copy(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  dst[0] = '\0';
  if (!src) return;
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

static inline void director_diag_format_uptime(char *dst, size_t n, uint32_t sec) {
  if (!dst || n == 0) return;
  const uint32_t h = sec / 3600UL;
  const uint32_t m = (sec / 60UL) % 60UL;
  const uint32_t s = sec % 60UL;
  snprintf(dst, n, "%02lu:%02lu:%02lu",
           (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

static inline void director_diag_format_age(char *dst, size_t n, bool seen,
                                            uint32_t nowMs, uint32_t lastMs) {
  if (!dst || n == 0) return;
  if (!seen) {
    director_diag_copy(dst, n, "NOT REPORTED");
    return;
  }
  const uint32_t age = (nowMs >= lastMs) ? (nowMs - lastMs) : 0;
  if (age < 1000UL) {
    snprintf(dst, n, "%lu ms ago", (unsigned long)age);
  } else if (age < 60000UL) {
    snprintf(dst, n, "%lu s ago", (unsigned long)(age / 1000UL));
  } else {
    snprintf(dst, n, "%lu min ago", (unsigned long)(age / 60000UL));
  }
}

static inline void director_diag_format_heap(char *dst, size_t n, uint32_t bytes) {
  if (!dst || n == 0) return;
  snprintf(dst, n, "%lu KB", (unsigned long)(bytes / 1024UL));
}

#endif
