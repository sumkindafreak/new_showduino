#pragma once
// Read-only measurements, supplied by the existing Director UI task.
// No ESP32 calls or storage access here; thresholds must be measured on bench.
#include <cstdint>
namespace ShowduinoV11 {
struct ResourceSample {
  bool known = false;
  uint32_t freeInternal = 0, largestInternal = 0, freePsram = 0;
};
struct ResourceBudget {
  uint32_t minimumInternal = 0, minimumLargest = 0, minimumPsram = 0;
  bool configured = false;
};
inline bool resourceBudgetOk(const ResourceSample &s, const ResourceBudget &b) {
  return b.configured && s.known && s.largestInternal <= s.freeInternal &&
         s.freeInternal >= b.minimumInternal && s.largestInternal >= b.minimumLargest &&
         s.freePsram >= b.minimumPsram;
}
struct ResourceSummary {
  bool valid = false;
  uint32_t samples = 0, minimumInternal = 0, minimumLargest = 0, minimumPsram = 0;
  uint32_t maximumLoopUs = 0, maximumTransitionUs = 0;
};
class ResourceMonitor {
 public:
  void observeMemory(const ResourceSample &s) {
    if (!s.known || s.largestInternal > s.freeInternal) return;
    if (!summary_.valid || s.freeInternal < summary_.minimumInternal)
      summary_.minimumInternal = s.freeInternal;
    if (!summary_.valid || s.largestInternal < summary_.minimumLargest)
      summary_.minimumLargest = s.largestInternal;
    if (!summary_.valid || s.freePsram < summary_.minimumPsram)
      summary_.minimumPsram = s.freePsram;
    summary_.valid = true;
    if (summary_.samples != UINT32_MAX) ++summary_.samples;
  }
  void observeLoop(uint32_t us) {
    if (us > summary_.maximumLoopUs) summary_.maximumLoopUs = us;
  }
  void observeTransition(uint32_t us) {
    if (us > summary_.maximumTransitionUs) summary_.maximumTransitionUs = us;
  }
  ResourceSummary snapshot() const { return summary_; }
  void clear() { summary_ = {}; }
 private:
  ResourceSummary summary_{};
};
} // namespace ShowduinoV11
