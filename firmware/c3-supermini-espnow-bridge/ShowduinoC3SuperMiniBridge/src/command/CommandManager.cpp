#include "CommandManager.h"
#include "../capability/CapabilityManager.h"
#include "../routing/DeviceRouter.h"
#include "../time/TimeService.h"
#include <string.h>

CommandManager gCommandManager;
CommandManager *CommandManager::sSelf = nullptr;

void CommandManager::emit(const char *ev, const ShowCommand *cmd, const char *extra) {
  if (eventFn_) eventFn_(ev, cmd, extra);
}

void CommandManager::onDispatchEvent(const char *eventName, const ShowCommand &cmd) {
  if (sSelf) sSelf->emit(eventName, &cmd, nullptr);
}

void CommandManager::begin() {
  sSelf = this;
  /* Stage 7: wire real CapabilityManager + DeviceRouter; Stage Runtime still null. */
  dispatcher_.begin(&queue_, &history_, &gDeviceRouter, &gCapabilityManager, &nullStage_);
  dispatcher_.setEventCallback(onDispatchEvent);
  ready_ = true;
  Serial.println("[CommandBus] ShowCommand framework ready (router+caps; no hardware sink)");
}

void CommandManager::loop(uint32_t nowMs) {
  if (!ready_) return;
  dispatcher_.loop(nowMs);
}

bool CommandManager::submitJson(const String &json, String &responseJson, int &httpStatus) {
  responseJson = "";
  if (!ready_) {
    httpStatus = 503;
    responseJson = "{\"error\":\"command bus not ready\"}";
    return false;
  }

  ShowCommand cmd;
  String parseErr;
  if (!showCommandFromJson(json, cmd, parseErr)) {
    httpStatus = 400;
    responseJson = "{\"error\":\"";
    responseJson += parseErr;
    responseJson += "\"}";
    emit("command.rejected", nullptr, responseJson.c_str());
    return false;
  }

  if (!cmd.id[0]) showCommandAssignId(cmd, seq_++);
  cmd.timestampMs = millis();
  cmd.createdEpoch = gTimeService.ready() ? gTimeService.epochSeconds() : 0;
  cmd.status = CommandStatus::Received;
  emit("command.received", &cmd, nullptr);

  CommandValidationResult vr = validator_.validate(cmd);
  if (!vr.ok) {
    cmd.status = CommandStatus::Rejected;
    strncpy(cmd.result, vr.error, sizeof(cmd.result) - 1);
    history_.record(cmd);
    httpStatus = 400;
    responseJson = "{\"error\":\"";
    responseJson += vr.error;
    responseJson += "\",\"command\":";
    showCommandToJson(cmd, responseJson);
    responseJson += '}';
    emit("command.rejected", &cmd, nullptr);
    return false;
  }

  cmd.status = CommandStatus::Validated;
  emit("command.validated", &cmd, nullptr);

  cmd.status = CommandStatus::Queued;
  cmd.queuedEpoch = gTimeService.ready() ? gTimeService.epochSeconds() : 0;
  if (!queue_.enqueue(cmd)) {
    cmd.status = CommandStatus::Failed;
    strncpy(cmd.result, "queue full", sizeof(cmd.result) - 1);
    history_.record(cmd);
    httpStatus = 503;
    responseJson = "{\"error\":\"queue full\"}";
    emit("command.rejected", &cmd, nullptr);
    return false;
  }

  emit("command.queued", &cmd, nullptr);
  emit("queue.updated", nullptr, nullptr);

  httpStatus = 202;
  responseJson = "{\"status\":\"queued\",\"command\":";
  showCommandToJson(cmd, responseJson);
  responseJson += '}';
  return true;
}

bool CommandManager::getById(const char *id, String &outJson) const {
  ShowCommand cmd;
  if (queue_.peekById(id, cmd)) {
    showCommandToJson(cmd, outJson);
    return true;
  }
  if (history_.getById(id, cmd)) {
    showCommandToJson(cmd, outJson);
    return true;
  }
  return false;
}

bool CommandManager::cancelById(const char *id, String &outJson, int &httpStatus) {
  ShowCommand removed;
  if (!queue_.cancel(id, removed)) {
    httpStatus = 404;
    outJson = "{\"error\":\"not queued or already dispatched\"}";
    return false;
  }
  history_.record(removed);
  emit("command.cancelled", &removed, nullptr);
  emit("queue.updated", nullptr, nullptr);
  httpStatus = 200;
  outJson = "{\"status\":\"cancelled\",\"command\":";
  showCommandToJson(removed, outJson);
  outJson += '}';
  return true;
}

void CommandManager::appendCommandsApiJson(String &out) const {
  out += '{';
  out += "\"queue\":";
  queue_.appendQueueJson(out);
  out += ",\"running\":";
  dispatcher_.appendRunningJson(out);
  out += ",\"history\":";
  history_.appendHistoryJson(out, 200);
  out += ",\"queueDepth\":";
  out += String((unsigned)queue_.size());
  out += ",\"emergencyDepth\":";
  out += String((unsigned)queue_.emergencyDepth());
  out += '}';
}