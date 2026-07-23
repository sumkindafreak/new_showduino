#ifndef SHOWDUINO_TIMEZONE_MANAGER_H
#define SHOWDUINO_TIMEZONE_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

/**
 * Stage 7.5 — timezone / DST policy holder.
 * Full TZ database + DST transitions are future work.
 */
class TimezoneManager {
 public:
  void begin(const char *tzName, bool dstEnabled);
  const char *timezoneName() const { return tzName_; }
  bool dstEnabled() const { return dstEnabled_; }
  bool dstActive() const { return dstActive_; }
  int32_t utcOffsetSeconds() const { return utcOffsetSec_; }

  /** Convert UTC epoch → local epoch (currently UTC-only). */
  uint32_t toLocal(uint32_t utcEpoch) const;
  uint32_t toUtc(uint32_t localEpoch) const;

 private:
  char tzName_[24] = "UTC";
  bool dstEnabled_ = false;
  bool dstActive_ = false;
  int32_t utcOffsetSec_ = 0;
};

#endif