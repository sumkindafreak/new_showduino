#pragma once
#include <stdint.h>

// Director UX only. This never owns or releases the Stage safety latch.
class DirectorEmergencyClearFlow {
public:
  enum class Phase { Idle, Request, Confirm, Result };
  static constexpr uint32_t RESPONSE_MS = 4000;
  static constexpr uint32_t CONFIRM_MS = 10000;
  void request(uint32_t now) { phase_ = Phase::Request; since_ = now; }
  bool acknowledge(uint32_t now) {
    if (phase_ != Phase::Request || elapsed(now, RESPONSE_MS)) return false;
    phase_ = Phase::Confirm; since_ = now; return true;
  }
  bool confirm(uint32_t now) {
    if (phase_ != Phase::Confirm || elapsed(now, CONFIRM_MS)) return false;
    phase_ = Phase::Result; since_ = now; return true;
  }
  bool tick(uint32_t now) {
    if (phase_ == Phase::Idle) return false;
    if (!elapsed(now, phase_ == Phase::Confirm ? CONFIRM_MS : RESPONSE_MS)) return false;
    cancel(); return true;
  }
  void cancel() { phase_ = Phase::Idle; }
  Phase phase() const { return phase_; }
private:
  bool elapsed(uint32_t now, uint32_t limit) const { return uint32_t(now - since_) >= limit; }
  Phase phase_ = Phase::Idle;
  uint32_t since_ = 0;
};
