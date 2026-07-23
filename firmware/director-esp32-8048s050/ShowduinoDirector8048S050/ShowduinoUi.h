#ifndef SHOWDUINO_UI_H
#define SHOWDUINO_UI_H

#include <Arduino.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "backlight.h"
#include "src/ShowManager.h"
#include "src/ShowThumb.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_show_runtime.h"
#include "DirectorStatusBar.h"
#include "ShowduinoOsUi.h"

// =========================================================
// Showduino OS — LVGL Director shell (Stage 7.9 design system)
// Desktop is the canonical visual reference for all pages.
// Presentation only — no protocol / runtime / comms changes.
// =========================================================

/* Stage 7.9 — layout aliases to Showduino OS design system */
#define SHOWDUINO_TOPBAR_Y           OS_TITLE_Y
#define SHOWDUINO_CONTENT_Y          OS_PRIMARY_Y
#define SHOWDUINO_CONTENT_H          OS_PRIMARY_H
#define SHOWDUINO_OPERATOR_LOG_Y     OS_BODY_Y
#define SHOWDUINO_EMERGENCY_BANNER_Y DirectorStatusBar::HEIGHT
#define SHOWDUINO_DOCK_Y             OS_DOCK_Y
#define SHOWDUINO_DESK_BODY_Y        OS_BODY_Y
#define SHOWDUINO_DESK_BODY_H        OS_BODY_H

typedef void (*ShowduinoCommandCallback)(const String &command);

enum class DeskRelayView : uint8_t {
  Unknown = 0,
  ConfirmedOff,
  ConfirmedOn,
  PendingOff,
  PendingOn,
  Fault
};

enum class DeskShowView : uint8_t {
  Unknown = 0,
  Idle,
  Playing,
  Emergency,
  Finished
};

#ifndef OPERATOR_EVENT_LOG_MAX
#define OPERATOR_EVENT_LOG_MAX 250
#endif
#ifndef OPERATOR_EVENT_LINE_LEN
#define OPERATOR_EVENT_LINE_LEN 80
#endif

#include <esp_heap_caps.h>

class ShowduinoUi {
public:
  enum class DeskPage : uint8_t {
    Desktop = 0, Live, Shows, Details, Nodes, Settings
  };

  void begin(ShowduinoCommandCallback callback) {
    commandCallback = callback;
    ensureEventLogStorage();
    Serial.println("[UI] Showduino OS theme…");
    initTheme();
    Serial.println("[UI] status bar…");
    statusBar_.setLogCallback(statusBarLogThunk);
    statusBarSelf_ = this;
    statusBar_.begin();
    Serial.println("[UI] building screens…");
    buildScreens();
    Serial.println("[UI] loading desktop…");
    showDesktop();
    syncStatusBarHealth();
    Serial.println("[UI] ready");
  }

  void setBootTime(unsigned long startedAt) { bootMs = startedAt; }
  void setLinkState(uint8_t state) {
    if (linkState == state) return;
    linkState = state;
    statusDirty = true;
    if (state == LINK_DISCONNECTED) statusBar_.noteLinkDown();
    else statusBar_.noteWaitingForSue();
    syncStatusBarHealth();
  }
  uint8_t getLinkState() const { return linkState; }
  void setEmergencyLocked(bool locked) {
    const bool wasLocked = emergencyLocked;
    if (wasLocked == locked && emergencyOverlayBuilt) {
      if (!locked) updateEmergencyResumeButton();
      updatePersistentBanner();
      syncStatusBarHealth();
      return;
    }
    emergencyLocked = locked;
    statusDirty = true;

    if (locked && !wasLocked) {
      emergencyOverlayDismissed = false;
      emergencyAcknowledged = false;
      emergencySessionOpen = true;
      emergencyActiveSinceMs = millis();
      sessionEmergencyCount++;
      captureEmergencySnapshot();
      showEmergencyOverlay();
      pushOperatorEvent("Emergency Activated");
      emergencyAlarmOnHook();
    } else if (!locked && wasLocked) {
      pushOperatorEvent("Emergency Cleared");
      emergencyAlarmOffHook();
      updateEmergencyResumeButton();
      /* Mid-show: keep overlay so operator can RESUME or ABORT.
         No active playback: return to desk immediately. */
      if (mirroredState == SHOW_STATE_IDLE || mirroredState == SHOW_STATE_SHOW_LOADED ||
          mirroredState == SHOW_STATE_BOOTING) {
        emergencySessionOpen = false;
        emergencyOverlayDismissed = true;
        pendingAbortAwait = false;
        pendingResumeAwait = false;
        hideAbortConfirm();
        hideEmergencyOverlay();
        restorePageAfterEmergency();
        setShowView(DeskShowView::Idle);
      } else if (emergencyOverlayVisible && emergencyOverlayRoot) {
        refreshEmergencyOverlayContent();
      }
    }
    updatePersistentBanner();
    syncStatusBarHealth();
  }

  bool isEmergencyLocked() const { return emergencyLocked; }

  /** Snapshot timeline/show context for the overlay (call before/with lock). */
  void setEmergencyPlaybackSnapshot(const char *showName, const char *playStateBefore,
                                    uint32_t elapsedMs, uint16_t cueIndex, uint16_t cueTotal,
                                    uint32_t remainMs = 0, bool stageOk = true) {
    strncpy(estopShowName, showName && showName[0] ? showName : "-", sizeof(estopShowName) - 1);
    estopShowName[sizeof(estopShowName) - 1] = '\0';
    strncpy(estopPlayStateBefore, playStateBefore && playStateBefore[0] ? playStateBefore : "Stopped",
            sizeof(estopPlayStateBefore) - 1);
    estopPlayStateBefore[sizeof(estopPlayStateBefore) - 1] = '\0';
    estopElapsedMs = elapsedMs;
    estopRemainMs = remainMs;
    estopCueIndex = cueIndex;
    estopCueTotal = cueTotal;
    estopStageConnected = stageOk;
    if (emergencyOverlayVisible) refreshEmergencyOverlayContent();
  }

  void setNodeCount(uint8_t n) {
    if (nodeCount == n) return;
    nodeCount = n;
    liveStatusDirty = true;
    syncStatusBarHealth();
  }

  /**
   * Stage 7: mirror ShowRuntime into operator UX (overlay / banner / LIVE / complete).
   * Does not mutate runtime — display + pending-confirm handling only.
   */
  void applyRuntimeMirror(const ShowRuntime &rt) {
    ShowState prev = mirroredState;
    mirroredState = rt.state;
    mirroredRevision = rt.revision;

    if (rt.showName[0]) setLoadedShowName(rt.showName);

    uint8_t pct = 0;
    if (rt.totalDurationMs > 0) {
      if (rt.elapsedMs >= rt.totalDurationMs) pct = 100;
      else pct = (uint8_t)((rt.elapsedMs * 100UL) / rt.totalDurationMs);
    } else if (rt.finished) {
      pct = 100;
    }

    liveCue = rt.currentCue;
    liveCueTotal = rt.totalCues;
    liveElapsedMs = rt.elapsedMs;
    liveRemainMs = rt.remainingMs;
    liveProgressPct = pct;
    liveStageConnected = (rt.stageConnected != 0) || (linkState == LINK_READY);
    strncpy(liveStateName, showStateName(rt.state), sizeof(liveStateName) - 1);
    liveStateName[sizeof(liveStateName) - 1] = '\0';
    liveStatusDirty = true;
    statusDirty = true;

    setTimelinePlayback(rt.showName[0] ? rt.showName : "-", liveStateName,
                        rt.elapsedMs, rt.remainingMs, pct);
    refreshLiveStatusPanel();

    /* Activation: ShowRuntime EMERGENCY_STOP */
    if (rt.state == SHOW_STATE_EMERGENCY_STOP) {
      setShowView(DeskShowView::Emergency);
      if (prev != SHOW_STATE_EMERGENCY_STOP) {
        setEmergencyPlaybackSnapshot(rt.showName, showStateName(prev == SHOW_STATE_BOOTING ? SHOW_STATE_RUNNING : prev),
                                     rt.elapsedMs, (uint16_t)rt.currentCue,
                                     (uint16_t)rt.totalCues, rt.remainingMs,
                                     liveStageConnected);
      } else {
        setEmergencyPlaybackSnapshot(rt.showName, estopPlayStateBefore,
                                     rt.elapsedMs, (uint16_t)rt.currentCue,
                                     (uint16_t)rt.totalCues, rt.remainingMs,
                                     liveStageConnected);
      }
      if (!emergencyLocked) {
        setEmergencyLocked(true);
      } else if (!emergencyOverlayVisible && !emergencyOverlayDismissed && !emergencyVisitingDiag) {
        showEmergencyOverlay();
      }
      updatePersistentBanner();
    } else {
      /* Left EMERGENCY_STOP via Stage CLEAR / STOP — unlock from runtime alone. */
      if (prev == SHOW_STATE_EMERGENCY_STOP || (emergencyLocked && !rt.emergency)) {
        setEmergencyLocked(false);
      }
      updatePersistentBanner();
    }

    bool justConfirmedResume = false;
    bool justConfirmedAbort = false;

    /* Pending RESUME: dismiss only after Stage confirms RUNNING */
    if (pendingResumeAwait && rt.state == SHOW_STATE_RUNNING) {
      pendingResumeAwait = false;
      emergencySessionOpen = false;
      hideEmergencyOverlay();
      emergencyOverlayDismissed = true;
      restorePageAfterEmergency();
      pushOperatorEvent("Resumed");
      setShowView(DeskShowView::Playing);
      justConfirmedResume = true;
    }

    /* Pending ABORT: dismiss after Stage confirms IDLE (or SHOW_LOADED after clear+stop) */
    if (pendingAbortAwait &&
        (rt.state == SHOW_STATE_IDLE || rt.state == SHOW_STATE_SHOW_LOADED)) {
      pendingAbortAwait = false;
      emergencySessionOpen = false;
      hideEmergencyOverlay();
      hideAbortConfirm();
      emergencyOverlayDismissed = true;
      if (emergencyLocked) setEmergencyLocked(false);
      showDesktop();
      pushOperatorEvent("Show Aborted");
      setShowView(DeskShowView::Idle);
      justConfirmedAbort = true;
    }

    /* State-transition operator events */
    if (prev != rt.state) {
      switch (rt.state) {
        case SHOW_STATE_SHOW_LOADED: pushOperatorEvent("Show Loaded"); break;
        case SHOW_STATE_RUNNING:
          if (prev == SHOW_STATE_PAUSED && !justConfirmedResume) pushOperatorEvent("Resumed");
          else if (prev != SHOW_STATE_EMERGENCY_STOP && prev != SHOW_STATE_PAUSED)
            pushOperatorEvent("Show Started");
          break;
        case SHOW_STATE_PAUSED:
          if (prev == SHOW_STATE_RUNNING) pushOperatorEvent("Paused");
          break;
        case SHOW_STATE_FINISHED:
          pushOperatorEvent("Show Finished");
          showCompleteScreen(rt);
          break;
        case SHOW_STATE_ERROR:
          sessionErrorCount++;
          pushOperatorEvent(rt.lastError[0] ? rt.lastError : "Error");
          break;
        case SHOW_STATE_IDLE:
          if (!justConfirmedAbort && prev == SHOW_STATE_FINISHED)
            /* menu return path */;
          break;
        default: break;
      }
    }

    if (rt.state == SHOW_STATE_FINISHED) {
      setShowView(DeskShowView::Finished);
    } else if (rt.state == SHOW_STATE_RUNNING || rt.state == SHOW_STATE_PAUSED) {
      if (completeOverlayVisible) hideCompleteOverlay();
      setShowView(DeskShowView::Playing);
    } else if (rt.state != SHOW_STATE_EMERGENCY_STOP) {
      if (rt.state == SHOW_STATE_IDLE || rt.state == SHOW_STATE_SHOW_LOADED) {
        setShowView(DeskShowView::Idle);
      }
    }
    syncStatusBarHealth();
  }

