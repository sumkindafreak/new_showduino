#ifndef SHOWDUINO_CAPABILITY_MANAGER_H
#define SHOWDUINO_CAPABILITY_MANAGER_H

#include <Arduino.h>
#include "../command/CommandInterfaces.h"
#include "../command/ShowCommand.h"
#include "../DeviceRecord.h"
#include "CapabilityTypes.h"

typedef void (*CapabilityEventFn)(const char *eventName, const char *detailJson);

class CapabilityManager : public ICapabilityManager {
 public:
  void begin();
  bool ready() const { return ready_; }
  void setEventCallback(CapabilityEventFn fn) { eventFn_ = fn; }
  void setEventLog(class DeviceEventLog *log) { log_ = log; }

  /** Rebuild advertisement index from Device Registry. */
  void refreshFromRegistry();

  bool supports(const ShowCommand &cmd) override;

  bool deviceHasCapability(const DeviceRecord &d, const char *capName) const;
  bool deviceHasCapability(const DeviceRecord &d, CapabilityId id) const;

  /** Map command category/action â†’ required capability name (empty = none / broadcast). */
  bool requiredCapability(const ShowCommand &cmd, char *out, size_t outLen) const;

  void appendCapabilitiesCatalogJson(String &out) const;
  void appendDeviceCapabilitiesJson(String &out) const;

  void notifyCapabilityChanged(const DeviceRecord &d, const char *reason);

 private:
  CapabilityEventFn eventFn_ = nullptr;
  class DeviceEventLog *log_ = nullptr;
  bool ready_ = false;

  void emit(const char *eventName, const char *detailJson);
  void logEvent(const char *event, const char *detail);
};

extern CapabilityManager gCapabilityManager;

#endif