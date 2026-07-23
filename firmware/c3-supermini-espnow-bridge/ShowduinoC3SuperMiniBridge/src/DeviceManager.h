#ifndef SHOWDUINO_DEVICE_MANAGER_H
#define SHOWDUINO_DEVICE_MANAGER_H

#include <Arduino.h>
#include "DeviceRecord.h"
#include "HeartbeatManager.h"
#include "NetworkStatistics.h"
#include "DeviceEventLog.h"

typedef void (*DeviceChangeFn)(const char *eventName, const DeviceRecord &device);

class DeviceManager {
 public:
  static const size_t MAX_DEVICES = 16;

  void begin(HeartbeatManager *hb, DeviceEventLog *log);
  bool ready() const { return ready_; }
  void setChangeCallback(DeviceChangeFn fn) { changeFn_ = fn; }
  void loop(uint32_t nowMs);

  void registerLocal(const char *boardType, const char *friendlyName, const uint8_t mac[6],
                     const char *fw, const char *proto, const char *caps, const char *ip);
  void noteEspNowSighting(const char *boardType, const char *friendlyName, const uint8_t mac[6],
                          int8_t rssi, const char *caps);
  void noteUartSighting(const char *boardType, const char *friendlyName, const char *idHint,
                        const char *caps);
  bool renameDevice(const char *id, const char *friendlyName);
  bool setDeviceCapabilities(const char *id, const char *caps);
  bool setDevicePreferred(const char *id, bool preferred, uint8_t priority);

  size_t count() const;
  const DeviceRecord *getById(const char *id) const;
  const DeviceRecord *getByIndex(size_t index) const;
  void computeNetworkStats(NetworkStatistics &out, uint32_t nowMs) const;

  void appendDevicesJson(String &out) const;
  bool appendDeviceJsonById(const char *id, String &out) const;
  void appendNetworkJson(String &out, uint32_t nowMs) const;
  static void appendOneDeviceJson(const DeviceRecord &d, String &out);

  HeartbeatManager *heartbeat() { return hb_; }
  DeviceEventLog *eventLog() { return log_; }
  void noteHeartbeatPulse();

 private:
  DeviceRecord *findByMac(const uint8_t mac[6]);
  DeviceRecord *findById(const char *id);
  DeviceRecord *allocSlot();
  void touch(DeviceRecord &d, uint32_t nowMs, int8_t rssi, bool hasRssi);
  void emit(const char *eventName, const DeviceRecord &d);
  void applyBoardDefaults(DeviceRecord &d);
  void syncAvailability(DeviceRecord &d);

  DeviceRecord devices_[MAX_DEVICES];
  HeartbeatManager *hb_ = nullptr;
  DeviceEventLog *log_ = nullptr;
  DeviceChangeFn changeFn_ = nullptr;
  uint32_t lastEvalMs_ = 0;
  uint32_t heartbeatPulses_ = 0;
  uint32_t lastPulseWindowMs_ = 0;
  uint32_t pulsesInWindow_ = 0;
  uint32_t ratePerMin_ = 0;
  bool ready_ = false;
};

extern DeviceManager gDeviceManager;

#endif