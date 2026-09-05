#include "ProductionFormat.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace {

class JsonReader {
public:
  JsonReader(const char *json, size_t length) : p_(json), end_(json + length) {}

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
        switch (c) {
          case '"': case '\\': case '/': break;
          case 'b': c = '\b'; break;
          case 'f': c = '\f'; break;
          case 'n': c = '\n'; break;
          case 'r': c = '\r'; break;
          case 't': c = '\t'; break;
          case 'u': {
            unsigned value = 0;
            for (int i = 0; i < 4; ++i) {
              if (p_ >= end_ || !isxdigit((unsigned char)*p_)) return false;
              char h = *p_++;
              value = (value << 4) | (unsigned)(isdigit((unsigned char)h)
                  ? h - '0' : 10 + (tolower((unsigned char)h) - 'a'));
            }
            c = (value >= 0x20 && value <= 0x7E) ? (unsigned char)value : '?';
            break;
          }
          default: return false;
        }
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
    if (*p_ == '0' && p_ + 1 < end_ && isdigit((unsigned char)p_[1])) return false;
    uint64_t value = 0;
    while (p_ < end_ && isdigit((unsigned char)*p_)) {
      value = value * 10U + (uint64_t)(*p_++ - '0');
      if (value > UINT32_MAX) return false;
    }
    *out = (uint32_t)value;
    return true;
  }

  bool skipValue(unsigned depth = 0) {
    if (depth > 8) return false;
    ws();
    if (p_ >= end_) return false;
    if (*p_ == '"') {
      char sink[2];
      return skipString(sink, sizeof(sink));
    }
    if (*p_ == '{') {
      ++p_;
      ws();
      if (p_ < end_ && *p_ == '}') { ++p_; return true; }
      while (p_ < end_) {
        char key[2];
        if (!skipString(key, sizeof(key)) || !take(':') || !skipValue(depth + 1)) return false;
        ws();
        if (p_ < end_ && *p_ == '}') { ++p_; return true; }
        if (p_ >= end_ || *p_++ != ',') return false;
      }
      return false;
    }
    if (*p_ == '[') {
      ++p_;
      ws();
      if (p_ < end_ && *p_ == ']') { ++p_; return true; }
      while (p_ < end_) {
        if (!skipValue(depth + 1)) return false;
        ws();
        if (p_ < end_ && *p_ == ']') { ++p_; return true; }
        if (p_ >= end_ || *p_++ != ',') return false;
      }
      return false;
    }
    if (match("true") || match("false") || match("null")) return true;
    return skipNumber();
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
        char esc = *p_++;
        if (esc == 'u') {
          for (int i = 0; i < 4; ++i) {
            if (p_ >= end_ || !isxdigit((unsigned char)*p_++)) return false;
          }
        } else if (!strchr("\"\\/bfnrt", esc)) {
          return false;
        }
      }
    }
    return false;
  }

  bool skipNumber() {
    const char *start = p_;
    if (p_ < end_ && *p_ == '-') ++p_;
    if (p_ >= end_) return false;
    if (*p_ == '0') {
      ++p_;
    } else {
      if (!isdigit((unsigned char)*p_)) return false;
      while (p_ < end_ && isdigit((unsigned char)*p_)) ++p_;
    }
    if (p_ < end_ && *p_ == '.') {
      ++p_;
      if (p_ >= end_ || !isdigit((unsigned char)*p_)) return false;
      while (p_ < end_ && isdigit((unsigned char)*p_)) ++p_;
    }
    if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
      ++p_;
      if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
      if (p_ >= end_ || !isdigit((unsigned char)*p_)) return false;
      while (p_ < end_ && isdigit((unsigned char)*p_)) ++p_;
    }
    return p_ > start;
  }
};

static void setResult(ProductionParseResult *out, ProductionParseResult value) {
  if (out) *out = value;
}

static bool textIsSingleLine(const char *text) {
  if (!text) return false;
  for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
    if (*p < 0x20 || *p == 0x7F) return false;
  }
  return true;
}

static bool finishMember(JsonReader &r, bool *done) {
  if (r.take('}')) { *done = true; return true; }
  if (r.take(',')) { *done = false; return true; }
  return false;
}

