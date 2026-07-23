#include "CommandQueue.h"
#include <string.h>

CommandQueue::CommandQueue() {
  mux_ = xSemaphoreCreateMutex();
  memset(normal_, 0, sizeof(normal_));
  memset(emergency_, 0, sizeof(emergency_));
}

CommandQueue::~CommandQueue() {
  if (mux_) vSemaphoreDelete(mux_);
}

bool CommandQueue::pushRing(ShowCommand *ring, size_t &head, size_t &tail, size_t &count, const ShowCommand &cmd) {
  if (count >= CAPACITY) return false;
  ring[tail] = cmd;
  ring[tail].status = CommandStatus::Queued;
  ring[tail].inUse = true;
  tail = (tail + 1) % CAPACITY;
  count++;
  (void)head;
  return true;
}

bool CommandQueue::popRing(ShowCommand *ring, size_t &head, size_t &tail, size_t &count, ShowCommand &out) {
  if (count == 0) return false;
  out = ring[head];
  ring[head].inUse = false;
  head = (head + 1) % CAPACITY;
  count--;
  (void)tail;
  return true;
}

bool CommandQueue::cancelRing(ShowCommand *ring, size_t &head, size_t &tail, size_t &count, const char *id, ShowCommand &removed) {
  if (!id || count == 0) return false;
  for (size_t n = 0; n < count; n++) {
    size_t idx = (head + n) % CAPACITY;
    if (strcmp(ring[idx].id, id) != 0) continue;
    removed = ring[idx];
    for (size_t m = n; m + 1 < count; m++) {
      size_t a = (head + m) % CAPACITY;
      size_t b = (head + m + 1) % CAPACITY;
      ring[a] = ring[b];
    }
    count--;
    tail = (head + count) % CAPACITY;
    return true;
  }
  return false;
}

bool CommandQueue::enqueue(const ShowCommand &cmd) {
  if (!mux_) return false;
  xSemaphoreTake(mux_, portMAX_DELAY);
  bool ok = false;
  if (cmd.priority == CommandPriority::Emergency) {
    ok = pushRing(emergency_, eHead_, eTail_, eCount_, cmd);
  } else if (cmd.priority == CommandPriority::High && nCount_ < CAPACITY) {
    /* Insert at front of normal queue (ahead of Normal). */
    if (nCount_ == 0) {
      ok = pushRing(normal_, nHead_, nTail_, nCount_, cmd);
    } else {
      nHead_ = (nHead_ + CAPACITY - 1) % CAPACITY;
      normal_[nHead_] = cmd;
      normal_[nHead_].status = CommandStatus::Queued;
      normal_[nHead_].inUse = true;
      nCount_++;
      ok = true;
    }
  } else {
    ok = pushRing(normal_, nHead_, nTail_, nCount_, cmd);
  }
  xSemaphoreGive(mux_);
  return ok;
}

bool CommandQueue::dequeue(ShowCommand &out) {
  if (!mux_) return false;
  xSemaphoreTake(mux_, portMAX_DELAY);
  bool ok = false;
  if (eCount_ > 0) ok = popRing(emergency_, eHead_, eTail_, eCount_, out);
  else ok = popRing(normal_, nHead_, nTail_, nCount_, out);
  xSemaphoreGive(mux_);
  return ok;
}

bool CommandQueue::cancel(const char *id, ShowCommand &removed) {
  if (!mux_) return false;
  xSemaphoreTake(mux_, portMAX_DELAY);
  bool ok = cancelRing(emergency_, eHead_, eTail_, eCount_, id, removed);
  if (!ok) ok = cancelRing(normal_, nHead_, nTail_, nCount_, id, removed);
  xSemaphoreGive(mux_);
  if (ok) {
    removed.status = CommandStatus::Cancelled;
    strncpy(removed.result, "cancelled before dispatch", sizeof(removed.result) - 1);
  }
  return ok;
}

bool CommandQueue::peekById(const char *id, ShowCommand &out) const {
  if (!mux_ || !id) return false;
  xSemaphoreTake(mux_, portMAX_DELAY);
  bool found = false;
  for (size_t n = 0; n < eCount_; n++) {
    size_t idx = (eHead_ + n) % CAPACITY;
    if (strcmp(emergency_[idx].id, id) == 0) { out = emergency_[idx]; found = true; break; }
  }
  if (!found) {
    for (size_t n = 0; n < nCount_; n++) {
      size_t idx = (nHead_ + n) % CAPACITY;
      if (strcmp(normal_[idx].id, id) == 0) { out = normal_[idx]; found = true; break; }
    }
  }
  xSemaphoreGive(mux_);
  return found;
}

size_t CommandQueue::size() const {
  if (!mux_) return 0;
  xSemaphoreTake(mux_, portMAX_DELAY);
  size_t s = nCount_ + eCount_;
  xSemaphoreGive(mux_);
  return s;
}

size_t CommandQueue::emergencyDepth() const {
  if (!mux_) return 0;
  xSemaphoreTake(mux_, portMAX_DELAY);
  size_t s = eCount_;
  xSemaphoreGive(mux_);
  return s;
}

size_t CommandQueue::normalDepth() const {
  if (!mux_) return 0;
  xSemaphoreTake(mux_, portMAX_DELAY);
  size_t s = nCount_;
  xSemaphoreGive(mux_);
  return s;
}

void CommandQueue::appendQueueJson(String &out) const {
  if (!mux_) { out += "[]"; return; }
  xSemaphoreTake(mux_, portMAX_DELAY);
  out += '[';
  bool first = true;
  for (size_t n = 0; n < eCount_; n++) {
    if (!first) out += ',';
    first = false;
    showCommandToJson(emergency_[(eHead_ + n) % CAPACITY], out);
  }
  for (size_t n = 0; n < nCount_; n++) {
    if (!first) out += ',';
    first = false;
    showCommandToJson(normal_[(nHead_ + n) % CAPACITY], out);
  }
  out += ']';
  xSemaphoreGive(mux_);
}