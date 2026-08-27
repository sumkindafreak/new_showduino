#include "PluginJson.h"
#include "PluginTypes.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *skipWs(const char *p) {
  while (p && *p && isspace((unsigned char)*p)) p++;
  return p;
}

static bool keyAt(const char *p, const char *key) {
  size_t n = strlen(key);
  if (*p != '"') return false;
  p++;
  if (strncmp(p, key, n) != 0) return false;
  p += n;
  return *p == '"';
}

static const char *findKey(const char *json, const char *end, const char *key) {
  if (!json || !key) return nullptr;
  const char *p = skipWs(json);
  int depth = 0;
  bool inStr = false;
  bool esc = false;
  while (p < end && *p) {
    char c = *p;
    if (inStr) {
      if (esc) esc = false;
      else if (c == '\\') esc = true;
      else if (c == '"') inStr = false;
      p++;
      continue;
    }
    if (c == '"') {
      if (depth == 1 && keyAt(p, key)) {
        p += strlen(key) + 2;
        p = skipWs(p);
        if (*p == ':') return skipWs(p + 1);
      }
      inStr = true;
      p++;
      continue;
    }
    if (c == '{' || c == '[') depth++;
    else if (c == '}' || c == ']') {
      depth--;
      if (depth < 0) break;
    }
    p++;
  }
  return nullptr;
}

static const char *objectEnd(const char *begin) {
  if (!begin) return nullptr;
  const char *p = skipWs(begin);
  if (*p != '{') return nullptr;
  int depth = 0;
  bool inStr = false;
  bool esc = false;
  while (*p) {
    char c = *p;
    if (inStr) {
      if (esc) esc = false;
      else if (c == '\\') esc = true;
      else if (c == '"') inStr = false;
      p++;
      continue;
    }
    if (c == '"') {
      inStr = true;
      p++;
      continue;
    }
    if (c == '{') depth++;
    else if (c == '}') {
      depth--;
      p++;
      if (depth == 0) return p;
      continue;
    }
    p++;
  }
  return nullptr;
}

bool pluginJsonExtractString(const char *json, const char *key, char *out, size_t outLen) {
  if (!json || !out || outLen < 1) return false;
  out[0] = '\0';
  const char *end = json + strlen(json);
  const char *v = findKey(json, end, key);
  if (!v || *v != '"') return false;
  v++;
  size_t n = 0;
  while (*v && *v != '"' && n + 1 < outLen) {
    if (*v == '\\' && v[1]) v++;
    out[n++] = *v++;
  }
  out[n] = '\0';
  return n > 0;
}

bool pluginJsonExtractU32(const char *json, const char *key, uint32_t *out) {
  if (!json || !out) return false;
  const char *end = json + strlen(json);
  const char *v = findKey(json, end, key);
  if (!v) return false;
  if (*v == '"') v++;
  char *term = nullptr;
  unsigned long n = strtoul(v, &term, 0);
  if (term == v) return false;
  *out = (uint32_t)n;
  return true;
}

bool pluginJsonExtractHexByte(const char *json, const char *key, uint8_t *out) {
  uint32_t v = 0;
  if (!pluginJsonExtractU32(json, key, &v)) return false;
  if (v > 255) return false;
  *out = (uint8_t)v;
  return true;
}

bool pluginJsonObjectSlice(const char *json, const char *key, const char **begin, const char **end) {
  if (!json || !begin || !end) return false;
  const char *docEnd = json + strlen(json);
  const char *v = findKey(json, docEnd, key);
  if (!v || *v != '{') return false;
  const char *e = objectEnd(v);
  if (!e) return false;
  *begin = v;
  *end = e;
  return true;
}

static const char *arraySlice(const char *json, const char *arrayKey, const char **endOut) {
  const char *docEnd = json + strlen(json);
  const char *v = findKey(json, docEnd, arrayKey);
  if (!v || *v != '[') return nullptr;
  const char *p = v;
  int depth = 0;
  bool inStr = false;
  bool esc = false;
  while (*p) {
    char c = *p;
    if (inStr) {
      if (esc) esc = false;
      else if (c == '\\') esc = true;
      else if (c == '"') inStr = false;
      p++;
      continue;
    }
    if (c == '"') {
      inStr = true;
      p++;
      continue;
    }
    if (c == '[' || c == '{') depth++;
    else if (c == ']' || c == '}') {
      if (c == ']' && depth == 1) {
        if (endOut) *endOut = p + 1;
        return v;
      }
      depth--;
    }
    p++;
  }
  return nullptr;
}

int pluginJsonArrayObjectCount(const char *json, const char *arrayKey) {
  const char *arrEnd = nullptr;
  const char *arr = arraySlice(json, arrayKey, &arrEnd);
  if (!arr) return 0;
  int count = 0;
  const char *p = arr + 1;
  while (p < arrEnd) {
    p = skipWs(p);
    if (*p == ']') break;
    if (*p == '{') {
      const char *e = objectEnd(p);
      if (!e) break;
      count++;
      p = e;
      p = skipWs(p);
      if (*p == ',') p++;
    } else {
      break;
    }
  }
  return count;
}

bool pluginJsonArrayObjectAt(const char *json, const char *arrayKey, int index,
                             const char **begin, const char **end) {
  if (!begin || !end || index < 0) return false;
  const char *arrEnd = nullptr;
  const char *arr = arraySlice(json, arrayKey, &arrEnd);
  if (!arr) return false;
  int i = 0;
  const char *p = arr + 1;
  while (p < arrEnd) {
    p = skipWs(p);
    if (*p != '{') return false;
    const char *e = objectEnd(p);
    if (!e) return false;
    if (i == index) {
      *begin = p;
      *end = e;
      return true;
    }
    i++;
    p = skipWs(e);
    if (*p == ',') p++;
  }
  return false;
}

bool pluginJsonParseAddressRange(const char *text, uint8_t *minAddr, uint8_t *maxAddr) {
  if (!text || !minAddr || !maxAddr) return false;
  char buf[24];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char *dash = strchr(buf, '-');
  if (dash) {
    *dash = '\0';
    unsigned long a = strtoul(buf, nullptr, 0);
    unsigned long b = strtoul(dash + 1, nullptr, 0);
    if (a > 0x7F || b > 0x7F || a > b) return false;
    *minAddr = (uint8_t)a;
    *maxAddr = (uint8_t)b;
    return true;
  }
  unsigned long a = strtoul(buf, nullptr, 0);
  if (a > 0x7F) return false;
  *minAddr = *maxAddr = (uint8_t)a;
  return true;
}

uint32_t pluginJsonParseCapabilities(const char *json) {
  const char *arrEnd = nullptr;
  const char *arr = arraySlice(json, "capabilities", &arrEnd);
  if (!arr) return 0;
  uint32_t caps = 0;
  const char *p = arr + 1;
  while (p < arrEnd) {
    p = skipWs(p);
    if (*p == ']') break;
    if (*p != '"') break;
    p++;
    char name[24];
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < sizeof(name)) name[n++] = *p++;
    name[n] = '\0';
    if (*p == '"') p++;
    caps |= pluginCapFromName(name);
    p = skipWs(p);
    if (*p == ',') p++;
  }
  return caps;
}