static bool parseCue(JsonReader &r, ProductionCue *cue, ProductionParseResult *result) {
  if (!cue || !r.take('{')) return false;
  *cue = ProductionCue{};
  enum : uint8_t { SeenId = 1, SeenTime = 2, SeenType = 4, SeenTarget = 8,
                   SeenAction = 16, SeenValue = 32, SeenParams = 64 };
  uint8_t seen = 0;
  bool done = false;
  if (r.take('}')) return false;
  while (!done) {
    char key[32];
    if (!r.string(key, sizeof(key)) || !r.take(':')) return false;
    uint8_t bit = 0;
    if (strcmp(key, "id") == 0) bit = SeenId;
    else if (strcmp(key, "timeMs") == 0) bit = SeenTime;
    else if (strcmp(key, "type") == 0) bit = SeenType;
    else if (strcmp(key, "target") == 0) bit = SeenTarget;
    else if (strcmp(key, "action") == 0) bit = SeenAction;
    else if (strcmp(key, "value") == 0) bit = SeenValue;
    else if (strcmp(key, "parameters") == 0) bit = SeenParams;
    if (bit && (seen & bit)) return false;
    seen |= bit;

    if (bit == SeenId) {
      if (!r.string(cue->id, sizeof(cue->id))) return false;
    } else if (bit == SeenTime) {
      if (!r.u32(&cue->timeMs)) {
        setResult(result, ProductionParseResult::InvalidCueTime);
        return false;
      }
    } else if (bit == SeenType) {
      if (!r.string(cue->type, sizeof(cue->type))) return false;
    } else if (bit == SeenTarget) {
      if (!r.string(cue->target, sizeof(cue->target))) return false;
    } else if (bit == SeenAction) {
      if (!r.string(cue->action, sizeof(cue->action))) return false;
    } else if (bit == SeenValue) {
      if (!r.string(cue->value, sizeof(cue->value))) return false;
    } else if (bit == SeenParams) {
      if (r.peek() != '{' || !r.skipValue()) return false;
    } else if (!r.skipValue()) {
      return false;
    }
    if (!finishMember(r, &done)) return false;
  }

  if ((seen & (SeenId | SeenTime | SeenType)) !=
      (SeenId | SeenTime | SeenType) || !cue->id[0]) {
    setResult(result, ProductionParseResult::MissingField);
    return false;
  }
  for (const char *p = cue->id; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '_' || c == '-')) {
      setResult(result, ProductionParseResult::InvalidJson);
      return false;
    }
  }
  if (!textIsSingleLine(cue->value) || !textIsSingleLine(cue->target)) {
    setResult(result, ProductionParseResult::InvalidJson);
    return false;
  }
  if (strcmp(cue->type, "TEST") != 0 && strcmp(cue->type, "LOG") != 0) {
    setResult(result, ProductionParseResult::UnsupportedCueType);
    return false;
  }
  if (cue->action[0] && strcmp(cue->action, "LOG") != 0) {
    setResult(result, ProductionParseResult::InvalidCueAction);
    return false;
  }
  int n = snprintf(cue->command, sizeof(cue->command), "INTERNAL:%s:%s:%s",
                   cue->type, cue->id, cue->value);
  if (n <= 0 || (size_t)n >= sizeof(cue->command)) {
    setResult(result, ProductionParseResult::CommandTooLong);
    return false;
  }
  return true;
}

} // namespace

bool productionIdIsValid(const char *id) {
  if (!id || !id[0]) return false;
  size_t n = strlen(id);
  if (n >= SHOWDUINO_PRODUCTION_ID_MAX) return false;
  if (!islower((unsigned char)id[0]) && !isdigit((unsigned char)id[0])) return false;
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)id[i];
    if (!(islower(c) || isdigit(c) || c == '_' || c == '-')) return false;
  }
  return true;
}

bool productionRelativePathIsValid(const char *path) {
  if (!path || !path[0] || path[0] == '/' || strchr(path, '\\') || strstr(path, "..")) {
    return false;
  }
  size_t n = strlen(path);
  if (n >= SHOWDUINO_PRODUCTION_PATH_MAX || path[n - 1] == '/') return false;
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)path[i];
    if (!(isalnum(c) || c == '_' || c == '-' || c == '.' || c == '/')) return false;
  }
  return true;
}

