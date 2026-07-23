#ifndef SHOWDUINO_TIME_SERVICE_H
#define SHOWDUINO_TIME_SERVICE_H

#include <Arduino.h>
#include <stdint.h>
#include "TimezoneManager.h"
#include "TimeSync.h"
#include "RtcManager.h"

enum class TimeSource : uint8_t {
  Unknown = 0,
  Rtc,
  SoftClock,
  CompileSeed
};

typedef void (*TimeEventFn)(const char *eventName, const char *detailJson);

/**
 * Authoritative time provider for Showduino.
 * All subsystems must obtain time through TimeService — never via RTC directly.
 */
class TimeService {
 public:
  void begin();
  void loop(uint32_t nowMs);
  void setEventCallback(TimeEventFn fn) { eventFn_ = fn; }

  bool ready() const { return ready_; }
  uint32_t epochSeconds() const;
  uint32_t epochMillisApprox() const;
  void nowIso8601(char *out, size_t outLen) const;
  void nowDate(char *out, size_t outLen) const;
  void nowTime(char *out, size_t outLen) const;
  uint8_t dayOfWeek() const;
  const char *dayOfWeekName() const;
  const char *timezone() const;
  bool dstEnabled() const;
  bool dstActive() const;
  float rtcTemperatureC() const;
  const char *rtcStatus() const;
  const char *rtcHealth() const;
  const char *batteryStatus() const;
  const char *sourceName() const;
  TimeSource source() const { return source_; }
  uint32_t uptimeSeconds() const;
  void firmwareBuildTimestamp(char *out, size_t outLen) const;
  uint32_t lastSyncEpoch() const;
  uint32_t lastSyncMs() const;
  int32_t driftMs() const;
  bool rtcPresent() const;
  bool rtcHealthy() const;
  bool rtcLostPower() const;
  bool sqwEnabled() const;
  bool alarmArmed() const;
  uint32_t alarmEpoch() const;

  /** Arm DS3231 Alarm1 for a future UTC epoch (timed shows). */
  bool scheduleAlarmAtEpoch(uint32_t epochSec);
  /** Arm daily HH:MM:SS (UTC) repeating match. */
  bool scheduleDailyAlarm(uint8_t hour, uint8_t minute, uint8_t second = 0);
  void cancelAlarm();

  /** Desk wire (fits command[96]): TIME:epoch|HH:MM:SS|Wed 22 Jul 2026|health|source */
  bool formatDirectorWire(char *out, size_t outLen) const;
  void appendTimeJson(String &out) const;
  void appendStatusJson(String &out) const;

  TimezoneManager &timezoneManager() { return tz_; }
  TimeSync &sync() { return sync_; }

 private:
  TimezoneManager tz_;
  TimeSync sync_;
  TimeEventFn eventFn_ = nullptr;
  bool ready_ = false;
  TimeSource source_ = TimeSource::Unknown;
  uint32_t cachedEpoch_ = 0;
  uint32_t cachedAtMs_ = 0;
  uint32_t softBaseEpoch_ = 0;
  uint32_t softBaseMs_ = 0;
  uint32_t lastUpdateEmitMs_ = 0;
  RtcHealth lastEmittedHealth_ = RtcHealth::Offline;

  void refreshCache();
  void emit(const char *eventName, const char *detailJson);
  void emitRtcStatus();
};

extern TimeService gTimeService;

#endif