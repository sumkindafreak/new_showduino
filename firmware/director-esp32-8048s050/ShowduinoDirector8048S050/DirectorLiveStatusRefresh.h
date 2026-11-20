#pragma once
#include <stdint.h>

class DirectorLiveStatusRefresh {
public:
  enum class State { Idle, Waiting, Received, SendFailed, TimedOut };
  static constexpr uint32_t TIMEOUT_MS = 3000;
  static constexpr uint32_t FEEDBACK_MS = 2500;

  bool request(uint32_t now) {
    if (state_ == State::Waiting) return false;
    set(State::Waiting, now);
    return true;
  }

  void received(uint32_t now) {
    if (state_ != State::Waiting) return;
    set(uint32_t(now - since_) >= TIMEOUT_MS ? State::TimedOut : State::Received, now);
  }

  void sendFailed(uint32_t now) { set(State::SendFailed, now); }

  bool tick(uint32_t now) {
    if (state_ == State::Idle) return false;
    const uint32_t limit = state_ == State::Waiting ? TIMEOUT_MS : FEEDBACK_MS;
    if (uint32_t(now - since_) < limit) return false;
    set(state_ == State::Waiting ? State::TimedOut : State::Idle, now);
    return true;
  }

  State state() const { return state_; }

private:
  void set(State state, uint32_t now) {
    state_ = state;
    since_ = now;
  }
  State state_ = State::Idle;
  uint32_t since_ = 0;
};