const char *productionParseResultName(ProductionParseResult result) {
  switch (result) {
    case ProductionParseResult::Ok: return "OK";
    case ProductionParseResult::InvalidJson: return "INVALID_JSON";
    case ProductionParseResult::MissingField: return "MISSING_FIELD";
    case ProductionParseResult::UnsupportedVersion: return "UNSUPPORTED_VERSION";
    case ProductionParseResult::InvalidProductionId: return "INVALID_PRODUCTION_ID";
    case ProductionParseResult::InvalidTimelinePath: return "INVALID_TIMELINE_PATH";
    case ProductionParseResult::EmptyTimeline: return "EMPTY_TIMELINE";
    case ProductionParseResult::TooManyCues: return "TOO_MANY_CUES";
    case ProductionParseResult::DuplicateCueId: return "DUPLICATE_CUE_ID";
    case ProductionParseResult::InvalidCueTime: return "INVALID_CUE_TIME";
    case ProductionParseResult::UnsupportedCueType: return "UNSUPPORTED_CUE_TYPE";
    case ProductionParseResult::InvalidCueAction: return "INVALID_CUE_ACTION";
    case ProductionParseResult::CommandTooLong: return "COMMAND_TOO_LONG";
    default: return "INVALID_JSON";
  }
}

bool productionParseManifest(const char *json, size_t jsonLen,
                             ProductionManifest *out,
                             ProductionParseResult *result) {
  if (!json || !out || jsonLen == 0) {
    setResult(result, ProductionParseResult::InvalidJson);
    return false;
  }
  setResult(result, ProductionParseResult::Ok);
  ProductionManifest manifest;
  JsonReader r(json, jsonLen);
  enum : uint16_t { SeenVersion = 1, SeenId = 2, SeenName = 4, SeenDescription = 8,
                    SeenAuthor = 16, SeenTimeline = 32, SeenCreated = 64,
                    SeenModified = 128, SeenRevision = 256 };
  uint16_t seen = 0;
  bool done = false;
  if (!r.take('{') || r.take('}')) goto invalid;
  while (!done) {
    char key[32];
    if (!r.string(key, sizeof(key)) || !r.take(':')) goto invalid;
    uint16_t bit = 0;
    if (strcmp(key, "formatVersion") == 0) bit = SeenVersion;
    else if (strcmp(key, "productionId") == 0) bit = SeenId;
    else if (strcmp(key, "name") == 0) bit = SeenName;
    else if (strcmp(key, "description") == 0) bit = SeenDescription;
    else if (strcmp(key, "author") == 0) bit = SeenAuthor;
    else if (strcmp(key, "timeline") == 0) bit = SeenTimeline;
    else if (strcmp(key, "created") == 0) bit = SeenCreated;
    else if (strcmp(key, "modified") == 0) bit = SeenModified;
    else if (strcmp(key, "revision") == 0) bit = SeenRevision;
    if (bit && (seen & bit)) goto invalid;
    seen |= bit;
    if (bit == SeenVersion) {
      uint32_t version = 0;
      if (!r.u32(&version) || version > UINT16_MAX) goto invalid;
      manifest.formatVersion = (uint16_t)version;
    } else if (bit == SeenId) {
      if (!r.string(manifest.productionId, sizeof(manifest.productionId))) goto invalid;
    } else if (bit == SeenName) {
      if (!r.string(manifest.name, sizeof(manifest.name))) goto invalid;
    } else if (bit == SeenDescription) {
      if (!r.string(manifest.description, sizeof(manifest.description))) goto invalid;
    } else if (bit == SeenAuthor) {
      if (!r.string(manifest.author, sizeof(manifest.author))) goto invalid;
    } else if (bit == SeenTimeline) {
      if (!r.string(manifest.timeline, sizeof(manifest.timeline))) goto invalid;
    } else if (bit == SeenCreated) {
      if (!r.string(manifest.created, sizeof(manifest.created))) goto invalid;
    } else if (bit == SeenModified) {
      if (!r.string(manifest.modified, sizeof(manifest.modified))) goto invalid;
    } else if (bit == SeenRevision) {
      if (!r.u32(&manifest.revision)) goto invalid;
    } else if (!r.skipValue()) {
      goto invalid;
    }
    if (!finishMember(r, &done)) goto invalid;
  }
  if (!r.finished()) goto invalid;
  if ((seen & (SeenVersion | SeenId | SeenName | SeenTimeline)) !=
      (SeenVersion | SeenId | SeenName | SeenTimeline) || !manifest.name[0]) {
    setResult(result, ProductionParseResult::MissingField);
    return false;
  }
  if (manifest.formatVersion != SHOWDUINO_PRODUCTION_FORMAT_VERSION) {
    setResult(result, ProductionParseResult::UnsupportedVersion);
    return false;
  }
  if (!productionIdIsValid(manifest.productionId)) {
    setResult(result, ProductionParseResult::InvalidProductionId);
    return false;
  }
  if (!productionRelativePathIsValid(manifest.timeline)) {
    setResult(result, ProductionParseResult::InvalidTimelinePath);
    return false;
  }
  if (!textIsSingleLine(manifest.name) || !textIsSingleLine(manifest.description) ||
      !textIsSingleLine(manifest.author) || !textIsSingleLine(manifest.created) ||
      !textIsSingleLine(manifest.modified)) {
    setResult(result, ProductionParseResult::InvalidJson);
    return false;
  }
  *out = manifest;
  setResult(result, ProductionParseResult::Ok);
  return true;

invalid:
  setResult(result, ProductionParseResult::InvalidJson);
  return false;
}

