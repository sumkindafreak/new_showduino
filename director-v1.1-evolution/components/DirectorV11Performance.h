#ifndef SHOWDUINO_DIRECTOR_V11_PERFORMANCE_H
#define SHOWDUINO_DIRECTOR_V11_PERFORMANCE_H

// Bench-only, allocation-free timing statistics for the Director.
// This component deliberately does not take over the LVGL task or ESP-NOW.
// Call observe() with measured microseconds; read snapshot() at low frequency.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace ShowduinoV11 {

struct TimingSnapshot {
  bool valid = false;
  uint32_t window_samples = 0;
  uint64_t total_samples = 0;
  uint32_t minimum_us = 0;
  uint32_t maximum_us = 0;
  uint32_t mean_us = 0;
  uint32_t p95_us = 0;
};

template <size_t Capacity = 64>
class TimingWindow {
 public:
  static_assert(Capacity > 0, "TimingWindow requires a nonzero capacity");

  // O(1), no allocation, safe for typical single-threaded UI instrumentation.
  // Calls from another task must be synchronised externally.
  void observe(uint32_t elapsedUs) {
    if (count_ == Capacity) {
      sum_ -= measurements_[next_];
    } else {
      ++count_;
    }
    measurements_[next_] = elapsedUs;
    sum_ += elapsedUs;
    next_ = (next_ + 1) % Capacity;
    ++total_;
  }

  void clear() {
    measurements_.fill(0);
    next_ = 0;
    count_ = 0;
    sum_ = 0;
    total_ = 0;
  }

  // Sorting at snapshot time is intentionally NOT for each frame.
  TimingSnapshot snapshot() const {
    TimingSnapshot value;
    value.total_samples = total_;
    value.window_samples = static_cast<uint32_t>(count_);
    if (count_ == 0) return value;

    std::array<uint32_t, Capacity> sorted = measurements_;
    std::sort(sorted.begin(), sorted.begin() + count_);
    value.valid = true;
    value.minimum_us = sorted[0];
    value.maximum_us = sorted[count_ - 1];
    value.mean_us = static_cast<uint32_t>(sum_ / count_);
    const size_t rank = (95 * count_ + 99) / 100;  // nearest-rank p95
    value.p95_us = sorted[rank - 1];
    return value;
  }

 private:
  std::array<uint32_t, Capacity> measurements_{};
  size_t next_ = 0;
  size_t count_ = 0;
  uint64_t sum_ = 0;
  uint64_t total_ = 0;
};

struct PerformanceSnapshot {
  TimingSnapshot ui_loop;
  TimingSnapshot lvgl_handler;
};

class PerformanceMonitor {
 public:
  void observeUiLoop(uint32_t elapsedUs) { ui_loop_.observe(elapsedUs); }
  void observeLvglHandler(uint32_t elapsedUs) { lvgl_handler_.observe(elapsedUs); }

  PerformanceSnapshot snapshot() const {
    return {ui_loop_.snapshot(), lvgl_handler_.snapshot()};
  }

  void clear() {
    ui_loop_.clear();
    lvgl_handler_.clear();
  }

 private:
  TimingWindow<64> ui_loop_;
  TimingWindow<64> lvgl_handler_;
};

}  // namespace ShowduinoV11

#endif  // SHOWDUINO_DIRECTOR_V11_PERFORMANCE_H
