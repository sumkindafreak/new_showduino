#ifndef SHOWDUINO_COMMAND_MANAGER_H
#define SHOWDUINO_COMMAND_MANAGER_H

#include "ShowCommand.h"
#include "CommandValidator.h"
#include "CommandQueue.h"
#include "CommandDispatcher.h"
#include "CommandHistory.h"
#include "CommandInterfaces.h"

typedef void (*CommandBusEventFn)(const char *eventName, const ShowCommand *cmd, const char *extraJson);

class CommandManager {
 public:
  void begin();
  void loop(uint32_t nowMs);
  void setEventCallback(CommandBusEventFn fn) { eventFn_ = fn; }

  bool submitJson(const String &json, String &responseJson, int &httpStatus);
  bool getById(const char *id, String &outJson) const;
  bool cancelById(const char *id, String &outJson, int &httpStatus);
  void appendCommandsApiJson(String &out) const;

  CommandQueue &queue() { return queue_; }
  CommandHistory &history() { return history_; }

 private:
  CommandValidator validator_;
  CommandQueue queue_;
  CommandHistory history_;
  CommandDispatcher dispatcher_;
  NullStageRuntimeBridge nullStage_;
  CommandBusEventFn eventFn_ = nullptr;
  uint32_t seq_ = 1;
  bool ready_ = false;

  void emit(const char *ev, const ShowCommand *cmd, const char *extra = nullptr);
  static void onDispatchEvent(const char *eventName, const ShowCommand &cmd);
  static CommandManager *sSelf;
};

extern CommandManager gCommandManager;

#endif