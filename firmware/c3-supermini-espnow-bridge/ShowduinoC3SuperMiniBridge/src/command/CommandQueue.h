#ifndef SHOWDUINO_COMMAND_QUEUE_H
#define SHOWDUINO_COMMAND_QUEUE_H

#include "ShowCommand.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class CommandQueue {
 public:
  static const size_t CAPACITY = 48;

  CommandQueue();
  ~CommandQueue();

  bool enqueue(const ShowCommand &cmd);
  bool dequeue(ShowCommand &out);
  bool cancel(const char *id, ShowCommand &removed);
  bool peekById(const char *id, ShowCommand &out) const;
  size_t size() const;
  size_t emergencyDepth() const;
  size_t normalDepth() const;
  void appendQueueJson(String &out) const;

 private:
  ShowCommand normal_[CAPACITY];
  ShowCommand emergency_[CAPACITY];
  size_t nHead_ = 0, nTail_ = 0, nCount_ = 0;
  size_t eHead_ = 0, eTail_ = 0, eCount_ = 0;
  mutable SemaphoreHandle_t mux_ = nullptr;

  bool pushRing(ShowCommand *ring, size_t &head, size_t &tail, size_t &count, const ShowCommand &cmd);
  bool popRing(ShowCommand *ring, size_t &head, size_t &tail, size_t &count, ShowCommand &out);
  bool cancelRing(ShowCommand *ring, size_t &head, size_t &tail, size_t &count, const char *id, ShowCommand &removed);
};

#endif