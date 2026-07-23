#include "TimeSync.h"

void TimeSync::begin() {
  lastSource_ = TimeSyncSource::None;
  lastSyncEpoch_ = 0;
  lastSyncMs_ = 0;
  driftMs_ = 0;
}

const char *TimeSync::sourceName() const {
  switch (lastSource_) {
    case TimeSyncSource::CompileTime: return "compile-time";
    case TimeSyncSource::Manual: return "manual";
    case TimeSyncSource::Ntp: return "ntp";
    case TimeSyncSource::Gps: return "gps";
    case TimeSyncSource::EspNow: return "esp-now";
    case TimeSyncSource::RtcBattery: return "rtc";
    default: return "none";
  }
}

void TimeSync::noteSync(TimeSyncSource src, uint32_t epochSec, int32_t driftMs) {
  lastSource_ = src;
  lastSyncEpoch_ = epochSec;
  lastSyncMs_ = millis();
  driftMs_ = driftMs;
}