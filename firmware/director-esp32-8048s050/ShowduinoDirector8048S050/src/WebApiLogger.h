#ifndef SHOWDUINO_WEB_API_LOGGER_H
#define SHOWDUINO_WEB_API_LOGGER_H

#include <Arduino.h>

enum WebApiLogSeverity : uint8_t {
  WEB_LOG_DEBUG = 0,
  WEB_LOG_INFO,
  WEB_LOG_WARN,
  WEB_LOG_ERROR
};

struct WebApiLogEntry {
  uint32_t timestampMs;
  uint8_t severity;
  char source[24];
  char message[96];
};

class WebApiLogger {
public:
  static const size_t CAPACITY = 250;

  WebApiLogger();
  ~WebApiLogger();

  void log(WebApiLogSeverity severity, const char *source, const char *message);
  void logHttpRequest(const char *method, const char *path);
  void appendJsonArray(String &out);

  static const char *severityName(WebApiLogSeverity sev);

private:
  WebApiLogEntry *buffer;
  size_t head;
  size_t count;
  mutable portMUX_TYPE mux;

  void appendEntry(uint32_t ts, WebApiLogSeverity sev, const char *source, const char *msg);
  size_t entryIndex(size_t offsetFromOldest);
};

extern WebApiLogger gWebApiLogger;

#endif
