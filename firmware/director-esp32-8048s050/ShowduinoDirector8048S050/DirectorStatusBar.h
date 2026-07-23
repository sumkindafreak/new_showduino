#ifndef SHOWDUINO_DIRECTOR_STATUS_BAR_H
#define SHOWDUINO_DIRECTOR_STATUS_BAR_H

#include <Arduino.h>
#include <lvgl.h>
#include <string.h>
#include "BoardConfig.h"

/**
 * Persistent Director OS status bar — live ecosystem health (display-only).
 * Wall clock from SUE TIME:; runtime/network/nodes from existing desk state.
 * Selective LVGL label updates only — no full redraw.
 */
class DirectorStatusBar {
 public:
  enum class SyncState : uint8_t { Unavailable = 0, Synchronising, Synced };
  enum class Level : uint8_t { Unknown = 0, Ok, Warn, Fault };

  enum class SystemState : uint8_t {
    Booting = 0, Discovery, Ready, Running, Paused, Stopped, Emergency, Ota, Error
  };
  enum class NetworkState : uint8_t {
    Online = 0, Degraded, Offline, Lost
  };
  enum class EmergencyState : uint8_t {
    Normal = 0, EmergencyStop, Fault
  };

  static const int HEIGHT = 40;
  static const uint32_t STALE_MS = 2500UL;