  void tickEmergencyOverlay(unsigned long nowMs) {
    tickOperatorUx(nowMs);
  }

  void tickOperatorUx(unsigned long nowMs) {
    /* Persistent banner pulse while runtime emergency. */
    if (persistentBannerRoot && !lv_obj_has_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN)) {
      bool flashOn = ((nowMs / 500UL) % 2UL) == 0;
      lv_obj_set_style_bg_opa(persistentBannerRoot, flashOn ? LV_OPA_COVER : LV_OPA_80, 0);
    }

    if (emergencyOverlayVisible && emergencyOverlayRoot) {
      bool flashOn = ((nowMs / 500UL) % 2UL) == 0;
      if (estopWarnIcon) {
        lv_obj_set_style_opa(estopWarnIcon, flashOn ? LV_OPA_COVER : LV_OPA_40, 0);
      }
      /* Subtle dark-red breathe on overlay background. */
      lv_obj_set_style_bg_color(emergencyOverlayRoot,
                                lv_color_hex(flashOn ? 0x450A0A : 0x3F0A0A), 0);

      if (estopTimerLabel && emergencyActiveSinceMs > 0) {
        uint32_t activeMs = nowMs - emergencyActiveSinceMs;
        char tbuf[48];
        char clock[16];
        formatClock(activeMs, clock, sizeof(clock));
        snprintf(tbuf, sizeof(tbuf), "Emergency active: %s", clock);
        const char *cur = lv_label_get_text(estopTimerLabel);
        if (!cur || strcmp(cur, tbuf) != 0) lv_label_set_text(estopTimerLabel, tbuf);
      }
      updateEmergencyResumeButton();
    }

    if (liveProgressBar && liveStatusDirty) {
      refreshLiveStatusPanel();
    }
  }

  bool emergencyOverlayIsVisible() const { return emergencyOverlayVisible; }
  bool completeOverlayIsVisible() const { return completeOverlayVisible; }

  /** Future alarm sound hooks — no audio yet. */
  static void emergencyAlarmOnHook() {
    /* Future: start looping alarm sample. */
  }
  static void emergencyAlarmOffHook() {
    /* Future: stop alarm sample. */
  }
  void setTraffic(uint32_t tx, uint32_t rx) {
    if (txCount == tx && rxCount == rx) return;
    txCount = tx;
    rxCount = rx;
    trafficDirty = true;
  }

  void setSynchronising(bool on) {
    synchronising = on;
    statusDirty = true;
    syncStatusBarHealth();
  }
  bool isSynchronising() const { return synchronising; }

  void setShowView(DeskShowView v) {
    if (showView == v) return;
    showView = v;
    statusDirty = true;
    syncStatusBarHealth();
  }

  void markRelayStaleUnknown() {
    for (uint8_t i = 0; i < 8; i++) {
      relayView[i] = DeskRelayView::Unknown;
      refreshRelayButton(i);
    }
    showView = DeskShowView::Unknown;
    statusDirty = true;
  }

  void beginSnapshot() {
    snapshotActive = true;
    synchronising = true;
    statusDirty = true;
    syncStatusBarHealth();
  }

  void endSnapshot() {
    snapshotActive = false;
    synchronising = false;
    statusDirty = true;
    syncStatusBarHealth();
  }

  bool snapshotInProgress() const { return snapshotActive; }

  void applyConfirmedRelay(uint8_t channel, bool on) {
    if (channel < 1 || channel > 8) return;
    uint8_t idx = channel - 1;
    relayView[idx] = on ? DeskRelayView::ConfirmedOn : DeskRelayView::ConfirmedOff;
    refreshRelayButton(idx);
  }

  void applyRelayUnknown(uint8_t channel) {
    if (channel < 1 || channel > 8) return;
    relayView[channel - 1] = DeskRelayView::Unknown;
    refreshRelayButton(channel - 1);
  }

  void applyRelayFault(uint8_t channel) {
    if (channel < 1 || channel > 8) return;
    relayView[channel - 1] = DeskRelayView::Fault;
    refreshRelayButton(channel - 1);
  }

  void clearRelayPendingKeepLast(uint8_t channel) {
    if (channel < 1 || channel > 8) return;
    uint8_t idx = channel - 1;
    if (lastConfirmed[idx] == DeskRelayView::ConfirmedOn ||
        lastConfirmed[idx] == DeskRelayView::ConfirmedOff) {
      relayView[idx] = lastConfirmed[idx];
    } else {
      relayView[idx] = DeskRelayView::Unknown;
    }
    refreshRelayButton(idx);
  }

  void noteConfirmedSnapshot(uint8_t channel, bool on) {
    if (channel < 1 || channel > 8) return;
    lastConfirmed[channel - 1] = on ? DeskRelayView::ConfirmedOn : DeskRelayView::ConfirmedOff;
    applyConfirmedRelay(channel, on);
  }

  DeskRelayView getRelayView(uint8_t channel) const {
    if (channel < 1 || channel > 8) return DeskRelayView::Unknown;
    return relayView[channel - 1];
  }

  /* Legacy helpers used by older call sites — map to confirmed only */
  void setRelayState(uint8_t channel, bool on) { applyConfirmedRelay(channel, on); }

  void setAllRelaysOff() {
    for (uint8_t i = 1; i <= 8; i++) applyConfirmedRelay(i, false);
  }

  void appendLog(const String &line) {
    Serial.println(line);
    pushOperatorEvent(line.c_str());
  }

  void pushOperatorEvent(const char *msg) {
    if (!msg || !msg[0]) return;
    if (!eventLog) ensureEventLogStorage();
    if (!eventLog) {
      Serial.println(msg);
      return;
    }
    /* Newest first: shift down, insert at 0. */
    uint16_t n = eventLogCount;
    if (n >= OPERATOR_EVENT_LOG_MAX) n = OPERATOR_EVENT_LOG_MAX - 1;
    for (int i = (int)n; i > 0; i--) {
      memcpy(eventSlot(i), eventSlot(i - 1), OPERATOR_EVENT_LINE_LEN);
    }
    strncpy(eventSlot(0), msg, OPERATOR_EVENT_LINE_LEN - 1);
    eventSlot(0)[OPERATOR_EVENT_LINE_LEN - 1] = '\0';
    if (eventLogCount < OPERATOR_EVENT_LOG_MAX) eventLogCount++;

    /* Build display text (newest first, capped for LVGL). */
    uiLogText = "";
    uint16_t showN = eventLogCount;
    if (showN > 40) showN = 40;
    for (uint16_t i = 0; i < showN; i++) {
      uiLogText += eventSlot(i);
      uiLogText += "\n";
    }
    if (operatorLogLabel != nullptr) {
      lv_label_set_text(operatorLogLabel, uiLogText.c_str());
      if (operatorLogScroll != nullptr) {
        lv_obj_scroll_to_y(operatorLogScroll, 0, LV_ANIM_OFF);
      }
    }
  }

  /** Refresh SHOWS list from ShowManager (SD scan results). */
  void refreshShowLibrary(const ShowManager &sm) {
    rebuildShowList(sm);
  }

  const char *selectedShowId() const { return selectedShowIdBuf; }
  bool hasSelectedShow() const { return selectedShowIdBuf[0] != '\0'; }

  void setLoadedShowName(const char *name) {
    if (!name) name = "";
    if (strcmp(loadedShowNameBuf, name) == 0) return;
    strncpy(loadedShowNameBuf, name, sizeof(loadedShowNameBuf) - 1);
    loadedShowNameBuf[sizeof(loadedShowNameBuf) - 1] = '\0';
    statusDirty = true;
  }

  /** Timeline / runtime readout (Stage 5–7). */
  void setTimelinePlayback(const char *showName, const char *stateText,
                           uint32_t elapsedMs, uint32_t remainMs, uint8_t progressPct) {
    char line[160];
    char et[16], rt[16];
    formatClock(elapsedMs, et, sizeof(et));
    formatClock(remainMs, rt, sizeof(rt));
    snprintf(line, sizeof(line), "%s | %s | %s / -%s | %u%%",
             showName && showName[0] ? showName : "-",
             stateText ? stateText : "Stopped",
             et, rt, (unsigned)progressPct);
    if (timelineStatusLabel) {
      const char *cur = lv_label_get_text(timelineStatusLabel);
      if (cur == nullptr || strcmp(cur, line) != 0) {
        lv_label_set_text(timelineStatusLabel, line);
      }
    }
    if (timelineDetailLabel) {
      char detail[192];
      snprintf(detail, sizeof(detail),
               "Show: %s\nState: %s\nElapsed: %s\nRemaining: %s\nProgress: %u%%",
               showName && showName[0] ? showName : "-",
               stateText ? stateText : "Stopped",
               et, rt, (unsigned)progressPct);
      const char *cur = lv_label_get_text(timelineDetailLabel);
      if (cur == nullptr || strcmp(cur, detail) != 0) {
        lv_label_set_text(timelineDetailLabel, detail);
      }
    }
    if (liveProgressBar) {
      lv_bar_set_value(liveProgressBar, progressPct, LV_ANIM_ON);
    }
  }

  static void formatClock(uint32_t ms, char *out, size_t outLen) {
    uint32_t sec = ms / 1000UL;
    uint32_t m = sec / 60UL;
    uint32_t s = sec % 60UL;
    snprintf(out, outLen, "%lu:%02lu", (unsigned long)m, (unsigned long)s);
  }

  /** Refresh Settings auto-backlight readout (0 = never off). */
  void setScreenTimeoutMinutes(uint8_t minutes) {
    screenTimeoutMinutes = minutes;
    refreshTimeoutLabel();
  }

  uint8_t getScreenTimeoutMinutes() const { return screenTimeoutMinutes; }

  /** SUE TimeService wire (TIME:…) — display only, no local clock. */
  bool applySueTimeWire(const char *line) { return statusBar_.applyTimeWire(line); }

  /** Derive OS status-bar health from existing desk state (no new protocol). */
  void syncStatusBarHealth() {
    using SB = DirectorStatusBar;

    SB::EmergencyState em = SB::EmergencyState::Normal;
    if (emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) {
      em = SB::EmergencyState::EmergencyStop;
    } else if (mirroredState == SHOW_STATE_ERROR) {
      em = SB::EmergencyState::Fault;
    }
    statusBar_.setEmergencyState(em);

    /* Status bar = fabric health only. Show playback belongs on the Desktop header. */
    SB::SystemState sys = SB::SystemState::Booting;
    if (em == SB::EmergencyState::EmergencyStop) {
      sys = SB::SystemState::Emergency;
    } else if (mirroredState == SHOW_STATE_ERROR) {
      sys = SB::SystemState::Error;
    } else if (mirroredState == SHOW_STATE_BOOTING && linkState != LINK_READY) {
      sys = SB::SystemState::Booting;
    } else if (linkState != LINK_READY || synchronising) {
      sys = SB::SystemState::Discovery;
    } else {
      sys = SB::SystemState::Ready;
    }
    statusBar_.setSystemState(sys);

    SB::NetworkState net = SB::NetworkState::Offline;
    if (linkState == LINK_DISCONNECTED) {
      net = SB::NetworkState::Lost;
    } else if (linkState == LINK_SEARCHING) {
      net = SB::NetworkState::Offline;
    } else if (synchronising || nodeCount < SHOWDUINO_EXPECTED_NODES) {
      net = SB::NetworkState::Degraded;
    } else {
      net = SB::NetworkState::Online;
    }
    statusBar_.setNetworkState(net);
    statusBar_.setNodeCounts(nodeCount, (uint8_t)SHOWDUINO_EXPECTED_NODES);
  }

  // Call often from loop. Only touches LVGL when something actually changed.
  void updateStatusWidgets(bool refreshTrafficAndUptime = false) {
    syncStatusBarHealth();
    statusBar_.update(millis());

    unsigned long now = millis();
    unsigned long uptimeSec = (now - bootMs) / 1000UL;
    bool uptimeChanged = refreshTrafficAndUptime && (uptimeSec != lastDrawnUptimeSec);
    bool drawTraffic = refreshTrafficAndUptime && trafficDirty;

    if (!statusDirty && !uptimeChanged && !drawTraffic) return;

    if (statusDirty) {
      const char *showVal = loadedShowNameBuf[0] ? loadedShowNameBuf : "No Show Loaded";
      if (sumShowValue_) {
        const char *cur = lv_label_get_text(sumShowValue_);
        if (!cur || strcmp(cur, showVal) != 0) lv_label_set_text(sumShowValue_, showVal);
      }

      const char *rt = deskRuntimeWord();
      lv_color_t runtimeColor = lv_color_hex(0xE5E7EB);
      if (mirroredState == SHOW_STATE_RUNNING) runtimeColor = lv_color_hex(0x4ADE80);
      else if (mirroredState == SHOW_STATE_PAUSED) runtimeColor = lv_color_hex(0xFBBF24);
      else if (mirroredState == SHOW_STATE_EMERGENCY_STOP || mirroredState == SHOW_STATE_ERROR)
        runtimeColor = lv_color_hex(0xF87171);
      if (sumRuntimeValue_) {
        const char *cur = lv_label_get_text(sumRuntimeValue_);
        if (!cur || strcmp(cur, rt) != 0) {
          lv_label_set_text(sumRuntimeValue_, rt);
          lv_obj_set_style_text_color(sumRuntimeValue_, runtimeColor, 0);
        }
      }

      const char *safetyVal = "CLEAR";
      lv_color_t safetyColor = lv_color_hex(0x4ADE80);
      if (emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) {
        safetyVal = "E-STOP";
        safetyColor = lv_color_hex(0xF87171);
      } else if (mirroredState == SHOW_STATE_ERROR) {
        safetyVal = "FAULT";
        safetyColor = lv_color_hex(0xF87171);
      }
      if (sumSafetyValue_) {
        const char *cur = lv_label_get_text(sumSafetyValue_);
        if (!cur || strcmp(cur, safetyVal) != 0) {
          lv_label_set_text(sumSafetyValue_, safetyVal);
          lv_obj_set_style_text_color(sumSafetyValue_, safetyColor, 0);
        }
      }
    }

    if (uptimeChanged && sumUptimeValue_) {
      char uptimeText[16];
      uint32_t h = uptimeSec / 3600UL;
      uint32_t m = (uptimeSec / 60UL) % 60UL;
      uint32_t s = uptimeSec % 60UL;
      snprintf(uptimeText, sizeof(uptimeText), "%02lu:%02lu:%02lu",
               (unsigned long)h, (unsigned long)m, (unsigned long)s);
      lv_label_set_text(sumUptimeValue_, uptimeText);
    }

    if (drawTraffic && sumTrafficValue_) {
      char trafficText[40];
      snprintf(trafficText, sizeof(trafficText), "TX %lu / RX %lu",
               (unsigned long)txCount, (unsigned long)rxCount);
      lv_label_set_text(sumTrafficValue_, trafficText);
    }

    if (uptimeChanged) lastDrawnUptimeSec = uptimeSec;
    statusDirty = false;
    if (drawTraffic) trafficDirty = false;
  }

