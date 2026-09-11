#ifndef SHOWDUINO_SHDO_H
#define SHOWDUINO_SHDO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "showduino_version.h"
#include "showduino_lamp_node.h"
#include "showduino_pixel_node.h"

/*
 * SHDO v2 authoring-document validator and P4 runtime projector.
 * Host-testable. No Arduino, SD, or UART side effects.
 */

#define SHOWDUINO_SHDO_ID_MAX 48
#define SHOWDUINO_SHDO_NAME_MAX 64
#define SHOWDUINO_SHDO_TEXT_MAX 128
#define SHOWDUINO_SHDO_AUTHOR_MAX 64
#define SHOWDUINO_SHDO_CUE_ID_MAX 40
#define SHOWDUINO_SHDO_CMD_MAX 64
#define SHOWDUINO_SHDO_TYPE_MAX 12
#define SHOWDUINO_SHDO_MAX_DEVICES 32
#define SHOWDUINO_SHDO_MAX_CUES 512
#define SHOWDUINO_SHDO_PIXEL_SLOTS 16

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ShdoStatus {
  SHDO_OK = 0,
  SHDO_INVALID_JSON,
  SHDO_UNSUPPORTED_SCHEMA,
  SHDO_UNSUPPORTED_PACKAGE,
  SHDO_MISSING_FIELD,
  SHDO_INVALID_PRODUCTION_ID,
  SHDO_SAFETY_WEAKENED,
  SHDO_UNSUPPORTED_ARCHITECTURE,
  SHDO_UNSUPPORTED_ACTION,
  SHDO_MISSING_DEVICE,
  SHDO_UNSUPPORTED_DEVICE,
  SHDO_DMX_OUT_OF_SCOPE,
  SHDO_COMMAND_TOO_LONG,
  SHDO_TOO_MANY_CUES,
  SHDO_EMPTY_TIMELINE
} ShdoStatus;

typedef struct ShdoManifest {
  char productionId[SHOWDUINO_SHDO_ID_MAX];
  char name[SHOWDUINO_SHDO_NAME_MAX];
  char description[SHOWDUINO_SHDO_TEXT_MAX];
  char author[SHOWDUINO_SHDO_AUTHOR_MAX];
  uint32_t revision;
} ShdoManifest;

typedef struct ShdoCue {
  char id[SHOWDUINO_SHDO_CUE_ID_MAX];
  uint32_t timeMs;
  char type[SHOWDUINO_SHDO_TYPE_MAX];
  char command[SHOWDUINO_SHDO_CMD_MAX];
} ShdoCue;

static inline const char *shdoStatusName(ShdoStatus status) {
  switch (status) {
    case SHDO_OK: return "OK";
    case SHDO_INVALID_JSON: return "INVALID_JSON";
    case SHDO_UNSUPPORTED_SCHEMA: return "UNSUPPORTED_SHDO_VERSION";
    case SHDO_UNSUPPORTED_PACKAGE: return "UNSUPPORTED_PACKAGE";
    case SHDO_MISSING_FIELD: return "MISSING_FIELD";
    case SHDO_INVALID_PRODUCTION_ID: return "INVALID_PRODUCTION_ID";
    case SHDO_SAFETY_WEAKENED: return "SAFETY_WEAKENED";
    case SHDO_UNSUPPORTED_ARCHITECTURE: return "UNSUPPORTED_ARCHITECTURE";
    case SHDO_UNSUPPORTED_ACTION: return "UNSUPPORTED_ACTION";
    case SHDO_MISSING_DEVICE: return "MISSING_DEVICE";
    case SHDO_UNSUPPORTED_DEVICE: return "UNSUPPORTED_DEVICE";
    case SHDO_DMX_OUT_OF_SCOPE: return "DMX_OUT_OF_SCOPE";
    case SHDO_COMMAND_TOO_LONG: return "COMMAND_TOO_LONG";
    case SHDO_TOO_MANY_CUES: return "TOO_MANY_CUES";
    case SHDO_EMPTY_TIMELINE: return "EMPTY_TIMELINE";
    default: return "INVALID_JSON";
  }
}

#ifdef __cplusplus
}

namespace {

class ShdoJson {
public:
  ShdoJson(const char *json, size_t length) : p_(json), end_(json + length) {}

  void ws() {
    while (p_ < end_ && isspace((unsigned char)*p_)) ++p_;
  }

  bool take(char expected) {
    ws();
    if (p_ >= end_ || *p_ != expected) return false;
    ++p_;
    return true;
  }

  char peek() {
    ws();
    return p_ < end_ ? *p_ : '\0';
  }

  bool finished() {
    ws();
    return p_ == end_;
  }

  bool string(char *out, size_t outLen) {
    if (!out || outLen == 0 || !take('"')) return false;
    size_t used = 0;
    while (p_ < end_) {
      unsigned char c = (unsigned char)*p_++;
      if (c == '"') {
        out[used] = '\0';
        return true;
      }
      if (c < 0x20) return false;
      if (c == '\\') {
        if (p_ >= end_) return false;
        c = (unsigned char)*p_++;
        if (c == 'n' || c == 'r' || c == 't') c = ' ';
        else if (c != '"' && c != '\\' && c != '/') return false;
      }
      if (used + 1 >= outLen) return false;
      out[used++] = (char)c;
    }
    return false;
  }

  bool u32(uint32_t *out) {
    if (!out) return false;
    ws();
    if (p_ >= end_ || !isdigit((unsigned char)*p_)) return false;
    uint64_t value = 0;
    while (p_ < end_ && isdigit((unsigned char)*p_)) {
      value = value * 10u + (uint64_t)(*p_++ - '0');
      if (value > 0xFFFFFFFFull) return false;
    }
    *out = (uint32_t)value;
    return true;
  }

  bool boolean(bool *out) {
    if (!out) return false;
    ws();
    if (match("true")) {
      *out = true;
      return true;
    }
    if (match("false")) {
      *out = false;
      return true;
    }
    return false;
  }

