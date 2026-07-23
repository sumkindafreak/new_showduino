#ifndef SHOWDUINO_RTC_MANAGER_H
#define SHOWDUINO_RTC_MANAGER_H

#include <Arduino.h>
#include <stdint.h>
#include <RTClib.h>

enum class RtcHealth : uint8_t {
  Offline = 0,
  Unsynced,
  Healthy
};

/**
 * Sole owner of the DS3231 RTC object. No other subsystem may create an RTC.
 * SQW/INT pin drives hardware alarms for future timed shows.
 */
class RtcManager {
 public:
  bool begin(int sdaPin, int sclPin, uint32_t i2cHz, int sqwPin = -1);
  bool present() const { return present_; }
  RtcHealth health() const { return health_; }
  bool lostPower() const { return lostPower_; }
  const char *healthName() const;
  const char *batteryStatus() const { return batteryStatus_; }
  bool sqwEnabled() const { return sqwEnabled_; }
  int sqwPin() const { return sqwPin_; }

  bool readNow(DateTime &out);
  bool adjust(const DateTime &dt, bool markSynced = true);
  float temperatureC() const { return temperatureC_; }
  uint32_t lastOkMs() const { return lastOkMs_; }

  /** Arm Alarm1 at absolute UTC DateTime (match date+H+M+S). */
  bool setAlarm1At(const DateTime &dt);
  /** Arm Alarm1 for daily HH:MM:SS (match hour+minute+second every day). */
  bool setAlarm1Daily(uint8_t hour, uint8_t minute, uint8_t second = 0);
  void clearAlarms();
  bool alarm1Armed() const { return alarm1Armed_; }
  uint32_t alarm1Epoch() const { return alarm1Epoch_; }

  /**
   * Service SQW edge + I2C alarm flags.
   * Returns true once when an alarm fires (consumes the event).
   */
  bool pollAlarmFired();

  /** Refresh cached status / temperature. */
  void poll();

  RTC_DS3231 &rtc() { return rtc_; }

 private:
  RTC_DS3231 rtc_;
  bool present_ = false;
  bool lostPower_ = false;
  bool stickyUnsynced_ = false;
  bool sqwEnabled_ = false;
  bool alarm1Armed_ = false;
  int sqwPin_ = -1;
  RtcHealth health_ = RtcHealth::Offline;
  float temperatureC_ = 0.0f;
  uint32_t lastOkMs_ = 0;
  uint32_t alarm1Epoch_ = 0;
  char batteryStatus_[16] = "unknown";

  static volatile bool sqwIrqFlag_;
  static void IRAM_ATTR onSqwIsr();

  void setBatteryFromFlags();
  void configureSqwInterrupt();
};

extern RtcManager gRtcManager;

#endif