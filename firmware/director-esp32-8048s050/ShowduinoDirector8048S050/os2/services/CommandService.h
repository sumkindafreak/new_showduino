#pragma once

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Service.h"
#include "AssetService.h"
#include "ShowService.h"
#include "../OsTypes.h"
#include "../events/EventBus.h"
#include "../Compatibility.h"

namespace Os2 {

/**
 * CommandService — Compatibility contract v1 (Api::CommandService).
 * Intent only. Services answer questions; commands express actions.
 */
enum class CommandType : uint8_t {
  None = 0,
  LoadProduction,
  StartShow,
  StopShow,
  PauseShow,
  ResumeShow,
  EmergencyStop,
  EmergencyClear,
  SelectProduction,
  OpenApp,
};

enum class CommandStatus : uint8_t {
  Queued = 0,
  Rejected,
  Executing,
  Succeeded,
  Failed,
};

struct Command {
  uint32_t seq;
  CommandType type;
  CommandStatus status;
  char arg[64];
  char detail[48]; /* reject/fail reason or note */
};

class CommandService : public IService {
 public:
  static constexpr uint16_t kApiVersion = Api::CommandService;
  static constexpr int kQueueMax = 8;
  static constexpr int kHistoryMax = 16;

  const char *id() const override { return "command"; }

  void begin() override {
    head_ = tail_ = 0;
    histCount_ = 0;
    nextSeq_ = 1;
  }

  /** Queue operator intent. Returns false if rejected or queue full. */
  bool execute(CommandType type, const char *arg = nullptr) {
    Command cmd{};
    cmd.seq = nextSeq_++;
    cmd.type = type;
    cmd.status = CommandStatus::Queued;
    cmd.arg[0] = '\0';
    cmd.detail[0] = '\0';
    if (arg && arg[0]) {
      strncpy(cmd.arg, arg, sizeof(cmd.arg) - 1);
      cmd.arg[sizeof(cmd.arg) - 1] = '\0';
    }

    const char *why = nullptr;
    if (!validate(cmd, &why)) {
      cmd.status = CommandStatus::Rejected;
      if (why) {
        strncpy(cmd.detail, why, sizeof(cmd.detail) - 1);
        cmd.detail[sizeof(cmd.detail) - 1] = '\0';
      }
      pushHistory(cmd);
      events().publish(Event::CommandRejected, (uint32_t)type);
      return false;
    }

    if (!enqueue(cmd)) {
      cmd.status = CommandStatus::Rejected;
      strncpy(cmd.detail, "queue full", sizeof(cmd.detail) - 1);
      pushHistory(cmd);
      events().publish(Event::CommandRejected, (uint32_t)type);
      return false;
    }

    pushHistory(cmd);
    events().publish(Event::CommandAccepted, (uint32_t)type, cmd.seq);
    return true;
  }

  bool loadProduction(const char *id) {
    return execute(CommandType::LoadProduction, id);
  }
  bool startShow() { return execute(CommandType::StartShow); }
  bool stopShow() { return execute(CommandType::StopShow); }
  bool pauseShow() { return execute(CommandType::PauseShow); }
  bool resumeShow() { return execute(CommandType::ResumeShow); }
  bool emergencyStop() { return execute(CommandType::EmergencyStop); }
  bool emergencyClear() { return execute(CommandType::EmergencyClear); }
  bool selectProduction(const char *id) {
    return execute(CommandType::SelectProduction, id);
  }
  bool openApp(const char *appId) {
    return execute(CommandType::OpenApp, appId);
  }

  /** Communication layer drains one queued command. */
  bool takeNext(Command &out) {
    if (head_ == tail_) return false;
    out = queue_[head_];
    head_ = (head_ + 1) % kQueueMax;
    out.status = CommandStatus::Executing;
    updateHistoryStatus(out.seq, CommandStatus::Executing, nullptr);
    events().publish(Event::CommandExecuting, (uint32_t)out.type, out.seq);
    return true;
  }

  void markSucceeded(uint32_t seq, const char *note = nullptr) {
    updateHistoryStatus(seq, CommandStatus::Succeeded, note);
    events().publish(Event::CommandSucceeded, seq);
  }

  void markFailed(uint32_t seq, const char *reason) {
    updateHistoryStatus(seq, CommandStatus::Failed, reason);
    events().publish(Event::CommandFailed, seq);
  }

  int historyCount() const { return histCount_; }
  const Command *historyAt(int i) const {
    /* 0 = most recent */
    if (i < 0 || i >= histCount_) return nullptr;
    int idx = histCount_ - 1 - i;
    return &history_[idx];
  }

  static const char *typeName(CommandType t) {
    switch (t) {
      case CommandType::LoadProduction:    return "LoadProduction";
      case CommandType::StartShow:         return "StartShow";
      case CommandType::StopShow:          return "StopShow";
      case CommandType::PauseShow:         return "PauseShow";
      case CommandType::ResumeShow:        return "ResumeShow";
      case CommandType::EmergencyStop:     return "EmergencyStop";
      case CommandType::EmergencyClear:    return "EmergencyClear";
      case CommandType::SelectProduction:  return "SelectProduction";
      case CommandType::OpenApp:           return "OpenApp";
      default:                             return "None";
    }
  }

 private:
  bool validate(const Command &cmd, const char **why) const {
    switch (cmd.type) {
      case CommandType::LoadProduction:
      case CommandType::SelectProduction:
        if (!cmd.arg[0]) { *why = "missing id"; return false; }
        /* Catalogue check when AssetService has entries; empty catalogue = allow (boot). */
        {
          AssetService *assets = services().get<AssetService>("asset");
          if (assets && assets->count() > 0 && !assets->find(cmd.arg)) {
            *why = "unknown production";
            return false;
          }
        }
        return true;
      case CommandType::StartShow:
      case CommandType::StopShow:
      case CommandType::PauseShow:
      case CommandType::ResumeShow:
      case CommandType::EmergencyStop:
      case CommandType::EmergencyClear:
      case CommandType::OpenApp:
        return true;
      default:
        *why = "unknown command";
        return false;
    }
  }

  bool enqueue(const Command &cmd) {
    int next = (tail_ + 1) % kQueueMax;
    if (next == head_) return false;
    queue_[tail_] = cmd;
    tail_ = next;
    return true;
  }

  void pushHistory(const Command &cmd) {
    if (histCount_ < kHistoryMax) {
      history_[histCount_++] = cmd;
    } else {
      for (int i = 1; i < kHistoryMax; ++i) history_[i - 1] = history_[i];
      history_[kHistoryMax - 1] = cmd;
    }
  }

  void updateHistoryStatus(uint32_t seq, CommandStatus st, const char *note) {
    for (int i = histCount_ - 1; i >= 0; --i) {
      if (history_[i].seq == seq) {
        history_[i].status = st;
        if (note && note[0]) {
          strncpy(history_[i].detail, note, sizeof(history_[i].detail) - 1);
          history_[i].detail[sizeof(history_[i].detail) - 1] = '\0';
        }
        return;
      }
    }
  }

  Command queue_[kQueueMax]{};
  int head_ = 0;
  int tail_ = 0;
  Command history_[kHistoryMax]{};
  int histCount_ = 0;
  uint32_t nextSeq_ = 1;
};

}  // namespace Os2