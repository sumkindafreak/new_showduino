#pragma once

#include <string.h>
#include <stdint.h>
#include "Service.h"
#include "../OsTypes.h"
#include "../events/EventBus.h"

namespace Os2 {

/**
 * SessionService — operator continuity (quiet subsystem).
 *
 * Remembers: last production, last app, recent productions.
 * Boot: Shell → SessionService.restore() → Dashboard + context.
 *
 * Not a user-facing app. No file paths. Preferences only.
 */
class SessionService : public IService {
 public:
  static constexpr int kRecentMax = 8;

  const char *id() const override { return "session"; }

  void begin() override {
    lastProduction_[0] = '\0';
    lastApp_[0] = '\0';
    recentCount_ = 0;
    restored_ = false;
  }

  void setLastProduction(const char *id) {
    if (!id) id = "";
    strncpy(lastProduction_, id, sizeof(lastProduction_) - 1);
    lastProduction_[sizeof(lastProduction_) - 1] = '\0';
    if (id[0]) pushRecent(id);
  }

  void setLastApp(const char *appId) {
    if (!appId) appId = "";
    strncpy(lastApp_, appId, sizeof(lastApp_) - 1);
    lastApp_[sizeof(lastApp_) - 1] = '\0';
  }

  const char *lastProduction() const { return lastProduction_; }
  const char *lastApp() const { return lastApp_[0] ? lastApp_ : "dashboard"; }

  int recentCount() const { return recentCount_; }
  const char *recentAt(int i) const {
    return (i >= 0 && i < recentCount_) ? recent_[i] : nullptr;
  }

  /** Called once after services + apps are up. */
  void restore() {
    if (restored_) return;
    restored_ = true;
    events().publish(Event::SessionRestored);
  }

  bool wasRestored() const { return restored_; }

 private:
  void pushRecent(const char *id) {
    /* move existing to front or insert */
    int found = -1;
    for (int i = 0; i < recentCount_; ++i) {
      if (strcmp(recent_[i], id) == 0) { found = i; break; }
    }
    if (found >= 0) {
      for (int i = found; i > 0; --i) {
        strncpy(recent_[i], recent_[i - 1], sizeof(recent_[i]) - 1);
      }
    } else {
      if (recentCount_ < kRecentMax) ++recentCount_;
      for (int i = recentCount_ - 1; i > 0; --i) {
        strncpy(recent_[i], recent_[i - 1], sizeof(recent_[i]) - 1);
      }
    }
    strncpy(recent_[0], id, sizeof(recent_[0]) - 1);
    recent_[0][sizeof(recent_[0]) - 1] = '\0';
  }

  char lastProduction_[64]{};
  char lastApp_[24]{};
  char recent_[kRecentMax][64]{};
  int recentCount_ = 0;
  bool restored_ = false;
};

}  // namespace Os2