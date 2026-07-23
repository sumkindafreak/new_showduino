#include "RtcManager.h"
#include <Wire.h>
#include <string.h>

RtcManager gRtcManager;
volatile bool RtcManager::sqwIrqFlag_ = false;

void IRAM_ATTR RtcManager::onSqwIsr() {
  sqwIrqFlag_ = true;
}

bool RtcManager::begin(int sdaPin, int sclPin, uint32_t i2cHz, int sqwPin) {
  present_ = false;
  lostPower_ = false;
  health_ = RtcHealth::Offline;
  temperatureC_ = 0.0f;
  sqwPin_ = sqwPin;
  sqwEnabled_ = false;
  alarm1Armed_ = false;
  alarm1Epoch_ = 0;
  sqwIrqFlag_ = false;
  strncpy(batteryStatus_, "unknown", sizeof(batteryStatus_) - 1);

  Wire.begin(sdaPin, sclPin);
  Wire.setClock(i2cHz);

  if (!rtc_.begin(&Wire)) {
    Serial.println("[RTC] DS3231 not detected — continuing without hardware clock");
    health_ = RtcHealth::Offline;
    strncpy(batteryStatus_, "n/a", sizeof(batteryStatus_) - 1);
    return false;
  }

  present_ = true;
  lostPower_ = rtc_.lostPower();
  stickyUnsynced_ = false;
  if (lostPower_) {
    DateTime compiled = DateTime(F(__DATE__), F(__TIME__));
    rtc_.adjust(compiled);
    health_ = RtcHealth::Unsynced;
    stickyUnsynced_ = true;
    strncpy(batteryStatus_, "replace?", sizeof(batteryStatus_) - 1);
    Serial.println("[RTC] lost power — set from firmware compile time (RTC_UNSYNCED)");
  } else {
    health_ = RtcHealth::Healthy;
    strncpy(batteryStatus_, "ok", sizeof(batteryStatus_) - 1);
    Serial.println("[RTC] DS3231 online (healthy)");
  }

  rtc_.disable32K();
  /* DS3231_OFF enables INTCN — SQW pin becomes alarm interrupt (active-low). */
  rtc_.writeSqwPinMode(DS3231_OFF);
  rtc_.clearAlarm(1);
  rtc_.clearAlarm(2);
  rtc_.disableAlarm(1);
  rtc_.disableAlarm(2);

  if (sqwPin_ >= 0) {
    configureSqwInterrupt();
    Serial.printf("[RTC] SQW/INT on GPIO%d (timed-show alarms)\n", sqwPin_);
  } else {
    Serial.println("[RTC] SQW pin disabled — alarms via I2C poll only");
  }

  temperatureC_ = rtc_.getTemperature();
  lastOkMs_ = millis();
  return true;
}

void RtcManager::configureSqwInterrupt() {
  if (sqwPin_ < 0) return;
  pinMode(sqwPin_, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sqwPin_), onSqwIsr, FALLING);
  sqwEnabled_ = true;
}

const char *RtcManager::healthName() const {
  switch (health_) {
    case RtcHealth::Healthy: return "healthy";
    case RtcHealth::Unsynced: return "unsynced";
    default: return "offline";
  }
}

void RtcManager::setBatteryFromFlags() {
  if (!present_) {
    strncpy(batteryStatus_, "n/a", sizeof(batteryStatus_) - 1);
    return;
  }
  if (lostPower_) strncpy(batteryStatus_, "replace?", sizeof(batteryStatus_) - 1);
  else strncpy(batteryStatus_, "ok", sizeof(batteryStatus_) - 1);
}

bool RtcManager::readNow(DateTime &out) {
  if (!present_) return false;
  out = rtc_.now();
  if (!out.isValid()) return false;
  lastOkMs_ = millis();
  return true;
}

bool RtcManager::adjust(const DateTime &dt, bool markSynced) {
  if (!present_) return false;
  rtc_.adjust(dt);
  lostPower_ = false;
  if (markSynced) {
    stickyUnsynced_ = false;
    health_ = RtcHealth::Healthy;
  } else {
    stickyUnsynced_ = true;
    health_ = RtcHealth::Unsynced;
  }
  setBatteryFromFlags();
  lastOkMs_ = millis();
  return true;
}

bool RtcManager::setAlarm1At(const DateTime &dt) {
  if (!present_ || !dt.isValid()) return false;
  rtc_.clearAlarm(1);
  if (!rtc_.setAlarm1(dt, DS3231_A1_Date)) {
    Serial.println("[RTC] setAlarm1 failed (INTCN?)");
    return false;
  }
  alarm1Armed_ = true;
  alarm1Epoch_ = dt.unixtime();
  sqwIrqFlag_ = false;
  Serial.printf("[RTC] Alarm1 armed @ epoch %lu\n", (unsigned long)alarm1Epoch_);
  return true;
}

bool RtcManager::setAlarm1Daily(uint8_t hour, uint8_t minute, uint8_t second) {
  if (!present_) return false;
  DateTime now = rtc_.now();
  if (!now.isValid()) return false;
  DateTime target(now.year(), now.month(), now.day(), hour, minute, second);
  rtc_.clearAlarm(1);
  if (!rtc_.setAlarm1(target, DS3231_A1_Hour)) {
    Serial.println("[RTC] setAlarm1 daily failed");
    return false;
  }
  alarm1Armed_ = true;
  alarm1Epoch_ = target.unixtime();
  sqwIrqFlag_ = false;
  Serial.printf("[RTC] Alarm1 daily %02u:%02u:%02u\n", hour, minute, second);
  return true;
}

void RtcManager::clearAlarms() {
  if (!present_) return;
  rtc_.clearAlarm(1);
  rtc_.clearAlarm(2);
  rtc_.disableAlarm(1);
  rtc_.disableAlarm(2);
  alarm1Armed_ = false;
  alarm1Epoch_ = 0;
  sqwIrqFlag_ = false;
  Serial.println("[RTC] alarms cleared");
}

bool RtcManager::pollAlarmFired() {
  if (!present_) return false;

  bool irq = sqwIrqFlag_;
  if (irq) sqwIrqFlag_ = false;

  bool a1 = rtc_.alarmFired(1);
  bool a2 = rtc_.alarmFired(2);
  if (!irq && !a1 && !a2) return false;

  if (a1) {
    rtc_.clearAlarm(1);
    alarm1Armed_ = false;
  }
  if (a2) rtc_.clearAlarm(2);

  Serial.println("[RTC] alarm fired (SQW/INT)");
  return true;
}

void RtcManager::poll() {
  if (!present_) {
    health_ = RtcHealth::Offline;
    return;
  }
  DateTime now = rtc_.now();
  if (!now.isValid()) {
    health_ = RtcHealth::Offline;
    present_ = false;
    Serial.println("[RTC] communication lost — OFFLINE");
    return;
  }
  lostPower_ = rtc_.lostPower();
  temperatureC_ = rtc_.getTemperature();
  lastOkMs_ = millis();
  if (lostPower_ || stickyUnsynced_) {
    health_ = RtcHealth::Unsynced;
  } else {
    health_ = RtcHealth::Healthy;
  }
  setBatteryFromFlags();
}