  void begin() {
    if (root_) return;
    lv_obj_t *layer = lv_layer_top();
    root_ = lv_obj_create(layer);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, SCREEN_WIDTH, HEIGHT);
    lv_obj_set_pos(root_, 0, 0);
    applyBarChrome(false);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_CLICKABLE);

    title_ = makeLabel(root_, "SHOWDUINO", 8, 10, 0xF87171);

    date_ = makeLabel(root_, "--- -- --- ----", 108, 2, 0xD4D4D8);
    time_ = makeLabel(root_, "--:--:--", 108, 20, 0xFAFAFA);
    lv_obj_set_style_text_font(time_, &lv_font_montserrat_16, 0);

    /* Global health only — RTC detail removed (date/time imply clock feed). */
    sysLabel_ = makeLabel(root_, "SYS —", 280, 4, 0xA1A1AA);
    netLabel_ = makeLabel(root_, "NET —", 280, 20, 0xA1A1AA);
    nodesLabel_ = makeLabel(root_, "Nodes -/-", 520, 4, 0xA1A1AA);
    emergLabel_ = makeLabel(root_, "CLEAR", 520, 20, 0x4ADE80);

    /* Reserved strip for future widgets */
    expandSlot_ = makeLabel(root_, "", 680, 10, 0x52525B);
    lv_obj_set_width(expandSlot_, 110);

    syncState_ = SyncState::Synchronising;
    dirtyTime_ = dirtySys_ = dirtyNet_ = dirtyNodes_ = dirtyEmerg_ = dirtyChrome_ = true;
  }

  lv_obj_t *root() const { return root_; }
  int height() const { return HEIGHT; }

  bool applyTimeWire(const char *line) {
    if (!line || strncmp(line, "TIME:", 5) != 0) return false;
    const char *p = line + 5;
    char epochBuf[16] = {0}, tod[12] = {0}, longDate[28] = {0}, health[16] = {0}, source[16] = {0};
    if (!takeField(&p, epochBuf, sizeof(epochBuf))) return false;
    if (!takeField(&p, tod, sizeof(tod))) return false;
    if (!takeField(&p, longDate, sizeof(longDate))) return false;
    takeField(&p, health, sizeof(health));
    takeField(&p, source, sizeof(source));

    bool changed = false;
    if (strcmp(timeText_, tod) != 0) {
      strncpy(timeText_, tod, sizeof(timeText_) - 1);
      changed = true;
    }
    if (strcmp(dateText_, longDate) != 0) {
      strncpy(dateText_, longDate, sizeof(dateText_) - 1);
      changed = true;
    }
    lastRxMs_ = millis();
    hasTime_ = (tod[0] && strcmp(tod, "--:--:--") != 0);

    SyncState next = SyncState::Synced;
    if (!hasTime_ || strcmp(health, "offline") == 0) next = SyncState::Unavailable;
    else if (strcmp(health, "unsynced") == 0 || strcmp(source, "compile-seed") == 0 ||
             strcmp(source, "soft-clock") == 0) next = SyncState::Synchronising;

    if (wasStale_ && next == SyncState::Synced) {
      wasStale_ = false;
      syncState_ = next;
      changed = true;
      logEvent("Time restored");
    } else if (next != syncState_) {
      syncState_ = next;
      changed = true;
      if (next == SyncState::Synced) logEvent("Time synchronised");
      else if (next == SyncState::Synchronising) logEvent("Time synchronising");
      else logEvent("RTC unavailable");
    }
    if (changed) dirtyTime_ = true;
    return true;
  }

  void noteLinkDown() {
    if (hasTime_) {
      syncState_ = SyncState::Unavailable;
      wasStale_ = true;
      dirtyTime_ = true;
      logEvent("Time synchronisation lost");
    } else {
      syncState_ = SyncState::Unavailable;
      dirtyTime_ = true;
    }
  }

  void noteWaitingForSue() {
    if (!hasTime_ && syncState_ != SyncState::Synchronising) {
      syncState_ = SyncState::Synchronising;
      dirtyTime_ = true;
    }
  }

  void setSystemState(SystemState st) {
    if (systemState_ == st) return;
    systemState_ = st;
    dirtySys_ = true;
    logEvent("Runtime State Changed");
  }

  void setNetworkState(NetworkState st) {
    if (networkState_ == st) return;
    NetworkState prev = networkState_;
    networkState_ = st;
    dirtyNet_ = true;
    if (st == NetworkState::Lost) logEvent("Communication Lost");
    else if (prev == NetworkState::Lost || prev == NetworkState::Offline) {
      if (st == NetworkState::Online || st == NetworkState::Degraded) logEvent("Communication Restored");
    }
  }

  void setNodeCounts(uint8_t connected, uint8_t expected) {
    if (nodesConnected_ == connected && nodesExpected_ == expected) return;
    uint8_t prev = nodesConnected_;
    nodesConnected_ = connected;
    nodesExpected_ = expected ? expected : 1;
    dirtyNodes_ = true;
    if (connected > prev) logEvent("Node Joined");
    else if (connected < prev) logEvent("Node Lost");
  }

  void setEmergencyState(EmergencyState st) {
    if (emergencyState_ == st) return;
    emergencyState_ = st;
    dirtyEmerg_ = true;
    dirtyChrome_ = true;
    /* Emergency Activated/Cleared are logged by ShowduinoUi::setEmergencyLocked. */
  }

  void update(uint32_t nowMs) {
    if (!root_) return;

    if (hasTime_ && lastRxMs_ != 0 && (nowMs - lastRxMs_) > STALE_MS) {
      if (!wasStale_) {
        wasStale_ = true;
        syncState_ = SyncState::Unavailable;
        dirtyTime_ = true;
        logEvent("Time synchronisation lost");
      }
    }

    if (dirtyChrome_) {
      dirtyChrome_ = false;
      applyBarChrome(emergencyState_ == EmergencyState::EmergencyStop);
    }

    if (dirtyTime_) {
      dirtyTime_ = false;
      setLabelIfChanged(date_, dateText_);
      setLabelIfChanged(time_, timeText_);
    }

    if (dirtySys_) {
      dirtySys_ = false;
      const char *t = systemStateName(systemState_);
      uint32_t c = levelColor(systemLevel(systemState_));
      char buf[24];
      snprintf(buf, sizeof(buf), "SYS %s", t);
      setLabelColor(sysLabel_, buf, c);
    }

    if (dirtyNet_) {
      dirtyNet_ = false;
      const char *t = networkStateName(networkState_);
      uint32_t c = levelColor(networkLevel(networkState_));
      setLabelColor(netLabel_, t, c);
    }

    if (dirtyNodes_) {
      dirtyNodes_ = false;
      char buf[28];
      snprintf(buf, sizeof(buf), "Nodes %u/%u", (unsigned)nodesConnected_, (unsigned)nodesExpected_);
      Level lv = (nodesConnected_ >= nodesExpected_) ? Level::Ok
               : (nodesConnected_ == 0 ? Level::Fault : Level::Warn);
      setLabelColor(nodesLabel_, buf, levelColor(lv));
    }

    if (dirtyEmerg_) {
      dirtyEmerg_ = false;
      /* Word "NORMAL" reserved for Desktop Safety — keep emergency distinct. */
      const char *t = "CLEAR";
      uint32_t c = 0x4ADE80;
      if (emergencyState_ == EmergencyState::EmergencyStop) {
        t = "E-STOP";
        c = 0xF87171;
      } else if (emergencyState_ == EmergencyState::Fault) {
        t = "FAULT";
        c = 0xF87171;
      }
      setLabelColor(emergLabel_, t, c);
    }
  }

  typedef void (*LogFn)(const char *msg);
  void setLogCallback(LogFn fn) { logFn_ = fn; }

 private:
  lv_obj_t *root_ = nullptr;
  lv_obj_t *title_ = nullptr;
  lv_obj_t *date_ = nullptr;
  lv_obj_t *time_ = nullptr;
  lv_obj_t *sysLabel_ = nullptr;
  lv_obj_t *netLabel_ = nullptr;
  lv_obj_t *nodesLabel_ = nullptr;
  lv_obj_t *emergLabel_ = nullptr;
  lv_obj_t *expandSlot_ = nullptr;

  char timeText_[12] = "--:--:--";
  char dateText_[28] = "--- -- --- ----";
  uint32_t lastRxMs_ = 0;
  bool hasTime_ = false;
  bool wasStale_ = false;
  SyncState syncState_ = SyncState::Synchronising;
  SystemState systemState_ = SystemState::Booting;
  NetworkState networkState_ = NetworkState::Offline;
  EmergencyState emergencyState_ = EmergencyState::Normal;
  uint8_t nodesConnected_ = 0;
  uint8_t nodesExpected_ = 1;

  bool dirtyTime_ = true;
  bool dirtySys_ = true;
  bool dirtyNet_ = true;
  bool dirtyNodes_ = true;
  bool dirtyEmerg_ = true;
  bool dirtyChrome_ = true;
  LogFn logFn_ = nullptr;

  static lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(l, x, y);
    return l;
  }

  static void setLabelIfChanged(lv_obj_t *lab, const char *text) {
    if (!lab || !text) return;
    const char *cur = lv_label_get_text(lab);
    if (!cur || strcmp(cur, text) != 0) lv_label_set_text(lab, text);
  }

  static void setLabelColor(lv_obj_t *lab, const char *text, uint32_t color) {
    if (!lab || !text) return;
    const char *cur = lv_label_get_text(lab);
    if (!cur || strcmp(cur, text) != 0) lv_label_set_text(lab, text);
    lv_obj_set_style_text_color(lab, lv_color_hex(color), 0);
  }

  void applyBarChrome(bool emergency) {
    if (!root_) return;
    if (emergency) {
      lv_obj_set_style_bg_color(root_, lv_color_hex(0x450A0A), 0);
      lv_obj_set_style_border_color(root_, lv_color_hex(0xDC2626), 0);
    } else {
      lv_obj_set_style_bg_color(root_, lv_color_hex(0x111113), 0);
      lv_obj_set_style_border_color(root_, lv_color_hex(0x3F3F46), 0);
    }
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(root_, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(root_, 1, 0);
  }

  static bool takeField(const char **pp, char *out, size_t outLen) {
    if (!pp || !*pp || !out || outLen == 0) return false;
    const char *p = *pp;
    size_t i = 0;
    while (*p && *p != '|' && i + 1 < outLen) out[i++] = *p++;
    out[i] = '\0';
    if (*p == '|') p++;
    *pp = p;
    return out[0] != '\0';
  }

  static uint32_t levelColor(Level lv) {
    switch (lv) {
      case Level::Ok: return 0x4ADE80;
      case Level::Warn: return 0xFBBF24;
      case Level::Fault: return 0xF87171;
      default: return 0xA1A1AA;
    }
  }

  static const char *systemStateName(SystemState st) {
    switch (st) {
      case SystemState::Booting: return "BOOTING";
      case SystemState::Discovery: return "DISCOVERY";
      case SystemState::Ready: return "READY";
      case SystemState::Running: return "RUNNING";
      case SystemState::Paused: return "PAUSED";
      case SystemState::Stopped: return "STOPPED";
      case SystemState::Emergency: return "EMERGENCY";
      case SystemState::Ota: return "OTA";
      case SystemState::Error: return "FAULT";
      default: return "—";
    }
  }

  static Level systemLevel(SystemState st) {
    switch (st) {
      case SystemState::Ready:
      case SystemState::Running: return Level::Ok;
      case SystemState::Booting:
      case SystemState::Discovery:
      case SystemState::Paused:
      case SystemState::Ota: return Level::Warn;
      case SystemState::Emergency:
      case SystemState::Error: return Level::Fault;
      case SystemState::Stopped: return Level::Unknown;
      default: return Level::Unknown;
    }
  }

  static const char *networkStateName(NetworkState st) {
    switch (st) {
      case NetworkState::Online: return "ONLINE";
      case NetworkState::Degraded: return "DEGRADED";
      case NetworkState::Offline: return "OFFLINE";
      case NetworkState::Lost: return "LOST";
      default: return "—";
    }
  }

  static Level networkLevel(NetworkState st) {
    switch (st) {
      case NetworkState::Online: return Level::Ok;
      case NetworkState::Degraded: return Level::Warn;
      case NetworkState::Offline:
      case NetworkState::Lost: return Level::Fault;
      default: return Level::Unknown;
    }
  }

  void logEvent(const char *msg) {
    if (logFn_ && msg) logFn_(msg);
  }
};

#endif