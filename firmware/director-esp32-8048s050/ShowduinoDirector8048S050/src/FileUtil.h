#ifndef SHOWDUINO_FILE_UTIL_H
#define SHOWDUINO_FILE_UTIL_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include "StorageConfig.h"

namespace ShowduinoFileUtil {

inline bool pathLooksSafe(const char *path) {
  if (path == nullptr || path[0] != '/') return false;
  if (strlen(path) >= STORAGE_MAX_PATH_LEN) return false;
  if (strstr(path, "..") != nullptr) return false;
  return true;
}

inline String parentDir(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash <= 0) return "/";
  return path.substring(0, slash);
}

inline bool ensureDir(const char *path) {
  if (!pathLooksSafe(path)) return false;

  auto isDir = [](const char *p) -> bool {
    if (!SD.exists(p)) return false;
    File f = SD.open(p);
    bool ok = f && f.isDirectory();
    if (f) f.close();
    return ok;
  };

  if (isDir(path)) return true;

  Serial.printf("[Storage] mkdir %s\n", path);
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    bool created = SD.mkdir(path);
    if (created) {
      if (isDir(path)) return true;
      Serial.printf("[Storage] mkdir returned true but path is not a dir: %s\n", path);
    }
    delay(30);
    if (isDir(path)) return true;  // some cards report mkdir false but succeed
  }
  Serial.printf("[Storage] mkdir exhausted for %s (exists=%d)\n", path, (int)SD.exists(path));
  return isDir(path);
}

inline bool ensureParentDirs(const char *filePath) {
  String path = parentDir(String(filePath));
  if (path.length() <= 1) return true;

  // Build progressively: /a, /a/b, /a/b/c
  String built;
  int start = 1;
  while (start < (int)path.length()) {
    int next = path.indexOf('/', start);
    if (next < 0) next = path.length();
    built = path.substring(0, next);
    if (!ensureDir(built.c_str())) return false;
    start = next + 1;
  }
  return true;
}

inline bool readTextFile(const char *path, String &out, size_t maxBytes = STORAGE_MAX_JSON_BYTES) {
  out = "";
  if (!pathLooksSafe(path) || !SD.exists(path)) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  size_t n = 0;
  while (f.available() && n < maxBytes) {
    out += (char)f.read();
    n++;
  }
  f.close();
  return true;
}

inline bool writeTextDirect(const char *path, const String &content) {
  if (!pathLooksSafe(path)) return false;
  if (!ensureParentDirs(path)) return false;
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  size_t wrote = f.print(content);
  f.flush();
  f.close();
  return wrote == content.length();
}

inline bool fileNonEmpty(const char *path) {
  if (!SD.exists(path)) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  size_t sz = f.size();
  f.close();
  return sz > 0;
}

inline bool looksLikeJsonObject(const String &s) {
  int a = s.indexOf('{');
  int b = s.lastIndexOf('}');
  return a >= 0 && b > a;
}

inline bool validateJsonFile(const char *path) {
  String body;
  if (!readTextFile(path, body)) return false;
  return looksLikeJsonObject(body);
}

inline bool atomicWriteTextFile(const char *path, const String &content) {
  if (!pathLooksSafe(path)) return false;
  if (!ensureParentDirs(path)) return false;

  String tmp = String(path) + ".tmp";
  String bak = String(path) + ".bak";

  Serial.printf("[Storage] atomic write %s (%u bytes)\n", path, (unsigned)content.length());

  if (SD.exists(tmp.c_str())) SD.remove(tmp.c_str());
  if (!writeTextDirect(tmp.c_str(), content)) {
    Serial.println("[Storage] atomic tmp write failed");
    return false;
  }
  if (!validateJsonFile(tmp.c_str()) && content.indexOf('{') >= 0) {
    // Non-JSON text files still allowed if no '{' present.
    if (content.indexOf('{') >= 0) {
      Serial.println("[Storage] atomic tmp JSON validate failed");
      SD.remove(tmp.c_str());
      return false;
    }
  }

  if (SD.exists(path)) {
    if (SD.exists(bak.c_str())) SD.remove(bak.c_str());
    if (!SD.rename(path, bak.c_str())) {
      Serial.println("[Storage] atomic rename to .bak failed");
      return false;
    }
  }

  if (!SD.rename(tmp.c_str(), path)) {
    Serial.println("[Storage] atomic rename tmp->final failed; restoring bak");
    if (SD.exists(bak.c_str())) SD.rename(bak.c_str(), path);
    return false;
  }

  if (!fileNonEmpty(path)) {
    Serial.println("[Storage] atomic final missing; restoring bak");
    if (SD.exists(bak.c_str())) SD.rename(bak.c_str(), path);
    return false;
  }

  // Keep .bak until next successful save (useful for recovery).
  return true;
}

