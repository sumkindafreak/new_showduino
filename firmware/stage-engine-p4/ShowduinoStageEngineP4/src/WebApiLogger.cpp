#include "WebApiLogger.h"
#include "WebJson.h"
#include <esp_heap_caps.h>

WebApiLogger gWebApiLogger;

WebApiLogger::WebApiLogger()
    : buffer(nullptr), head(0), count(0), mux(portMUX_INITIALIZER_UNLOCKED) {
  buffer = (WebApiLogEntry *)heap_caps_malloc(CAPACITY * sizeof(WebApiLogEntry),
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buffer) {
    buffer = (WebApiLogEntry *)malloc(CAPACITY * sizeof(WebApiLogEntry));
  }
  if (buffer) {
    memset(buffer, 0, CAPACITY * sizeof(WebApiLogEntry));
  }
}

const char *WebApiLogger::severityName(WebApiLogSeverity sev) {
  switch (sev) {
    case WEB_LOG_DEBUG: return "debug";
    case WEB_LOG_INFO: return "info";
    case WEB_LOG_WARN: return "warn";
    case WEB_LOG_ERROR: return "error";
  }
  return "info";
}

void WebApiLogger::appendEntry(uint32_t ts, WebApiLogSeverity sev, const char *source, const char *msg) {
  if (!buffer) return;

  portENTER_CRITICAL(&mux);
  WebApiLogEntry &e = buffer[head];
  e.timestampMs = ts;
  e.severity = (uint8_t)sev;
  strncpy(e.source, source ? source : "WebUI", sizeof(e.source) - 1);
  e.source[sizeof(e.source) - 1] = '\0';
  strncpy(e.message, msg ? msg : "", sizeof(e.message) - 1);
  e.message[sizeof(e.message) - 1] = '\0';
  head = (head + 1) % CAPACITY;
  if (count < CAPACITY) count++;
  portEXIT_CRITICAL(&mux);
}

void WebApiLogger::log(WebApiLogSeverity severity, const char *source, const char *message) {
  appendEntry(millis(), severity, source, message);
}

void WebApiLogger::logHttpRequest(const char *method, const char *path) {
  char msg[96];
  snprintf(msg, sizeof(msg), "%s %s", method ? method : "?", path ? path : "/");
  log(WEB_LOG_INFO, "HTTP", msg);
}

size_t WebApiLogger::entryIndex(size_t offsetFromOldest) {
  if (count < CAPACITY) return offsetFromOldest;
  return (head + offsetFromOldest) % CAPACITY;
}

void WebApiLogger::appendJsonArray(String &out) {
  if (!buffer) {
    out += "[]";
    return;
  }

  out += '[';
  portENTER_CRITICAL(&mux);
  const size_t n = count;
  for (size_t i = 0; i < n; i++) {
    const WebApiLogEntry &e = buffer[entryIndex(i)];
    if (i > 0) out += ',';
    out += "\n  {";
    out += "\"timestampMs\":" + String(e.timestampMs);
    out += ",\"severity\":\"" + String(severityName((WebApiLogSeverity)e.severity)) + "\"";
    out += ",\"source\":\"" + ShowduinoWebJson::escape(String(e.source)) + "\"";
    out += ",\"message\":\"" + ShowduinoWebJson::escape(String(e.message)) + "\"";
    out += '}';
  }
  portEXIT_CRITICAL(&mux);
  out += "\n]";
}
