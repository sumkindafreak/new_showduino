#pragma once

#include <stdint.h>
#include "../OsTypes.h"
#include "../Compatibility.h"

namespace Os2 {

/**
 * Event Bus — Compatibility contract v1 (Api::EventBus).
 * Services publish. Apps subscribe. No polling.
 */

enum class Event : uint16_t {
  ShowLoaded = 1,
  ShowStarted,
  ShowStopped,
  ShowPaused,
  ShowFinished,
  CueChanged,
  ShowProgress,
  EmergencyChanged,

  LinkChanged,
  NodeJoined,
  NodeLost,
  NetworkHealth,

  DeviceChanged,

  CatalogueChanged,
  ProductionSelected,
  SessionRestored,

  CommandAccepted,
  CommandRejected,
  CommandExecuting,
  CommandSucceeded,
  CommandFailed,

  ThemeChanged,
  TimeUpdated,
  SettingsChanged,

  SystemStatus,
};

struct EventPayload {
  Event type;
  uint32_t a;
  uint32_t b;
};

using EventHandler = void (*)(const EventPayload &ev, void *user);

class EventBus {
 public:
  static constexpr uint16_t kApiVersion = Api::EventBus;
  static constexpr int kMaxSubs = 32;

  static EventBus &instance() {
    static EventBus bus;
    return bus;
  }

  bool subscribe(Event type, EventHandler handler, void *user) {
    return add(type, handler, user, false);
  }

  bool subscribeAll(EventHandler handler, void *user) {
    return add(Event::SystemStatus, handler, user, true);
  }

  void unsubscribeAll(void *user) {
    for (int i = 0; i < count_; ++i) {
      if (subs_[i].user == user) {
        subs_[i] = subs_[--count_];
        --i;
      }
    }
  }

  void publish(Event type, uint32_t a = 0, uint32_t b = 0) {
    EventPayload ev{type, a, b};
    const int n = count_;
    for (int i = 0; i < n; ++i) {
      if (!subs_[i].handler) continue;
      if (subs_[i].all || subs_[i].type == type) {
        subs_[i].handler(ev, subs_[i].user);
      }
    }
  }

  static const char *name(Event e) {
    switch (e) {
      case Event::ShowLoaded:         return "ShowLoaded";
      case Event::ShowStarted:        return "ShowStarted";
      case Event::ShowStopped:        return "ShowStopped";
      case Event::ShowPaused:         return "ShowPaused";
      case Event::ShowFinished:       return "ShowFinished";
      case Event::CueChanged:         return "CueChanged";
      case Event::ShowProgress:       return "ShowProgress";
      case Event::EmergencyChanged:   return "EmergencyChanged";
      case Event::LinkChanged:        return "LinkChanged";
      case Event::NodeJoined:         return "NodeJoined";
      case Event::NodeLost:           return "NodeLost";
      case Event::NetworkHealth:      return "NetworkHealth";
      case Event::DeviceChanged:      return "DeviceChanged";
      case Event::CatalogueChanged:   return "CatalogueChanged";
      case Event::ProductionSelected: return "ProductionSelected";
      case Event::SessionRestored:    return "SessionRestored";
      case Event::CommandAccepted:    return "CommandAccepted";
      case Event::CommandRejected:    return "CommandRejected";
      case Event::CommandExecuting:   return "CommandExecuting";
      case Event::CommandSucceeded:   return "CommandSucceeded";
      case Event::CommandFailed:      return "CommandFailed";
      case Event::ThemeChanged:       return "ThemeChanged";
      case Event::TimeUpdated:        return "TimeUpdated";
      case Event::SettingsChanged:    return "SettingsChanged";
      case Event::SystemStatus:       return "SystemStatus";
      default:                        return "Unknown";
    }
  }

 private:
  struct Sub {
    Event type;
    EventHandler handler;
    void *user;
    bool all;
  };

  EventBus() = default;

  bool add(Event type, EventHandler handler, void *user, bool all) {
    if (!handler || count_ >= kMaxSubs) return false;
    for (int i = 0; i < count_; ++i) {
      if (subs_[i].handler == handler && subs_[i].user == user &&
          subs_[i].all == all && (all || subs_[i].type == type)) {
        return true;
      }
    }
    subs_[count_++] = Sub{type, handler, user, all};
    return true;
  }

  Sub subs_[kMaxSubs]{};
  int count_ = 0;
};

inline EventBus &events() { return EventBus::instance(); }

}  // namespace Os2