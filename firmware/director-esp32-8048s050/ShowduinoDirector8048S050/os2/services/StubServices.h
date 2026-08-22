#pragma once

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Service.h"
#include "ShowService.h"
#include "NetworkService.h"
#include "DeviceService.h"
#include "AssetService.h"
#include "SessionService.h"
#include "CommandService.h"
#include "../OsTypes.h"

namespace Os2 {

class LightingService : public IService {
 public:
  const char *id() const override { return "lighting"; }
  void begin() override { connected_ = false; fixtures_ = 0; }
  bool connected() const { return connected_; }
  int fixtures() const { return fixtures_; }
  StatusLevel status() const {
    return connected_ ? StatusLevel::Healthy : StatusLevel::Inactive;
  }
 private:
  bool connected_ = false;
  int fixtures_ = 0;
};

class AudioService : public IService {
 public:
  const char *id() const override { return "audio"; }
  void begin() override { connected_ = false; }
  bool connected() const { return connected_; }
  StatusLevel status() const {
    return connected_ ? StatusLevel::Healthy : StatusLevel::Inactive;
  }
 private:
  bool connected_ = false;
};

class TimeService : public IService {
 public:
  const char *id() const override { return "time"; }
  void begin() override {}
  void formatClock(char *buf, size_t n) const {
    if (!buf || n < 6) return;
    snprintf(buf, n, "%02u:%02u", (unsigned)hours_, (unsigned)minutes_);
  }
  void setClock(uint8_t h, uint8_t m) { hours_ = h; minutes_ = m; }
 private:
  uint8_t hours_ = 0;
  uint8_t minutes_ = 0;
};

class SettingsService : public IService {
 public:
  const char *id() const override { return "settings"; }
  void begin() override {}
};

inline ShowService &showService() {
  static ShowService s;
  return s;
}
inline NetworkService &networkService() {
  static NetworkService s;
  return s;
}
inline DeviceService &deviceService() {
  static DeviceService s;
  return s;
}
inline AssetService &assetService() {
  static AssetService s;
  return s;
}
inline SessionService &sessionService() {
  static SessionService s;
  return s;
}
inline CommandService &commandService() {
  static CommandService s;
  return s;
}

inline void registerDefaultServices() {
  static bool once = false;
  if (once) return;
  once = true;

  static LightingService lighting;
  static AudioService audio;
  static TimeService timeSvc;
  static SettingsService settings;

  services().add(&showService());
  services().add(&networkService());
  services().add(&deviceService());
  services().add(&assetService());
  services().add(&sessionService());
  services().add(&commandService());
  services().add(&lighting);
  services().add(&audio);
  services().add(&timeSvc);
  services().add(&settings);
}

}  // namespace Os2