  bool skipValue(unsigned depth = 0) {
    if (depth > 10) return false;
    ws();
    if (p_ >= end_) return false;
    if (*p_ == '"') {
      char sink[2];
      return skipString(sink, sizeof(sink));
    }
    if (*p_ == '{') {
      ++p_;
      ws();
      if (p_ < end_ && *p_ == '}') {
        ++p_;
        return true;
      }
      while (p_ < end_) {
        char key[2];
        if (!skipString(key, sizeof(key)) || !take(':') || !skipValue(depth + 1)) {
          return false;
        }
        ws();
        if (p_ < end_ && *p_ == '}') {
          ++p_;
          return true;
        }
        if (p_ >= end_ || *p_++ != ',') return false;
      }
      return false;
    }
    if (*p_ == '[') {
      ++p_;
      ws();
      if (p_ < end_ && *p_ == ']') {
        ++p_;
        return true;
      }
      while (p_ < end_) {
        if (!skipValue(depth + 1)) return false;
        ws();
        if (p_ < end_ && *p_ == ']') {
          ++p_;
          return true;
        }
        if (p_ >= end_ || *p_++ != ',') return false;
      }
      return false;
    }
    if (match("true") || match("false") || match("null")) return true;
    ws();
    if (p_ < end_ && (*p_ == '-' || isdigit((unsigned char)*p_))) {
      if (*p_ == '-') ++p_;
      while (p_ < end_ && (isdigit((unsigned char)*p_) || *p_ == '.' ||
                           *p_ == 'e' || *p_ == 'E' || *p_ == '+' || *p_ == '-')) {
        ++p_;
      }
      return true;
    }
    return false;
  }

  bool finishMember(bool *done) {
    if (take('}')) {
      *done = true;
      return true;
    }
    if (take(',')) {
      *done = false;
      return true;
    }
    return false;
  }

private:
  const char *p_;
  const char *end_;

  bool match(const char *word) {
    size_t n = strlen(word);
    if ((size_t)(end_ - p_) < n || strncmp(p_, word, n) != 0) return false;
    p_ += n;
    return true;
  }

  bool skipString(char *, size_t) {
    ws();
    if (p_ >= end_ || *p_++ != '"') return false;
    while (p_ < end_) {
      unsigned char c = (unsigned char)*p_++;
      if (c == '"') return true;
      if (c < 0x20) return false;
      if (c == '\\') {
        if (p_ >= end_) return false;
        ++p_;
      }
    }
    return false;
  }
};

struct ShdoDevice {
  char id[SHOWDUINO_SHDO_ID_MAX];
  char type[24];
  char route[32];
  char nodeId[16];
  uint32_t pixelStart;
  uint32_t pixelCount;
};

struct ShdoParams {
  char file[80];
  char effect[24];
  char secondary[16];
  uint32_t startPixel;
  uint32_t length;
  uint32_t count;
  uint32_t r, g, b;
  uint32_t brightness;
  uint32_t speed;
  uint32_t intensity;
  uint32_t randomness;
  uint32_t volume;
  bool reverse;
  bool loop;
  bool blackoutAtEnd;
  bool haveR, haveG, haveB;
  bool haveLength, haveCount;
};

struct ShdoClip {
  char id[SHOWDUINO_SHDO_ID_MAX];
  char name[SHOWDUINO_SHDO_NAME_MAX];
  char type[24];
  char targetDeviceId[SHOWDUINO_SHDO_ID_MAX];
  char command[SHOWDUINO_SHDO_CMD_MAX];
  uint32_t startMs;
  uint32_t durationMs;
  bool enabled;
  ShdoParams params;
};

static void shdoLowerCopy(char *out, size_t outLen, const char *in) {
  size_t i = 0;
  if (!out || outLen == 0) return;
  if (!in) {
    out[0] = '\0';
    return;
  }
  while (in[i] && i + 1 < outLen) {
    out[i] = (char)tolower((unsigned char)in[i]);
    ++i;
  }
  out[i] = '\0';
}

static bool shdoIdOk(const char *id) {
  if (!id || !id[0] || strlen(id) >= SHOWDUINO_SHDO_ID_MAX) return false;
  if (!islower((unsigned char)id[0]) && !isdigit((unsigned char)id[0])) return false;
  for (const char *p = id; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (!(islower(c) || isdigit(c) || c == '_' || c == '-')) return false;
  }
  return true;
}

static uint32_t shdoClampU32(uint32_t value, uint32_t lo, uint32_t hi, uint32_t fallback) {
  if (value < lo || value > hi) return fallback;
  return value;
}

static void shdoHexRgb(const char *text, uint32_t *r, uint32_t *g, uint32_t *b) {
  const char *raw = text ? text : "";
  if (raw[0] == '#') ++raw;
  size_t n = strlen(raw);
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
  };
  if (n == 3) {
    int r1 = nib(raw[0]), g1 = nib(raw[1]), b1 = nib(raw[2]);
    if (r1 >= 0 && g1 >= 0 && b1 >= 0) {
      *r = (uint32_t)(r1 * 17);
      *g = (uint32_t)(g1 * 17);
      *b = (uint32_t)(b1 * 17);
    }
    return;
  }
  if (n == 6) {
    int v[6];
    for (int i = 0; i < 6; ++i) v[i] = nib(raw[i]);
    if (v[0] >= 0 && v[1] >= 0 && v[2] >= 0 && v[3] >= 0 && v[4] >= 0 && v[5] >= 0) {
      *r = (uint32_t)((v[0] << 4) | v[1]);
      *g = (uint32_t)((v[2] << 4) | v[3]);
      *b = (uint32_t)((v[4] << 4) | v[5]);
    }
  }
}

