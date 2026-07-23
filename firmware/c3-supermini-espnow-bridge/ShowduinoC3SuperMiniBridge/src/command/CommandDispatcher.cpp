#include "CommandDispatcher.h"
#include "../routing/DeviceRouter.h"
#include "../time/TimeService.h"
#include <string.h>

void CommandDispatcher::begin(CommandQueue *queue, CommandHistory *history,
                              IDeviceRouter *router, ICapabilityManager *caps,
                              IStageRuntimeBridge *stage) {
  queue_ = queue;
  history_ = history;
  router_ = router;
  caps_ = caps;
  stage_ = stage;
  memset(running_, 0, sizeof(running_));
  runningCount_ = 0;
}

void CommandDispatcher::emit(const char *ev, const ShowCommand &cmd) {
  if (eventFn_) eventFn_(ev, cmd);
}

void CommandDispatcher::finish(ShowCommand &cmd, CommandStatus st, const char *result, uint32_t nowMs) {
  cmd.status = st;
  cmd.completedMs = nowMs;
  cmd.completedEpoch = gTimeService.ready() ? gTimeService.epochSeconds() : 0;
  if (cmd.startedMs > 0 && nowMs >= cmd.startedMs) cmd.executionTimeMs = nowMs - cmd.startedMs;
  else cmd.executionTimeMs = 0;
  if (result) strncpy(cmd.result, result, sizeof(cmd.result) - 1);
  if (history_) history_->record(cmd);
  if (st == CommandStatus::Completed) emit("command.completed", cmd);
  else if (st == CommandStatus::Failed) emit("command.failed", cmd);
  else if (st == CommandStatus::Cancelled) emit("command.cancelled", cmd);
}

void CommandDispatcher::loop(uint32_t nowMs) {
  for (size_t i = 0; i < runningCount_; ) {
    finish(running_[i], CommandStatus::Completed, running_[i].result[0] ? running_[i].result
                                                                        : "dispatched (no hardware sink)",
           nowMs);
    for (size_t j = i + 1; j < runningCount_; j++) running_[j - 1] = running_[j];
    runningCount_--;
  }

  if (!queue_ || runningCount_ >= 8) return;

  ShowCommand cmd;
  if (!queue_->dequeue(cmd)) return;

  cmd.status = CommandStatus::Started;
  cmd.startedMs = nowMs;
  cmd.startedEpoch = gTimeService.ready() ? gTimeService.epochSeconds() : 0;
  emit("command.started", cmd);

  if (caps_ && !caps_->supports(cmd)) {
    finish(cmd, CommandStatus::Failed, "capability denied", nowMs);
    return;
  }

  if (router_) {
    bool routed = router_->route(cmd);
    if (!routed) {
      finish(cmd, CommandStatus::Failed, gDeviceRouter.lastDecision().reason[0]
                                             ? gDeviceRouter.lastDecision().reason
                                             : "route failed",
             nowMs);
      return;
    }
    snprintf(cmd.result, sizeof(cmd.result), "routed:%s (%s)",
             gDeviceRouter.lastDecision().deviceId,
             gDeviceRouter.lastDecision().decision);
  }

  /* Stage 8 will connect Stage Runtime here. */
  if (stage_) (void)stage_->submit(cmd);

  if (runningCount_ < 8) {
    running_[runningCount_++] = cmd;
  } else {
    finish(cmd, CommandStatus::Completed, cmd.result[0] ? cmd.result : "dispatched", nowMs);
  }
}

void CommandDispatcher::appendRunningJson(String &out) const {
  out += '[';
  for (size_t i = 0; i < runningCount_; i++) {
    if (i) out += ',';
    showCommandToJson(running_[i], out);
  }
  out += ']';
}