inline bool atomicWriteBinaryFile(const char *path, const uint8_t *data, size_t length) {
  if (!pathLooksSafe(path) || data == nullptr) return false;
  if (!ensureParentDirs(path)) return false;

  String tmp = String(path) + ".tmp";
  String bak = String(path) + ".bak";
  if (SD.exists(tmp.c_str())) SD.remove(tmp.c_str());

  File f = SD.open(tmp.c_str(), FILE_WRITE);
  if (!f) return false;
  size_t wrote = f.write(data, length);
  f.flush();
  f.close();
  if (wrote != length) {
    SD.remove(tmp.c_str());
    return false;
  }

  if (SD.exists(path)) {
    if (SD.exists(bak.c_str())) SD.remove(bak.c_str());
    if (!SD.rename(path, bak.c_str())) return false;
  }
  if (!SD.rename(tmp.c_str(), path)) {
    if (SD.exists(bak.c_str())) SD.rename(bak.c_str(), path);
    return false;
  }
  return true;
}

inline bool recoverAtomicFile(const char *path) {
  if (!pathLooksSafe(path)) return false;
  String tmp = String(path) + ".tmp";
  String bak = String(path) + ".bak";

  if (SD.exists(path) && fileNonEmpty(path)) {
    if (SD.exists(tmp.c_str())) {
      Serial.printf("[Storage] removing stale tmp for %s\n", path);
      SD.remove(tmp.c_str());
    }
    return true;
  }

  if (SD.exists(tmp.c_str()) && fileNonEmpty(tmp.c_str())) {
    Serial.printf("[Storage] recovering %s from .tmp\n", path);
    if (SD.exists(path)) SD.remove(path);
    if (SD.rename(tmp.c_str(), path)) return true;
  }

  if (SD.exists(bak.c_str()) && fileNonEmpty(bak.c_str())) {
    Serial.printf("[Storage] recovering %s from .bak\n", path);
    if (SD.exists(path)) SD.remove(path);
    if (SD.rename(bak.c_str(), path)) return true;
  }

  return SD.exists(path);
}

inline String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '\\' || c == '"') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

inline String jsonGetString(const String &json, const char *key, const char *fallback = "") {
  String pattern = String("\"") + key + "\"";
  int k = json.indexOf(pattern);
  if (k < 0) return String(fallback);
  int colon = json.indexOf(':', k + pattern.length());
  if (colon < 0) return String(fallback);
  int q1 = json.indexOf('"', colon + 1);
  if (q1 < 0) return String(fallback);
  int q2 = q1 + 1;
  while (q2 < (int)json.length()) {
    if (json[q2] == '"' && json[q2 - 1] != '\\') break;
    q2++;
  }
  if (q2 >= (int)json.length()) return String(fallback);
  return json.substring(q1 + 1, q2);
}

inline long jsonGetLong(const String &json, const char *key, long fallback = 0) {
  String pattern = String("\"") + key + "\"";
  int k = json.indexOf(pattern);
  if (k < 0) return fallback;
  int colon = json.indexOf(':', k + pattern.length());
  if (colon < 0) return fallback;
  int i = colon + 1;
  while (i < (int)json.length() && (json[i] == ' ' || json[i] == '\t')) i++;
  return json.substring(i).toInt();
}

inline bool jsonGetBool(const String &json, const char *key, bool fallback = false) {
  String pattern = String("\"") + key + "\"";
  int k = json.indexOf(pattern);
  if (k < 0) return fallback;
  int colon = json.indexOf(':', k + pattern.length());
  if (colon < 0) return fallback;
  String rest = json.substring(colon + 1);
  rest.trim();
  if (rest.startsWith("true")) return true;
  if (rest.startsWith("false")) return false;
  return fallback;
}

inline void formatIsoTimestamp(char *buf, size_t buflen) {
  // No RTC required — use uptime-based stamp; replace with RTC when available.
  unsigned long ms = millis();
  snprintf(buf, buflen, "T+%lu.%03luZ", ms / 1000UL, ms % 1000UL);
}

}  // namespace ShowduinoFileUtil

#endif
