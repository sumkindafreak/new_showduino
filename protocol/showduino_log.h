#ifndef SHOWDUINO_LOG_H
#define SHOWDUINO_LOG_H

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * Shared Showduino Serial logging.
 * Protocol, discovery, playback, and ownership behaviour must not live here.
 * Default INFO describes what the system is doing, not every byte it moves.
 */

#define SHOWDUINO_LOG_NONE  0
#define SHOWDUINO_LOG_ERROR 1
#define SHOWDUINO_LOG_WARN  2
#define SHOWDUINO_LOG_INFO  3
#define SHOWDUINO_LOG_DEBUG 4
#define SHOWDUINO_LOG_TRACE 5

#ifndef SHOWDUINO_LOG_LEVEL
#define SHOWDUINO_LOG_LEVEL SHOWDUINO_LOG_INFO
#endif

#ifndef SHOWDUINO_LOG_WARN_INTERVAL_MS
#define SHOWDUINO_LOG_WARN_INTERVAL_MS 5000UL
#endif

#ifndef SHOWDUINO_LOG_LINE_MAX
#define SHOWDUINO_LOG_LINE_MAX 192
#endif

#ifdef __cplusplus

inline uint8_t *showduino_log_level_ptr() {
  static uint8_t level = (uint8_t)SHOWDUINO_LOG_LEVEL;
  return &level;
}

inline uint8_t showduino_log_get_level() {
  return *showduino_log_level_ptr();
}

inline void showduino_log_set_level(uint8_t level) {
  if (level > SHOWDUINO_LOG_TRACE) level = SHOWDUINO_LOG_TRACE;
  *showduino_log_level_ptr() = level;
}

inline const char *showduino_log_level_name(uint8_t level) {
  switch (level) {
    case SHOWDUINO_LOG_NONE: return "NONE";
    case SHOWDUINO_LOG_ERROR: return "ERROR";
    case SHOWDUINO_LOG_WARN: return "WARN";
    case SHOWDUINO_LOG_INFO: return "INFO";
    case SHOWDUINO_LOG_DEBUG: return "DEBUG";
    case SHOWDUINO_LOG_TRACE: return "TRACE";
    default: return "INFO";
  }
}

inline int showduino_log_parse_level(const char *name) {
  if (!name || !name[0]) return -1;
  if (!strcasecmp(name, "NONE") || !strcmp(name, "0")) return SHOWDUINO_LOG_NONE;
  if (!strcasecmp(name, "ERROR") || !strcasecmp(name, "ERR") || !strcmp(name, "1")) {
    return SHOWDUINO_LOG_ERROR;
  }
  if (!strcasecmp(name, "WARN") || !strcasecmp(name, "WARNING") || !strcmp(name, "2")) {
    return SHOWDUINO_LOG_WARN;
  }
  if (!strcasecmp(name, "INFO") || !strcmp(name, "3")) return SHOWDUINO_LOG_INFO;
  if (!strcasecmp(name, "DEBUG") || !strcasecmp(name, "DBG") || !strcmp(name, "4")) {
    return SHOWDUINO_LOG_DEBUG;
  }
  if (!strcasecmp(name, "TRACE") || !strcmp(name, "5")) return SHOWDUINO_LOG_TRACE;
  return -1;
}

inline const char *showduino_log_level_tag(uint8_t level) {
  switch (level) {
    case SHOWDUINO_LOG_ERROR: return "[ERR] ";
    case SHOWDUINO_LOG_WARN: return "[WARN] ";
    case SHOWDUINO_LOG_DEBUG: return "[DBG] ";
    case SHOWDUINO_LOG_TRACE: return "[TRACE] ";
    default: return "";
  }
}

inline void showduino_log_emit(uint8_t level, const char *tag, const char *body) {
  if (level > showduino_log_get_level()) return;
  Serial.printf("[%s]%s%s\n", tag ? tag : "SYS", showduino_log_level_tag(level),
                body ? body : "");
}

inline void showduino_logf(uint8_t level, const char *tag, const char *fmt, ...) {
  if (level > showduino_log_get_level()) return;
  if (!fmt) return;
  char body[SHOWDUINO_LOG_LINE_MAX];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(body, sizeof(body), fmt, ap);
  va_end(ap);
  showduino_log_emit(level, tag, body);
}

