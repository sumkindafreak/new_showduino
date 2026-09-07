#ifndef SHOWDUINO_STORAGE_H
#define SHOWDUINO_STORAGE_H

/*
 * Host-testable P4 storage rules.
 * No Arduino, SD, or filesystem side effects.
 *
 * SD is the persistent backbone, not the safety backbone.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_STORAGE_FORMAT_VERSION   1
#define SHOWDUINO_STORAGE_ROOT             "/showduino"
#define SHOWDUINO_STORAGE_PATH_MAX         191
#define SHOWDUINO_STORAGE_CONFIG_MAX       8192
#define SHOWDUINO_STORAGE_SYSTEM_MAX       4096
#define SHOWDUINO_STORAGE_NAME_MAX         47
#define SHOWDUINO_STORAGE_LOG_ROTATE_BYTES 32768
#define SHOWDUINO_STORAGE_LOG_KEEP         3

typedef enum ShowduinoStorageState {
  SHOWDUINO_STORAGE_ONLINE = 0,
  SHOWDUINO_STORAGE_DEGRADED,
  SHOWDUINO_STORAGE_READ_ONLY,
  SHOWDUINO_STORAGE_OFFLINE,
  SHOWDUINO_STORAGE_FAULT
} ShowduinoStorageState;

typedef enum ShowduinoPathStatus {
  SHOWDUINO_PATH_OK = 0,
  SHOWDUINO_PATH_NULL,
  SHOWDUINO_PATH_EMPTY,
  SHOWDUINO_PATH_TOO_LONG,
  SHOWDUINO_PATH_NOT_ABSOLUTE,
  SHOWDUINO_PATH_BACKSLASH,
  SHOWDUINO_PATH_DOTDOT,
  SHOWDUINO_PATH_DOUBLE_SLASH,
  SHOWDUINO_PATH_OUTSIDE_ROOT,
  SHOWDUINO_PATH_BAD_NAME
} ShowduinoPathStatus;

typedef enum ShowduinoConfigStatus {
  SHOWDUINO_CFG_OK = 0,
  SHOWDUINO_CFG_NULL,
  SHOWDUINO_CFG_EMPTY,
  SHOWDUINO_CFG_TOO_LARGE,
  SHOWDUINO_CFG_NOT_OBJECT,
  SHOWDUINO_CFG_MISSING_VERSION,
  SHOWDUINO_CFG_BAD_VERSION,
  SHOWDUINO_CFG_UNSUPPORTED_VERSION,
  SHOWDUINO_CFG_DUPLICATE_VERSION
} ShowduinoConfigStatus;

typedef enum ShowduinoAtomicStep {
  SHOWDUINO_ATOMIC_IDLE = 0,
  SHOWDUINO_ATOMIC_VALIDATE_PATH,
  SHOWDUINO_ATOMIC_PRESERVE,
  SHOWDUINO_ATOMIC_WRITE_TEMP,
  SHOWDUINO_ATOMIC_FLUSH,
  SHOWDUINO_ATOMIC_VALIDATE_TEMP,
  SHOWDUINO_ATOMIC_REPLACE,
  SHOWDUINO_ATOMIC_DONE,
  SHOWDUINO_ATOMIC_FAIL
} ShowduinoAtomicStep;

typedef enum ShowduinoAtomicEvent {
  SHOWDUINO_ATOMIC_EV_START = 0,
  SHOWDUINO_ATOMIC_EV_PATH_OK,
  SHOWDUINO_ATOMIC_EV_PATH_BAD,
  SHOWDUINO_ATOMIC_EV_PRESERVE_OK,
  SHOWDUINO_ATOMIC_EV_PRESERVE_FAIL,
  SHOWDUINO_ATOMIC_EV_WRITE_OK,
  SHOWDUINO_ATOMIC_EV_WRITE_FAIL,
  SHOWDUINO_ATOMIC_EV_FLUSH_OK,
  SHOWDUINO_ATOMIC_EV_FLUSH_FAIL,
  SHOWDUINO_ATOMIC_EV_VALIDATE_OK,
  SHOWDUINO_ATOMIC_EV_VALIDATE_FAIL,
  SHOWDUINO_ATOMIC_EV_REPLACE_OK,
  SHOWDUINO_ATOMIC_EV_REPLACE_FAIL
} ShowduinoAtomicEvent;

static inline const char *showduino_storage_state_name(ShowduinoStorageState state) {
  switch (state) {
    case SHOWDUINO_STORAGE_ONLINE: return "ONLINE";
    case SHOWDUINO_STORAGE_DEGRADED: return "DEGRADED";
    case SHOWDUINO_STORAGE_READ_ONLY: return "READ_ONLY";
    case SHOWDUINO_STORAGE_OFFLINE: return "OFFLINE";
    case SHOWDUINO_STORAGE_FAULT: return "FAULT";
    default: return "FAULT";
  }
}

static inline const char *showduino_storage_path_name(ShowduinoPathStatus st) {
  switch (st) {
    case SHOWDUINO_PATH_OK: return "ok";
    case SHOWDUINO_PATH_NULL: return "null";
    case SHOWDUINO_PATH_EMPTY: return "empty";
    case SHOWDUINO_PATH_TOO_LONG: return "too_long";
    case SHOWDUINO_PATH_NOT_ABSOLUTE: return "not_absolute";
    case SHOWDUINO_PATH_BACKSLASH: return "backslash";
    case SHOWDUINO_PATH_DOTDOT: return "dotdot";
    case SHOWDUINO_PATH_DOUBLE_SLASH: return "double_slash";
    case SHOWDUINO_PATH_OUTSIDE_ROOT: return "outside_root";
    case SHOWDUINO_PATH_BAD_NAME: return "bad_name";
    default: return "invalid";
  }
}

static inline const char *showduino_storage_cfg_name(ShowduinoConfigStatus st) {
  switch (st) {
    case SHOWDUINO_CFG_OK: return "ok";
    case SHOWDUINO_CFG_NULL: return "null";
    case SHOWDUINO_CFG_EMPTY: return "empty";
    case SHOWDUINO_CFG_TOO_LARGE: return "too_large";
    case SHOWDUINO_CFG_NOT_OBJECT: return "not_object";
    case SHOWDUINO_CFG_MISSING_VERSION: return "missing_version";
    case SHOWDUINO_CFG_BAD_VERSION: return "bad_version";
    case SHOWDUINO_CFG_UNSUPPORTED_VERSION: return "unsupported_version";
    case SHOWDUINO_CFG_DUPLICATE_VERSION: return "duplicate_version";
    default: return "invalid";
  }
}

static inline ShowduinoPathStatus showduino_storage_path_check(const char *path) {
  if (!path) return SHOWDUINO_PATH_NULL;
  const size_t n = strlen(path);
  if (n == 0) return SHOWDUINO_PATH_EMPTY;
  if (n > SHOWDUINO_STORAGE_PATH_MAX) return SHOWDUINO_PATH_TOO_LONG;
  if (path[0] != '/') return SHOWDUINO_PATH_NOT_ABSOLUTE;
  if (strchr(path, '\\')) return SHOWDUINO_PATH_BACKSLASH;
  if (strstr(path, "..")) return SHOWDUINO_PATH_DOTDOT;
  if (strstr(path, "//")) return SHOWDUINO_PATH_DOUBLE_SLASH;

  const char *root = SHOWDUINO_STORAGE_ROOT;
  const size_t rl = strlen(root);
  if (n < rl) return SHOWDUINO_PATH_OUTSIDE_ROOT;
  if (strncmp(path, root, rl) != 0) return SHOWDUINO_PATH_OUTSIDE_ROOT;
  if (n > rl && path[rl] != '/') return SHOWDUINO_PATH_OUTSIDE_ROOT;
  return SHOWDUINO_PATH_OK;
}

static inline int showduino_storage_name_ok(const char *name) {
  if (!name || !name[0]) return 0;
  if (strlen(name) > SHOWDUINO_STORAGE_NAME_MAX) return 0;
  if (strchr(name, '/') || strchr(name, '\\')) return 0;
  if (strstr(name, "..")) return 0;
  return 1;
}

static inline ShowduinoPathStatus showduino_storage_path_join(const char *parent,
                                                             const char *name,
                                                             char *out,
                                                             size_t outLen) {
  if (!out || outLen < 2) return SHOWDUINO_PATH_TOO_LONG;
  out[0] = '\0';
  if (!parent || !name) return SHOWDUINO_PATH_NULL;
  if (!showduino_storage_name_ok(name)) return SHOWDUINO_PATH_BAD_NAME;

  const ShowduinoPathStatus parentSt = showduino_storage_path_check(parent);
  if (parentSt != SHOWDUINO_PATH_OK) return parentSt;

  const size_t pl = strlen(parent);
  const size_t nl = strlen(name);
  const int needSlash = (pl > 0 && parent[pl - 1] != '/');
  const size_t total = pl + (needSlash ? 1u : 0u) + nl;
  if (total >= outLen || total > SHOWDUINO_STORAGE_PATH_MAX) return SHOWDUINO_PATH_TOO_LONG;

  memcpy(out, parent, pl);
  size_t o = pl;
  if (needSlash) out[o++] = '/';
  memcpy(out + o, name, nl + 1);
  return showduino_storage_path_check(out);
}

static inline ShowduinoPathStatus showduino_storage_temp_path(const char *finalPath,
                                                             char *out,
                                                             size_t outLen) {
  const ShowduinoPathStatus st = showduino_storage_path_check(finalPath);
  if (st != SHOWDUINO_PATH_OK) {
    if (out && outLen) out[0] = '\0';
    return st;
  }
  const size_t n = strlen(finalPath);
  if (n + 4 >= outLen || n + 4 > SHOWDUINO_STORAGE_PATH_MAX) return SHOWDUINO_PATH_TOO_LONG;
  memcpy(out, finalPath, n);
  memcpy(out + n, ".tmp", 5);
  return SHOWDUINO_PATH_OK;
}

static inline const char *showduino_storage_basename(const char *path) {
  if (!path || !path[0]) return "";
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static inline uint8_t showduino_storage_count_key(const char *json, size_t len, const char *key) {
  if (!json || !key || !key[0] || len == 0) return 0;
  char needle[48];
  needle[0] = '"';
  const size_t kl = strlen(key);
  if (kl + 3 >= sizeof(needle)) return 0;
  memcpy(needle + 1, key, kl);
  needle[kl + 1] = '"';
  needle[kl + 2] = '\0';
  const size_t nl = kl + 2;
  uint8_t count = 0;
  for (size_t i = 0; i + nl <= len; i++) {
    if (memcmp(json + i, needle, nl) == 0) {
      if (count < 255) count++;
    }
  }
  return count;
}

static inline int showduino_storage_json_u32(const char *json, size_t len,
                                             const char *key, uint32_t *out) {
  if (!json || !key || !out || len == 0) return 0;
  char needle[48];
  needle[0] = '"';
  const size_t kl = strlen(key);
  if (kl + 3 >= sizeof(needle)) return 0;
  memcpy(needle + 1, key, kl);
  needle[kl + 1] = '"';
  needle[kl + 2] = '\0';
  const size_t nl = kl + 2;
  for (size_t i = 0; i + nl <= len; i++) {
    if (memcmp(json + i, needle, nl) != 0) continue;
    size_t j = i + nl;
    while (j < len && (json[j] == ' ' || json[j] == '\t' || json[j] == '\n' || json[j] == '\r')) j++;
    if (j >= len || json[j] != ':') continue;
    j++;
    while (j < len && (json[j] == ' ' || json[j] == '\t' || json[j] == '\n' || json[j] == '\r')) j++;
    if (j >= len || json[j] < '0' || json[j] > '9') return 0;
    uint32_t v = 0;
    while (j < len && json[j] >= '0' && json[j] <= '9') {
      const uint32_t d = (uint32_t)(json[j] - '0');
      if (v > (0xFFFFFFFFu - d) / 10u) return 0;
      v = v * 10u + d;
      j++;
    }
    *out = v;
    return 1;
  }
  return 0;
}

static inline ShowduinoConfigStatus showduino_storage_config_check(const char *json,
                                                                  size_t len,
                                                                  size_t maxLen,
                                                                  uint8_t expectedVersion) {
  if (!json && len > 0) return SHOWDUINO_CFG_NULL;
  if (!json || len == 0) return SHOWDUINO_CFG_EMPTY;
  if (len > maxLen) return SHOWDUINO_CFG_TOO_LARGE;

  size_t i = 0;
  while (i < len && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')) i++;
  if (i >= len || json[i] != '{') return SHOWDUINO_CFG_NOT_OBJECT;

  const uint8_t versions = showduino_storage_count_key(json, len, "formatVersion");
  if (versions == 0) return SHOWDUINO_CFG_MISSING_VERSION;
  if (versions > 1) return SHOWDUINO_CFG_DUPLICATE_VERSION;

  uint32_t ver = 0;
  if (!showduino_storage_json_u32(json, len, "formatVersion", &ver)) {
    return SHOWDUINO_CFG_BAD_VERSION;
  }
  if (ver == 0 || ver > 255u) return SHOWDUINO_CFG_BAD_VERSION;
  if ((uint8_t)ver != expectedVersion) return SHOWDUINO_CFG_UNSUPPORTED_VERSION;
  return SHOWDUINO_CFG_OK;
}

static inline int showduino_storage_boot_behaviour_ok(const char *value) {
  return value && strcmp(value, "idle") == 0;
}

static inline int showduino_storage_production_id_ok(const char *id) {
  if (!id) return 0;
  if (!id[0]) return 1; /* empty means none */
  const size_t n = strlen(id);
  if (n > SHOWDUINO_STORAGE_NAME_MAX) return 0;
  const char first = id[0];
  if (!((first >= 'a' && first <= 'z') || (first >= '0' && first <= '9'))) return 0;
  for (size_t i = 0; i < n; i++) {
    const char c = id[i];
    const int ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return 0;
  }
  return 1;
}