static void shdoEffectToken(const char *in, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  const char *src = (in && in[0]) ? in : "SOLID";
  size_t used = 0;
  for (const char *p = src; *p && used + 1 < outLen; ++p) {
    unsigned char c = (unsigned char)*p;
    if (isspace(c) || c == '-') c = '_';
    else c = (unsigned char)toupper(c);
    if (!(isalnum(c) || c == '_')) continue;
    out[used++] = (char)c;
  }
  if (used == 0) {
    strncpy(out, "SOLID", outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }
  out[used] = '\0';
}

static bool shdoAddCue(ShdoCue *cues, uint16_t *count, uint32_t timeMs,
                       const char *type, const char *command, ShdoStatus *status) {
  if (*count >= SHOWDUINO_SHDO_MAX_CUES) {
    *status = SHDO_TOO_MANY_CUES;
    return false;
  }
  if (!command || !command[0] || strlen(command) >= SHOWDUINO_SHDO_CMD_MAX) {
    *status = SHDO_COMMAND_TOO_LONG;
    return false;
  }
  ShdoCue &cue = cues[*count];
  memset(&cue, 0, sizeof(cue));
  snprintf(cue.id, sizeof(cue.id), "c%04u", (unsigned)(*count + 1u));
  cue.timeMs = timeMs;
  strncpy(cue.type, type, sizeof(cue.type) - 1);
  strncpy(cue.command, command, sizeof(cue.command) - 1);
  ++(*count);
  return true;
}

static bool shdoParseParams(ShdoJson &r, ShdoParams *params) {
  if (!r.take('{')) return false;
  *params = ShdoParams{};
  params->g = 255;
  params->b = 200;
  params->brightness = 255;
  params->speed = 50;
  params->intensity = 100;
  params->volume = 100;
  params->length = 1;
  bool done = false;
  if (r.take('}')) return true;
  while (!done) {
    char key[32];
    if (!r.string(key, sizeof(key)) || !r.take(':')) return false;
    if (strcmp(key, "file") == 0 || strcmp(key, "asset") == 0) {
      if (!r.string(params->file, sizeof(params->file))) return false;
    } else if (strcmp(key, "effect") == 0) {
      if (!r.string(params->effect, sizeof(params->effect))) return false;
    } else if (strcmp(key, "secondary") == 0 || strcmp(key, "color") == 0) {
      if (!r.string(params->secondary, sizeof(params->secondary))) return false;
    } else if (strcmp(key, "startPixel") == 0) {
      if (!r.u32(&params->startPixel)) return false;
    } else if (strcmp(key, "length") == 0) {
      if (!r.u32(&params->length)) return false;
      params->haveLength = true;
    } else if (strcmp(key, "count") == 0) {
      if (!r.u32(&params->count)) return false;
      params->haveCount = true;
    } else if (strcmp(key, "r") == 0) {
      if (!r.u32(&params->r)) return false;
      params->haveR = true;
    } else if (strcmp(key, "g") == 0) {
      if (!r.u32(&params->g)) return false;
      params->haveG = true;
    } else if (strcmp(key, "b") == 0) {
      if (!r.u32(&params->b)) return false;
      params->haveB = true;
    } else if (strcmp(key, "brightness") == 0) {
      if (!r.u32(&params->brightness)) return false;
    } else if (strcmp(key, "speed") == 0) {
      if (!r.u32(&params->speed)) return false;
    } else if (strcmp(key, "intensity") == 0) {
      if (!r.u32(&params->intensity)) return false;
    } else if (strcmp(key, "randomness") == 0) {
      if (!r.u32(&params->randomness)) return false;
    } else if (strcmp(key, "volume") == 0) {
      if (!r.u32(&params->volume)) return false;
    } else if (strcmp(key, "reverse") == 0) {
      if (!r.boolean(&params->reverse)) return false;
    } else if (strcmp(key, "loop") == 0) {
      if (!r.boolean(&params->loop)) return false;
    } else if (strcmp(key, "blackoutAtEnd") == 0) {
      if (!r.boolean(&params->blackoutAtEnd)) return false;
    } else if (!r.skipValue()) {
      return false;
    }
    if (!r.finishMember(&done)) return false;
  }
  return true;
}

static bool shdoParseAction(ShdoJson &r, ShdoClip *clip) {
  if (!r.take('{')) return false;
  bool done = false;
  if (r.take('}')) return true;
  while (!done) {
    char key[32];
    if (!r.string(key, sizeof(key)) || !r.take(':')) return false;
    if (strcmp(key, "type") == 0) {
      char type[24];
      if (!r.string(type, sizeof(type))) return false;
      if (!clip->type[0]) shdoLowerCopy(clip->type, sizeof(clip->type), type);
    } else if (strcmp(key, "targetDeviceId") == 0) {
      if (!r.string(clip->targetDeviceId, sizeof(clip->targetDeviceId))) return false;
    } else if (strcmp(key, "value") == 0) {
      if (!clip->params.file[0] &&
          !r.string(clip->params.file, sizeof(clip->params.file))) {
        return false;
      } else if (clip->params.file[0] && !r.skipValue()) {
        return false;
      }
    } else if (strcmp(key, "params") == 0) {
      if (!shdoParseParams(r, &clip->params)) return false;
    } else if (!r.skipValue()) {
      return false;
    }
    if (!r.finishMember(&done)) return false;
  }
  return true;
}

static bool shdoParseClip(ShdoJson &r, ShdoClip *clip) {
  if (!r.take('{')) return false;
  *clip = ShdoClip{};
  clip->enabled = true;
  clip->params.g = 255;
  clip->params.b = 200;
  clip->params.brightness = 255;
  clip->params.speed = 50;
  clip->params.intensity = 100;
  clip->params.volume = 100;
  clip->params.length = 1;
  bool done = false;
  if (r.take('}')) return true;
  while (!done) {
    char key[32];
    if (!r.string(key, sizeof(key)) || !r.take(':')) return false;
    if (strcmp(key, "id") == 0) {
      if (!r.string(clip->id, sizeof(clip->id))) return false;
    } else if (strcmp(key, "name") == 0) {
      if (!r.string(clip->name, sizeof(clip->name))) return false;
    } else if (strcmp(key, "type") == 0) {
      char type[24];
      if (!r.string(type, sizeof(type))) return false;
      shdoLowerCopy(clip->type, sizeof(clip->type), type);
    } else if (strcmp(key, "targetDeviceId") == 0) {
      if (!r.string(clip->targetDeviceId, sizeof(clip->targetDeviceId))) return false;
    } else if (strcmp(key, "command") == 0) {
      if (!r.string(clip->command, sizeof(clip->command))) return false;
    } else if (strcmp(key, "startMs") == 0) {
      if (!r.u32(&clip->startMs)) return false;
    } else if (strcmp(key, "durationMs") == 0) {
      if (!r.u32(&clip->durationMs)) return false;
    } else if (strcmp(key, "enabled") == 0) {
      if (!r.boolean(&clip->enabled)) return false;
    } else if (strcmp(key, "action") == 0) {
      if (!shdoParseAction(r, clip)) return false;
    } else if (strcmp(key, "params") == 0) {
      if (!shdoParseParams(r, &clip->params)) return false;
    } else if (!r.skipValue()) {
      return false;
    }
    if (!r.finishMember(&done)) return false;
  }
  return true;
}

static bool shdoParseDevice(ShdoJson &r, ShdoDevice *device) {
  if (!r.take('{')) return false;
  *device = ShdoDevice{};
  bool done = false;
  if (r.take('}')) return true;
  while (!done) {
    char key[32];
    if (!r.string(key, sizeof(key)) || !r.take(':')) return false;
    if (strcmp(key, "id") == 0) {
      if (!r.string(device->id, sizeof(device->id))) return false;
    } else if (strcmp(key, "type") == 0) {
      if (!r.string(device->type, sizeof(device->type))) return false;
    } else if (strcmp(key, "binding") == 0) {
      if (!r.take('{')) return false;
      bool bindDone = false;
      if (r.take('}')) {
        /* empty binding */
      } else {
        while (!bindDone) {
          char bkey[32];
          if (!r.string(bkey, sizeof(bkey)) || !r.take(':')) return false;
          if (strcmp(bkey, "route") == 0) {
            if (!r.string(device->route, sizeof(device->route))) return false;
          } else if (strcmp(bkey, "pixelStart") == 0) {
            if (!r.u32(&device->pixelStart)) return false;
          } else if (strcmp(bkey, "pixelCount") == 0) {
            if (!r.u32(&device->pixelCount)) return false;
          } else if (strcmp(bkey, "nodeId") == 0) {
            if (!r.string(device->nodeId, sizeof(device->nodeId))) return false;
          } else if (!r.skipValue()) {
            return false;
          }
          if (!r.finishMember(&bindDone)) return false;
        }
      }
    } else if (!r.skipValue()) {
      return false;
    }
    if (!r.finishMember(&done)) return false;
  }
  return true;
}

static const ShdoDevice *shdoFindDevice(const ShdoDevice *devices, uint8_t n,
                                        const char *id) {
  if (!id || !id[0]) return nullptr;
  for (uint8_t i = 0; i < n; ++i) {
    if (strcmp(devices[i].id, id) == 0) return &devices[i];
  }
  return nullptr;
}

static bool shdoCompilePixel(const ShdoClip &clip, const ShdoDevice *device,
                             uint8_t slot, ShdoCue *cues, uint16_t *count,
                             ShdoStatus *status) {
  if (!device) {
    *status = SHDO_MISSING_DEVICE;
    return false;
  }
  char prefix[40];
  prefix[0] = '\0';
  if (strcmp(device->route, "p4-show-pixels") == 0) {
    strncpy(prefix, "PIXEL:", sizeof(prefix) - 1);
  } else if (strcmp(device->route, "pixel-node") == 0) {
    const char *nid = device->nodeId[0] ? device->nodeId : device->id;
    if (!nid[0] || strlen(nid) > SHOWDUINO_PIXEL_ID_MAX) {
      *status = SHDO_UNSUPPORTED_DEVICE;
      return false;
    }
    snprintf(prefix, sizeof(prefix), "PIXEL:NODE:%s:", nid);
  } else {
    *status = SHDO_UNSUPPORTED_DEVICE;
    return false;
  }
  if (slot >= SHOWDUINO_SHDO_PIXEL_SLOTS) {
    *status = SHDO_TOO_MANY_CUES;
    return false;
  }
  const uint32_t localStart = clip.params.startPixel;
  uint32_t pixCount = 1;
  if (clip.params.haveCount) pixCount = clip.params.count;
  else if (clip.params.haveLength) pixCount = clip.params.length;
  if (pixCount < 1) pixCount = 1;
  if (device->pixelCount > 0 && localStart + pixCount > device->pixelCount) {
    *status = SHDO_UNSUPPORTED_ACTION;
    return false;
  }
  const uint32_t start = device->pixelStart + localStart;
  uint32_t r = clip.params.haveR ? shdoClampU32(clip.params.r, 0, 255, 0) : 0;
  uint32_t g = clip.params.haveG ? shdoClampU32(clip.params.g, 0, 255, 255) : 255;
  uint32_t b = clip.params.haveB ? shdoClampU32(clip.params.b, 0, 255, 200) : 200;
  uint32_t r2 = 16, g2 = 24, b2 = 32;
  if (clip.params.secondary[0]) shdoHexRgb(clip.params.secondary, &r2, &g2, &b2);
  const uint32_t brightness = shdoClampU32(clip.params.brightness, 0, 255, 255);
  const uint32_t speed = shdoClampU32(clip.params.speed, 1, 100, 50);
  const uint32_t intensity = shdoClampU32(clip.params.intensity, 0, 100, 100);
  const uint32_t randomness = shdoClampU32(clip.params.randomness, 0, 100, 0);
  char fx[24];
  shdoEffectToken(clip.params.effect, fx, sizeof(fx));
  char cmd[SHOWDUINO_SHDO_CMD_MAX];
  auto add = [&](const char *line) -> bool {
    return shdoAddCue(cues, count, clip.startMs, "PIXEL", line, status);
  };
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:RANGE:%lu:%lu",
           prefix, (unsigned)slot, (unsigned long)start, (unsigned long)pixCount);
  if (strlen(cmd) >= SHOWDUINO_SHDO_CMD_MAX) { *status = SHDO_COMMAND_TOO_LONG; return false; }
  if (!add(cmd)) return false;
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:FX:%s", prefix, (unsigned)slot, fx);
  if (!add(cmd)) return false;
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:COLOR:%lu:%lu:%lu",
           prefix, (unsigned)slot, (unsigned long)r, (unsigned long)g, (unsigned long)b);
  if (!add(cmd)) return false;
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:COLOR2:%lu:%lu:%lu",
           prefix, (unsigned)slot, (unsigned long)r2, (unsigned long)g2, (unsigned long)b2);
  if (!add(cmd)) return false;
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:BRIGHTNESS:%lu",
           prefix, (unsigned)slot, (unsigned long)brightness);
  if (!add(cmd)) return false;
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:SPEED:%lu",
           prefix, (unsigned)slot, (unsigned long)speed);
  if (!add(cmd)) return false;
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:INTENSITY:%lu",
           prefix, (unsigned)slot, (unsigned long)intensity);
  if (!add(cmd)) return false;
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:RANDOMNESS:%lu",
           prefix, (unsigned)slot, (unsigned long)randomness);
  if (!add(cmd)) return false;
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:REVERSE:%u",
           prefix, (unsigned)slot, clip.params.reverse ? 1u : 0u);
  if (!add(cmd)) return false;
  if (clip.durationMs > 0) {
    snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:DURATION:%lu",
             prefix, (unsigned)slot, (unsigned long)clip.durationMs);
    if (!add(cmd)) return false;
  }
  snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:START", prefix, (unsigned)slot);
  if (!add(cmd)) return false;
  if (clip.durationMs > 0 && clip.params.blackoutAtEnd) {
    snprintf(cmd, sizeof(cmd), "%sSEGMENT:%u:STOP", prefix, (unsigned)slot);
    if (!shdoAddCue(cues, count, clip.startMs + clip.durationMs, "PIXEL", cmd, status)) {
      return false;
    }
  }
  return true;
}