#define SD_LOGE(tag, fmt, ...) \
  showduino_logf(SHOWDUINO_LOG_ERROR, (tag), (fmt), ##__VA_ARGS__)
#define SD_LOGW(tag, fmt, ...) \
  showduino_logf(SHOWDUINO_LOG_WARN, (tag), (fmt), ##__VA_ARGS__)
#define SD_LOGI(tag, fmt, ...) \
  showduino_logf(SHOWDUINO_LOG_INFO, (tag), (fmt), ##__VA_ARGS__)
#define SD_LOGD(tag, fmt, ...) \
  showduino_logf(SHOWDUINO_LOG_DEBUG, (tag), (fmt), ##__VA_ARGS__)
#define SD_LOGT(tag, fmt, ...) \
  showduino_logf(SHOWDUINO_LOG_TRACE, (tag), (fmt), ##__VA_ARGS__)

inline void showduino_log_emergency(bool active) {
  Serial.println(active ? "!!! EMERGENCY ACTIVE !!!" : "!!! EMERGENCY CLEARED !!!");
}

inline bool showduino_log_rate_ok(uint32_t *lastMs, uint32_t now, uint32_t intervalMs) {
  if (!lastMs) return true;
  if (*lastMs != 0 && (uint32_t)(now - *lastMs) < intervalMs) return false;
  *lastMs = now;
  return true;
}

inline bool showduino_log_rate(uint32_t *lastMs, uint32_t intervalMs) {
  return showduino_log_rate_ok(lastMs, millis(), intervalMs);
}

#define SD_LOG_EMERGENCY(tag, active) \
  do { \
    (void)(tag); \
    showduino_log_emergency((active) != 0); \
  } while (0)

inline bool showduino_log_changed(char *cache, size_t cacheLen, const char *value) {
  if (!cache || cacheLen == 0) return true;
  if (!value) value = "";
  if (strcmp(cache, value) == 0) return false;
  strncpy(cache, value, cacheLen - 1);
  cache[cacheLen - 1] = '\0';
  return true;
}

inline bool showduino_log_is_heartbeat(const char *s) {
  if (!s || !s[0]) return false;
  return !strcmp(s, "HEARTBEAT") || !strcmp(s, "OK:HEARTBEAT") ||
         !strcmp(s, "ACK:HEARTBEAT") || !strcmp(s, "DIAG:PING") ||
         !strcmp(s, "DIAG:PONG") || !strcmp(s, "PING") || !strcmp(s, "PONG");
}

inline bool showduino_log_is_routine_payload(const char *s) {
  if (!s || !s[0]) return false;
  if (!strncmp(s, "SOUND:STATUS", 12)) return true;
  if (!strncmp(s, "ANNOUNCE:", 9)) return true;
  if (!strncmp(s, "AUDIO:OWNED:", 12)) return true;
  if (!strncmp(s, "AUDIO:OWNER:", 12)) return true;
  if (!strncmp(s, "AUDIO:CAPS:", 11)) return true;
  if (!strncmp(s, "AUDIO:META:", 11)) return true;
  if (!strncmp(s, "AUDIO:INVENTORY:", 16)) return true;
  if (!strncmp(s, "AUDIO:STATUS:", 13)) return true;
  if (!strncmp(s, "STATUS:", 7)) return true;
  if (!strncmp(s, "LAMP:OWNED:", 11)) return true;
  if (!strncmp(s, "OWNED:", 6)) return true;
  return false;
}

inline bool showduino_log_is_routine_wire(const char *s) {
  if (!s || !s[0]) return false;
  if (showduino_log_is_heartbeat(s)) return true;
  if (showduino_log_is_routine_payload(s)) return true;
  if (!strncmp(s, "NODE:AUDIO:SOUND:STATUS", 23)) return true;
  if (!strncmp(s, "NODE:AUDIO:ANNOUNCE:", 20)) return true;
  if (!strncmp(s, "NODE:AUDIO:AUDIO:OWNED:", 23)) return true;
  if (!strncmp(s, "NODE:AUDIO:AUDIO:OWNER:", 23)) return true;
  if (!strncmp(s, "NODE:AUDIO:AUDIO:CAPS:", 22)) return true;
  if (!strncmp(s, "NODE:AUDIO:AUDIO:META:", 22)) return true;
  if (!strncmp(s, "NODE:AUDIO:AUDIO:INVENTORY:", 27)) return true;
  if (!strncmp(s, "NODE:AUDIO:AUDIO:STATUS:", 24)) return true;
  if (!strncmp(s, "NODE:AUDIO:STATUS:", 18)) return true;
  if (!strncmp(s, "NODE:LAMP:ANNOUNCE:", 19)) return true;
  if (!strncmp(s, "NODE:LAMP:STATUS:", 17)) return true;
  if (!strncmp(s, "NODE:PIXEL:ANNOUNCE:", 20)) return true;
  if (!strncmp(s, "NODE:PIXEL:STATUS:", 18)) return true;
  if (!strncmp(s, "STATE:", 6)) return true;
  if (!strncmp(s, "SHOW:STATE:", 11)) return true;
  if (!strcmp(s, "STATUS:READY") || !strcmp(s, "STATUS:REQUEST") ||
      !strcmp(s, "SHOW:STATE?")) {
    return true;
  }
  if (!strncmp(s, "TIME:", 5) && strcmp(s, "TIME:REQUEST") != 0) return true;
  if (!strncmp(s, "ROUTE:AUDIO:", 12)) {
    if (strstr(s, "AUDIO:NODE:STATUS") || strstr(s, "AUDIO:NODE:OWN:GRANT") ||
        strstr(s, "AUDIO:NODE:INVENTORY")) {
      return true;
    }
  }
  return false;
}

inline void showduino_log_packet(const char *tag, const char *dir, const char *line) {
  if (!line) return;
  if (showduino_log_is_heartbeat(line) || showduino_log_is_routine_wire(line)) {
    SD_LOGT(tag, "%s %s", dir ? dir : "", line);
  } else {
    SD_LOGD(tag, "%s %s", dir ? dir : "", line);
  }
}

inline bool showduino_log_handle_command(const char *line) {
  if (!line || !line[0]) return false;
  if (!strcmp(line, "LOG:LEVEL")) {
    Serial.println(showduino_log_level_name(showduino_log_get_level()));
    return true;
  }
  if (!strncmp(line, "LOG:LEVEL:", 10)) {
    const int lvl = showduino_log_parse_level(line + 10);
    if (lvl < 0) {
      Serial.println("ERR:LOG:LEVEL");
      return true;
    }
    showduino_log_set_level((uint8_t)lvl);
    Serial.print("OK:");
    Serial.println(showduino_log_level_name((uint8_t)lvl));
    return true;
  }
  return false;
}

#endif /* __cplusplus */

#endif /* SHOWDUINO_LOG_H */