private:
  ShowduinoCommandCallback commandCallback = nullptr;
  DirectorStatusBar statusBar_;
  static inline ShowduinoUi *statusBarSelf_ = nullptr;
  static void statusBarLogThunk(const char *msg) {
    if (statusBarSelf_) statusBarSelf_->pushOperatorEvent(msg);
  }

  /** Desktop SYSTEM SUMMARY — consistent operator vocabulary. */
  const char *deskRuntimeWord() const {
    switch (mirroredState) {
      case SHOW_STATE_BOOTING: return "BOOTING";
      case SHOW_STATE_IDLE: return "IDLE";
      case SHOW_STATE_SHOW_LOADED: return "LOADED";
      case SHOW_STATE_RUNNING: return "RUNNING";
      case SHOW_STATE_PAUSED: return "PAUSED";
      case SHOW_STATE_FINISHED: return "STOPPED";
      case SHOW_STATE_EMERGENCY_STOP: return "E-STOP";
      case SHOW_STATE_ERROR: return "FAULT";
      default: return "IDLE";
    }
  }
  lv_obj_t *desktopScreen = nullptr;
  lv_obj_t *liveScreen = nullptr;
  lv_obj_t *showsScreen = nullptr;
  lv_obj_t *showDetailsScreen = nullptr;
  lv_obj_t *diagnosticsScreen = nullptr;
  lv_obj_t *settingsScreen = nullptr;
  lv_obj_t *timeoutLabel = nullptr;
  lv_obj_t *showsListPanel = nullptr;
  lv_obj_t *showsListTitle = nullptr;
  lv_obj_t *showListScroll = nullptr;
  lv_obj_t *detailsNameLabel = nullptr;
  lv_obj_t *detailsDescLabel = nullptr;
  lv_obj_t *detailsMetaLabel = nullptr;
  lv_obj_t *detailsIconHost = nullptr;
  lv_obj_t *detailsCanvas = nullptr;
  lv_obj_t *timelineStatusLabel = nullptr;
  lv_obj_t *timelineDetailLabel = nullptr;
  lv_obj_t *liveStatusLabel = nullptr;
  lv_obj_t *liveProgressBar = nullptr;
  lv_obj_t *liveEmergencyDot = nullptr;
  uint8_t screenTimeoutMinutes = 10;
  char selectedShowIdBuf[64] = {};
  char loadedShowNameBuf[64] = {};
  char showOpenCmds[SHOW_INDEX_MAX][80] = {};
  ShowIndexEntry showListCache[SHOW_INDEX_MAX] = {};
  uint8_t showListCount = 0;

  DeskPage currentPage = DeskPage::Desktop;
  DeskPage pageBeforeEmergency = DeskPage::Desktop;

  lv_obj_t *emergencyOverlayRoot = nullptr;
  lv_obj_t *estopWarnIcon = nullptr;
  lv_obj_t *estopTitleLabel = nullptr;
  lv_obj_t *estopDetailLabel = nullptr;
  lv_obj_t *estopTimerLabel = nullptr;
  lv_obj_t *estopBannerLabel = nullptr;
  lv_obj_t *estopResumeBtn = nullptr;
  lv_obj_t *persistentBannerRoot = nullptr;
  lv_obj_t *persistentBannerLabel = nullptr;
  lv_obj_t *abortConfirmRoot = nullptr;
  lv_obj_t *completeOverlayRoot = nullptr;
  lv_obj_t *completeDetailLabel = nullptr;
  bool emergencyOverlayBuilt = false;
  bool emergencyOverlayVisible = false;
  bool emergencyOverlayDismissed = false;
  bool emergencyVisitingDiag = false;
  bool emergencyAcknowledged = false;
  bool pendingResumeAwait = false;
  bool pendingAbortAwait = false;
  bool emergencySessionOpen = false;
  bool completeOverlayVisible = false;
  bool liveStatusDirty = true;
  unsigned long emergencyActiveSinceMs = 0;
  char estopShowName[64] = "-";
  char estopPlayStateBefore[24] = "Stopped";
  uint32_t estopElapsedMs = 0;
  uint32_t estopRemainMs = 0;
  uint16_t estopCueIndex = 0;
  uint16_t estopCueTotal = 0;
  bool estopStageConnected = true;
  unsigned long estopOccurredMs = 0;

  ShowState mirroredState = SHOW_STATE_BOOTING;
  uint32_t mirroredRevision = 0;
  uint32_t liveCue = 0;
  uint32_t liveCueTotal = 0;
  uint32_t liveElapsedMs = 0;
  uint32_t liveRemainMs = 0;
  uint8_t liveProgressPct = 0;
  bool liveStageConnected = false;
  char liveStateName[24] = "IDLE";
  uint8_t nodeCount = 0;
  uint16_t sessionEmergencyCount = 0;
  uint16_t sessionWarningCount = 0;
  uint16_t sessionErrorCount = 0;

  char eventLogStorageHint = 0; /* keeps layout stable; real buffer is heap/PSRAM */
  char *eventLog = nullptr;     /* OPERATOR_EVENT_LOG_MAX * OPERATOR_EVENT_LINE_LEN */
  uint16_t eventLogCount = 0;

  char *eventSlot(uint16_t index) {
    return eventLog + ((size_t)index * OPERATOR_EVENT_LINE_LEN);
  }

  void ensureEventLogStorage() {
    if (eventLog) return;
    const size_t bytes = (size_t)OPERATOR_EVENT_LOG_MAX * OPERATOR_EVENT_LINE_LEN;
    eventLog = (char *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!eventLog) {
      eventLog = (char *)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (eventLog) {
      memset(eventLog, 0, bytes);
      Serial.printf("[UI] event log %u bytes allocated\n", (unsigned)bytes);
    } else {
      Serial.println("[UI] WARN: event log alloc failed");
    }
    (void)eventLogStorageHint;
  }

  ShowduinoOsTheme os_;
  lv_obj_t *sumShowValue_ = nullptr;
  lv_obj_t *liveCueLabel_ = nullptr;
  lv_obj_t *liveElapsedLabel_ = nullptr;
  lv_obj_t *liveRemainLabel_ = nullptr;
  lv_obj_t *showsSummaryLabel_ = nullptr;
  lv_obj_t *sumRuntimeValue_ = nullptr;
  lv_obj_t *sumSafetyValue_ = nullptr;
  lv_obj_t *sumUptimeValue_ = nullptr;
  lv_obj_t *sumTrafficValue_ = nullptr;
  lv_obj_t *operatorLogRoot = nullptr;
  lv_obj_t *operatorLogScroll = nullptr;
  lv_obj_t *operatorLogLabel = nullptr;
  lv_obj_t *relayButtons[8] = {};
  DeskRelayView relayView[8] = {};
  DeskRelayView lastConfirmed[8] = {};
  DeskShowView showView = DeskShowView::Unknown;

  static inline const char *const kRelayCmds[8] = {
    "UI:RELAY:1", "UI:RELAY:2", "UI:RELAY:3", "UI:RELAY:4",
    "UI:RELAY:5", "UI:RELAY:6", "UI:RELAY:7", "UI:RELAY:8"
  };
  lv_style_t styleScreen, stylePanel, styleButton, styleDangerButton, styleTitle, styleSmall;
  String uiLogText;
  uint8_t linkState = LINK_SEARCHING;
  bool emergencyLocked = false;
  bool synchronising = false;
  bool snapshotActive = false;
  bool emergencyActivating = false;
  bool statusDirty = true;
  bool trafficDirty = true;
  uint32_t txCount = 0;
  uint32_t rxCount = 0;
  unsigned long bootMs = 0;
  unsigned long lastDrawnUptimeSec = UINT32_MAX;

  static void staticEventHandler(lv_event_t *event) {
    ShowduinoUi *ui = (ShowduinoUi *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    const char *command = (const char *)lv_obj_get_user_data(target);
    if (ui != nullptr && command != nullptr) ui->runCommand(String(command));
  }

  void runCommand(const String &command) {
    backlightNotifyActivity();

    /* Emergency overlay actions — handled even while overlay blocks the desk. */
    if (command == "UI:ESTOP:RESUME") {
      if (emergencyLocked) {
        pushOperatorEvent("Resume blocked — clear emergency on Stage first");
        return;
      }
      pendingResumeAwait = true;
      pushOperatorEvent("Resume requested — awaiting Stage");
      if (commandCallback) commandCallback("UI:ESTOP:RESUME");
      return;
    }
    if (command == "UI:ESTOP:ABORT") {
      showAbortConfirm();
      return;
    }
    if (command == "UI:ESTOP:ABORT:YES") {
      hideAbortConfirm();
      pendingAbortAwait = true;
      pushOperatorEvent("Abort confirmed — awaiting Stage");
      if (commandCallback) commandCallback("UI:ESTOP:ABORT");
      return;
    }
    if (command == "UI:ESTOP:ABORT:NO") {
      hideAbortConfirm();
      return;
    }
    if (command == "UI:ESTOP:DIAG") {
      emergencyVisitingDiag = true;
      hideEmergencyOverlay();
      /* Persistent banner stays while EMERGENCY_STOP. */
      showDiagnostics();
      if (commandCallback) commandCallback("UI:ESTOP:DIAG");
      return;
    }
    if (command == "UI:ESTOP:DESK") {
      /* Allow returning to desk UI; banner stays if Stage still in EMERGENCY_STOP.
         After CLEAR, this fully exits the session. */
      if (!emergencyLocked) {
        emergencySessionOpen = false;
        emergencyOverlayDismissed = true;
        hideEmergencyOverlay();
        hideAbortConfirm();
        showDesktop();
        setShowView(DeskShowView::Idle);
        pushOperatorEvent("Returned to Desktop");
      } else {
        emergencyVisitingDiag = true;
        hideEmergencyOverlay();
        showDesktop();
        pushOperatorEvent("Desktop — banner active until E-Stop cleared");
      }
      return;
    }
    if (command == "UI:ESTOP:ACK") {
      emergencyAcknowledged = true;
      pushOperatorEvent("Operator Acknowledged");
      if (commandCallback) commandCallback("UI:ESTOP:ACK");
      refreshEmergencyOverlayContent();
      return;
    }
    if (command == "UI:COMPLETE:RUN") {
      hideCompleteOverlay();
      if (commandCallback) commandCallback("UI:SHOW:RUN");
      return;
    }
    if (command == "UI:COMPLETE:MENU") {
      hideCompleteOverlay();
      showDesktop();
      return;
    }
    if (command == "UI:COMPLETE:EXPORT") {
      pushOperatorEvent("Export Log — coming soon");
      return;
    }

    if (command == "SCREEN:DESKTOP") {
      notePage(DeskPage::Desktop);
      showDesktop();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:LIVE") {
      notePage(DeskPage::Live);
      showLive();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:SHOWS") {
      notePage(DeskPage::Shows);
      showShows();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:DIAG" || command == "SCREEN:NODES") {
      notePage(DeskPage::Nodes);
      showDiagnostics();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:SETTINGS") {
      notePage(DeskPage::Settings);
      showSettings();
      maybeRestoreEmergencyOverlay();
      return;
    }

    if (command == "UI:SHOW:BACK") {
      notePage(DeskPage::Shows);
      showShows();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command.startsWith("UI:SHOW:OPEN:")) {
      notePage(DeskPage::Details);
      openShowDetails(command.substring(strlen("UI:SHOW:OPEN:")).c_str());
      return;
    }

    /* Block desk commands while emergency overlay is up (except E-STOP / CLEAR). */
    if (emergencyOverlayVisible && command != "EMERGENCY:STOP" && command != "EMERGENCY:CLEAR") {
      return;
    }
    if (completeOverlayVisible &&
        command != "UI:COMPLETE:RUN" && command != "UI:COMPLETE:MENU" &&
        command != "UI:COMPLETE:EXPORT" && command != "EMERGENCY:STOP") {
      return;
    }

    String outbound = command;

    /* Absolute relay request from channel tap — never send TOGGLE */
    if (command.startsWith("UI:RELAY:")) {
      uint8_t ch = (uint8_t)command.substring(9).toInt();
      if (ch < 1 || ch > 8) return;
      DeskRelayView v = relayView[ch - 1];
      if (v == DeskRelayView::PendingOn || v == DeskRelayView::PendingOff) {
        appendLog(String("R") + ch + " busy (pending)");
        return;
      }
      if (v != DeskRelayView::ConfirmedOn && v != DeskRelayView::ConfirmedOff) {
        appendLog(String("R") + ch + " state unknown — wait for sync");
        return;
      }
      bool wantOn = (v == DeskRelayView::ConfirmedOff);
      lastConfirmed[ch - 1] = v;
      relayView[ch - 1] = wantOn ? DeskRelayView::PendingOn : DeskRelayView::PendingOff;
      refreshRelayButton(ch - 1);
      outbound = String("RELAY:") + ch + (wantOn ? ":ON" : ":OFF");
    } else if (command == "RELAY:ALL:OFF" || command == "STOP:ALL" || command == "SHOW:STOP" ||
               command == "UI:SHOW:STOP") {
      for (uint8_t i = 0; i < 8; i++) {
        if (relayView[i] == DeskRelayView::ConfirmedOn ||
            relayView[i] == DeskRelayView::ConfirmedOff) {
          lastConfirmed[i] = relayView[i];
        }
        relayView[i] = DeskRelayView::PendingOff;
        refreshRelayButton(i);
      }
      if (command == "UI:SHOW:STOP") outbound = "SHOW:STOP";
    } else if (command == "UI:SHOW:LOAD") {
      outbound = "UI:SHOW:LOAD";
    } else if (command == "UI:SHOW:RUN") {
      outbound = "UI:SHOW:RUN";
    } else if (command == "UI:SHOW:PAUSE" || command == "SHOW:PAUSE") {
      outbound = "SHOW:PAUSE";
    } else if (command == "UI:SHOW:RESUME" || command == "SHOW:RESUME") {
      outbound = "SHOW:RESUME";
    } else if (command == "UI:SHOW:REFRESH") {
      outbound = "UI:SHOW:REFRESH";
    } else if (command == "EMERGENCY:STOP") {
      emergencyActivating = true;
      pageBeforeEmergency = currentPage;
      for (uint8_t i = 0; i < 8; i++) {
        relayView[i] = DeskRelayView::PendingOff;
        refreshRelayButton(i);
      }
      /* Overlay + timeline pause applied in handleUiCommand / Stage STATE path. */
    } else if (command == "EMERGENCY:CLEAR") {
      /* Do not clear lock until STATE:EMERGENCY:CLEAR */
      appendLog("E-CLEAR requested…");
    }

    statusDirty = true;
    updateStatusWidgets(true);
    if (commandCallback != nullptr) commandCallback(outbound);
  }

  void refreshRelayButton(uint8_t idx) {
    if (idx >= 8 || relayButtons[idx] == nullptr) return;
    DeskRelayView v = relayView[idx];
    uint32_t bg = 0x3F3F46;
    uint32_t border = 0x71717A;
    switch (v) {
      case DeskRelayView::ConfirmedOn:
        bg = 0xB91C1C; border = 0xEF4444; break;
      case DeskRelayView::ConfirmedOff:
        bg = 0x3F3F46; border = 0x71717A; break;
      case DeskRelayView::PendingOn:
      case DeskRelayView::PendingOff:
        bg = 0x1E3A5F; border = 0x60A5FA; break;
      case DeskRelayView::Fault:
        bg = 0x7C2D12; border = 0xF59E0B; break;
      case DeskRelayView::Unknown:
      default:
        bg = 0x27272A; border = 0x52525B; break;
    }
    lv_obj_set_style_bg_color(relayButtons[idx], lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(relayButtons[idx], lv_color_hex(border), 0);
  }

  void initTheme() {
    os_.begin();
    /* Legacy style aliases — kept so existing overlay code continues to compile. */
    styleScreen = os_.screen;
    stylePanel = os_.panel;
    styleButton = os_.button;
    styleDangerButton = os_.buttonDanger;
    styleTitle = os_.title;
    styleSmall = os_.caption;
  }

  lv_obj_t *makeScreen() { return os_.makeScreen(); }

  lv_obj_t *makePanel(lv_obj_t *parent, int x, int y, int w, int h) {
    return os_.makePanel(parent, x, y, w, h);
  }

  lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y) {
    return os_.makeLabel(parent, text, x, y);
  }

  lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                       const char *command, bool danger = false) {
    return os_.makeButton(parent, text, x, y, w, h, staticEventHandler, this, command, danger);
  }

  void createTopBar(lv_obj_t *screen, const char *title) {
    os_.makePageChrome(screen, title);
  }

  void createDock(lv_obj_t *screen) {
    os_.makeDock(screen, staticEventHandler, this);
  }
  void createSystemSummary(lv_obj_t *parent) {
    os_.makeHeading(parent, "SYSTEM SUMMARY", 10, 4);

    os_.makeCaption(parent, "Current Show", 10, 28);
    sumShowValue_ = makeLabel(parent, "No Show Loaded", 10, 46);
    lv_obj_add_style(sumShowValue_, &os_.title, 0);
    lv_obj_set_width(sumShowValue_, 430);
    lv_label_set_long_mode(sumShowValue_, LV_LABEL_LONG_CLIP);

    os_.makeCaption(parent, "Runtime", 10, 74);
    sumRuntimeValue_ = makeLabel(parent, "IDLE", 10, 92);
    lv_obj_add_style(sumRuntimeValue_, &os_.title, 0);

    os_.makeCaption(parent, "Safety", 240, 74);
    sumSafetyValue_ = makeLabel(parent, "CLEAR", 240, 92);
    lv_obj_add_style(sumSafetyValue_, &os_.title, 0);
    lv_obj_set_style_text_color(sumSafetyValue_, lv_color_hex(OsColor::Ok), 0);

    os_.makeCaption(parent, "Uptime", 10, 124);
    sumUptimeValue_ = makeLabel(parent, "00:00:00", 10, 142);
    lv_obj_add_style(sumUptimeValue_, &os_.caption, 0);

    os_.makeCaption(parent, "Traffic", 240, 124);
    sumTrafficValue_ = makeLabel(parent, "TX 0 / RX 0", 240, 142);
    lv_obj_add_style(sumTrafficValue_, &os_.caption, 0);
  }

  void createQuickActions(lv_obj_t *parent) {
    os_.makeHeading(parent, "QUICK ACTIONS", 10, 4);
    const int bw = 210;
    const int bh = OS_DOCK_H;
    const int gap = 12;
    const int x0 = 14;
    const int y0 = 36;
    makeButton(parent, "Live Control", x0, y0, bw, bh, "SCREEN:LIVE");
    makeButton(parent, "Show Library", x0 + bw + gap, y0, bw, bh, "SCREEN:SHOWS");
    makeButton(parent, "Node Manager", x0, y0 + bh + gap, bw, bh, "SCREEN:NODES");
    makeButton(parent, "Settings", x0 + bw + gap, y0 + bh + gap, bw, bh, "SCREEN:SETTINGS");
  }

  void uiBuildPump(const char *step = nullptr) {
    if (step != nullptr) {
      Serial.println(step);
      Serial.flush();
    }
    yield();
    lv_timer_handler();
  }

  void createSharedOperatorLog() {
    if (operatorLogRoot != nullptr) return;
    lv_obj_t *layer = lv_layer_top();
    operatorLogRoot = makePanel(layer, OS_CONTENT_RIGHT_X, OS_BODY_Y, OS_CONTENT_RIGHT_W, OS_BODY_H);
    lv_obj_set_style_pad_all(operatorLogRoot, 6, 0);
    os_.makeHeading(operatorLogRoot, "EVENT LOG", 6, 2);
    operatorLogScroll = lv_obj_create(operatorLogRoot);
    lv_obj_remove_style_all(operatorLogScroll);
    lv_obj_set_pos(operatorLogScroll, 2, 26);
    lv_obj_set_size(operatorLogScroll, OS_CONTENT_RIGHT_W - 12, OS_BODY_H - 34);
    lv_obj_set_style_bg_opa(operatorLogScroll, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(operatorLogScroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(operatorLogScroll, LV_SCROLLBAR_MODE_AUTO);
    operatorLogLabel = lv_label_create(operatorLogScroll);
    lv_obj_set_pos(operatorLogLabel, 2, 0);
    lv_obj_set_width(operatorLogLabel, OS_CONTENT_RIGHT_W - 20);
    lv_obj_add_style(operatorLogLabel, &os_.body, 0);
    lv_label_set_long_mode(operatorLogLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(operatorLogLabel, "Showduino ready.\n");
  }

  void createLogPanel(lv_obj_t *screen) { (void)screen; }

  void buildScreens() {
    Serial.printf("[UI] heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    createSharedOperatorLog();
    uiBuildPump();

    /* ---- DESKTOP (design reference) ---- */
    Serial.println("[UI] desktop…");
    desktopScreen = makeScreen();
    uiBuildPump("[UI] desktop");
    createDock(desktopScreen);
    lv_obj_t *summary = makePanel(desktopScreen, OS_MARGIN, OS_BODY_Y, OS_CONTENT_LEFT_W, OS_DESK_SUMMARY_H);
    createSystemSummary(summary);
    lv_obj_t *actions = makePanel(desktopScreen, OS_MARGIN, OS_DESK_ACTIONS_Y, OS_CONTENT_LEFT_W, OS_DESK_ACTIONS_H);
    createQuickActions(actions);
    uiBuildPump();

    /* ---- LIVE — What is happening right now? ---- */
    Serial.println("[UI] live…");
    liveScreen = makeScreen();
    uiBuildPump("[UI] live");
    createDock(liveScreen);
    lv_obj_t *liveSum = os_.makePageChrome(liveScreen, "LIVE");
    os_.makeCaption(liveSum, "Cue", 10, 8);
    liveCueLabel_ = makeLabel(liveSum, "0 / 0", 10, 28);
    lv_obj_add_style(liveCueLabel_, &os_.title, 0);
    os_.makeCaption(liveSum, "Elapsed", 160, 8);
    liveElapsedLabel_ = makeLabel(liveSum, "0:00", 160, 28);
    lv_obj_add_style(liveElapsedLabel_, &os_.body, 0);
    os_.makeCaption(liveSum, "Remaining", 300, 8);
    liveRemainLabel_ = makeLabel(liveSum, "0:00", 300, 28);
    lv_obj_add_style(liveRemainLabel_, &os_.body, 0);
    /* Reserved icon/scene slots — no placeholder text */

    lv_obj_t *live = os_.makePrimaryPanel(liveScreen);
    os_.makeHeading(live, "TRANSPORT", 8, 2);
    makeButton(live, "Start", 10, 24, 84, 40, "SHOW:START");
    makeButton(live, "Pause", 102, 24, 76, 40, "SHOW:PAUSE");
    makeButton(live, "Resume", 186, 24, 84, 40, "SHOW:RESUME");
    makeButton(live, "Stop", 278, 24, 68, 40, "SHOW:STOP", true);
    makeButton(live, "Status", 354, 24, 76, 40, "STATUS:REQUEST");

    liveEmergencyDot = lv_obj_create(live);
    lv_obj_remove_style_all(liveEmergencyDot);
    lv_obj_set_size(liveEmergencyDot, 12, 12);
    lv_obj_set_pos(liveEmergencyDot, 442, 6);
    lv_obj_set_style_radius(liveEmergencyDot, 6, 0);
    lv_obj_set_style_bg_color(liveEmergencyDot, lv_color_hex(OsColor::Unknown), 0);
    lv_obj_set_style_bg_opa(liveEmergencyDot, LV_OPA_COVER, 0);

    liveStatusLabel = makeLabel(live, "0%", 10, 70);
    lv_obj_set_width(liveStatusLabel, 60);
    lv_obj_add_style(liveStatusLabel, &os_.body, 0);

    liveProgressBar = lv_bar_create(live);
    lv_obj_set_pos(liveProgressBar, 70, 74);
    lv_obj_set_size(liveProgressBar, 380, 10);
    lv_bar_set_range(liveProgressBar, 0, 100);
    lv_bar_set_value(liveProgressBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(liveProgressBar, lv_color_hex(0x27272A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(liveProgressBar, lv_color_hex(OsColor::DangerBorder), LV_PART_INDICATOR);

    timelineStatusLabel = makeLabel(live, "", 10, 88);
    lv_obj_add_flag(timelineStatusLabel, LV_OBJ_FLAG_HIDDEN);

    os_.makeHeading(live, "RELAYS", 8, 96);
    for (uint8_t i = 0; i < 8; i++) {
      int row = i / 4;
      int col = i % 4;
      int x = 10 + col * 112;
      int y = 118 + row * 40;
      static const char *relayNames[8] = { "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8" };
      relayButtons[i] = makeButton(live, relayNames[i], x, y, 104, 36, kRelayCmds[i]);
      refreshRelayButton(i);
    }
    makeButton(live, "All Off", 10, 202, 100, 36, "RELAY:ALL:OFF");
    makeButton(live, "Pulse R1", 118, 202, 100, 36, "RELAY:1:PULSE:1000");
    makeButton(live, "E-Clear", 226, 202, 100, 36, "EMERGENCY:CLEAR");
    uiBuildPump();

    /* ---- SHOWS — What can I run? ---- */
    Serial.println("[UI] shows…");
    showsScreen = makeScreen();
    uiBuildPump("[UI] shows");
    createDock(showsScreen);
    lv_obj_t *showSum = os_.makePageChrome(showsScreen, "SHOWS");
    os_.makeCaption(showSum, "Library", 10, 8);
    showsSummaryLabel_ = makeLabel(showSum, "Scan SD to list packages", 10, 28);
    lv_obj_add_style(showsSummaryLabel_, &os_.body, 0);
    lv_obj_set_width(showsSummaryLabel_, 440);

    showsListPanel = os_.makePrimaryPanel(showsScreen);
    os_.makeHeading(showsListPanel, "SHOW LIBRARY", 8, 2);
    makeButton(showsListPanel, "Resync", 360, 2, 90, 36, "UI:SHOW:REFRESH");
    showsListTitle = makeLabel(showsListPanel, "", 8, 4);
    lv_obj_add_flag(showsListTitle, LV_OBJ_FLAG_HIDDEN);
    showListScroll = lv_obj_create(showsListPanel);
    lv_obj_remove_style_all(showListScroll);
    lv_obj_set_pos(showListScroll, 4, 42);
    lv_obj_set_size(showListScroll, OS_CONTENT_LEFT_W - 20, OS_PRIMARY_H - 52);
    lv_obj_set_style_bg_opa(showListScroll, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(showListScroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(showListScroll, LV_SCROLLBAR_MODE_AUTO);
    makeLabel(showListScroll, "No shows yet — insert SD and tap Resync", 8, 8);
    uiBuildPump();

    /* ---- SHOW DETAILS ---- */
    Serial.println("[UI] details…");
    showDetailsScreen = makeScreen();
    uiBuildPump("[UI] details");
    createDock(showDetailsScreen);
    lv_obj_t *detSum = os_.makePageChrome(showDetailsScreen, "SHOW DETAILS");
    detailsNameLabel = makeLabel(detSum, "Show", 10, 8);
    lv_obj_add_style(detailsNameLabel, &os_.title, 0);
    lv_obj_set_width(detailsNameLabel, 440);

    lv_obj_t *det = os_.makePrimaryPanel(showDetailsScreen);
    detailsIconHost = lv_obj_create(det);
    lv_obj_remove_style_all(detailsIconHost);
    lv_obj_set_pos(detailsIconHost, 8, 8);
    lv_obj_set_size(detailsIconHost, 96, 64);
    ShowduinoShowThumb::makeDefaultIcon(detailsIconHost, 0, 0, 96, 64);
    detailsDescLabel = makeLabel(det, "Description", 116, 12);
    lv_obj_set_width(detailsDescLabel, 330);
    lv_obj_add_style(detailsDescLabel, &os_.body, 0);
    lv_label_set_long_mode(detailsDescLabel, LV_LABEL_LONG_WRAP);
    detailsMetaLabel = makeLabel(det, "Duration / Version / Author", 8, 84);
    lv_obj_set_width(detailsMetaLabel, 440);
    lv_obj_add_style(detailsMetaLabel, &os_.caption, 0);
    lv_label_set_long_mode(detailsMetaLabel, LV_LABEL_LONG_WRAP);
    timelineDetailLabel = makeLabel(det, "Playback: STOPPED", 8, 110);
    lv_obj_set_width(timelineDetailLabel, 440);
    lv_obj_add_style(timelineDetailLabel, &os_.body, 0);
    makeButton(det, "Load", 8, 155, 72, 44, "UI:SHOW:LOAD");
    makeButton(det, "Run", 88, 155, 72, 44, "UI:SHOW:RUN");
    makeButton(det, "Pause", 168, 155, 72, 44, "SHOW:PAUSE");
    makeButton(det, "Resume", 248, 155, 80, 44, "SHOW:RESUME");
    makeButton(det, "Stop", 336, 155, 64, 44, "UI:SHOW:STOP");
    makeButton(det, "Back", 408, 155, 50, 44, "UI:SHOW:BACK");
    uiBuildPump();

    /* ---- NODES (Quick Actions / future nav) ---- */
    Serial.println("[UI] nodes…");
    diagnosticsScreen = makeScreen();
    uiBuildPump("[UI] nodes");
    createDock(diagnosticsScreen);
    lv_obj_t *nodeSum = os_.makePageChrome(diagnosticsScreen, "NODES");
    os_.makeCaption(nodeSum, "Fabric", 10, 8);
    makeLabel(nodeSum, "Director → C3 → Stage → Nodes", 10, 28);
    lv_obj_t *nodes = os_.makePrimaryPanel(diagnosticsScreen);
    os_.makeHeading(nodes, "TOOLS", 8, 2);
    makeButton(nodes, "SD Status", 12, 36, 140, 48, "STORAGE:STATUS");
    makeButton(nodes, "Backup", 160, 36, 140, 48, "STORAGE:BACKUP");
    makeButton(nodes, "Export", 308, 36, 140, 48, "STORAGE:EXPORT");
    makeButton(nodes, "Repair Dirs", 12, 96, 140, 48, "STORAGE:REPAIR");
    makeButton(nodes, "Stage Status", 160, 96, 140, 48, "STATUS:REQUEST");
    makeButton(nodes, "Self Test", 308, 96, 140, 48, "SELFTEST:START");
    makeButton(nodes, "Stage Hello", 12, 156, 140, 48, "HELLO");
    uiBuildPump();

    /* ---- SETTINGS — How is the system configured? ---- */
    Serial.println("[UI] settings…");
    settingsScreen = makeScreen();
    uiBuildPump("[UI] settings");
    createDock(settingsScreen);
    lv_obj_t *setSum = os_.makePageChrome(settingsScreen, "SETTINGS");
    os_.makeCaption(setSum, "Display", 10, 8);
    timeoutLabel = makeLabel(setSum, "Auto backlight: 10 min", 10, 28);
    lv_obj_add_style(timeoutLabel, &os_.body, 0);

    lv_obj_t *settings = os_.makePrimaryPanel(settingsScreen);
    os_.makeHeading(settings, "DISPLAY", 8, 2);
    makeButton(settings, "Never", 8, 32, 70, 40, "SETTINGS:TIMEOUT:0");
    makeButton(settings, "1m", 86, 32, 54, 40, "SETTINGS:TIMEOUT:1");
    makeButton(settings, "3m", 148, 32, 54, 40, "SETTINGS:TIMEOUT:3");
    makeButton(settings, "5m", 210, 32, 54, 40, "SETTINGS:TIMEOUT:5");
    makeButton(settings, "10m", 272, 32, 62, 40, "SETTINGS:TIMEOUT:10");
    makeButton(settings, "30m", 342, 32, 62, 40, "SETTINGS:TIMEOUT:30");
    makeButton(settings, "Cycle", 412, 32, 48, 40, "SETTINGS:TIMEOUT:CYCLE");
    os_.makeCaption(settings, "Dim at half timeout, then off. Touch wakes.", 8, 80);

    os_.makeHeading(settings, "SYSTEM", 8, 108);
    makeButton(settings, "Clear E-Stop", 8, 136, 150, 48, "EMERGENCY:CLEAR");
    makeButton(settings, "Create Backup", 168, 136, 150, 48, "STORAGE:BACKUP");
    makeButton(settings, "Export", 328, 136, 120, 48, "STORAGE:EXPORT");
    makeButton(settings, "Unmount SD", 8, 196, 150, 48, "STORAGE:UNMOUNT");
    makeButton(settings, "About", 168, 196, 120, 48, "SETTINGS:ABOUT");
    /* Reserved: Network / Time / Audio sections */
    refreshTimeoutLabel();
    uiBuildPump();

    Serial.println("[UI] overlays…");
    buildPersistentBanner();
    uiBuildPump();
    buildEmergencyOverlay();
    uiBuildPump();
    buildAbortConfirm();
    uiBuildPump();
    buildCompleteOverlay();
    if (statusBar_.root()) lv_obj_move_foreground(statusBar_.root());
    pushOperatorEvent("Showduino ready");
    Serial.printf("[UI] screens built heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  }
  void notePage(DeskPage p) { currentPage = p; }

  void captureEmergencySnapshot() {
    estopOccurredMs = millis();
    if (loadedShowNameBuf[0]) {
      strncpy(estopShowName, loadedShowNameBuf, sizeof(estopShowName) - 1);
      estopShowName[sizeof(estopShowName) - 1] = '\0';
    }
  }

  void buildPersistentBanner() {
    if (persistentBannerRoot) return;
    lv_obj_t *top = lv_layer_top();
    persistentBannerRoot = lv_obj_create(top);
    lv_obj_remove_style_all(persistentBannerRoot);
    lv_obj_set_size(persistentBannerRoot, SCREEN_WIDTH, 36);
    lv_obj_set_pos(persistentBannerRoot, 0, SHOWDUINO_EMERGENCY_BANNER_Y);
    lv_obj_set_style_bg_color(persistentBannerRoot, lv_color_hex(0x991B1B), 0);
    lv_obj_set_style_bg_opa(persistentBannerRoot, LV_OPA_COVER, 0);
    lv_obj_add_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(persistentBannerRoot, LV_OBJ_FLAG_CLICKABLE);
    persistentBannerLabel = lv_label_create(persistentBannerRoot);
    lv_label_set_text(persistentBannerLabel, "EMERGENCY STOP ACTIVE");
    lv_obj_set_style_text_color(persistentBannerLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(persistentBannerLabel);
  }

  void updatePersistentBanner() {
    if (!persistentBannerRoot) buildPersistentBanner();
    /* Banner follows Stage runtime state — hides when state leaves EMERGENCY_STOP. */
    const bool show = (mirroredState == SHOW_STATE_EMERGENCY_STOP);
    if (show) {
      char et[16];
      formatClock(liveElapsedMs ? liveElapsedMs : estopElapsedMs, et, sizeof(et));
      char line[160];
      snprintf(line, sizeof(line), "EMERGENCY STOP ACTIVE  |  %s  |  %s  |  %s",
               estopShowName[0] ? estopShowName : (loadedShowNameBuf[0] ? loadedShowNameBuf : "-"),
               liveStateName[0] ? liveStateName : "EMERGENCY_STOP",
               et);
      if (persistentBannerLabel) lv_label_set_text(persistentBannerLabel, line);
      lv_obj_clear_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(persistentBannerRoot);
      if (statusBar_.root()) lv_obj_move_foreground(statusBar_.root());
      if (emergencyOverlayRoot && emergencyOverlayVisible) {
        lv_obj_move_foreground(emergencyOverlayRoot);
      }
      if (abortConfirmRoot && !lv_obj_has_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(abortConfirmRoot);
      }
    } else {
      lv_obj_add_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void buildEmergencyOverlay() {
    if (emergencyOverlayBuilt) return;
    lv_obj_t *top = lv_layer_top();
    emergencyOverlayRoot = lv_obj_create(top);
    lv_obj_remove_style_all(emergencyOverlayRoot);
    lv_obj_set_size(emergencyOverlayRoot, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(emergencyOverlayRoot, lv_color_hex(0x450A0A), 0);
    lv_obj_set_style_bg_opa(emergencyOverlayRoot, LV_OPA_COVER, 0);
    lv_obj_add_flag(emergencyOverlayRoot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(emergencyOverlayRoot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(emergencyOverlayRoot, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *banner = lv_obj_create(emergencyOverlayRoot);
    lv_obj_remove_style_all(banner);
    lv_obj_set_size(banner, SCREEN_WIDTH, 48);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0x7F1D1D), 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    estopBannerLabel = lv_label_create(banner);
    lv_label_set_text(estopBannerLabel, "EMERGENCY STOP ACTIVE");
    lv_obj_set_style_text_color(estopBannerLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(estopBannerLabel);

    estopWarnIcon = lv_obj_create(emergencyOverlayRoot);
    lv_obj_remove_style_all(estopWarnIcon);
    lv_obj_set_size(estopWarnIcon, 88, 88);
    lv_obj_set_pos(estopWarnIcon, (SCREEN_WIDTH - 88) / 2, 58);
    lv_obj_set_style_bg_color(estopWarnIcon, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_bg_opa(estopWarnIcon, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(estopWarnIcon, 12, 0);
    lv_obj_t *bang = lv_label_create(estopWarnIcon);
    lv_label_set_text(bang, "!");
    lv_obj_set_style_text_color(bang, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(bang);

    estopTitleLabel = lv_label_create(emergencyOverlayRoot);
    lv_label_set_text(estopTitleLabel, "EMERGENCY STOP ACTIVE");
    lv_obj_set_style_text_color(estopTitleLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_style(estopTitleLabel, &styleTitle, 0);
    lv_obj_set_pos(estopTitleLabel, 0, 156);
    lv_obj_set_width(estopTitleLabel, SCREEN_WIDTH);
    lv_obj_set_style_text_align(estopTitleLabel, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *sub = lv_label_create(emergencyOverlayRoot);
    lv_label_set_text(sub, "Show halted. Press CLEAR E-STOP, then Resume or Abort.");
    lv_obj_set_style_text_color(sub, lv_color_hex(0xFECACA), 0);
    lv_obj_set_pos(sub, 40, 188);
    lv_obj_set_width(sub, SCREEN_WIDTH - 80);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);

    estopDetailLabel = lv_label_create(emergencyOverlayRoot);
    lv_label_set_text(estopDetailLabel, "Show: -");
    lv_obj_set_style_text_color(estopDetailLabel, lv_color_hex(0xFFE4E6), 0);
    lv_obj_set_pos(estopDetailLabel, 60, 240);
    lv_obj_set_width(estopDetailLabel, SCREEN_WIDTH - 120);

    estopTimerLabel = lv_label_create(emergencyOverlayRoot);
    lv_label_set_text(estopTimerLabel, "Emergency active: 0:00");
    lv_obj_set_style_text_color(estopTimerLabel, lv_color_hex(0xFCA5A5), 0);
    lv_obj_set_pos(estopTimerLabel, 0, 360);
    lv_obj_set_width(estopTimerLabel, SCREEN_WIDTH);
    lv_obj_set_style_text_align(estopTimerLabel, LV_TEXT_ALIGN_CENTER, 0);

    /* CLEAR must be on the overlay — Live/Settings E-CLEAR sits under this layer. */
    makeButton(emergencyOverlayRoot, "CLEAR E-STOP", 20, 400, 150, 56, "EMERGENCY:CLEAR");
    estopResumeBtn = makeButton(emergencyOverlayRoot, "RESUME", 180, 400, 110, 56, "UI:ESTOP:RESUME");
    makeButton(emergencyOverlayRoot, "ABORT", 300, 400, 100, 56, "UI:ESTOP:ABORT", true);
    makeButton(emergencyOverlayRoot, "NODES", 410, 400, 90, 56, "UI:ESTOP:DIAG");
    makeButton(emergencyOverlayRoot, "ACK", 510, 400, 90, 56, "UI:ESTOP:ACK");
    makeButton(emergencyOverlayRoot, "DESK", 610, 400, 90, 56, "UI:ESTOP:DESK");

    emergencyOverlayBuilt = true;
    updateEmergencyResumeButton();
  }

  void buildAbortConfirm() {
    if (abortConfirmRoot) return;
    lv_obj_t *top = lv_layer_top();
    abortConfirmRoot = lv_obj_create(top);
    lv_obj_remove_style_all(abortConfirmRoot);
    lv_obj_set_size(abortConfirmRoot, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(abortConfirmRoot, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(abortConfirmRoot, LV_OPA_70, 0);
    lv_obj_add_flag(abortConfirmRoot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *box = lv_obj_create(abortConfirmRoot);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 420, 200);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1C1917), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 10, 0);

    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, "ABORT SHOW?");
    lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(t, 24, 24);

    lv_obj_t *m = lv_label_create(box);
    lv_label_set_text(m, "Stop playback and return to Desktop.\nShow package remains loaded on Stage.");
    lv_obj_set_style_text_color(m, lv_color_hex(0xD1D5DB), 0);
    lv_obj_set_pos(m, 24, 60);
    lv_obj_set_width(m, 370);

    makeButton(box, "CONFIRM ABORT", 24, 130, 180, 48, "UI:ESTOP:ABORT:YES", true);
    makeButton(box, "CANCEL", 220, 130, 160, 48, "UI:ESTOP:ABORT:NO");
  }

  void showAbortConfirm() {
    if (!abortConfirmRoot) buildAbortConfirm();
    lv_obj_clear_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(abortConfirmRoot);
  }

  void hideAbortConfirm() {
    if (abortConfirmRoot) lv_obj_add_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN);
  }

  void buildCompleteOverlay() {
    if (completeOverlayRoot) return;
    lv_obj_t *top = lv_layer_top();
    completeOverlayRoot = lv_obj_create(top);
    lv_obj_remove_style_all(completeOverlayRoot);
    lv_obj_set_size(completeOverlayRoot, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(completeOverlayRoot, lv_color_hex(0x052e16), 0);
    lv_obj_set_style_bg_opa(completeOverlayRoot, LV_OPA_COVER, 0);
    lv_obj_add_flag(completeOverlayRoot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(completeOverlayRoot, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(completeOverlayRoot);
    lv_label_set_text(title, "SHOW COMPLETE");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_style(title, &styleTitle, 0);
    lv_obj_set_pos(title, 0, 80);
    lv_obj_set_width(title, SCREEN_WIDTH);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    completeDetailLabel = lv_label_create(completeOverlayRoot);
    lv_label_set_text(completeDetailLabel, "Show: -");
    lv_obj_set_style_text_color(completeDetailLabel, lv_color_hex(0xBBF7D0), 0);
    lv_obj_set_pos(completeDetailLabel, 80, 140);
    lv_obj_set_width(completeDetailLabel, SCREEN_WIDTH - 160);

    makeButton(completeOverlayRoot, "RUN AGAIN", 120, 360, 160, 56, "UI:COMPLETE:RUN");
    makeButton(completeOverlayRoot, "RETURN TO MENU", 300, 360, 180, 56, "UI:COMPLETE:MENU");
    makeButton(completeOverlayRoot, "EXPORT LOG", 500, 360, 160, 56, "UI:COMPLETE:EXPORT");
  }

  void showCompleteScreen(const ShowRuntime &rt) {
    if (!completeOverlayRoot) buildCompleteOverlay();
    if (emergencyOverlayVisible) return; /* emergency wins */
    char et[16], done[16];
    formatClock(rt.elapsedMs ? rt.elapsedMs : rt.totalDurationMs, et, sizeof(et));
    formatClock(millis() - bootMs, done, sizeof(done));
    char detail[320];
    snprintf(detail, sizeof(detail),
             "Show: %s\nTotal runtime: %s\nCues executed: %lu / %lu\nWarnings: %u\nErrors: %u\nEmergency count: %u\nCompletion time: T+%s",
             rt.showName[0] ? rt.showName : "-",
             et,
             (unsigned long)rt.currentCue,
             (unsigned long)rt.totalCues,
             (unsigned)sessionWarningCount,
             (unsigned)sessionErrorCount,
             (unsigned)sessionEmergencyCount,
             done);
    if (completeDetailLabel) lv_label_set_text(completeDetailLabel, detail);
    lv_obj_clear_flag(completeOverlayRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(completeOverlayRoot);
    completeOverlayVisible = true;
  }

  void hideCompleteOverlay() {
    if (completeOverlayRoot) lv_obj_add_flag(completeOverlayRoot, LV_OBJ_FLAG_HIDDEN);
    completeOverlayVisible = false;
  }

  void refreshLiveStatusPanel() {
    liveStatusDirty = false;
    char et[16], rt[16];
    formatClock(liveElapsedMs, et, sizeof(et));
    formatClock(liveRemainMs, rt, sizeof(rt));

    char cueBuf[24];
    snprintf(cueBuf, sizeof(cueBuf), "%lu / %lu",
             (unsigned long)liveCue, (unsigned long)liveCueTotal);
    ShowduinoOsTheme::setTextIfChanged(liveCueLabel_, cueBuf);
    ShowduinoOsTheme::setTextIfChanged(liveElapsedLabel_, et);
    ShowduinoOsTheme::setTextIfChanged(liveRemainLabel_, rt);

    if (liveStatusLabel) {
      char line[64];
      snprintf(line, sizeof(line), "%u%%", (unsigned)liveProgressPct);
      ShowduinoOsTheme::setTextIfChanged(liveStatusLabel, line);
    }
    if (liveProgressBar) {
      lv_bar_set_value(liveProgressBar, liveProgressPct, LV_ANIM_ON);
    }
    if (liveEmergencyDot) {
      bool em = (mirroredState == SHOW_STATE_EMERGENCY_STOP) || emergencyLocked;
      lv_obj_set_style_bg_color(liveEmergencyDot,
                                lv_color_hex(em ? OsColor::DangerBorder : OsColor::Unknown), 0);
    }
  }

  void showEmergencyOverlay() {
    if (!emergencyOverlayBuilt) buildEmergencyOverlay();
    if (!emergencyOverlayRoot || emergencyOverlayDismissed) return;
    emergencySessionOpen = true;
    refreshEmergencyOverlayContent();
    updateEmergencyResumeButton();
    lv_obj_clear_flag(emergencyOverlayRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(emergencyOverlayRoot);
    emergencyOverlayVisible = true;
    emergencyVisitingDiag = false;
    updatePersistentBanner();
  }

  void hideEmergencyOverlay() {
    if (emergencyOverlayRoot) {
      lv_obj_add_flag(emergencyOverlayRoot, LV_OBJ_FLAG_HIDDEN);
    }
    emergencyOverlayVisible = false;
  }

  void maybeRestoreEmergencyOverlay() {
    if (emergencyVisitingDiag) {
      emergencyVisitingDiag = false;
    }
    if (emergencySessionOpen && !emergencyOverlayDismissed) {
      showEmergencyOverlay();
    }
  }

  void restorePageAfterEmergency() {
    switch (pageBeforeEmergency) {
      case DeskPage::Live: showLive(); break;
      case DeskPage::Shows: showShows(); break;
      case DeskPage::Details: showShows(); break;
      case DeskPage::Nodes: showDiagnostics(); break;
      case DeskPage::Settings: showSettings(); break;
      default: showDesktop(); break;
    }
  }

  void updateEmergencyResumeButton() {
    if (!estopResumeBtn) return;
    const bool canResume = !emergencyLocked && !pendingResumeAwait;
    if (canResume) {
      lv_obj_clear_state(estopResumeBtn, LV_STATE_DISABLED);
      lv_obj_set_style_bg_opa(estopResumeBtn, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(estopResumeBtn, lv_color_hex(0x166534), 0);
    } else {
      lv_obj_add_state(estopResumeBtn, LV_STATE_DISABLED);
      lv_obj_set_style_bg_opa(estopResumeBtn, LV_OPA_50, 0);
      lv_obj_set_style_bg_color(estopResumeBtn, lv_color_hex(0x3F3F46), 0);
    }
  }

  void refreshEmergencyOverlayContent() {
    if (!estopDetailLabel) return;
    char clock[16], rem[16], occurred[16];
    formatClock(estopElapsedMs, clock, sizeof(clock));
    formatClock(estopRemainMs, rem, sizeof(rem));
    formatClock(estopOccurredMs > bootMs ? (estopOccurredMs - bootMs) : 0, occurred, sizeof(occurred));
    char detail[360];
    snprintf(detail, sizeof(detail),
             "Show: %s\nPlayback before stop: %s\nCurrent cue: %u / %u\nElapsed: %s    Remaining: %s\nStage link: %s\nEmergency occurred: T+%s%s%s%s",
             estopShowName,
             estopPlayStateBefore,
             (unsigned)estopCueIndex,
             (unsigned)estopCueTotal,
             clock, rem,
             estopStageConnected ? "CONNECTED" : "DISCONNECTED",
             occurred,
             emergencyAcknowledged ? "\nAcknowledged: YES" : "\nAcknowledged: NO",
             emergencyLocked ? "\nStage: LOCKED — press CLEAR E-STOP" : "\nStage: CLEARED — Resume or Abort",
             pendingResumeAwait ? "\nWaiting for Stage RESUME…" :
             (pendingAbortAwait ? "\nWaiting for Stage STOP…" : ""));
    lv_label_set_text(estopDetailLabel, detail);
  }

  void refreshTimeoutLabel() {
    if (timeoutLabel == nullptr) return;
    char buf[48];
    if (screenTimeoutMinutes == 0) {
      snprintf(buf, sizeof(buf), "Auto backlight: NEVER (always on)");
    } else {
      snprintf(buf, sizeof(buf), "Auto backlight: %u min (dim then off)",
               (unsigned)screenTimeoutMinutes);
    }
    lv_label_set_text(timeoutLabel, buf);
  }

  void clearShowListChildren() {
    if (showListScroll == nullptr) return;
    lv_obj_clean(showListScroll);
  }

  void rebuildShowList(const ShowManager &sm) {
    showListCount = 0;
    clearShowListChildren();
    if (showsSummaryLabel_) {
      char title[48];
      snprintf(title, sizeof(title), "%u package%s available",
               (unsigned)sm.size(), sm.size() == 1 ? "" : "s");
      ShowduinoOsTheme::setTextIfChanged(showsSummaryLabel_, title);
    }
    if (showsListTitle) {
      char title[40];
      snprintf(title, sizeof(title), "SHOWS ON SD (%u)", (unsigned)sm.size());
      lv_label_set_text(showsListTitle, title);
    }

    if (sm.size() == 0) {
      makeLabel(showListScroll, "No shows found under /showduino/shows/packages", 8, 8);
      return;
    }

    int y = 4;
    for (uint8_t i = 0; i < sm.size() && showListCount < SHOW_INDEX_MAX; i++) {
      const ShowIndexEntry *e = sm.get(i);
      if (!e) continue;
      showListCache[showListCount] = *e;
      snprintf(showOpenCmds[showListCount], sizeof(showOpenCmds[showListCount]),
               "UI:SHOW:OPEN:%s", e->id);

      lv_obj_t *row = lv_obj_create(showListScroll);
      lv_obj_remove_style_all(row);
      lv_obj_set_pos(row, 4, y);
      lv_obj_set_size(row, 448, 72);
      lv_obj_set_style_bg_color(row, lv_color_hex(OsColor::Button), 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(row, lv_color_hex(OsColor::PanelBorder), 0);
      lv_obj_set_style_border_width(row, 1, 0);
      lv_obj_set_style_radius(row, OS_BTN_RADIUS, 0);

      ShowduinoShowThumb::makeDefaultIcon(row, 6, 6, 56, 56);
      if (e->hasThumbnail) {
        lv_obj_t *badge = lv_label_create(row);
        lv_label_set_text(badge, "BMP");
        lv_obj_set_style_text_color(badge, lv_color_hex(0x4ADE80), 0);
        lv_obj_set_pos(badge, 14, 48);
      }

      char dur[16];
      ShowduinoShowThumb::formatDuration(e->durationSeconds, dur, sizeof(dur));
      char line1[96];
      snprintf(line1, sizeof(line1), "%s", e->name);
      lv_obj_t *n = lv_label_create(row);
      lv_label_set_text(n, line1);
      lv_obj_set_style_text_color(n, lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_pos(n, 72, 6);

      char line2[128];
      snprintf(line2, sizeof(line2), "%s  ·  v%s  ·  %s",
               dur, e->version[0] ? e->version : "-", e->author[0] ? e->author : "-");
      lv_obj_t *m = lv_label_create(row);
      lv_label_set_text(m, line2);
      lv_obj_set_style_text_color(m, lv_color_hex(0xA1A1AA), 0);
      lv_obj_set_pos(m, 72, 28);

      char line3[96];
      if (e->description[0]) {
        snprintf(line3, sizeof(line3), "%.70s", e->description);
      } else {
        snprintf(line3, sizeof(line3), "(no description)");
      }
      lv_obj_t *d = lv_label_create(row);
      lv_label_set_text(d, line3);
      lv_obj_set_style_text_color(d, lv_color_hex(0xD4D4D8), 0);
      lv_obj_set_pos(d, 72, 48);

      lv_obj_t *hit = lv_button_create(row);
      lv_obj_remove_style_all(hit);
      lv_obj_set_size(hit, 448, 72);
      lv_obj_set_pos(hit, 0, 0);
      lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
      lv_obj_add_event_cb(hit, staticEventHandler, LV_EVENT_CLICKED, this);
      lv_obj_set_user_data(hit, (void *)showOpenCmds[showListCount]);

      showListCount++;
      y += 78;
      yield();
    }
  }

  const ShowIndexEntry *cachedShow(const char *id) const {
    if (!id) return nullptr;
    for (uint8_t i = 0; i < showListCount; i++) {
      if (strcmp(showListCache[i].id, id) == 0) return &showListCache[i];
    }
    return nullptr;
  }

  void openShowDetails(const char *showId) {
    if (!showId || !showId[0]) return;
    strncpy(selectedShowIdBuf, showId, sizeof(selectedShowIdBuf) - 1);
    selectedShowIdBuf[sizeof(selectedShowIdBuf) - 1] = '\0';

    const ShowIndexEntry *e = cachedShow(showId);
    const char *name = e ? e->name : showId;
    const char *desc = e && e->description[0] ? e->description : "(no description)";
    const char *author = e && e->author[0] ? e->author : "-";
    const char *version = e && e->version[0] ? e->version : "-";
    uint32_t durSec = e ? e->durationSeconds : 0;

    if (detailsNameLabel) lv_label_set_text(detailsNameLabel, name);
    if (detailsDescLabel) lv_label_set_text(detailsDescLabel, desc);

    char dur[16];
    ShowduinoShowThumb::formatDuration(durSec, dur, sizeof(dur));
    char meta[160];
    snprintf(meta, sizeof(meta), "Duration  %s\nVersion   %s\nAuthor    %s",
             dur, version, author);
    if (detailsMetaLabel) lv_label_set_text(detailsMetaLabel, meta);

    /* Rebuild icon host: default Showduino icon, replace with BMP when available. */
    if (detailsIconHost) {
      if (detailsCanvas) {
        ShowduinoShowThumb::freeCanvasBuffer(detailsCanvas);
        detailsCanvas = nullptr;
      }
      lv_obj_clean(detailsIconHost);
      bool showedBmp = false;
      if (e && e->hasThumbnail) {
        char thumb[STORAGE_MAX_PATH_LEN];
        snprintf(thumb, sizeof(thumb), "%s/thumbnail.bmp", e->folder);
        detailsCanvas = lv_canvas_create(detailsIconHost);
        lv_obj_set_pos(detailsCanvas, 0, 0);
        if (ShowduinoShowThumb::loadBmpToCanvas(detailsCanvas, thumb, 96, 64)) {
          showedBmp = true;
        } else {
          ShowduinoShowThumb::freeCanvasBuffer(detailsCanvas);
          lv_obj_delete(detailsCanvas);
          detailsCanvas = nullptr;
        }
      }
      if (!showedBmp) {
        ShowduinoShowThumb::makeDefaultIcon(detailsIconHost, 0, 0, 96, 64);
      }
    }

    lv_screen_load(showDetailsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }

  void showDesktop() {
    notePage(DeskPage::Desktop);
    lv_screen_load(desktopScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showLive() {
    notePage(DeskPage::Live);
    lv_screen_load(liveScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showShows() {
    notePage(DeskPage::Shows);
    lv_screen_load(showsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showDiagnostics() {
    notePage(DeskPage::Nodes);
    lv_screen_load(diagnosticsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showSettings() {
    notePage(DeskPage::Settings);
    lv_screen_load(settingsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
};

#endif