static bool shdoCompileAudio(const ShdoClip &clip, const ShdoDevice *device,
                             ShdoCue *cues, uint16_t *count, ShdoStatus *status) {
  if (!device) {
    *status = SHDO_MISSING_DEVICE;
    return false;
  }
  if (strcmp(device->route, "audio-node") != 0) {
    *status = SHDO_UNSUPPORTED_DEVICE;
    return false;
  }
  if (!clip.params.file[0] || strstr(clip.params.file, "..")) {
    *status = SHDO_UNSUPPORTED_ACTION;
    return false;
  }
  const uint32_t volume = shdoClampU32(clip.params.volume, 0, 100, 100);
  char cmd[SHOWDUINO_SHDO_CMD_MAX];
  snprintf(cmd, sizeof(cmd), "AUDIO:NODE:VOLUME:%lu", (unsigned long)volume);
  if (!shdoAddCue(cues, count, clip.startMs, "AUDIO", cmd, status)) return false;
  snprintf(cmd, sizeof(cmd), "AUDIO:NODE:%s:%s",
           clip.params.loop ? "LOOP" : "PLAY", clip.params.file);
  if (!shdoAddCue(cues, count, clip.startMs, "AUDIO", cmd, status)) return false;
  if (clip.durationMs > 0) {
    if (!shdoAddCue(cues, count, clip.startMs + clip.durationMs, "AUDIO",
                    "AUDIO:NODE:STOP", status)) {
      return false;
    }
  }
  return true;
}

