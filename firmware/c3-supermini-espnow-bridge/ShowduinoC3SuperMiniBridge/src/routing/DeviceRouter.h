#ifndef SHOWDUINO_DEVICE_ROUTER_H
#define SHOWDUINO_DEVICE_ROUTER_H

#include <Arduino.h>
#include "../command/CommandInterfaces.h"
#include "../command/ShowCommand.h"
#include "RouteDecision.h"

typedef void (*RouteEventFn)(const char *eventName, const char *detailJson);

class DeviceRouter : public IDeviceRouter {
 public:
  void begin();
  void setEventCallback(RouteEventFn fn) { eventFn_ = fn; }
  void setEventLog(class DeviceEventLog *log) { log_ = log; }

  /** Resolve without hardware execution. */
  bool resolve(const ShowCommand &cmd, RouteDecision &out) const;

  /** Resolve + emit WS/log events. Used by CommandDispatcher. */
  bool route(const ShowCommand &cmd) override;

  void appendRoutesJson(String &out) const;
  void appendRouteTestJson(const ShowCommand &cmd, String &out) const;

  const RouteDecision &lastDecision() const { return last_; }

 private:
  RouteEventFn eventFn_ = nullptr;
  class DeviceEventLog *log_ = nullptr;
  RouteDecision last_;
  bool ready_ = false;

  void emit(const char *eventName, const RouteDecision &d, const ShowCommand *cmd);
  void logEvent(const char *event, const char *detail);

  bool matchDestination(const char *dest, const class DeviceRecord &d) const;
  const class DeviceRecord *findPreferred(const char *capability) const;
  const class DeviceRecord *findBest(const char *capability, bool onlineOnly) const;
  const class DeviceRecord *findByDestination(const char *dest) const;
};

extern DeviceRouter gDeviceRouter;

#endif