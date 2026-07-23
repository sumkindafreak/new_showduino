#ifndef SHOWDUINO_COMMAND_DISPATCHER_H
#define SHOWDUINO_COMMAND_DISPATCHER_H

#include "ShowCommand.h"
#include "CommandQueue.h"
#include "CommandHistory.h"
#include "CommandInterfaces.h"

typedef void (*CommandEventFn)(const char *eventName, const ShowCommand &cmd);

class CommandDispatcher {
 public:
  void begin(CommandQueue *queue, CommandHistory *history,
             IDeviceRouter *router, ICapabilityManager *caps, IStageRuntimeBridge *stage);
  void setEventCallback(CommandEventFn fn) { eventFn_ = fn; }
  void loop(uint32_t nowMs);
  size_t runningCount() const { return runningCount_; }
  void appendRunningJson(String &out) const;

 private:
  CommandQueue *queue_ = nullptr;
  CommandHistory *history_ = nullptr;
  IDeviceRouter *router_ = nullptr;
  ICapabilityManager *caps_ = nullptr;
  IStageRuntimeBridge *stage_ = nullptr;
  CommandEventFn eventFn_ = nullptr;
  ShowCommand running_[8];
  size_t runningCount_ = 0;

  void emit(const char *ev, const ShowCommand &cmd);
  void finish(ShowCommand &cmd, CommandStatus st, const char *result, uint32_t nowMs);
};

#endif