static bool shdoCompileLamp(const ShdoClip &clip, const ShdoDevice *device,
                            ShdoCue *cues, uint16_t *count, ShdoStatus *status) {
  if (!device) {
    *status = SHDO_MISSING_DEVICE;
    return false;
  }
  if (strcmp(device->route, "lamp-node") != 0) {
    *status = SHDO_UNSUPPORTED_DEVICE;
    return false;
  }
  char fx[24];
  shdoEffectToken(clip.params.effect, fx, sizeof(fx));
  ShowduinoLampFx lampFx;
  if (showduino_lamp_fx_from_token(fx, &lampFx) != 0) {
    *status = SHDO_UNSUPPORTED_ACTION;
    return false;
  }
  uint32_t bri = clip.params.brightness;
  if (bri > 100) bri = (bri * 100u + 127u) / 255u;
  bri = shdoClampU32(bri, 0, 100, 80);
  char cmd[SHOWDUINO_SHDO_CMD_MAX];
  snprintf(cmd, sizeof(cmd), "LAMP:FX:%s:BRI=%lu", fx, (unsigned long)bri);
  if (!shdoAddCue(cues, count, clip.startMs, "LAMP", cmd, status)) return false;
  if (clip.durationMs > 0) {
    if (!shdoAddCue(cues, count, clip.startMs + clip.durationMs, "LAMP",
                    "LAMP:OFF", status)) {
      return false;
    }
  }
  return true;
}

static int shdoClipOrder(const void *a, const void *b) {
  const ShdoClip *left = (const ShdoClip *)a;
  const ShdoClip *right = (const ShdoClip *)b;
  if (left->startMs < right->startMs) return -1;
  if (left->startMs > right->startMs) return 1;
  return 0;
}

static void shdoJsonEscape(const char *in, char *out, size_t outLen) {
  size_t used = 0;
  if (!out || outLen == 0) return;
  if (!in) in = "";
  for (const unsigned char *p = (const unsigned char *)in; *p && used + 2 < outLen; ++p) {
    if (*p == '"' || *p == '\\') {
      if (used + 3 >= outLen) break;
      out[used++] = '\\';
      out[used++] = (char)*p;
    } else if (*p >= 0x20) {
      out[used++] = (char)*p;
    }
  }
  out[used] = '\0';
}

}  // namespace

