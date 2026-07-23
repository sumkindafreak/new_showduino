#ifndef SHOWDUINO_DEVICE_EVENT_LOG_H
#define SHOWDUINO_DEVICE_EVENT_LOG_H

#include <Arduino.h>
#include <stdint.h>
#include "time/TimeService.h"

class DeviceEventLog {
 public:
  static const size_t CAPACITY = 48;

  void log(const char *event, const char *detail) {
    Entry &e = entries_[head_];
    e.ms = millis();
    if (gTimeService.ready()) {
      gTimeService.nowIso8601(e.iso, sizeof(e.iso));
      e.epoch = gTimeService.epochSeconds();
    } else {
      strncpy(e.iso, "", sizeof(e.iso) - 1);
      e.epoch = 0;
    }
    strncpy(e.event, event ? event : "event", sizeof(e.event) - 1);
    e.event[sizeof(e.event) - 1] = '\0';
    strncpy(e.detail, detail ? detail : "", sizeof(e.detail) - 1);
    e.detail[sizeof(e.detail) - 1] = '\0';
    head_ = (head_ + 1) % CAPACITY;
    if (count_ < CAPACITY) count_++;
    if (e.iso[0]) Serial.printf("[DeviceLog] %s %s — %s\n", e.iso, e.event, e.detail);
    else Serial.printf("[DeviceLog] %s — %s\n", e.event, e.detail);
  }

  size_t count() const { return count_; }

  void appendJsonArray(String &out) const {
    out += '[';
    const size_t n = count_;
    for (size_t i = 0; i < n; i++) {
      size_t idx = (count_ < CAPACITY) ? i : ((head_ + i) % CAPACITY);
      const Entry &e = entries_[idx];
      if (i) out += ',';
      out += "{\"timestampMs\":";
      out += String(e.ms);
      out += ",\"iso\":\"";
      out += e.iso;
      out += "\",\"epoch\":";
      out += String(e.epoch);
      out += ",\"event\":\"";
      out += e.event;
      out += "\",\"detail\":\"";
      out += e.detail;
      out += "\"}";
    }
    out += ']';
  }

 private:
  struct Entry {
    uint32_t ms = 0;
    uint32_t epoch = 0;
    char iso[24] = {0};
    char event[28] = {0};
    char detail[72] = {0};
  };
  Entry entries_[CAPACITY];
  size_t head_ = 0;
  size_t count_ = 0;
};

#endif