bool productionParseTimeline(const char *json, size_t jsonLen,
                             ProductionCue *cueBuffer, size_t cueCapacity,
                             ProductionTimeline *out,
                             ProductionParseResult *result) {
  if (!json || !cueBuffer || !out || jsonLen == 0 || cueCapacity == 0) {
    setResult(result, ProductionParseResult::InvalidJson);
    return false;
  }
  setResult(result, ProductionParseResult::Ok);
  ProductionTimeline timeline;
  JsonReader r(json, jsonLen);
  bool seenVersion = false;
  bool seenCues = false;
  bool done = false;
  if (!r.take('{') || r.take('}')) goto invalid;
  while (!done) {
    char key[32];
    if (!r.string(key, sizeof(key)) || !r.take(':')) goto invalid;
    if (strcmp(key, "formatVersion") == 0) {
      if (seenVersion) goto invalid;
      seenVersion = true;
      uint32_t version = 0;
      if (!r.u32(&version) || version > UINT16_MAX) goto invalid;
      timeline.formatVersion = (uint16_t)version;
    } else if (strcmp(key, "cues") == 0) {
      if (seenCues || !r.take('[')) goto invalid;
      seenCues = true;
      if (!r.take(']')) {
        while (true) {
          if (timeline.cueCount >= cueCapacity ||
              timeline.cueCount >= SHOWDUINO_PRODUCTION_MAX_CUES) {
            setResult(result, ProductionParseResult::TooManyCues);
            return false;
          }
          ProductionCue &cue = cueBuffer[timeline.cueCount];
          if (!parseCue(r, &cue, result)) {
            if (result && *result != ProductionParseResult::Ok) return false;
            goto invalid;
          }
          if (timeline.cueCount > 0 &&
              cue.timeMs < cueBuffer[timeline.cueCount - 1].timeMs) {
            setResult(result, ProductionParseResult::InvalidCueTime);
            return false;
          }
          for (uint16_t i = 0; i < timeline.cueCount; ++i) {
            if (strcmp(cue.id, cueBuffer[i].id) == 0) {
              setResult(result, ProductionParseResult::DuplicateCueId);
              return false;
            }
          }
          ++timeline.cueCount;
          if (r.take(']')) break;
          if (!r.take(',')) goto invalid;
        }
      }
    } else if (!r.skipValue()) {
      goto invalid;
    }
    if (!finishMember(r, &done)) goto invalid;
  }
  if (!r.finished()) goto invalid;
  if (!seenVersion || !seenCues) {
    setResult(result, ProductionParseResult::MissingField);
    return false;
  }
  if (timeline.formatVersion != SHOWDUINO_TIMELINE_FORMAT_VERSION) {
    setResult(result, ProductionParseResult::UnsupportedVersion);
    return false;
  }
  if (timeline.cueCount == 0) {
    setResult(result, ProductionParseResult::EmptyTimeline);
    return false;
  }
  *out = timeline;
  setResult(result, ProductionParseResult::Ok);
  return true;

invalid:
  setResult(result, ProductionParseResult::InvalidJson);
  return false;
}