extern "C" {
#endif

static inline ShdoStatus shdoCompile(const char *json, size_t jsonLen,
                                     ShdoManifest *manifestOut,
                                     ShdoCue *cueBuf, uint16_t cueCap,
                                     uint16_t *cueCount,
                                     char *error, size_t errorLen) {
  auto fail = [&](ShdoStatus status, const char *message) -> ShdoStatus {
    if (error && errorLen) {
      strncpy(error, message ? message : shdoStatusName(status), errorLen - 1);
      error[errorLen - 1] = '\0';
    }
    if (cueCount) *cueCount = 0;
    return status;
  };
  if (!json || jsonLen == 0 || !manifestOut || !cueBuf || !cueCap) {
    return fail(SHDO_INVALID_JSON, "missing SHDO document");
  }

#ifdef __cplusplus
  ShdoJson r(json, jsonLen);
  if (!r.take('{')) return fail(SHDO_INVALID_JSON, "SHDO is not a JSON object");

  char schema[40] = {};
  uint32_t packageVersion = 0;
  bool seenPackageVersion = false;
  char runtimeAuthority[40] = {};
  char transport[40] = {};
  bool seenSafety = false;
  bool autoResume = false;
  bool requiresManualClear = true;
  bool productionCannotDisable = true;
  bool stopTimeline = true;
  char pixelOverride[24] = "all-white";
  char policy[40] = "firmware-authoritative";
  ShdoManifest manifest{};
  ShdoDevice devices[SHOWDUINO_SHDO_MAX_DEVICES]{};
  uint8_t deviceCount = 0;
  ShdoClip clips[SHOWDUINO_SHDO_MAX_CUES]{};
  uint16_t clipCount = 0;

  bool done = false;
  if (r.take('}')) return fail(SHDO_MISSING_FIELD, "empty SHDO document");
  while (!done) {
    char key[32];
    if (!r.string(key, sizeof(key)) || !r.take(':')) {
      return fail(SHDO_INVALID_JSON, "malformed SHDO object");
    }
    if (strcmp(key, "schema") == 0) {
      if (!r.string(schema, sizeof(schema))) {
        return fail(SHDO_INVALID_JSON, "invalid schema field");
      }
    } else if (strcmp(key, "package") == 0) {
      if (!r.take('{')) return fail(SHDO_INVALID_JSON, "invalid package object");
      bool pkgDone = false;
      if (!r.take('}')) {
        while (!pkgDone) {
          char pkey[24];
          if (!r.string(pkey, sizeof(pkey)) || !r.take(':')) {
            return fail(SHDO_INVALID_JSON, "invalid package field");
          }
          if (strcmp(pkey, "version") == 0) {
            if (!r.u32(&packageVersion)) {
              return fail(SHDO_INVALID_JSON, "invalid package.version");
            }
            seenPackageVersion = true;
          } else if (!r.skipValue()) {
            return fail(SHDO_INVALID_JSON, "invalid package field");
          }
          if (!r.finishMember(&pkgDone)) {
            return fail(SHDO_INVALID_JSON, "invalid package object");
          }
        }
      }
    } else if (strcmp(key, "project") == 0) {
      if (!r.take('{')) return fail(SHDO_MISSING_FIELD, "invalid project object");
      bool projDone = false;
      if (!r.take('}')) {
        while (!projDone) {
          char pkey[24];
          if (!r.string(pkey, sizeof(pkey)) || !r.take(':')) {
            return fail(SHDO_INVALID_JSON, "invalid project field");
          }
          if (strcmp(pkey, "id") == 0) {
            char rawId[SHOWDUINO_SHDO_ID_MAX];
            if (!r.string(rawId, sizeof(rawId))) {
              return fail(SHDO_INVALID_PRODUCTION_ID, "invalid project.id");
            }
            shdoLowerCopy(manifest.productionId, sizeof(manifest.productionId), rawId);
          } else if (strcmp(pkey, "name") == 0) {
            if (!r.string(manifest.name, sizeof(manifest.name))) {
              return fail(SHDO_MISSING_FIELD, "invalid project.name");
            }
          } else if (strcmp(pkey, "description") == 0) {
            if (!r.string(manifest.description, sizeof(manifest.description))) {
              return fail(SHDO_INVALID_JSON, "invalid project.description");
            }
          } else if (strcmp(pkey, "author") == 0) {
            if (!r.string(manifest.author, sizeof(manifest.author))) {
              return fail(SHDO_INVALID_JSON, "invalid project.author");
            }
          } else if (strcmp(pkey, "revision") == 0 || strcmp(pkey, "version") == 0) {
            uint32_t revision = 0;
            if (r.peek() == '"') {
              char ver[24];
              if (!r.string(ver, sizeof(ver))) {
                return fail(SHDO_INVALID_JSON, "invalid project.version");
              }
              revision = (uint32_t)strtoul(ver, nullptr, 10);
            } else if (!r.u32(&revision)) {
              return fail(SHDO_INVALID_JSON, "invalid project.revision");
            }
            if (revision) manifest.revision = revision;
          } else if (!r.skipValue()) {
            return fail(SHDO_INVALID_JSON, "invalid project field");
          }
          if (!r.finishMember(&projDone)) {
            return fail(SHDO_INVALID_JSON, "invalid project object");
          }
        }
      }
    } else if (strcmp(key, "architecture") == 0) {
      if (!r.take('{')) return fail(SHDO_UNSUPPORTED_ARCHITECTURE, "invalid architecture");
      bool archDone = false;
      if (!r.take('}')) {
        while (!archDone) {
          char akey[32];
          if (!r.string(akey, sizeof(akey)) || !r.take(':')) {
            return fail(SHDO_INVALID_JSON, "invalid architecture field");
          }
          if (strcmp(akey, "runtimeAuthority") == 0) {
            if (!r.string(runtimeAuthority, sizeof(runtimeAuthority))) {
              return fail(SHDO_UNSUPPORTED_ARCHITECTURE, "invalid runtimeAuthority");
            }
          } else if (strcmp(akey, "transport") == 0) {
            if (!r.string(transport, sizeof(transport))) {
              return fail(SHDO_UNSUPPORTED_ARCHITECTURE, "invalid transport");
            }
          } else if (!r.skipValue()) {
            return fail(SHDO_INVALID_JSON, "invalid architecture field");
          }
          if (!r.finishMember(&archDone)) {
            return fail(SHDO_INVALID_JSON, "invalid architecture object");
          }
        }
      }
    } else if (strcmp(key, "safety") == 0) {
      seenSafety = true;
      if (!r.take('{')) return fail(SHDO_SAFETY_WEAKENED, "invalid safety object");
      bool safetyDone = false;
      if (!r.take('}')) {
        while (!safetyDone) {
          char skey[32];
          if (!r.string(skey, sizeof(skey)) || !r.take(':')) {
            return fail(SHDO_INVALID_JSON, "invalid safety field");
          }
          if (strcmp(skey, "policy") == 0) {
            if (!r.string(policy, sizeof(policy))) {
              return fail(SHDO_SAFETY_WEAKENED, "invalid safety.policy");
            }
          } else if (strcmp(skey, "productionCannotDisable") == 0) {
            if (!r.boolean(&productionCannotDisable)) {
              return fail(SHDO_SAFETY_WEAKENED, "invalid productionCannotDisable");
            }
          } else if (strcmp(skey, "emergency") == 0) {
            if (!r.take('{')) return fail(SHDO_SAFETY_WEAKENED, "invalid emergency object");
            bool emDone = false;
            if (!r.take('}')) {
              while (!emDone) {
                char ekey[32];
                if (!r.string(ekey, sizeof(ekey)) || !r.take(':')) {
                  return fail(SHDO_INVALID_JSON, "invalid emergency field");
                }
                if (strcmp(ekey, "autoResume") == 0) {
                  if (!r.boolean(&autoResume)) {
                    return fail(SHDO_SAFETY_WEAKENED, "invalid autoResume");
                  }
                } else if (strcmp(ekey, "requiresManualClear") == 0) {
                  if (!r.boolean(&requiresManualClear)) {
                    return fail(SHDO_SAFETY_WEAKENED, "invalid requiresManualClear");
                  }
                } else if (strcmp(ekey, "stopTimeline") == 0) {
                  if (!r.boolean(&stopTimeline)) {
                    return fail(SHDO_SAFETY_WEAKENED, "invalid stopTimeline");
                  }
                } else if (strcmp(ekey, "pixelOverride") == 0) {
                  if (!r.string(pixelOverride, sizeof(pixelOverride))) {
                    return fail(SHDO_SAFETY_WEAKENED, "invalid pixelOverride");
                  }
                } else if (!r.skipValue()) {
                  return fail(SHDO_INVALID_JSON, "invalid emergency field");
                }
                if (!r.finishMember(&emDone)) {
                  return fail(SHDO_INVALID_JSON, "invalid emergency object");
                }
              }
            }
          } else if (!r.skipValue()) {
            return fail(SHDO_INVALID_JSON, "invalid safety field");
          }
          if (!r.finishMember(&safetyDone)) {
            return fail(SHDO_INVALID_JSON, "invalid safety object");
          }
        }
      }
    } else if (strcmp(key, "devices") == 0) {
      if (!r.take('[')) return fail(SHDO_INVALID_JSON, "invalid devices array");
      if (!r.take(']')) {
        while (true) {
          if (deviceCount >= SHOWDUINO_SHDO_MAX_DEVICES) {
            return fail(SHDO_TOO_MANY_CUES, "too many devices");
          }
          if (!shdoParseDevice(r, &devices[deviceCount])) {
            return fail(SHDO_INVALID_JSON, "invalid device object");
          }
          ++deviceCount;
          if (r.take(']')) break;
          if (!r.take(',')) return fail(SHDO_INVALID_JSON, "invalid devices array");
        }
      }
    } else if (strcmp(key, "clips") == 0) {
      if (!r.take('[')) return fail(SHDO_INVALID_JSON, "invalid clips array");
      if (!r.take(']')) {
        while (true) {
          if (clipCount >= SHOWDUINO_SHDO_MAX_CUES) {
            return fail(SHDO_TOO_MANY_CUES, "too many clips");
          }
          if (!shdoParseClip(r, &clips[clipCount])) {
            return fail(SHDO_INVALID_JSON, "invalid clip object");
          }
          ++clipCount;
          if (r.take(']')) break;
          if (!r.take(',')) return fail(SHDO_INVALID_JSON, "invalid clips array");
        }
      }
    } else if (!r.skipValue()) {
      return fail(SHDO_INVALID_JSON, "invalid SHDO field");
    }
    if (!r.finishMember(&done)) return fail(SHDO_INVALID_JSON, "malformed SHDO object");
  }
  if (!r.finished()) return fail(SHDO_INVALID_JSON, "trailing SHDO data");

  if (strcmp(schema, SHOWDUINO_SHDO_SCHEMA_NAME) != 0) {
    return fail(SHDO_UNSUPPORTED_SCHEMA, "deployment requires canonical SHDO v2");
  }
  if (seenPackageVersion &&
      !showduino_shdo_package_supported((int)packageVersion)) {
    return fail(SHDO_UNSUPPORTED_PACKAGE, "unsupported SHDO package version");
  }
  if (!manifest.productionId[0] || !manifest.name[0]) {
    return fail(SHDO_MISSING_FIELD, "project.id and project.name are required");
  }
  if (!shdoIdOk(manifest.productionId)) {
    return fail(SHDO_INVALID_PRODUCTION_ID, "project.id is not a valid production id");
  }
  if (runtimeAuthority[0] &&
      strcmp(runtimeAuthority, "esp32-p4-show-engine") != 0) {
    return fail(SHDO_UNSUPPORTED_ARCHITECTURE, "runtimeAuthority must be the P4 Show Engine");
  }
  if (transport[0]) {
    if (strstr(transport, "sue") || strstr(transport, "c3-comms") ||
        strstr(transport, "onboard-c6") || strstr(transport, "esp32-c3")) {
      return fail(SHDO_UNSUPPORTED_ARCHITECTURE,
                  "obsolete C3/SUE/C6 communications architecture");
    }
    if (strcmp(transport, "esp32-s3-comms-controller") != 0) {
      return fail(SHDO_UNSUPPORTED_ARCHITECTURE,
                  "transport must be the dedicated Communications S3");
    }
  }
  if (seenSafety) {
    if (autoResume || !requiresManualClear || !productionCannotDisable ||
        !stopTimeline || strcmp(pixelOverride, "all-white") != 0 ||
        strcmp(policy, "firmware-authoritative") != 0) {
      return fail(SHDO_SAFETY_WEAKENED,
                  "production attempted to weaken firmware safety policy");
    }
  }

  qsort(clips, clipCount, sizeof(ShdoClip), shdoClipOrder);
  uint16_t compiled = 0;
  uint8_t pixelSlots[SHOWDUINO_SHDO_MAX_DEVICES];
  memset(pixelSlots, 0, sizeof(pixelSlots));
  ShdoStatus status = SHDO_OK;
  for (uint16_t i = 0; i < clipCount; ++i) {
    const ShdoClip &clip = clips[i];
    if (!clip.enabled) continue;
    if (clip.command[0]) {
      const char *type = "TEST";
      if (strncmp(clip.command, "PIXEL:", 6) == 0) type = "PIXEL";
      else if (strncmp(clip.command, "AUDIO:NODE:", 11) == 0) type = "AUDIO";
      else if (strncmp(clip.command, "LAMP:", 5) == 0) type = "LAMP";
      else if (strncmp(clip.command, "INTERNAL:", 9) == 0) type = "TEST";
      else {
        return fail(SHDO_UNSUPPORTED_ACTION, "unsupported raw clip command");
      }
      if (!shdoAddCue(cueBuf, &compiled, clip.startMs, type, clip.command, &status)) {
        return fail(status, shdoStatusName(status));
      }
      continue;
    }
    const char *type = clip.type;
    const ShdoDevice *device = shdoFindDevice(devices, deviceCount, clip.targetDeviceId);
    if (strcmp(type, "dmx") == 0) {
      return fail(SHDO_DMX_OUT_OF_SCOPE, "DMX remains outside Showduino V1 scope");
    }
    if (strcmp(type, "relay") == 0 || strcmp(type, "mosfet") == 0 ||
        strcmp(type, "trigger") == 0) {
      return fail(SHDO_UNSUPPORTED_ACTION, "action is not implemented in Showduino V1");
    }
    if (strcmp(type, "pixel") == 0) {
      uint8_t di = 0;
      for (; di < deviceCount; ++di) {
        if (strcmp(devices[di].id, clip.targetDeviceId) == 0) break;
      }
      if (di >= deviceCount) di = 0;
      uint8_t slot = pixelSlots[di]++;
      if (!shdoCompilePixel(clip, device, slot, cueBuf, &compiled, &status)) {
        return fail(status, status == SHDO_MISSING_DEVICE
                                ? "pixel clip is missing a bound show-pixel device"
                                : shdoStatusName(status));
      }
    } else if (strcmp(type, "audio") == 0) {
      if (!shdoCompileAudio(clip, device, cueBuf, &compiled, &status)) {
        return fail(status, status == SHDO_MISSING_DEVICE
                                ? "audio clip is missing a bound Audio Node"
                                : shdoStatusName(status));
      }
    } else if (strcmp(type, "lamp") == 0 || strcmp(type, "lighting") == 0 ||
               strcmp(type, "fx") == 0) {
      if (!shdoCompileLamp(clip, device, cueBuf, &compiled, &status)) {
        return fail(status, status == SHDO_MISSING_DEVICE
                                ? "lamp clip is missing a bound Lamp Node"
                                : shdoStatusName(status));
      }
    } else if (strcmp(type, "test") == 0 || strcmp(type, "log") == 0) {
      char cmd[SHOWDUINO_SHDO_CMD_MAX];
      const char *cueType = strcmp(type, "log") == 0 ? "LOG" : "TEST";
      int n = snprintf(cmd, sizeof(cmd), "INTERNAL:%s:%s:%s",
                       cueType, clip.id[0] ? clip.id : "clip",
                       clip.params.file[0] ? clip.params.file : clip.name);
      if (n <= 0 || (size_t)n >= sizeof(cmd)) {
        return fail(SHDO_COMMAND_TOO_LONG, "TEST/LOG command exceeds 63 characters");
      }
      if (!shdoAddCue(cueBuf, &compiled, clip.startMs, cueType, cmd, &status)) {
        return fail(status, shdoStatusName(status));
      }
    } else if (type[0]) {
      return fail(SHDO_UNSUPPORTED_ACTION, "clip type has no P4 compiler");
    }
  }
  if (compiled == 0) {
    return fail(SHDO_EMPTY_TIMELINE, "production has no deployable P4 timeline commands");
  }
  if (compiled > cueCap) {
    return fail(SHDO_TOO_MANY_CUES, "compiled timeline exceeds P4 cue capacity");
  }
  *manifestOut = manifest;
  if (cueCount) *cueCount = compiled;
  if (error && errorLen) {
    strncpy(error, "OK", errorLen - 1);
    error[errorLen - 1] = '\0';
  }
  return SHDO_OK;
#else
  (void)cueBuf;
  (void)cueCap;
  (void)cueCount;
  return fail(SHDO_INVALID_JSON, "SHDO compiler requires C++");
#endif
}

static inline size_t shdoWriteManifestJson(char *out, size_t cap,
                                           const ShdoManifest *manifest) {
  if (!out || !cap || !manifest) return 0;
  char name[SHOWDUINO_SHDO_NAME_MAX * 2];
  char desc[SHOWDUINO_SHDO_TEXT_MAX * 2];
  char author[SHOWDUINO_SHDO_AUTHOR_MAX * 2];
#ifdef __cplusplus
  shdoJsonEscape(manifest->name, name, sizeof(name));
  shdoJsonEscape(manifest->description, desc, sizeof(desc));
  shdoJsonEscape(manifest->author, author, sizeof(author));
#else
  strncpy(name, manifest->name, sizeof(name) - 1);
  strncpy(desc, manifest->description, sizeof(desc) - 1);
  strncpy(author, manifest->author, sizeof(author) - 1);
#endif
  int n = snprintf(out, cap,
                   "{\n"
                   "  \"formatVersion\": 1,\n"
                   "  \"productionId\": \"%s\",\n"
                   "  \"name\": \"%s\",\n"
                   "  \"description\": \"%s\",\n"
                   "  \"author\": \"%s\",\n"
                   "  \"timeline\": \"timeline.json\",\n"
                   "  \"source\": \"shdo-v2\",\n"
                   "  \"revision\": %lu\n"
                   "}\n",
                   manifest->productionId, name, desc, author,
                   (unsigned long)(manifest->revision ? manifest->revision : 1u));
  if (n <= 0 || (size_t)n >= cap) return 0;
  return (size_t)n;
}

static inline size_t shdoWriteTimelineJson(char *out, size_t cap,
                                           const ShdoCue *cues, uint16_t count) {
  if (!out || !cap || !cues || !count) return 0;
  size_t used = 0;
  int n = snprintf(out, cap, "{\n  \"formatVersion\": 1,\n  \"source\": \"shdo-v2\",\n  \"cues\": [\n");
  if (n <= 0 || (size_t)n >= cap) return 0;
  used = (size_t)n;
  for (uint16_t i = 0; i < count; ++i) {
    n = snprintf(out + used, cap - used,
                 "    {\"id\":\"%s\",\"timeMs\":%lu,\"type\":\"%s\",\"command\":\"%s\"}%s\n",
                 cues[i].id, (unsigned long)cues[i].timeMs, cues[i].type, cues[i].command,
                 (i + 1 < count) ? "," : "");
    if (n <= 0 || used + (size_t)n >= cap) return 0;
    used += (size_t)n;
  }
  n = snprintf(out + used, cap - used, "  ]\n}\n");
  if (n <= 0 || used + (size_t)n >= cap) return 0;
  used += (size_t)n;
  return used;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_SHDO_H */
