#ifndef SHOWDUINO_TIME_SYNC_H
#define SHOWDUINO_TIME_SYNC_H

#include <Arduino.h>
#include <stdint.h>

enum class TimeSyncSource : uint8_t {
  None = 0,
  CompileTime,
  Manual,
  Ntp,       /* future */
  Gps,       /* future */
  EspNow,    /* future */
  RtcBattery
};

/**
 * Stage 7.5 stub — future NTP / GPS / ESP-NOW / drift compensation plug-in point.
 * Does not replace TimeService; TimeService owns authority.
 */
class TimeSync {
 public:
  void begin();
  TimeSyncSource lastSource() const { return lastSource_; }
  uint32_t lastSyncEpoch() const { return lastSyncEpoch_; }
  uint32_t lastSyncMs() const { return lastSyncMs_; }
  int32_t driftMs() const { return driftMs_; }
  const char *sourceName() const;

  void noteSync(TimeSyncSource src, uint32_t epochSec, int32_t driftMs = 0);

  /* Future hooks (no-op in Stage 7.5). */
  bool requestNtp() { return false; }
  bool requestGps() { return false; }
  bool requestEspNowSync() { return false; }

 private:
  TimeSyncSource lastSource_ = TimeSyncSource::None;
  uint32_t lastSyncEpoch_ = 0;
  uint32_t lastSyncMs_ = 0;
  int32_t driftMs_ = 0;
};

#endif