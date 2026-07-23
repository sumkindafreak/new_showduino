#ifndef SHOWDUINO_COMMAND_HISTORY_H
#define SHOWDUINO_COMMAND_HISTORY_H

#include "ShowCommand.h"

/** Compact ring — 1000 entries for Stage 6 history API. */
struct CommandHistoryEntry {
  char id[20];
  uint32_t timestampMs;
  char source[20];
  char destination[20];
  CommandCategory category;
  char action[28];
  CommandPriority priority;
  CommandStatus status;
  uint32_t executionTimeMs;
};

class CommandHistory {
 public:
  static const size_t CAPACITY = 1000;

  void record(const ShowCommand &cmd);
  bool getById(const char *id, ShowCommand &out) const;
  size_t count() const { return count_; }
  void appendHistoryJson(String &out, size_t limit = 200) const;

 private:
  CommandHistoryEntry entries_[CAPACITY];
  size_t head_ = 0;
  size_t count_ = 0;
};

#endif