static inline ShowduinoAtomicStep showduino_storage_atomic_next(ShowduinoAtomicStep step,
                                                               ShowduinoAtomicEvent ev) {
  if (step == SHOWDUINO_ATOMIC_DONE || step == SHOWDUINO_ATOMIC_FAIL) return step;

  switch (step) {
    case SHOWDUINO_ATOMIC_IDLE:
      return (ev == SHOWDUINO_ATOMIC_EV_START) ? SHOWDUINO_ATOMIC_VALIDATE_PATH
                                               : SHOWDUINO_ATOMIC_FAIL;
    case SHOWDUINO_ATOMIC_VALIDATE_PATH:
      if (ev == SHOWDUINO_ATOMIC_EV_PATH_OK) return SHOWDUINO_ATOMIC_PRESERVE;
      return SHOWDUINO_ATOMIC_FAIL;
    case SHOWDUINO_ATOMIC_PRESERVE:
      /* Preserve is best-effort. Failure still continues to the temp write. */
      if (ev == SHOWDUINO_ATOMIC_EV_PRESERVE_OK || ev == SHOWDUINO_ATOMIC_EV_PRESERVE_FAIL) {
        return SHOWDUINO_ATOMIC_WRITE_TEMP;
      }
      return SHOWDUINO_ATOMIC_FAIL;
    case SHOWDUINO_ATOMIC_WRITE_TEMP:
      return (ev == SHOWDUINO_ATOMIC_EV_WRITE_OK) ? SHOWDUINO_ATOMIC_FLUSH
                                                  : SHOWDUINO_ATOMIC_FAIL;
    case SHOWDUINO_ATOMIC_FLUSH:
      return (ev == SHOWDUINO_ATOMIC_EV_FLUSH_OK) ? SHOWDUINO_ATOMIC_VALIDATE_TEMP
                                                  : SHOWDUINO_ATOMIC_FAIL;
    case SHOWDUINO_ATOMIC_VALIDATE_TEMP:
      return (ev == SHOWDUINO_ATOMIC_EV_VALIDATE_OK) ? SHOWDUINO_ATOMIC_REPLACE
                                                     : SHOWDUINO_ATOMIC_FAIL;
    case SHOWDUINO_ATOMIC_REPLACE:
      return (ev == SHOWDUINO_ATOMIC_EV_REPLACE_OK) ? SHOWDUINO_ATOMIC_DONE
                                                    : SHOWDUINO_ATOMIC_FAIL;
    default:
      return SHOWDUINO_ATOMIC_FAIL;
  }
}

static inline int showduino_storage_log_rotated_name(const char *channel,
                                                     uint8_t index,
                                                     char *out,
                                                     size_t outLen) {
  if (!channel || !out || outLen < 8 || index == 0 || index > 99) return 0;
  if (!showduino_storage_name_ok(channel)) return 0;
  char tmp[64];
  size_t n = 0;
  const char *p = channel;
  while (*p && n + 1 < sizeof(tmp)) tmp[n++] = *p++;
  if (n + 9 >= sizeof(tmp)) return 0;
  tmp[n++] = '-';
  tmp[n++] = '0';
  tmp[n++] = (char)('0' + (index / 10));
  tmp[n++] = (char)('0' + (index % 10));
  memcpy(tmp + n, ".log", 5);
  const size_t need = strlen(tmp);
  if (need + 1 > outLen) return 0;
  memcpy(out, tmp, need + 1);
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif
