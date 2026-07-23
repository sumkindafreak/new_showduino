#include "TimeService.h"
#include "TimeUtilities.h"
#include "../../BoardConfig.h"
#include <string.h>

TimeService gTimeService;

void TimeService::emit(const char *eventName, const char *detailJson) {
  if (eventFn_) eventFn_(eventName, detailJson ? detailJson : "{}");
}

void TimeService::emitRtcStatus() {
  String body;
  appendStatusJson(body);
  emit("rtc.status", body.c_str());
}

void TimeService::begin() {
#if !RTC_ENABLED
  ready_ = true;
  source_ = TimeSource::SoftClock;
  softBaseEpoch_ = 0;
  softBaseMs_ = millis();
  sync_.begin();
  tz_.begin(RTC_TIMEZONE, RTC_DST_ENABLED != 0);
  Serial.println("[TimeService] RTC disabled in BoardConfig — soft clock only");
  return;
#else
  sync_.begin();
  tz_.begin(RTC_TIMEZONE, RTC_DST_ENABLED != 0);

  int sqwPin = -1;
#if RTC_SQW_ENABLED
  sqwPin = RTC_SQW_PIN;
#endif
  bool ok = gRtcManager.begin(RTC_SDA_PIN, RTC_SCL_PIN, RTC_I2C_FREQUENCY, sqwPin);
  if (ok) {
    DateTime now;
    if (gRtcManager.readNow(now) && now.isValid()) {
      cachedEpoch_ = now.unixtime();
      cachedAtMs_ = millis();
      softBaseEpoch_ = cachedEpoch_;
      softBaseMs_ = cachedAtMs_;
      if (gRtcManager.health() == RtcHealth::Unsynced) {
        source_ = TimeSource::CompileSeed;
        sync_.noteSync(TimeSyncSource::CompileTime, cachedEpoch_);
        emit("time.unsynced", "{\"reason\":\"lost-power-compile-seed\"}");
      } else {
        source_ = TimeSource::Rtc;
        sync_.noteSync(TimeSyncSource::RtcBattery, cachedEpoch_);
        emit("time.sync", "{\"source\":\"rtc\"}");
      }
    }
  } else {
    source_ = TimeSource::SoftClock;
    softBaseEpoch_ = 0;
    softBaseMs_ = millis();
    emit("rtc.status", "{\"present\":false,\"healthy\":false}");
  }

  ready_ = true;
  lastEmittedHealth_ = gRtcManager.health();
  emitRtcStatus();
  Serial.printf("[TimeService] ready source=%s rtc=%s\n", sourceName(), gRtcManager.healthName());
#endif
}

void TimeService::refreshCache() {
#if RTC_ENABLED
  if (gRtcManager.present()) {
    gRtcManager.poll();
    DateTime now;
    if (gRtcManager.readNow(now) && now.isValid()) {
      cachedEpoch_ = now.unixtime();
      cachedAtMs_ = millis();
      softBaseEpoch_ = cachedEpoch_;
      softBaseMs_ = cachedAtMs_;
      if (gRtcManager.health() == RtcHealth::Unsynced) source_ = TimeSource::CompileSeed;
      else source_ = TimeSource::Rtc;
      return;
    }
  }
#endif
  /* Soft clock fallback from last known base. */
  source_ = TimeSource::SoftClock;
  uint32_t elapsed = (millis() - softBaseMs_) / 1000UL;
  cachedEpoch_ = softBaseEpoch_ + elapsed;
  cachedAtMs_ = millis();
}

void TimeService::loop(uint32_t nowMs) {
  if (!ready_) return;

#if RTC_ENABLED
  /* Alarm IRQ can arrive between 1 Hz ticks — check every loop. */
  if (gRtcManager.pollAlarmFired()) {
    String alarmJson = "{\"source\":\"rtc-sqw\",\"epoch\":";
    alarmJson += String(epochSeconds());
    alarmJson += ",\"purpose\":\"timed-show\"}";
    emit("time.alarm", alarmJson.c_str());
    Serial.println("[TimeService] timed-show alarm — Stage Runtime hook later");
  }
#endif

  if (nowMs - lastUpdateEmitMs_ < RTC_UPDATE_INTERVAL_MS) return;
  lastUpdateEmitMs_ = nowMs;

  RtcHealth prev = lastEmittedHealth_;
  refreshCache();

#if RTC_ENABLED
  if (gRtcManager.health() != prev) {
    lastEmittedHealth_ = gRtcManager.health();
    if (lastEmittedHealth_ == RtcHealth::Unsynced) {
      emit("time.unsynced", "{\"reason\":\"rtc-flag\"}");
    } else if (lastEmittedHealth_ == RtcHealth::Healthy) {
      emit("time.sync", "{\"source\":\"rtc\"}");
    }
    emitRtcStatus();
  }
#endif

  String body;
  appendTimeJson(body);
  emit("time.updated", body.c_str());
}

uint32_t TimeService::epochSeconds() const {
  if (!ready_) return 0;
  uint32_t elapsed = (millis() - cachedAtMs_) / 1000UL;
  return cachedEpoch_ + elapsed;
}

uint32_t TimeService::epochMillisApprox() const {
  return epochSeconds() * 1000UL + (millis() % 1000UL);
}

void TimeService::nowIso8601(char *out, size_t outLen) const {
  TimeUtilities::formatIso8601(epochSeconds(), out, outLen);
}

void TimeService::nowDate(char *out, size_t outLen) const {
  TimeUtilities::formatDate(epochSeconds(), out, outLen);
}

void TimeService::nowTime(char *out, size_t outLen) const {
  TimeUtilities::formatTime(epochSeconds(), out, outLen);
}

uint8_t TimeService::dayOfWeek() const {
  return TimeUtilities::dayOfWeekFromEpoch(epochSeconds());
}

const char *TimeService::dayOfWeekName() const {
  return TimeUtilities::dayOfWeekName(dayOfWeek());
}

const char *TimeService::timezone() const { return tz_.timezoneName(); }
bool TimeService::dstEnabled() const { return tz_.dstEnabled(); }
bool TimeService::dstActive() const { return tz_.dstActive(); }
float TimeService::rtcTemperatureC() const { return gRtcManager.temperatureC(); }
const char *TimeService::rtcStatus() const { return gRtcManager.healthName(); }
const char *TimeService::rtcHealth() const { return gRtcManager.healthName(); }
const char *TimeService::batteryStatus() const { return gRtcManager.batteryStatus(); }

const char *TimeService::sourceName() const {
  switch (source_) {
    case TimeSource::Rtc: return "rtc";
    case TimeSource::CompileSeed: return "compile-seed";
    case TimeSource::SoftClock: return "soft-clock";
    default: return "unknown";
  }
}

uint32_t TimeService::uptimeSeconds() const { return TimeUtilities::uptimeSeconds(); }

void TimeService::firmwareBuildTimestamp(char *out, size_t outLen) const {
  TimeUtilities::firmwareBuildTimestamp(out, outLen);
}

uint32_t TimeService::lastSyncEpoch() const { return sync_.lastSyncEpoch(); }
uint32_t TimeService::lastSyncMs() const { return sync_.lastSyncMs(); }
int32_t TimeService::driftMs() const { return sync_.driftMs(); }
bool TimeService::rtcPresent() const { return gRtcManager.present(); }
bool TimeService::rtcHealthy() const { return gRtcManager.health() == RtcHealth::Healthy; }
bool TimeService::rtcLostPower() const { return gRtcManager.lostPower(); }

bool TimeService::sqwEnabled() const { return gRtcManager.sqwEnabled(); }
bool TimeService::alarmArmed() const { return gRtcManager.alarm1Armed(); }
uint32_t TimeService::alarmEpoch() const { return gRtcManager.alarm1Epoch(); }

bool TimeService::scheduleAlarmAtEpoch(uint32_t epochSec) {
  if (!ready_ || !gRtcManager.present()) return false;
  if (epochSec <= epochSeconds()) return false;
  DateTime dt(epochSec);
  bool ok = gRtcManager.setAlarm1At(dt);
  if (ok) {
    String j = "{\"epoch\":";
    j += String(epochSec);
    j += ",\"mode\":\"once\"}";
    emit("time.alarm.armed", j.c_str());
  }
  return ok;
}

