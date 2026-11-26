#pragma once
// Display-only health and change tracking; never grants permission for commands.
#include <cstdint>
namespace ShowduinoV11 {
enum class Health : uint8_t { Unknown, Emergency, LinkLost, Stale, Fault, Degraded, Ready };
struct HealthEvidence {
  bool emergency = false;
  bool emergencyKnown = false;
  bool commsOnline = false;
  bool p4Online = false;
  bool p4StateFresh = false;
  bool p4Ready = false;
  bool fault = false;
  bool memoryKnown = false;
  bool memoryOk = false;
  bool storageKnown = false;
  bool storageOk = false;
};
inline Health classifyHealth(const HealthEvidence &e) {
  if (e.emergency) return Health::Emergency;
  if (!e.commsOnline || !e.p4Online) return Health::LinkLost;
  if (!e.p4StateFresh) return Health::Stale;
  if (e.fault) return Health::Fault;
  if (!e.emergencyKnown || !e.memoryKnown || !e.storageKnown) return Health::Unknown;
  if (!e.memoryOk || !e.storageOk || !e.p4Ready) return Health::Degraded;
  return Health::Ready;
}
inline const char *healthLabel(Health h) {
  switch (h) {
    case Health::Emergency: return "EMERGENCY";
    case Health::LinkLost: return "LINK LOST";
    case Health::Stale: return "STALE STATE";
    case Health::Fault: return "FAULT";
    case Health::Degraded: return "NOT READY";
    case Health::Ready: return "READY";
    default: return "UNKNOWN";
  }
}
// Per-object cache: only redraw on change; invalidate whenever a page is rebuilt.
template<class T> class ChangeTracker {
 public:
  bool update(T next) {
    if (valid_ && next == value_) return false;
    value_ = next; valid_ = true; return true;
  }
  void invalidate() { valid_ = false; }
 private:
  T value_{};
  bool valid_ = false;
};
// Unsigned subtraction handles millis rollover; age is meaningful only if seen.
struct Freshness {
  bool seen = false;
  uint32_t lastMs = 0;
  void observe(uint32_t now) { seen = true; lastMs = now; }
  void invalidate() { seen = false; }
  bool fresh(uint32_t now, uint32_t maxAge) const {
    return seen && uint32_t(now - lastMs) <= maxAge;
  }
};
} // namespace ShowduinoV11
