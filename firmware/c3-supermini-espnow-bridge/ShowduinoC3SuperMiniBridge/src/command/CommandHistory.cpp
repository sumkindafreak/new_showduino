#include "CommandHistory.h"
#include <string.h>

void CommandHistory::record(const ShowCommand &cmd) {
  CommandHistoryEntry &e = entries_[head_];
  memset(&e, 0, sizeof(e));
  strncpy(e.id, cmd.id, sizeof(e.id) - 1);
  e.timestampMs = cmd.timestampMs;
  strncpy(e.source, cmd.source, sizeof(e.source) - 1);
  strncpy(e.destination, cmd.destination, sizeof(e.destination) - 1);
  e.category = cmd.category;
  strncpy(e.action, cmd.action, sizeof(e.action) - 1);
  e.priority = cmd.priority;
  e.status = cmd.status;
  e.executionTimeMs = cmd.executionTimeMs;
  head_ = (head_ + 1) % CAPACITY;
  if (count_ < CAPACITY) count_++;
}

bool CommandHistory::getById(const char *id, ShowCommand &out) const {
  if (!id) return false;
  for (size_t i = 0; i < count_; i++) {
    size_t idx = (count_ < CAPACITY) ? (count_ - 1 - i) : ((head_ + CAPACITY - 1 - i) % CAPACITY);
    if (count_ == CAPACITY) idx = (head_ + CAPACITY - 1 - i) % CAPACITY;
    else idx = count_ - 1 - i;
    const CommandHistoryEntry &e = entries_[idx];
    if (strcmp(e.id, id) != 0) continue;
    memset(&out, 0, sizeof(out));
    out.inUse = true;
    strncpy(out.id, e.id, sizeof(out.id) - 1);
    out.timestampMs = e.timestampMs;
    strncpy(out.source, e.source, sizeof(out.source) - 1);
    strncpy(out.destination, e.destination, sizeof(out.destination) - 1);
    out.category = e.category;
    strncpy(out.action, e.action, sizeof(out.action) - 1);
    out.priority = e.priority;
    out.status = e.status;
    out.executionTimeMs = e.executionTimeMs;
    return true;
  }
  return false;
}

void CommandHistory::appendHistoryJson(String &out, size_t limit) const {
  out += '[';
  size_t n = count_;
  if (limit > 0 && n > limit) n = limit;
  bool first = true;
  for (size_t i = 0; i < n; i++) {
    size_t idx;
    if (count_ < CAPACITY) idx = count_ - 1 - i;
    else idx = (head_ + CAPACITY - 1 - i) % CAPACITY;
    const CommandHistoryEntry &e = entries_[idx];
    if (!first) out += ',';
    first = false;
    out += '{';
    out += "\"id\":\""; out += e.id; out += "\",";
    out += "\"timestampMs\":"; out += String(e.timestampMs); out += ',';
    out += "\"source\":\""; out += e.source; out += "\",";
    out += "\"destination\":\""; out += e.destination; out += "\",";
    out += "\"category\":\""; out += commandCategoryName(e.category); out += "\",";
    out += "\"action\":\""; out += e.action; out += "\",";
    out += "\"priority\":\""; out += commandPriorityName(e.priority); out += "\",";
    out += "\"status\":\""; out += commandStatusName(e.status); out += "\",";
    out += "\"executionTimeMs\":"; out += String(e.executionTimeMs);
    out += '}';
  }
  out += ']';
}