bool TimeService::scheduleDailyAlarm(uint8_t hour, uint8_t minute, uint8_t second) {
  if (!ready_ || !gRtcManager.present()) return false;
  if (hour > 23 || minute > 59 || second > 59) return false;
  bool ok = gRtcManager.setAlarm1Daily(hour, minute, second);
  if (ok) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"mode\":\"daily\",\"time\":\"%02u:%02u:%02u\"}", hour, minute, second);
    emit("time.alarm.armed", buf);
  }
  return ok;
}

void TimeService::cancelAlarm() {
  gRtcManager.clearAlarms();
  emit("time.alarm.cleared", "{}");
}

bool TimeService::formatDirectorWire(char *out, size_t outLen) const {
  if (!out || outLen < 40 || !ready_) return false;
  char tod[10], longDate[24];
  nowTime(tod, sizeof(tod));
  TimeUtilities::formatLongDate(epochSeconds(), longDate, sizeof(longDate));
  snprintf(out, outLen, "TIME:%lu|%s|%s|%s|%s",
           (unsigned long)epochSeconds(), tod, longDate, rtcHealth(), sourceName());
  return true;
}
void TimeService::appendTimeJson(String &out) const {
  char iso[24], date[12], time[10], build[32], up[16];
  nowIso8601(iso, sizeof(iso));
  nowDate(date, sizeof(date));
  nowTime(time, sizeof(time));
  firmwareBuildTimestamp(build, sizeof(build));
  TimeUtilities::formatUptime(up, sizeof(up));

  out += '{';
  out += "\"date\":\""; out += date; out += "\",";
  out += "\"time\":\""; out += time; out += "\",";
  out += "\"iso\":\""; out += iso; out += "\",";
  out += "\"epoch\":"; out += String(epochSeconds()); out += ',';
  out += "\"timezone\":\""; out += timezone(); out += "\",";
  out += "\"dst\":"; out += dstActive() ? "true" : "false"; out += ',';
  out += "\"dstEnabled\":"; out += dstEnabled() ? "true" : "false"; out += ',';
  out += "\"rtcStatus\":\""; out += rtcStatus(); out += "\",";
  out += "\"rtcTemperature\":"; out += String(rtcTemperatureC(), 2); out += ',';
  out += "\"source\":\""; out += sourceName(); out += "\",";
  out += "\"dayOfWeek\":\""; out += dayOfWeekName(); out += "\",";
  out += "\"uptimeSeconds\":"; out += String(uptimeSeconds()); out += ',';
  out += "\"uptime\":\""; out += up; out += "\",";
  out += "\"firmwareBuild\":\""; out += build; out += "\",";
  out += "\"battery\":\""; out += batteryStatus(); out += "\"";
  out += '}';
}

void TimeService::appendStatusJson(String &out) const {
  char iso[24];
  if (lastSyncEpoch()) TimeUtilities::formatIso8601(lastSyncEpoch(), iso, sizeof(iso));
  else strncpy(iso, "n/a", sizeof(iso));

  out += '{';
  out += "\"present\":"; out += rtcPresent() ? "true" : "false"; out += ',';
  out += "\"healthy\":"; out += rtcHealthy() ? "true" : "false"; out += ',';
  out += "\"lostPower\":"; out += rtcLostPower() ? "true" : "false"; out += ',';
  out += "\"battery\":\""; out += batteryStatus(); out += "\",";
  out += "\"health\":\""; out += rtcHealth(); out += "\",";
  out += "\"lastSynchronisation\":\""; out += iso; out += "\",";
  out += "\"lastSyncEpoch\":"; out += String(lastSyncEpoch()); out += ',';
  out += "\"lastSyncMs\":"; out += String(lastSyncMs()); out += ',';
  out += "\"driftMs\":"; out += String(driftMs()); out += ',';
  out += "\"source\":\""; out += sourceName(); out += "\",";
  out += "\"syncSource\":\""; out += sync_.sourceName(); out += "\",";
  out += "\"temperature\":"; out += String(rtcTemperatureC(), 2); out += ',';
  out += "\"sqwEnabled\":"; out += sqwEnabled() ? "true" : "false"; out += ',';
  out += "\"sqwPin\":"; out += String(gRtcManager.sqwPin()); out += ',';
  out += "\"alarmArmed\":"; out += alarmArmed() ? "true" : "false"; out += ',';
  out += "\"alarmEpoch\":"; out += String(alarmEpoch());
  out += '}';
}