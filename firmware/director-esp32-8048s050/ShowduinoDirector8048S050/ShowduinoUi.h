#ifndef SHOWDUINO_UI_H
#define SHOWDUINO_UI_H

#include <Arduino.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "backlight.h"
#include "src/ShowManager.h"
#include "src/StorageConfig.h"
#include "src/DeviceDatabase.h"
#include "src/ShowThumb.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_show_runtime.h"
#include "DirectorStatusBar.h"
#include "DirectorAudioModel.h"
#include "DirectorP4AudioModel.h"
#include "DirectorP4AudioView.h"
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

enum class ShowLoadUiState : uint8_t {
  Ready = 0,
  LoadRequested,
  Loading,
  Loaded,
  Warning,
  Failed,
  AwaitingStage,
  TimedOut
};

enum class NodeDiscoveryUiState : uint8_t {
  Idle = 0,
  Requested,
  Discovering,
  Found,
  Completed,
  TimedOut,
  Failed
};

enum class UiNodeRole : uint8_t {
  Director = 0,
  Sue,
  Ian,
  Children,
  Grandchildren,
  Saviour
};

enum class UiNodeHealth : uint8_t {
  Fault = 0,
  Offline,
  Warning,
  Degraded,
  Online,
  Discovering,
  Unknown
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
    Desktop = 0, Live, Shows, Details, More, Nodes, NodeDetails, Diagnostics, Settings, Maintenance, About, Audio, Logs
  };

  void begin(ShowduinoCommandCallback callback) {
    commandCallback = callback;
    ensureEventLogStorage();
    audioModel_.resetPlaceholders();
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
    if (state == LINK_READY) {
      lastNodeSignalMs_ = millis();
      if (nodeDiscoveryPending_) nodeDiscoveryState_ = NodeDiscoveryUiState::Discovering;
    } else if (state == LINK_DISCONNECTED && nodeDiscoveryPending_) {
      nodeDiscoveryPending_ = false;
      nodeDiscoveryState_ = NodeDiscoveryUiState::Failed;
      strncpy(nodeDiscoveryResult_, "Discovery failed — link disconnected",
              sizeof(nodeDiscoveryResult_) - 1);
      nodeDiscoveryResult_[sizeof(nodeDiscoveryResult_) - 1] = '\0';
    }
    nodeViewDirty_ = true;
    refreshNodesPresentationIfActive();
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
      hideActionConfirm();   /* Emergency must cancel pending destructive confirmations. */
      hideAbortConfirm();
      emergencyOverlayDismissed = false;
      emergencyAcknowledged = false;
      pendingClearAwait_ = false;
      pendingClearRequestMs_ = 0;
      emergencySessionOpen = true;
      emergencyActiveSinceMs = millis();
      emergencyActivating = false;
      emergencyRequestMs_ = 0;
      sessionEmergencyCount++;
      captureEmergencySnapshot();
      showEmergencyOverlay();
      pushOperatorEvent("Emergency Activated");
      emergencyAlarmOnHook();
    } else if (!locked && wasLocked) {
      emergencyActivating = false;
      emergencyRequestMs_ = 0;
      pendingClearAwait_ = false;
      pendingClearRequestMs_ = 0;
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
    nodeViewDirty_ = true;
    liveStatusDirty = true;
    syncStatusBarHealth();
  }

  void setNodeAvailability(ShowduinoNodeAvailWire wireState) {
    if (nodeAvailWire_ == wireState && nodeAvailKnown_) return;
    nodeAvailWire_ = wireState;
    nodeAvailKnown_ = true;
    if (wireState == SHOWDUINO_NODE_WIRE_FAULT) {
      strncpy(nodeDiscoveryResult_, "Node fault reported by Stage", sizeof(nodeDiscoveryResult_) - 1);
    } else if (wireState == SHOWDUINO_NODE_WIRE_OFFLINE) {
      strncpy(nodeDiscoveryResult_, "Node reported offline", sizeof(nodeDiscoveryResult_) - 1);
    } else if (wireState == SHOWDUINO_NODE_WIRE_ONLINE) {
      strncpy(nodeDiscoveryResult_, "Node availability updated", sizeof(nodeDiscoveryResult_) - 1);
    } else {
      strncpy(nodeDiscoveryResult_, "Node availability unknown", sizeof(nodeDiscoveryResult_) - 1);
    }
    nodeDiscoveryResult_[sizeof(nodeDiscoveryResult_) - 1] = '\0';
    lastNodeSignalMs_ = millis();
    if (nodeDiscoveryPending_) {
      nodeDiscoveryState_ = (wireState == SHOWDUINO_NODE_WIRE_ONLINE) ? NodeDiscoveryUiState::Found
                                                                       : NodeDiscoveryUiState::Completed;
      nodeDiscoveryPending_ = false;
    }
    nodeViewDirty_ = true;
    refreshNodesPresentationIfActive();
  }

  void syncPairedDeviceSnapshot(const DeviceDatabase &db) {
    uint8_t nextCount = db.size();
    if (nextCount > DEVICE_DB_MAX) nextCount = DEVICE_DB_MAX;
    bool changed = (pairedDeviceCount_ != nextCount);
    pairedDeviceCount_ = nextCount;
    for (uint8_t i = 0; i < pairedDeviceCount_; i++) {
      const DeviceRecord *src = db.get(i);
      if (!src) continue;
      if (memcmp(&pairedDeviceCache_[i], src, sizeof(DeviceRecord)) != 0) {
        changed = true;
      }
      pairedDeviceCache_[i] = *src;
    }
    if (changed) {
      nodeViewDirty_ = true;
      refreshNodesPresentationIfActive();
    }
  }

  void setStorageStatusSnapshot(const StorageStatus &st) {
    nodeStorageStatus_ = st;
    refreshDiagnosticsSummary();
  }

  void beginNodeDiscoveryRequest() {
    nodeDiscoveryPending_ = true;
    nodeDiscoveryRequestMs_ = millis();
    nodeDiscoveryState_ = NodeDiscoveryUiState::Requested;
    strncpy(nodeDiscoveryResult_, "Discovery requested — awaiting network response",
            sizeof(nodeDiscoveryResult_) - 1);
    nodeDiscoveryResult_[sizeof(nodeDiscoveryResult_) - 1] = '\0';
    nodeViewDirty_ = true;
    refreshNodesPresentationIfActive();
  }

  bool nodeDiscoveryPending() const { return nodeDiscoveryPending_; }

  void noteNodeCommandSent(const char *command) {
    if (command && command[0]) {
      strncpy(nodeLastCommand_, command, sizeof(nodeLastCommand_) - 1);
      nodeLastCommand_[sizeof(nodeLastCommand_) - 1] = '\0';
    }
    nodeViewDirty_ = true;
    refreshDiagnosticsSummary();
  }

  void noteNodeResponse(const char *response) {
    if (!response || !response[0]) return;
    strncpy(nodeLastResponse_, response, sizeof(nodeLastResponse_) - 1);
    nodeLastResponse_[sizeof(nodeLastResponse_) - 1] = '\0';
    lastNodeSignalMs_ = millis();
    if (nodeDiscoveryPending_) {
      nodeDiscoveryState_ = NodeDiscoveryUiState::Completed;
      nodeDiscoveryPending_ = false;
      strncpy(nodeDiscoveryResult_, "Discovery completed — network responded",
              sizeof(nodeDiscoveryResult_) - 1);
      nodeDiscoveryResult_[sizeof(nodeDiscoveryResult_) - 1] = '\0';
    }
    refreshDiagnosticsSummary();
    refreshNodesPresentationIfActive();
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
    lastRuntimeMirrorMs_ = millis();
    strncpy(liveStateName, showStateName(rt.state), sizeof(liveStateName) - 1);
    liveStateName[sizeof(liveStateName) - 1] = '\0';
    liveStatusDirty = true;
    statusDirty = true;
    nodeViewDirty_ = true;

    setTimelinePlayback(rt.showName[0] ? rt.showName : "-", liveStateName,
                        rt.elapsedMs, rt.remainingMs, pct);
    refreshLiveStatusPanel();
    updateShowLoadStateFromRuntime(rt);

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

    if (showLoadRequested_ &&
        (showLoadState_ == ShowLoadUiState::AwaitingStage ||
         showLoadState_ == ShowLoadUiState::Loading ||
         showLoadState_ == ShowLoadUiState::LoadRequested)) {
      if (nowMs - showLoadRequestMs_ > 8000UL) {
        showLoadState_ = ShowLoadUiState::TimedOut;
        showLoadRequested_ = false;
        strncpy(showLoadStatusText_,
                "Load requested — awaiting Stage confirmation",
                sizeof(showLoadStatusText_) - 1);
        showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
        refreshShowDetailsPresentation();
      }
    }

    if (emergencyActivating && !emergencyLocked &&
        (nowMs - emergencyRequestMs_) > 5000UL) {
      emergencyActivating = false;
      emergencyRequestMs_ = 0;
      pushOperatorEvent("Emergency requested — awaiting Stage confirmation");
    }

    /* Clear-await timeout: allow retry if Stage doesn't respond in 12 s */
    if (pendingClearAwait_ && emergencyLocked &&
        pendingClearRequestMs_ > 0 &&
        (nowMs - pendingClearRequestMs_) > 12000UL) {
      pendingClearAwait_ = false;
      pendingClearRequestMs_ = 0;
      pushOperatorEvent("Clear timed out — Stage did not confirm");
      if (emergencyOverlayVisible) refreshEmergencyOverlayContent();
    }

    if (nodeDiscoveryPending_) {
      const unsigned long elapsedMs = nowMs - nodeDiscoveryRequestMs_;
      if (elapsedMs > 9000UL) {
        nodeDiscoveryPending_ = false;
        nodeDiscoveryState_ = NodeDiscoveryUiState::TimedOut;
        strncpy(nodeDiscoveryResult_, "Discovery requested — awaiting network response",
                sizeof(nodeDiscoveryResult_) - 1);
        nodeDiscoveryResult_[sizeof(nodeDiscoveryResult_) - 1] = '\0';
        nodeViewDirty_ = true;
      } else if (elapsedMs > 1400UL && nodeDiscoveryState_ == NodeDiscoveryUiState::Requested) {
        nodeDiscoveryState_ = NodeDiscoveryUiState::Discovering;
        nodeViewDirty_ = true;
      }
    }

    if (nodeViewDirty_) {
      refreshNodesPresentationIfActive();
      refreshDiagnosticsSummary();
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
    refreshDiagnosticsSummary();
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

    if (logsLivePaused_ && logsPausedUnseen_ < OPERATOR_EVENT_LOG_MAX) {
      logsPausedUnseen_++;
    }
    refreshLogsDisplay();
  }

  static const char *logSeverityTag(const char *msg) {
    if (!msg) return "INFO";
    if (strstr(msg, "Emergency") || strstr(msg, "EMERGENCY") || strstr(msg, "E-STOP"))
      return "EMERGENCY";
    if (strstr(msg, "Error") || strstr(msg, "ERROR") || strstr(msg, "FAULT") || strstr(msg, "failed"))
      return "ERROR";
    if (strstr(msg, "Warn") || strstr(msg, "lost") || strstr(msg, "Lost") || strstr(msg, "Degraded"))
      return "WARNING";
    return "INFO";
  }

  bool logPassesFilter(const char *msg) const {
    if (logsFilter_ == 0) return true; /* All */
    if (!msg) return false;
    /* 1=System, 2=Show, 3=Audio, 4=Network, 5=Emergency, 6=Warnings, 7=Errors */
    switch (logsFilter_) {
      case 1: /* System — exclude clearly show/audio/network/emergency entries */
        return !strstr(msg, "AUDIO") && !strstr(msg, "Audio") &&
               !strstr(msg, "ESP-NOW") && !strstr(msg, "Emergency") &&
               !strstr(msg, "E-STOP") && !strstr(msg, "Show Started") &&
               !strstr(msg, "Show Loaded") && !strstr(msg, "Show Finished");
      case 2: /* Show */
        return strstr(msg, "Show") || strstr(msg, "Cue") ||
               strstr(msg, "Runtime") || strstr(msg, "SHOW:") ||
               strstr(msg, "Loaded") || strstr(msg, "Running");
      case 3: /* Audio */
        return strstr(msg, "Audio") || strstr(msg, "AUDIO") ||
               strstr(msg, "P4 AUDIO");
      case 4: /* Network */
        return strstr(msg, "ESP-NOW") || strstr(msg, "Comms") ||
               strstr(msg, "Stage link") || strstr(msg, "LINK") ||
               strstr(msg, "HELLO") || strstr(msg, "disconnected") ||
               strstr(msg, "Stage DISCONNECTED");
      case 5: /* Emergency */
        return strstr(msg, "Emergency") || strstr(msg, "E-STOP") ||
               strstr(msg, "EMERGENCY") || strstr(msg, "Abort");
      case 6: /* Warnings */
        return strstr(msg, "Warn") || strstr(msg, "WARN") ||
               strstr(msg, "lost") || strstr(msg, "Lost") ||
               strstr(msg, "Degraded") || strstr(msg, "timeout") ||
               strstr(msg, "Timeout") || strstr(msg, "blocked");
      case 7: /* Errors */
        return strstr(msg, "Error") || strstr(msg, "ERROR") ||
               strstr(msg, "FAULT") || strstr(msg, "failed") ||
               strstr(msg, "Failed") || strstr(msg, "ERR:") ||
               strstr(msg, "rejected") || strstr(msg, "LOAD failed");
    }
    return true;
  }

  void refreshLogsDisplay() {
    if (logsPauseStateLabel_) {
      char state[64];
      if (logsLivePaused_) {
        snprintf(state, sizeof(state), "Live updates: PAUSED  (new %u)",
                 (unsigned)logsPausedUnseen_);
      } else {
        snprintf(state, sizeof(state), "Live updates: ON");
      }
      ShowduinoOsTheme::setTextIfChanged(logsPauseStateLabel_, state);
    }
    if (logsLivePaused_) return;

    uiLogText = "";
    uint16_t shown = 0;
    for (uint16_t i = 0; i < eventLogCount && shown < 60; i++) {
      const char *line = eventSlot(i);
      if (!logPassesFilter(line)) continue;
      uiLogText += "[";
      uiLogText += logSeverityTag(line);
      uiLogText += "] ";
      uiLogText += line;
      uiLogText += "\n";
      shown++;
    }
    if (operatorLogLabel != nullptr) {
      lv_label_set_text(operatorLogLabel, uiLogText.length() ? uiLogText.c_str() : "(no events)\n");
      if (operatorLogScroll != nullptr) lv_obj_scroll_to_y(operatorLogScroll, 0, LV_ANIM_OFF);
    }
    if (logsCountLabel_) {
      char buf[48];
      snprintf(buf, sizeof(buf), "Events: %u", (unsigned)eventLogCount);
      ShowduinoOsTheme::setTextIfChanged(logsCountLabel_, buf);
    }
    if (logsNewestLabel_) {
      const char *newest = (eventLogCount > 0) ? eventSlot(0) : "—";
      char buf[96];
      snprintf(buf, sizeof(buf), "Newest: %.70s", newest);
      ShowduinoOsTheme::setTextIfChanged(logsNewestLabel_, buf);
    }
  }

  void clearOperatorLogs() {
    if (!eventLog) return;
    memset(eventLog, 0, (size_t)OPERATOR_EVENT_LOG_MAX * OPERATOR_EVENT_LINE_LEN);
    eventLogCount = 0;
    uiLogText = "";
    logsLivePaused_ = false;
    logsPausedUnseen_ = 0;
    refreshLogsDisplay();
    pushOperatorEvent("Logs cleared");
  }

  void refreshAudioPresentation() {
    /* Update Audio page from P4 model */
    DirectorP4AudioView::updateState(audioVH_, os_, p4Audio_);

    /* Update Desktop audio summary card */
    if (deskAudioSummaryLabel_) {
      char sum[128];
      const char *engWord = p4Audio_.engineStateText();
      if (p4Audio_.emergencyAudio) {
        snprintf(sum, sizeof(sum), "P4 AUDIO\nEMERGENCY AUDIO\n%s", engWord);
      } else if (!p4Audio_.p4LinkOnline) {
        snprintf(sum, sizeof(sum), "P4 AUDIO\nP4 OFFLINE\nAwaiting link");
      } else if (p4Audio_.trackKnown && p4Audio_.trackName[0]) {
        char tname[40];
        strncpy(tname, p4Audio_.trackName, sizeof(tname) - 1);
        tname[sizeof(tname) - 1] = '\0';
        snprintf(sum, sizeof(sum), "P4 AUDIO\n%s\n%.38s", engWord, tname);
      } else {
        snprintf(sum, sizeof(sum), "P4 AUDIO\n%s\nAwaiting status", engWord);
      }
      ShowduinoOsTheme::setTextIfChanged(deskAudioSummaryLabel_, sum);
    }

    /* Update Live page compact audio summary */
    if (liveAudioStateLabel_) {
      char liveAudio[64];
      if (p4Audio_.emergencyAudio) {
        strncpy(liveAudio, "EMERGENCY AUDIO", sizeof(liveAudio) - 1);
      } else if (!p4Audio_.p4LinkOnline) {
        strncpy(liveAudio, "P4 Offline", sizeof(liveAudio) - 1);
      } else {
        strncpy(liveAudio, p4Audio_.engineStateText(), sizeof(liveAudio) - 1);
      }
      liveAudio[sizeof(liveAudio) - 1] = '\0';
      ShowduinoOsTheme::setTextIfChanged(liveAudioStateLabel_, liveAudio);
      lv_obj_set_style_text_color(liveAudioStateLabel_,
                                   lv_color_hex(p4Audio_.engineStateColor()), 0);
    }
  }

  /** Apply a P4 audio response line from Stage. Call from handleStageLine. */
  void applyP4AudioResponse(const String &line) {
    const bool rejected = line.startsWith("REJECTED:AUDIO:LOCAL:DISABLED");
    const bool ack      = line.startsWith("ACK:AUDIO:");
    const bool unsupp   = line.startsWith("UNSUPPORTED:AUDIO:") ||
                          line.startsWith("NOT_IMPLEMENTED:AUDIO:");
    const bool unavail  = line.startsWith("NODE_UNAVAILABLE:AUDIO");

    if (rejected)       p4Audio_.applyLocalDisabled();
    else if (ack)       p4Audio_.applyAck(line.c_str());
    else if (unsupp)    p4Audio_.applyUnsupported(line.c_str());
    else if (unavail)   p4Audio_.applyNodeUnavailable(line.c_str());

    if (currentPage == DeskPage::Audio) {
      DirectorP4AudioView::updateState(audioVH_, os_, p4Audio_);
    }
  }

  /** Track an outgoing audio command for pending state. */
  void noteAudioCommandSent(const char *cmd) {
    if (cmd) p4Audio_.trackCommand(cmd);
  }

  /** Tick audio model — call from loop to detect timeouts. */
  void tickAudioModel() {
    p4Audio_.tick();
  }

  /** Propagate Stage link state to P4 audio model. */
  void setP4AudioLinkState(bool online) {
    p4Audio_.setLinkState(online);
    statusDirty = true;
  }

  /** Update audio emergency state from Stage emergency. */
  void setP4AudioEmergency(bool active) {
    p4Audio_.setEmergencyAudio(active);
    if (currentPage == DeskPage::Audio) {
      DirectorP4AudioView::updateState(audioVH_, os_, p4Audio_);
    }
  }

  /** Show a maintenance operation result on the Maintenance page. */
  void setMaintenanceStatus(const char *msg) {
    if (maintStatusLabel_ && msg) {
      ShowduinoOsTheme::setTextIfChanged(maintStatusLabel_, msg);
    }
  }

  /** Update the About director name label and Settings identity. */
  void setAboutDirectorName(const char *name) {
    if (aboutNameLabel_ && name && name[0]) {
      ShowduinoOsTheme::setTextIfChanged(aboutNameLabel_, name);
    }
    if (settingsDirNameLabel_ && name && name[0]) {
      char buf[64];
      snprintf(buf, sizeof(buf), "Director: %.50s", name);
      ShowduinoOsTheme::setTextIfChanged(settingsDirNameLabel_, buf);
    }
  }

  void refreshDesktopFabric() {
    if (!deskFabricLabel_) return;
    const char *health = "UNKNOWN";
    if (nodeAvailKnown_) {
      if (nodeAvailWire_ == SHOWDUINO_NODE_WIRE_FAULT) health = "FAULT";
      else if (nodeAvailWire_ == SHOWDUINO_NODE_WIRE_OFFLINE) health = "OFFLINE";
      else if (nodeAvailWire_ == SHOWDUINO_NODE_WIRE_ONLINE) health = "HEALTHY";
      else health = "UNKNOWN";
    } else if (nodeCount >= SHOWDUINO_EXPECTED_NODES && linkState == LINK_READY) {
      health = "HEALTHY";
    } else if (linkState == LINK_DISCONNECTED) {
      health = "OFFLINE";
    }
    const char *warn = (nodeAvailKnown_ && nodeAvailWire_ == SHOWDUINO_NODE_WIRE_FAULT)
                         ? "Critical warning: relay node fault"
                         : ((linkState == LINK_DISCONNECTED) ? "Critical warning: fabric link lost" : "Critical warning: none");
    char buf[200];
    snprintf(buf, sizeof(buf),
             "Online nodes: %u\nExpected nodes: %u\nOverall health: %s\n%s",
             (unsigned)nodeCount, (unsigned)SHOWDUINO_EXPECTED_NODES, health, warn);
    ShowduinoOsTheme::setTextIfChanged(deskFabricLabel_, buf);
  }

  /** Refresh SHOWS list from ShowManager (SD scan results). */
  void refreshShowLibrary(const ShowManager &sm,
                          const StorageStatus *storageStatus = nullptr,
                          bool scanCompleted = true,
                          bool scanOk = true,
                          const char *scanNote = nullptr) {
    if (storageStatus) {
      showStorageMounted_ = storageStatus->mounted;
      showStorageRecovery_ = storageStatus->recoveryMode;
      strncpy(showStorageCardType_, storageStatus->cardType, sizeof(showStorageCardType_) - 1);
      showStorageCardType_[sizeof(showStorageCardType_) - 1] = '\0';
      strncpy(showStorageLastError_, storageStatus->lastError, sizeof(showStorageLastError_) - 1);
      showStorageLastError_[sizeof(showStorageLastError_) - 1] = '\0';
    }
    if (scanCompleted) {
      showLibraryScanned_ = true;
      showLibraryScanOk_ = scanOk;
      if (scanNote && scanNote[0]) {
        strncpy(showLibraryScanNote_, scanNote, sizeof(showLibraryScanNote_) - 1);
        showLibraryScanNote_[sizeof(showLibraryScanNote_) - 1] = '\0';
      } else {
        strncpy(showLibraryScanNote_, scanOk ? "Scan complete" : "Scan failed",
                sizeof(showLibraryScanNote_) - 1);
        showLibraryScanNote_[sizeof(showLibraryScanNote_) - 1] = '\0';
      }
    }
    rebuildShowList(sm);
    refreshShowDetailsPresentation();
    statusDirty = true;
  }

  const char *selectedShowId() const { return selectedShowIdBuf; }
  bool hasSelectedShow() const { return selectedShowIdBuf[0] != '\0' && selectedShowValid_; }

  void beginSelectedShowLoadRequest() {
    if (!hasSelectedShow()) return;
    strncpy(showLoadTargetId_, selectedShowIdBuf, sizeof(showLoadTargetId_) - 1);
    showLoadTargetId_[sizeof(showLoadTargetId_) - 1] = '\0';
    showLoadState_ = ShowLoadUiState::LoadRequested;
    showLoadRequestMs_ = millis();
    showLoadRequested_ = true;
    strncpy(showLoadStatusText_, "Load requested — preparing package",
            sizeof(showLoadStatusText_) - 1);
    showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
    refreshShowDetailsPresentation();
  }

  void markSelectedShowLoadAwaitingStage() {
    if (!hasSelectedShow()) return;
    if (!showLoadTargetId_[0]) {
      strncpy(showLoadTargetId_, selectedShowIdBuf, sizeof(showLoadTargetId_) - 1);
      showLoadTargetId_[sizeof(showLoadTargetId_) - 1] = '\0';
    }
    showLoadState_ = ShowLoadUiState::AwaitingStage;
    showLoadRequestMs_ = millis();
    showLoadRequested_ = true;
    strncpy(showLoadStatusText_, "Load requested — awaiting Stage",
            sizeof(showLoadStatusText_) - 1);
    showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
    refreshShowDetailsPresentation();
  }

  void markSelectedShowLoadFailure(const char *reason) {
    showLoadState_ = ShowLoadUiState::Failed;
    showLoadRequested_ = false;
    if (reason && reason[0]) {
      strncpy(showLoadStatusText_, reason, sizeof(showLoadStatusText_) - 1);
    } else {
      strncpy(showLoadStatusText_, "Load failed", sizeof(showLoadStatusText_) - 1);
    }
    showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
    refreshShowDetailsPresentation();
  }

  void setLoadedShowName(const char *name) {
    if (!name) name = "";
    if (strcmp(loadedShowNameBuf, name) == 0) return;
    strncpy(loadedShowNameBuf, name, sizeof(loadedShowNameBuf) - 1);
    loadedShowNameBuf[sizeof(loadedShowNameBuf) - 1] = '\0';
    statusDirty = true;
    refreshShowDetailsPresentation();
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

    if (statusDirty || drawTraffic) {
      refreshDesktopFabric();
      refreshAudioPresentation();
      if (deskProgressBar_) {
        lv_bar_set_value(deskProgressBar_, (int32_t)liveProgressPct, LV_ANIM_OFF);
      }
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
  bool isShowActiveForMaintenance() const {
    return mirroredState == SHOW_STATE_RUNNING ||
           mirroredState == SHOW_STATE_PAUSED ||
           mirroredState == SHOW_STATE_EMERGENCY_STOP;
  }
  lv_obj_t *desktopScreen = nullptr;
  lv_obj_t *liveScreen = nullptr;
  lv_obj_t *showsScreen = nullptr;
  lv_obj_t *showDetailsScreen = nullptr;
  lv_obj_t *moreScreen = nullptr;
  lv_obj_t *nodesScreen = nullptr;
  lv_obj_t *nodeDetailsScreen = nullptr;
  lv_obj_t *diagnosticsScreen = nullptr;
  lv_obj_t *settingsScreen = nullptr;
  lv_obj_t *maintenanceScreen = nullptr;
  lv_obj_t *aboutScreen = nullptr;
  lv_obj_t *maintStatusLabel_ = nullptr;     /* last maintenance op result */
  lv_obj_t *aboutNameLabel_ = nullptr;       /* director name on About page */
  lv_obj_t *settingsDirNameLabel_ = nullptr; /* director name on Settings page */
  lv_obj_t *brightnessLabel_ = nullptr;      /* brightness readout on Settings */
  lv_obj_t *timeoutLabel = nullptr;
  lv_obj_t *showsListPanel = nullptr;
  lv_obj_t *showsListTitle = nullptr;
  lv_obj_t *showsStorageLabel_ = nullptr;
  lv_obj_t *showsScanLabel_ = nullptr;
  lv_obj_t *showListScroll = nullptr;
  lv_obj_t *detailsNameLabel = nullptr;
  lv_obj_t *detailsSummaryLabel_ = nullptr;
  lv_obj_t *detailsDescLabel = nullptr;
  lv_obj_t *detailsMetaLabel = nullptr;
  lv_obj_t *detailsRequirementsLabel_ = nullptr;
  lv_obj_t *detailsValidationLabel_ = nullptr;
  lv_obj_t *detailsLoadBtn_ = nullptr;
  lv_obj_t *detailsOpenLiveBtn_ = nullptr;
  lv_obj_t *detailsIconHost = nullptr;
  lv_obj_t *detailsCanvas = nullptr;
  lv_obj_t *nodesSummaryLabel_ = nullptr;
  lv_obj_t *nodesDiscoveryLabel_ = nullptr;
  lv_obj_t *nodesLinkLabel_ = nullptr;
  lv_obj_t *nodesRefreshBtn_ = nullptr;
  lv_obj_t *nodesListScroll_ = nullptr;
  lv_obj_t *nodesMissingDataLabel_ = nullptr;
  lv_obj_t *nodeDetailsTitleLabel_ = nullptr;
  lv_obj_t *nodeDetailsIdentityLabel_ = nullptr;
  lv_obj_t *nodeDetailsConnectionLabel_ = nullptr;
  lv_obj_t *nodeDetailsRuntimeLabel_ = nullptr;
  lv_obj_t *diagTransportLabel_ = nullptr;
  lv_obj_t *diagStorageLabel_ = nullptr;
  lv_obj_t *diagPacketLabel_ = nullptr;
  lv_obj_t *diagLastCommandLabel_ = nullptr;
  lv_obj_t *diagLastResponseLabel_ = nullptr;
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
  ShowValidationResult showValidationCache_[SHOW_INDEX_MAX] = {};
  uint8_t showListCount = 0;
  bool selectedShowValid_ = false;
  ShowIndexEntry selectedShowEntry_ = {};
  ShowValidationResult selectedShowValidation_ = {};
  ShowLoadUiState showLoadState_ = ShowLoadUiState::Ready;
  bool showLoadRequested_ = false;
  unsigned long showLoadRequestMs_ = 0;
  char showLoadTargetId_[64] = {};
  bool showLibraryScanned_ = false;
  bool showLibraryScanOk_ = false;
  bool showStorageMounted_ = false;
  bool showStorageRecovery_ = false;
  char showStorageCardType_[16] = "Unknown";
  char showStorageLastError_[128] = {};
  char showLibraryScanNote_[96] = "Not scanned";
  char showLoadStatusText_[128] = "Ready to load";

  struct UiNodeEntry {
    bool used = false;
    UiNodeRole role = UiNodeRole::Children;
    UiNodeHealth health = UiNodeHealth::Unknown;
    bool online = false;
    bool onlineKnown = false;
    bool everSeen = false;
    char name[48] = {};
    char id[32] = {};
    char mac[24] = {};
    char firmware[20] = {};
    char lastSeen[32] = {};
    char roleText[20] = {};
    char activity[64] = {};
    char path[64] = {};
    char transport[20] = {};
    int16_t rssi = 0;
    bool hasRssi = false;
    char capabilities[96] = {};
  };

  static constexpr uint8_t NODE_VIEW_MAX = 32;
  UiNodeEntry nodeView_[NODE_VIEW_MAX] = {};
  uint8_t nodeViewCount_ = 0;
  char nodeOpenCmds_[NODE_VIEW_MAX][24] = {};
  int16_t selectedNodeIndex_ = -1;
  bool nodeViewDirty_ = true;
  DeviceRecord pairedDeviceCache_[DEVICE_DB_MAX] = {};
  uint8_t pairedDeviceCount_ = 0;
  ShowduinoNodeAvailWire nodeAvailWire_ = SHOWDUINO_NODE_WIRE_UNKNOWN;
  bool nodeAvailKnown_ = false;
  NodeDiscoveryUiState nodeDiscoveryState_ = NodeDiscoveryUiState::Idle;
  bool nodeDiscoveryPending_ = false;
  unsigned long nodeDiscoveryRequestMs_ = 0;
  unsigned long lastNodeSignalMs_ = 0;
  unsigned long lastRuntimeMirrorMs_ = 0;
  char nodeDiscoveryResult_[96] = "Not requested";
  char nodeLastCommand_[96] = "Not sent";
  char nodeLastResponse_[96] = "Not received";
  StorageStatus nodeStorageStatus_ = {};

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
  lv_obj_t *actionConfirmRoot = nullptr;
  lv_obj_t *actionConfirmTitleLabel_ = nullptr;
  lv_obj_t *actionConfirmBodyLabel_ = nullptr;
  char actionConfirmCommand_[48] = {};
  bool actionConfirmVisible_ = false;
  bool emergencyOverlayBuilt = false;
  bool emergencyOverlayVisible = false;
  bool emergencyOverlayDismissed = false;
  bool emergencyVisitingDiag = false;
  bool emergencyAcknowledged = false;
  bool pendingClearAwait_ = false;
  unsigned long pendingClearRequestMs_ = 0;
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
  lv_obj_t *deskFabricLabel_ = nullptr;
  lv_obj_t *deskAudioSummaryLabel_ = nullptr;
  lv_obj_t *deskProgressBar_ = nullptr;
  lv_obj_t *logsScreen = nullptr;
  lv_obj_t *audioScreen = nullptr;
  lv_obj_t *logsCountLabel_ = nullptr;
  lv_obj_t *logsNewestLabel_ = nullptr;
  lv_obj_t *logsFilterLabel_ = nullptr;
  lv_obj_t *logsPauseStateLabel_ = nullptr;
  bool logsLivePaused_ = false;
  uint16_t logsPausedUnseen_ = 0;
  uint8_t logsFilter_ = 0; /* 0=All, 1=System, 2=Show, 3=Audio, 4=Net, 5=E-Stop, 6=Warnings, 7=Errors */
  DeskAudioModel audioModel_;                          /* legacy; retained for compatibility */
  DirectorP4AudioModel p4Audio_;                       /* P4-mirrored audio state             */
  DirectorP4AudioView::PageHandles audioVH_;           /* Audio page LVGL handles             */
  lv_obj_t *audioLocalStatusLabel_ = nullptr;
  lv_obj_t *audioLocalDetailLabel_ = nullptr;
  lv_obj_t *audioNodesLabel_ = nullptr;
  lv_obj_t *audioRoutingLabel_ = nullptr;
  lv_obj_t *audioCmdStatusLabel_ = nullptr;
  lv_obj_t *liveAudioStateLabel_ = nullptr;  /* compact P4 audio summary on Live */
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
  unsigned long emergencyRequestMs_ = 0;
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

    if (command == "UI:CONFIRM:CANCEL") {
      hideActionConfirm();
      pushOperatorEvent("Action cancelled");
      return;
    }
    if (command == "UI:CONFIRM:OK") {
      if (!actionConfirmVisible_ || !actionConfirmCommand_[0]) return;
      char confirmed[48];
      strncpy(confirmed, actionConfirmCommand_, sizeof(confirmed) - 1);
      confirmed[sizeof(confirmed) - 1] = '\0';
      hideActionConfirm();
      runCommand(String(confirmed));
      return;
    }

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
      hideActionConfirm();
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
        hideActionConfirm();
        showDesktop();
        setShowView(DeskShowView::Idle);
        pushOperatorEvent("Returned to Desktop");
      } else {
        emergencyVisitingDiag = true;
        hideEmergencyOverlay();
        hideActionConfirm();
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
      /* Forward to .ino for exportDiagnostics() */
      if (commandCallback) commandCallback("UI:LOGS:EXPORT");
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
    if (command == "SCREEN:MORE") {
      notePage(DeskPage::More);
      showMore();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:NODES") {
      notePage(DeskPage::Nodes);
      showNodes();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:DIAG") {
      notePage(DeskPage::Diagnostics);
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
    if (command == "SCREEN:MAINT") {
      notePage(DeskPage::Maintenance);
      showMaintenance();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:ABOUT" || command == "SETTINGS:ABOUT") {
      notePage(DeskPage::About);
      showAbout();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "MAINTENANCE:FACTORY:RESET:CONFIRM") {
      showActionConfirm("FACTORY RESET — CONFIRM?",
                        "This will reset ALL Director configuration to defaults.\n"
                        "Director name, backlight settings and all preferences will be lost.\n"
                        "The running show on the Stage Engine is NOT affected.",
                        "MAINTENANCE:FACTORY:RESET");
      return;
    }
    if (command == "UI:SETTINGS:BRIGHTNESS:UP") {
      if (commandCallback) commandCallback("SETTINGS:BRIGHTNESS:UP");
      return;
    }
    if (command == "UI:SETTINGS:BRIGHTNESS:DOWN") {
      if (commandCallback) commandCallback("SETTINGS:BRIGHTNESS:DOWN");
      return;
    }
    if (command == "SCREEN:AUDIO") {
      notePage(DeskPage::Audio);
      showAudio();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:LOGS") {
      notePage(DeskPage::Logs);
      showLogs();
      maybeRestoreEmergencyOverlay();
      return;
    }

    if (command.startsWith("UI:LOGS:FILTER:")) {
      logsFilter_ = (uint8_t)command.substring(strlen("UI:LOGS:FILTER:")).toInt();
      static const char *names[] = {
        "All", "System", "Show", "Audio", "Network", "Emergency", "Warnings", "Errors"};
      static const uint8_t namesCount = 8;
      if (logsFilterLabel_ && logsFilter_ < namesCount) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Filter: %s", names[logsFilter_]);
        ShowduinoOsTheme::setTextIfChanged(logsFilterLabel_, buf);
      }
      const bool wasPaused = logsLivePaused_;
      logsLivePaused_ = false;
      refreshLogsDisplay();
      logsLivePaused_ = wasPaused;
      return;
    }
    if (command == "UI:LOGS:CLEAR") {
      showActionConfirm("CLEAR LOG HISTORY?",
                        "This removes operator log history from memory.\n"
                        "Running show state is not changed.\n"
                        "You can cancel to keep all current logs.",
                        "UI:LOGS:CLEAR:CONFIRMED");
      return;
    }
    if (command == "UI:LOGS:CLEAR:CONFIRMED") {
      clearOperatorLogs();
      return;
    }
    if (command == "UI:LOGS:EXPORT") {
      /* Forward to .ino for actual export via exportDiagnostics() */
      if (commandCallback) commandCallback("UI:LOGS:EXPORT");
      return;
    }
    if (command == "UI:LOGS:PAUSE") {
      pushOperatorEvent("Log live updates paused");
      logsLivePaused_ = true;
      logsPausedUnseen_ = 0;
      refreshLogsDisplay();
      return;
    }
    if (command == "UI:LOGS:RESUME") {
      logsLivePaused_ = false;
      logsPausedUnseen_ = 0;
      refreshLogsDisplay();
      pushOperatorEvent("Log live updates resumed");
      return;
    }
    if (command == "UI:LOGS:LATEST") {
      logsLivePaused_ = false;
      logsPausedUnseen_ = 0;
      refreshLogsDisplay();
      if (operatorLogScroll != nullptr) lv_obj_scroll_to_y(operatorLogScroll, 0, LV_ANIM_OFF);
      pushOperatorEvent("Logs jumped to latest");
      return;
    }

    if (command == "UI:SHOW:BACK") {
      notePage(DeskPage::Shows);
      showShows();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "UI:SHOW:OPENLIVE") {
      notePage(DeskPage::Live);
      showLive();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command.startsWith("UI:SHOW:OPEN:")) {
      notePage(DeskPage::Details);
      openShowDetails(command.substring(strlen("UI:SHOW:OPEN:")).c_str());
      return;
    }
    if (command == "UI:NODE:BACK") {
      notePage(DeskPage::Nodes);
      showNodes();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "UI:NODE:DIAG") {
      notePage(DeskPage::Diagnostics);
      showDiagnostics();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command.startsWith("UI:NODE:OPEN:")) {
      int idx = command.substring(strlen("UI:NODE:OPEN:")).toInt();
      if (idx >= 0 && idx < (int)nodeViewCount_) {
        openNodeDetails((uint8_t)idx);
      }
      return;
    }
    if (command == "UI:NODES:REFRESH") {
      if (emergencyOverlayVisible || emergencyLocked) {
        pushOperatorEvent("Discovery blocked during emergency");
        return;
      }
      if (nodeDiscoveryPending_) {
        pushOperatorEvent("Discovery already pending");
        return;
      }
      beginNodeDiscoveryRequest();
      if (commandCallback != nullptr) commandCallback("UI:NODES:REFRESH");
      return;
    }

    if (command == "STORAGE:REPAIR") {
      if (emergencyLocked || emergencyOverlayVisible) {
        pushOperatorEvent("Repair blocked during emergency");
        return;
      }
      if (isShowActiveForMaintenance()) {
        pushOperatorEvent("Repair blocked while show is active");
        return;
      }
      showActionConfirm("REPAIR STORAGE DIRECTORIES?",
                        "Repairs folder structure on SD.\n"
                        "This can interrupt maintenance workflows.\n"
                        "Confirm to continue.",
                        "STORAGE:REPAIR:CONFIRMED");
      return;
    }
    if (command == "STORAGE:REPAIR:CONFIRMED") {
      if (commandCallback) commandCallback("STORAGE:REPAIR");
      pushOperatorEvent("Storage repair requested");
      return;
    }
    if (command == "SELFTEST:START") {
      if (emergencyLocked || emergencyOverlayVisible) {
        pushOperatorEvent("Self-test blocked during emergency");
        return;
      }
      if (isShowActiveForMaintenance()) {
        pushOperatorEvent("Self-test blocked while show is active");
        return;
      }
      showActionConfirm("START SELF-TEST?",
                        "Self-test may pulse outputs for diagnostics.\n"
                        "Confirm only when the stage is safe.",
                        "SELFTEST:START:CONFIRMED");
      return;
    }
    if (command == "SELFTEST:START:CONFIRMED") {
      if (commandCallback) commandCallback("SELFTEST:START");
      pushOperatorEvent("Self-test requested");
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
    } else if (command == "AUDIO:LOCAL:VOLUME:+") {
      outbound = "AUDIO:LOCAL:VOLUME:+";
      p4Audio_.trackCommand(outbound.c_str());
    } else if (command == "AUDIO:LOCAL:VOLUME:-") {
      outbound = "AUDIO:LOCAL:VOLUME:-";
      p4Audio_.trackCommand(outbound.c_str());
    } else if (command.startsWith("AUDIO:LOCAL:") || command.startsWith("AUDIO:NODE:")) {
      /* Emergency blocks audio controls (except status requests) */
      if (emergencyLocked && !command.startsWith("AUDIO:LOCAL:STATUS") &&
          !command.startsWith("AUDIO:NODE:STATUS")) {
        pushOperatorEvent("Audio cmd blocked: emergency active");
        return;
      }
      outbound = command; /* colon-text to Stage; no PCM over ESP-NOW */
      p4Audio_.trackCommand(command.c_str());
      {
        char note[96];
        snprintf(note, sizeof(note), "Audio cmd %.80s", command.c_str());
        pushOperatorEvent(note);
      }
    } else if (command == "EMERGENCY:STOP") {
      if ((emergencyActivating && (millis() - emergencyRequestMs_) < 2000UL) ||
          emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) {
        pushOperatorEvent("Emergency already active/requested");
        return;
      }
      emergencyActivating = true;
      emergencyRequestMs_ = millis();
      hideActionConfirm();
      pageBeforeEmergency = currentPage;
      for (uint8_t i = 0; i < 8; i++) {
        relayView[i] = DeskRelayView::PendingOff;
        refreshRelayButton(i);
      }
      /* Overlay + timeline pause applied in handleUiCommand / Stage STATE path. */
    } else if (command == "EMERGENCY:CLEAR") {
      /* Do not clear lock until STATE:EMERGENCY:CLEAR.
         Debounce: do not resend if already awaiting Stage confirmation. */
      if (pendingClearAwait_) {
        pushOperatorEvent("Clear already requested — awaiting Stage");
        return;
      }
      pendingClearAwait_ = true;
      pendingClearRequestMs_ = millis();
      appendLog("E-CLEAR requested — awaiting Stage");
      refreshEmergencyOverlayContent();
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

    os_.makeCaption(parent, "Show Progress", 10, 168);
    deskProgressBar_ = lv_bar_create(parent);
    lv_obj_set_pos(deskProgressBar_, 10, 186);
    lv_obj_set_size(deskProgressBar_, OS_CONTENT_LEFT_W - 36, 14);
    lv_bar_set_range(deskProgressBar_, 0, 100);
    lv_bar_set_value(deskProgressBar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(deskProgressBar_, lv_color_hex(0x14532D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(deskProgressBar_, lv_color_hex(OsColor::Accent), LV_PART_INDICATOR);
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
    makeButton(parent, "Audio System", x0 + bw + gap, y0 + bh + gap, bw, bh, "SCREEN:AUDIO");
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
    /* Operator log moved to Settings → Logs (no layer-top panel). */
  }

  void createLogPanel(lv_obj_t *screen) { (void)screen; }

  void buildLogsPage() {
    logsScreen = makeScreen();
    createDock(logsScreen);
    lv_obj_t *sum = os_.makePageChrome(logsScreen, "SYSTEM LOGS");
    logsCountLabel_ = makeLabel(sum, "Events: 0", 10, 8);
    lv_obj_add_style(logsCountLabel_, &os_.body, 0);
    logsNewestLabel_ = makeLabel(sum, "Newest: —", 10, 28);
    lv_obj_add_style(logsNewestLabel_, &os_.caption, 0);
    lv_obj_set_width(logsNewestLabel_, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(logsNewestLabel_, LV_LABEL_LONG_CLIP);

    lv_obj_t *panel = os_.makePrimaryPanel(logsScreen);
    /* Filter row 1: All, System, Show, Audio, Net, E-Stop */
    os_.makeHeading(panel, "FILTERS", 8, 2);
    makeButton(panel, "All",    8, 24, 60, 32, "UI:LOGS:FILTER:0");
    makeButton(panel, "System",74, 24, 78, 32, "UI:LOGS:FILTER:1");
    makeButton(panel, "Show", 158, 24, 64, 32, "UI:LOGS:FILTER:2");
    makeButton(panel, "Audio", 228, 24, 66, 32, "UI:LOGS:FILTER:3");
    makeButton(panel, "Net",   300, 24, 56, 32, "UI:LOGS:FILTER:4");
    makeButton(panel, "E-Stop",362, 24, 74, 32, "UI:LOGS:FILTER:5");
    /* Filter row: Warnings and Errors (expanded severity filters) */
    makeButton(panel, "Warnings", 442, 24, 88, 32, "UI:LOGS:FILTER:6");
    makeButton(panel, "Errors",   536, 24, 72, 32, "UI:LOGS:FILTER:7");
    logsFilterLabel_ = makeLabel(panel, "Filter: All", 616, 28);
    lv_obj_add_style(logsFilterLabel_, &os_.caption, 0);
    lv_obj_set_width(logsFilterLabel_, OS_CONTENT_FULL_W - 626);
    logsPauseStateLabel_ = makeLabel(panel, "Live: ON", 616, 44);
    lv_obj_add_style(logsPauseStateLabel_, &os_.caption, 0);
    lv_obj_set_width(logsPauseStateLabel_, OS_CONTENT_FULL_W - 626);

    makeButton(panel, "Clear",  8, 62, 86, 34, "UI:LOGS:CLEAR", true);
    makeButton(panel, "Export",100, 62, 86, 34, "UI:LOGS:EXPORT");
    makeButton(panel, "Pause", 192, 62, 86, 34, "UI:LOGS:PAUSE");
    makeButton(panel, "Resume",284, 62, 86, 34, "UI:LOGS:RESUME");
    makeButton(panel, "Latest",376, 62, 86, 34, "UI:LOGS:LATEST");
    makeButton(panel, "Back",  468, 62, 86, 34, "SCREEN:MORE");

    operatorLogRoot = panel;
    operatorLogScroll = lv_obj_create(panel);
    lv_obj_remove_style_all(operatorLogScroll);
    lv_obj_set_pos(operatorLogScroll, 8, 104);
    lv_obj_set_size(operatorLogScroll, OS_CONTENT_FULL_W - 28, OS_PRIMARY_H - 114);
    lv_obj_set_style_bg_opa(operatorLogScroll, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(operatorLogScroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(operatorLogScroll, LV_SCROLLBAR_MODE_AUTO);
    operatorLogLabel = lv_label_create(operatorLogScroll);
    lv_obj_set_pos(operatorLogLabel, 2, 0);
    lv_obj_set_width(operatorLogLabel, OS_CONTENT_FULL_W - 40);
    lv_obj_add_style(operatorLogLabel, &os_.body, 0);
    lv_label_set_long_mode(operatorLogLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(operatorLogLabel, "(no events)\n");
  }

  void buildAudioPage() {
    audioScreen = makeScreen();
    createDock(audioScreen);
    /* Build Audio page using P4-aware view builder */
    audioVH_ = DirectorP4AudioView::buildChrome(audioScreen, os_, staticEventHandler, this);
    DirectorP4AudioView::updateState(audioVH_, os_, p4Audio_);
  }

  static const char *nodeRoleName(UiNodeRole role) {
    switch (role) {
      case UiNodeRole::Director: return "DIRECTOR";
      case UiNodeRole::Sue: return "SUE";
      case UiNodeRole::Ian: return "IAN";
      case UiNodeRole::Children: return "CHILDREN";
      case UiNodeRole::Grandchildren: return "GRANDCHILDREN";
      case UiNodeRole::Saviour: return "SAVIOUR";
      default: return "UNKNOWN";
    }
  }

  static const char *nodeHealthName(UiNodeHealth health) {
    switch (health) {
      case UiNodeHealth::Fault: return "FAULT";
      case UiNodeHealth::Offline: return "OFFLINE";
      case UiNodeHealth::Warning: return "WARNING";
      case UiNodeHealth::Degraded: return "DEGRADED";
      case UiNodeHealth::Online: return "ONLINE";
      case UiNodeHealth::Discovering: return "DISCOVERING";
      default: return "UNKNOWN";
    }
  }

  static uint8_t nodeHealthSortRank(UiNodeHealth health) {
    switch (health) {
      case UiNodeHealth::Fault: return 0;
      case UiNodeHealth::Offline: return 1;
      case UiNodeHealth::Warning:
      case UiNodeHealth::Degraded: return 2;
      case UiNodeHealth::Online: return 3;
      case UiNodeHealth::Discovering: return 4;
      default: return 5;
    }
  }

  static uint32_t nodeHealthColor(UiNodeHealth health) {
    switch (health) {
      case UiNodeHealth::Fault: return OsColor::Danger;
      case UiNodeHealth::Offline: return 0x334155;
      case UiNodeHealth::Warning: return 0xB45309;
      case UiNodeHealth::Degraded: return 0x92400E;
      case UiNodeHealth::Online: return OsColor::Ok;
      case UiNodeHealth::Discovering: return OsColor::Accent;
      default: return OsColor::TextMuted;
    }
  }

  static const char *nodeDiscoveryStateName(NodeDiscoveryUiState st) {
    switch (st) {
      case NodeDiscoveryUiState::Idle: return "Idle";
      case NodeDiscoveryUiState::Requested: return "Discovery requested";
      case NodeDiscoveryUiState::Discovering: return "Discovering";
      case NodeDiscoveryUiState::Found: return "Devices found";
      case NodeDiscoveryUiState::Completed: return "Completed";
      case NodeDiscoveryUiState::TimedOut: return "Timed out";
      case NodeDiscoveryUiState::Failed: return "Failed";
      default: return "Unknown";
    }
  }

  static UiNodeRole roleFromDeviceRecord(const DeviceRecord &d) {
    String type = String(d.type);
    String id = String(d.id);
    String caps = String(d.capabilities);
    type.toLowerCase();
    id.toLowerCase();
    caps.toLowerCase();
    if (type.indexOf("director") >= 0 || id.indexOf("director") >= 0) return UiNodeRole::Director;
    if (type.indexOf("sue") >= 0 || id.indexOf("sue") >= 0) return UiNodeRole::Sue;
    if (type.indexOf("ian") >= 0 || type.indexOf("stage") >= 0 ||
        id.indexOf("ian") >= 0 || id.indexOf("stage") >= 0) return UiNodeRole::Ian;
    if (type.indexOf("grandchild") >= 0 || caps.indexOf("grandchild") >= 0) return UiNodeRole::Grandchildren;
    if (type.indexOf("saviour") >= 0 || caps.indexOf("saviour") >= 0) return UiNodeRole::Saviour;
    return UiNodeRole::Children;
  }

  static bool stringsEqualCaseInsensitive(const char *a, const char *b) {
    if (!a || !b) return false;
    return strcasecmp(a, b) == 0;
  }

  bool nodeIdExists(const char *id) const {
    if (!id || !id[0]) return false;
    for (uint8_t i = 0; i < nodeViewCount_; i++) {
      if (!nodeView_[i].used) continue;
      if (stringsEqualCaseInsensitive(nodeView_[i].id, id)) return true;
    }
    return false;
  }

  UiNodeEntry *appendNodeEntry() {
    if (nodeViewCount_ >= NODE_VIEW_MAX) return nullptr;
    UiNodeEntry *entry = &nodeView_[nodeViewCount_++];
    memset(entry, 0, sizeof(UiNodeEntry));
    entry->used = true;
    entry->health = UiNodeHealth::Unknown;
    return entry;
  }

  void addCoreNodeEntries() {
    UiNodeEntry *director = appendNodeEntry();
    if (director) {
      director->role = UiNodeRole::Director;
      strncpy(director->name, "Showduino Director UI", sizeof(director->name) - 1);
      strncpy(director->id, "director-ui", sizeof(director->id) - 1);
      strncpy(director->roleText, nodeRoleName(director->role), sizeof(director->roleText) - 1);
      strncpy(director->activity, "Touchscreen control surface", sizeof(director->activity) - 1);
      strncpy(director->transport, "local", sizeof(director->transport) - 1);
      strncpy(director->path, "Local", sizeof(director->path) - 1);
      director->onlineKnown = true;
      director->online = true;
      director->health = UiNodeHealth::Online;
      director->everSeen = true;
      strncpy(director->firmware, STORAGE_FW_VERSION, sizeof(director->firmware) - 1);
    }

    UiNodeEntry *sue = appendNodeEntry();
    if (sue) {
      char sueMac[24];
      snprintf(sueMac, sizeof(sueMac), "%02X:%02X:%02X:%02X:%02X:%02X",
               SHOWDUINO_P4_C6_MAC_0, SHOWDUINO_P4_C6_MAC_1, SHOWDUINO_P4_C6_MAC_2,
               SHOWDUINO_P4_C6_MAC_3, SHOWDUINO_P4_C6_MAC_4, SHOWDUINO_P4_C6_MAC_5);
      sue->role = UiNodeRole::Sue;
      strncpy(sue->name, "SUE Communications Engine", sizeof(sue->name) - 1);
      strncpy(sue->id, "sue-bridge", sizeof(sue->id) - 1);
      strncpy(sue->mac, sueMac, sizeof(sue->mac) - 1);
      strncpy(sue->roleText, nodeRoleName(sue->role), sizeof(sue->roleText) - 1);
      strncpy(sue->transport, "esp-now", sizeof(sue->transport) - 1);
      strncpy(sue->path, "Director -> SUE", sizeof(sue->path) - 1);
      if (linkState == LINK_READY) {
        sue->onlineKnown = true;
        sue->online = true;
        sue->health = UiNodeHealth::Online;
        sue->everSeen = true;
      } else if (linkState == LINK_SEARCHING) {
        sue->onlineKnown = false;
        sue->online = false;
        sue->health = UiNodeHealth::Discovering;
      } else {
        sue->onlineKnown = true;
        sue->online = false;
        sue->health = UiNodeHealth::Offline;
      }
      strncpy(sue->activity, linkState == LINK_READY ? "Bridge responding" : "Awaiting bridge response",
              sizeof(sue->activity) - 1);
    }

    UiNodeEntry *ian = appendNodeEntry();
    if (ian) {
      ian->role = UiNodeRole::Ian;
      strncpy(ian->name, "IAN Show Engine", sizeof(ian->name) - 1);
      strncpy(ian->id, "ian-show-engine", sizeof(ian->id) - 1);
      strncpy(ian->roleText, nodeRoleName(ian->role), sizeof(ian->roleText) - 1);
      strncpy(ian->transport, "uart-via-sue", sizeof(ian->transport) - 1);
      strncpy(ian->path, "Director -> SUE -> IAN", sizeof(ian->path) - 1);
      ian->everSeen = (lastRuntimeMirrorMs_ > 0 || liveStageConnected);
      if (liveStageConnected) {
        ian->onlineKnown = true;
        ian->online = true;
        ian->health = UiNodeHealth::Online;
      } else if (linkState == LINK_SEARCHING) {
        ian->onlineKnown = false;
        ian->online = false;
        ian->health = UiNodeHealth::Discovering;
      } else if (linkState == LINK_DISCONNECTED) {
        ian->onlineKnown = true;
        ian->online = false;
        ian->health = UiNodeHealth::Offline;
      } else {
        ian->onlineKnown = false;
        ian->online = false;
        ian->health = UiNodeHealth::Unknown;
      }
      strncpy(ian->activity, liveStateName, sizeof(ian->activity) - 1);
    }

    if (nodeAvailKnown_) {
      UiNodeEntry *relayFabric = appendNodeEntry();
      if (relayFabric) {
        relayFabric->role = UiNodeRole::Children;
        strncpy(relayFabric->name, "Relay Fabric", sizeof(relayFabric->name) - 1);
        strncpy(relayFabric->id, "relay-fabric", sizeof(relayFabric->id) - 1);
        strncpy(relayFabric->roleText, nodeRoleName(relayFabric->role), sizeof(relayFabric->roleText) - 1);
        strncpy(relayFabric->transport, "stage-runtime", sizeof(relayFabric->transport) - 1);
        strncpy(relayFabric->path, "Director -> SUE -> IAN -> Relay", sizeof(relayFabric->path) - 1);
        relayFabric->onlineKnown = true;
        relayFabric->online = (nodeAvailWire_ == SHOWDUINO_NODE_WIRE_ONLINE);
        relayFabric->everSeen = true;
        if (nodeAvailWire_ == SHOWDUINO_NODE_WIRE_ONLINE) relayFabric->health = UiNodeHealth::Online;
        else if (nodeAvailWire_ == SHOWDUINO_NODE_WIRE_FAULT) relayFabric->health = UiNodeHealth::Fault;
        else if (nodeAvailWire_ == SHOWDUINO_NODE_WIRE_OFFLINE) relayFabric->health = UiNodeHealth::Offline;
        else relayFabric->health = UiNodeHealth::Unknown;
        strncpy(relayFabric->activity, "Availability mirrored from Stage", sizeof(relayFabric->activity) - 1);
      }
    }
  }

  void addPairedDeviceEntries() {
    for (uint8_t i = 0; i < pairedDeviceCount_ && i < DEVICE_DB_MAX; i++) {
      const DeviceRecord &src = pairedDeviceCache_[i];
      if (!src.id[0] && !src.mac[0]) continue;
      if (src.id[0] && nodeIdExists(src.id)) continue;
      UiNodeEntry *entry = appendNodeEntry();
      if (!entry) return;
      entry->role = roleFromDeviceRecord(src);
      strncpy(entry->roleText, nodeRoleName(entry->role), sizeof(entry->roleText) - 1);
      if (src.name[0]) strncpy(entry->name, src.name, sizeof(entry->name) - 1);
      else strncpy(entry->name, "Unnamed device", sizeof(entry->name) - 1);
      if (src.id[0]) strncpy(entry->id, src.id, sizeof(entry->id) - 1);
      else strncpy(entry->id, src.mac, sizeof(entry->id) - 1);
      strncpy(entry->mac, src.mac, sizeof(entry->mac) - 1);
      strncpy(entry->firmware, src.firmware, sizeof(entry->firmware) - 1);
      strncpy(entry->capabilities, src.capabilities, sizeof(entry->capabilities) - 1);
      strncpy(entry->transport, "paired-db", sizeof(entry->transport) - 1);
      strncpy(entry->path, "Stored paired record", sizeof(entry->path) - 1);
      if (src.lastSeen[0]) {
        entry->everSeen = true;
        strncpy(entry->lastSeen, src.lastSeen, sizeof(entry->lastSeen) - 1);
        entry->onlineKnown = true;
        entry->online = false;
        entry->health = UiNodeHealth::Offline;
        strncpy(entry->activity, "Previously seen (not live-reported)", sizeof(entry->activity) - 1);
      } else {
        entry->everSeen = false;
        entry->onlineKnown = false;
        entry->online = false;
        entry->health = UiNodeHealth::Unknown;
        strncpy(entry->activity, "Never seen on this runtime", sizeof(entry->activity) - 1);
      }
    }
  }

  void sortNodeView() {
    for (uint8_t i = 0; i < nodeViewCount_; i++) {
      for (uint8_t j = i + 1; j < nodeViewCount_; j++) {
        UiNodeEntry &a = nodeView_[i];
        UiNodeEntry &b = nodeView_[j];
        bool swap = false;
        if ((uint8_t)b.role < (uint8_t)a.role) swap = true;
        else if (a.role == b.role) {
          uint8_t ra = nodeHealthSortRank(a.health);
          uint8_t rb = nodeHealthSortRank(b.health);
          if (rb < ra) swap = true;
          else if (ra == rb) {
            const char *an = a.name[0] ? a.name : a.id;
            const char *bn = b.name[0] ? b.name : b.id;
            if (strcasecmp(bn, an) < 0) swap = true;
          }
        }
        if (swap) {
          UiNodeEntry tmp = a;
          a = b;
          b = tmp;
        }
      }
    }
  }

  void rebuildNodeViewModel() {
    nodeViewCount_ = 0;
    memset(nodeView_, 0, sizeof(nodeView_));
    addCoreNodeEntries();
    addPairedDeviceEntries();
    sortNodeView();
    for (uint8_t i = 0; i < nodeViewCount_; i++) {
      snprintf(nodeOpenCmds_[i], sizeof(nodeOpenCmds_[i]), "UI:NODE:OPEN:%u", (unsigned)i);
    }
    if (selectedNodeIndex_ >= (int16_t)nodeViewCount_) selectedNodeIndex_ = -1;
    nodeViewDirty_ = false;
  }

  void refreshNodesSummary() {
    if (!nodesSummaryLabel_) return;
    uint8_t warningFault = 0;
    for (uint8_t i = 0; i < nodeViewCount_; i++) {
      if (nodeView_[i].health == UiNodeHealth::Fault ||
          nodeView_[i].health == UiNodeHealth::Warning ||
          nodeView_[i].health == UiNodeHealth::Degraded) {
        warningFault++;
      }
    }
    uint8_t expected = (uint8_t)SHOWDUINO_EXPECTED_NODES;
    uint8_t online = nodeCount;
    uint8_t offline = (online >= expected) ? 0 : (uint8_t)(expected - online);

    char summary[200];
    snprintf(summary, sizeof(summary),
             "Expected: %u  |  Online: %u  |  Offline: %u  |  Warning/Fault: %u",
             (unsigned)expected, (unsigned)online, (unsigned)offline, (unsigned)warningFault);
    ShowduinoOsTheme::setTextIfChanged(nodesSummaryLabel_, summary);

    char discovery[200];
    snprintf(discovery, sizeof(discovery), "Discovery: %s\nLast result: %s",
             nodeDiscoveryStateName(nodeDiscoveryState_), nodeDiscoveryResult_);
    ShowduinoOsTheme::setTextIfChanged(nodesDiscoveryLabel_, discovery);

    const char *sue = (linkState == LINK_READY) ? "Connected" :
                      (linkState == LINK_SEARCHING) ? "Searching" : "Disconnected";
    const char *ian = liveStageConnected ? "Connected" : "Not reported";
    char linkLine[180];
    snprintf(linkLine, sizeof(linkLine), "SUE link: %s   |   IAN link: %s", sue, ian);
    ShowduinoOsTheme::setTextIfChanged(nodesLinkLabel_, linkLine);

    if (nodesMissingDataLabel_) {
      ShowduinoOsTheme::setTextIfChanged(
          nodesMissingDataLabel_,
          "Individual CHILDREN / GRANDCHILDREN / SAVIOUR live records are not currently streamed by Stage."
      );
    }

    if (nodesRefreshBtn_) {
      if (nodeDiscoveryPending_) {
        lv_obj_add_state(nodesRefreshBtn_, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(nodesRefreshBtn_, LV_OPA_50, 0);
      } else {
        lv_obj_clear_state(nodesRefreshBtn_, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(nodesRefreshBtn_, LV_OPA_COVER, 0);
      }
    }
  }

  void renderNodeGroup(UiNodeRole role, int &y, int w) {
    char heading[48];
    snprintf(heading, sizeof(heading), "%s", nodeRoleName(role));
    lv_obj_t *h = makeLabel(nodesListScroll_, heading, 8, y);
    lv_obj_add_style(h, &os_.title, 0);
    y += 24;

    bool any = false;
    for (uint8_t i = 0; i < nodeViewCount_; i++) {
      if (nodeView_[i].role != role) continue;
      any = true;
      const UiNodeEntry &n = nodeView_[i];
      const int cardH = 88;
      lv_obj_t *card = lv_obj_create(nodesListScroll_);
      lv_obj_add_style(card, &os_.panel, 0);
      lv_obj_set_pos(card, 6, y);
      lv_obj_set_size(card, w - 12, cardH);
      lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

      lv_obj_t *name = makeLabel(card, n.name[0] ? n.name : "Unnamed", 10, 8);
      lv_obj_add_style(name, &os_.body, 0);
      lv_obj_set_width(name, w - 170);
      lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);

      char stateChip[26];
      snprintf(stateChip, sizeof(stateChip), "[%s]", nodeHealthName(n.health));
      lv_obj_t *chip = makeLabel(card, stateChip, 10, 30);
      lv_obj_add_style(chip, &os_.caption, 0);
      lv_obj_set_style_text_color(chip, lv_color_hex(nodeHealthColor(n.health)), 0);

      char sub[196];
      snprintf(sub, sizeof(sub),
               "ID: %s\nLast seen: %s\nPath: %s",
               n.id[0] ? n.id : "Not reported",
               n.lastSeen[0] ? n.lastSeen : (n.everSeen ? "Previously seen" : "Never seen"),
               n.path[0] ? n.path : "Not available");
      lv_obj_t *detail = makeLabel(card, sub, 126, 8);
      lv_obj_add_style(detail, &os_.caption, 0);
      lv_obj_set_width(detail, w - 250);
      lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);

      makeButton(card, "Details", w - 122, 24, 102, 38, nodeOpenCmds_[i]);
      y += cardH + 8;
    }

    if (!any) {
      lv_obj_t *empty = makeLabel(nodesListScroll_, "No live records reported for this role.", 12, y);
      lv_obj_add_style(empty, &os_.caption, 0);
      lv_obj_set_style_text_color(empty, lv_color_hex(OsColor::TextMuted), 0);
      y += 26;
    }
    y += 8;
  }

  void rebuildNodesList() {
    if (!nodesListScroll_) return;
    lv_obj_clean(nodesListScroll_);
    int y = 4;
    const int w = OS_CONTENT_FULL_W - 40;
    renderNodeGroup(UiNodeRole::Director, y, w);
    renderNodeGroup(UiNodeRole::Sue, y, w);
    renderNodeGroup(UiNodeRole::Ian, y, w);
    renderNodeGroup(UiNodeRole::Children, y, w);
    renderNodeGroup(UiNodeRole::Grandchildren, y, w);
    renderNodeGroup(UiNodeRole::Saviour, y, w);
  }

  void refreshNodeDetailsPresentation() {
    if (!nodeDetailsScreen || !nodeDetailsTitleLabel_) return;
    if (selectedNodeIndex_ < 0 || selectedNodeIndex_ >= (int16_t)nodeViewCount_) {
      ShowduinoOsTheme::setTextIfChanged(nodeDetailsTitleLabel_, "No node selected");
      ShowduinoOsTheme::setTextIfChanged(nodeDetailsIdentityLabel_, "Identity\nNot selected");
      ShowduinoOsTheme::setTextIfChanged(nodeDetailsConnectionLabel_, "Connection\nNot selected");
      ShowduinoOsTheme::setTextIfChanged(nodeDetailsRuntimeLabel_, "Runtime\nNot selected");
      return;
    }

    const UiNodeEntry &n = nodeView_[(uint8_t)selectedNodeIndex_];
    char title[80];
    snprintf(title, sizeof(title), "%s  •  %s", n.name[0] ? n.name : "Unnamed", nodeRoleName(n.role));
    ShowduinoOsTheme::setTextIfChanged(nodeDetailsTitleLabel_, title);

    char identity[320];
    snprintf(identity, sizeof(identity),
             "Identity\n"
             "Name: %s\n"
             "Role: %s\n"
             "Device ID: %s\n"
             "MAC: %s\n"
             "IP: Not reported\n"
             "Firmware: %s\n"
             "Hardware: %s",
             n.name[0] ? n.name : "Not reported",
             nodeRoleName(n.role),
             n.id[0] ? n.id : "Not reported",
             n.mac[0] ? n.mac : "Not reported",
             n.firmware[0] ? n.firmware : "Not reported",
             n.transport[0] ? n.transport : "Not reported");
    ShowduinoOsTheme::setTextIfChanged(nodeDetailsIdentityLabel_, identity);

    char rssiText[20];
    if (n.hasRssi) snprintf(rssiText, sizeof(rssiText), "%d dBm", (int)n.rssi);
    else strncpy(rssiText, "Not reported", sizeof(rssiText) - 1);
    rssiText[sizeof(rssiText) - 1] = '\0';

    char connection[420];
    snprintf(connection, sizeof(connection),
             "Connection\n"
             "State: %s\n"
             "Last seen: %s\n"
             "Signal: %s\n"
             "Transport: %s\n"
             "Connection path: %s\n"
             "Discovery: %s\n"
             "Last command: %s\n"
             "Last response: %s\n"
             "Packet counters: TX %lu / RX %lu\n"
             "Error counters: Not reported",
             nodeHealthName(n.health),
             n.lastSeen[0] ? n.lastSeen : (n.everSeen ? "Previously seen" : "Never seen"),
             rssiText,
             n.transport[0] ? n.transport : "Not reported",
             n.path[0] ? n.path : "Not reported",
             nodeDiscoveryStateName(nodeDiscoveryState_),
             nodeLastCommand_,
             nodeLastResponse_,
             (unsigned long)txCount, (unsigned long)rxCount);
    ShowduinoOsTheme::setTextIfChanged(nodeDetailsConnectionLabel_, connection);

    char runtime[420];
    snprintf(runtime, sizeof(runtime),
             "Runtime\n"
             "Uptime: %lus\n"
             "Active show: %s\n"
             "Current state: %s\n"
             "Current cue: %lu / %lu\n"
             "Output state: Not reported\n"
             "Audio state: Not reported\n"
             "Sensor values: Not reported\n"
             "Battery level: Not reported",
             (unsigned long)(millis() / 1000UL),
             loadedShowNameBuf[0] ? loadedShowNameBuf : "Not loaded",
             liveStateName[0] ? liveStateName : "Unknown",
             (unsigned long)liveCue,
             (unsigned long)liveCueTotal);
    ShowduinoOsTheme::setTextIfChanged(nodeDetailsRuntimeLabel_, runtime);
  }

  void refreshDiagnosticsSummary() {
    if (!diagTransportLabel_) return;
    const char *link = (linkState == LINK_READY) ? "READY" :
                       (linkState == LINK_SEARCHING) ? "SEARCHING" : "DISCONNECTED";
    char transport[200];
    snprintf(transport, sizeof(transport),
             "Transport\nESP-NOW link: %s\nIAN / Stage: %s\nDiscovery: %s",
             link,
             liveStageConnected ? "CONNECTED" : "NOT REPORTED",
             nodeDiscoveryStateName(nodeDiscoveryState_));
    ShowduinoOsTheme::setTextIfChanged(diagTransportLabel_, transport);

    char storage[220];
    snprintf(storage, sizeof(storage),
             "Storage\nSD: %s (%s)\nFree: %llu MB\nLast error: %s",
             nodeStorageStatus_.mounted ? "READY" : "UNAVAILABLE",
             nodeStorageStatus_.cardType,
             (unsigned long long)(nodeStorageStatus_.freeBytes / (1024ULL * 1024ULL)),
             nodeStorageStatus_.lastError[0] ? nodeStorageStatus_.lastError : "None");
    ShowduinoOsTheme::setTextIfChanged(diagStorageLabel_, storage);

    char packet[180];
    snprintf(packet, sizeof(packet),
             "Telemetry\nTX %lu  RX %lu\nHeap %u  PSRAM %u",
             (unsigned long)txCount, (unsigned long)rxCount,
             (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    ShowduinoOsTheme::setTextIfChanged(diagPacketLabel_, packet);

    char lastCmd[120];
    snprintf(lastCmd, sizeof(lastCmd), "Last command: %.90s", nodeLastCommand_);
    ShowduinoOsTheme::setTextIfChanged(diagLastCommandLabel_, lastCmd);

    char lastRsp[120];
    snprintf(lastRsp, sizeof(lastRsp), "Last response: %.90s", nodeLastResponse_);
    ShowduinoOsTheme::setTextIfChanged(diagLastResponseLabel_, lastRsp);
  }

  void refreshNodesPresentationIfActive() {
    if (!nodesScreen) return;
    if (nodeViewDirty_) rebuildNodeViewModel();
    refreshNodesSummary();
    if (currentPage == DeskPage::Nodes) rebuildNodesList();
    if (currentPage == DeskPage::NodeDetails) refreshNodeDetailsPresentation();
  }

  void openNodeDetails(uint8_t idx) {
    if (idx >= nodeViewCount_) return;
    selectedNodeIndex_ = idx;
    refreshNodeDetailsPresentation();
    showNodeDetails();
  }

  void buildNodesPage() {
    nodesScreen = makeScreen();
    createDock(nodesScreen);
    lv_obj_t *sum = os_.makePageChrome(nodesScreen, "NODES");
    nodesSummaryLabel_ = makeLabel(sum, "Expected: 0  |  Online: 0  |  Offline: 0  |  Warning/Fault: 0", 10, 8);
    lv_obj_add_style(nodesSummaryLabel_, &os_.body, 0);
    lv_obj_set_width(nodesSummaryLabel_, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(nodesSummaryLabel_, LV_LABEL_LONG_CLIP);
    nodesDiscoveryLabel_ = makeLabel(sum, "Discovery: Idle", 10, 28);
    lv_obj_add_style(nodesDiscoveryLabel_, &os_.caption, 0);
    lv_obj_set_width(nodesDiscoveryLabel_, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(nodesDiscoveryLabel_, LV_LABEL_LONG_CLIP);
    nodesLinkLabel_ = makeLabel(sum, "SUE link: Unknown   |   IAN link: Unknown", 10, 48);
    lv_obj_add_style(nodesLinkLabel_, &os_.caption, 0);
    lv_obj_set_width(nodesLinkLabel_, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(nodesLinkLabel_, LV_LABEL_LONG_CLIP);

    lv_obj_t *panel = os_.makePrimaryPanel(nodesScreen);
    os_.makeHeading(panel, "SHOWDUINO FABRIC", 8, 2);
    nodesRefreshBtn_ = makeButton(panel, "Discover / Refresh", 8, 30, 170, 40, "UI:NODES:REFRESH");
    makeButton(panel, "Open Diagnostics", 186, 30, 170, 40, "SCREEN:DIAG");
    nodesMissingDataLabel_ = makeLabel(panel, "", 366, 34);
    lv_obj_add_style(nodesMissingDataLabel_, &os_.caption, 0);
    lv_obj_set_width(nodesMissingDataLabel_, OS_CONTENT_FULL_W - 392);
    lv_label_set_long_mode(nodesMissingDataLabel_, LV_LABEL_LONG_WRAP);

    nodesListScroll_ = lv_obj_create(panel);
    lv_obj_remove_style_all(nodesListScroll_);
    lv_obj_set_pos(nodesListScroll_, 4, 78);
    lv_obj_set_size(nodesListScroll_, OS_CONTENT_FULL_W - 28, OS_PRIMARY_H - 88);
    lv_obj_set_style_bg_opa(nodesListScroll_, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(nodesListScroll_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(nodesListScroll_, LV_SCROLLBAR_MODE_AUTO);
  }

  void buildNodeDetailsPage() {
    nodeDetailsScreen = makeScreen();
    createDock(nodeDetailsScreen);
    lv_obj_t *sum = os_.makePageChrome(nodeDetailsScreen, "NODE DETAILS");
    nodeDetailsTitleLabel_ = makeLabel(sum, "No node selected", 10, 8);
    lv_obj_add_style(nodeDetailsTitleLabel_, &os_.title, 0);
    lv_obj_set_width(nodeDetailsTitleLabel_, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(nodeDetailsTitleLabel_, LV_LABEL_LONG_CLIP);

    lv_obj_t *panel = os_.makePrimaryPanel(nodeDetailsScreen);
    nodeDetailsIdentityLabel_ = makeLabel(panel, "Identity", 8, 8);
    lv_obj_set_width(nodeDetailsIdentityLabel_, 252);
    lv_obj_add_style(nodeDetailsIdentityLabel_, &os_.caption, 0);
    lv_label_set_long_mode(nodeDetailsIdentityLabel_, LV_LABEL_LONG_WRAP);

    nodeDetailsConnectionLabel_ = makeLabel(panel, "Connection", 268, 8);
    lv_obj_set_width(nodeDetailsConnectionLabel_, 252);
    lv_obj_add_style(nodeDetailsConnectionLabel_, &os_.caption, 0);
    lv_label_set_long_mode(nodeDetailsConnectionLabel_, LV_LABEL_LONG_WRAP);

    nodeDetailsRuntimeLabel_ = makeLabel(panel, "Runtime", 528, 8);
    lv_obj_set_width(nodeDetailsRuntimeLabel_, 240);
    lv_obj_add_style(nodeDetailsRuntimeLabel_, &os_.caption, 0);
    lv_label_set_long_mode(nodeDetailsRuntimeLabel_, LV_LABEL_LONG_WRAP);

    makeButton(panel, "Refresh Status", 8, OS_PRIMARY_H - 48, 130, 38, "STATUS:REQUEST");
    makeButton(panel, "Rediscover", 146, OS_PRIMARY_H - 48, 110, 38, "UI:NODES:REFRESH");
    makeButton(panel, "Diagnostics", 264, OS_PRIMARY_H - 48, 120, 38, "UI:NODE:DIAG");
    makeButton(panel, "Back to Nodes", 392, OS_PRIMARY_H - 48, 130, 38, "UI:NODE:BACK");
  }

  void buildDiagnosticsPage() {
    diagnosticsScreen = makeScreen();
    createDock(diagnosticsScreen);
    lv_obj_t *sum = os_.makePageChrome(diagnosticsScreen, "DIAGNOSTICS");
    makeLabel(sum, "Technical status and protocol checks", 10, 14);

    lv_obj_t *panel = os_.makePrimaryPanel(diagnosticsScreen);
    os_.makeHeading(panel, "TRANSPORT", 8, 2);
    makeButton(panel, "Stage Status", 8, 28, 128, 38, "STATUS:REQUEST");
    makeButton(panel, "Stage Hello", 142, 28, 118, 38, "HELLO");
    makeButton(panel, "Rediscover", 266, 28, 110, 38, "UI:NODES:REFRESH");
    makeButton(panel, "Open Nodes", 382, 28, 110, 38, "SCREEN:NODES");
    makeButton(panel, "Back", 498, 28, 90, 38, "SCREEN:MORE");
    diagTransportLabel_ = makeLabel(panel, "Transport", 8, 72);
    lv_obj_add_style(diagTransportLabel_, &os_.caption, 0);
    lv_obj_set_width(diagTransportLabel_, 360);
    lv_label_set_long_mode(diagTransportLabel_, LV_LABEL_LONG_WRAP);

    os_.makeHeading(panel, "SERVICE TESTS", 8, 132);
    makeButton(panel, "SD Status", 8, 158, 110, 38, "STORAGE:STATUS");
    makeButton(panel, "Self Test", 124, 158, 96, 38, "SELFTEST:START");
    makeButton(panel, "Maintenance", 226, 158, 128, 38, "SCREEN:MAINT");
    diagStorageLabel_ = makeLabel(panel, "Storage", 8, 202);
    lv_obj_add_style(diagStorageLabel_, &os_.caption, 0);
    lv_obj_set_width(diagStorageLabel_, 360);
    lv_label_set_long_mode(diagStorageLabel_, LV_LABEL_LONG_WRAP);

    os_.makeHeading(panel, "TELEMETRY", 390, 72);
    diagPacketLabel_ = makeLabel(panel, "Telemetry", 390, 98);
    lv_obj_add_style(diagPacketLabel_, &os_.caption, 0);
    lv_obj_set_width(diagPacketLabel_, 380);
    lv_label_set_long_mode(diagPacketLabel_, LV_LABEL_LONG_WRAP);
    diagLastCommandLabel_ = makeLabel(panel, "Last command: Not sent", 390, 174);
    lv_obj_add_style(diagLastCommandLabel_, &os_.caption, 0);
    lv_obj_set_width(diagLastCommandLabel_, 380);
    lv_label_set_long_mode(diagLastCommandLabel_, LV_LABEL_LONG_CLIP);
    diagLastResponseLabel_ = makeLabel(panel, "Last response: Not received", 390, 194);
    lv_obj_add_style(diagLastResponseLabel_, &os_.caption, 0);
    lv_obj_set_width(diagLastResponseLabel_, 380);
    lv_label_set_long_mode(diagLastResponseLabel_, LV_LABEL_LONG_CLIP);
  }

  void buildMaintenancePage() {
    if (maintenanceScreen) return; /* Guard: prevent duplicate build and LVGL object leak */
    maintenanceScreen = makeScreen();
    createDock(maintenanceScreen);
    lv_obj_t *sum = os_.makePageChrome(maintenanceScreen, "MAINTENANCE");
    os_.makeCaption(sum, "Service operations and recovery tools", 10, 8);
    os_.makeCaption(sum, "Dangerous actions require confirmation.", 10, 28);

    lv_obj_t *panel = os_.makePrimaryPanel(maintenanceScreen);
    const int pW = OS_CONTENT_FULL_W - 24;

    os_.makeHeading(panel, "STORAGE", OS_PAD, 2);
    makeButton(panel, "SD Status",   OS_PAD,       28, 110, 40, "STORAGE:STATUS");
    makeButton(panel, "Backup",      OS_PAD+118,   28, 100, 40, "STORAGE:BACKUP");
    makeButton(panel, "Export Diag", OS_PAD+226,   28,  96, 40, "STORAGE:EXPORT");
    makeButton(panel, "Repair Dirs", OS_PAD+330,   28, 118, 40, "STORAGE:REPAIR");
    os_.makeCaption(panel, "Repair: safe — creates missing dirs only. Backup: copies config to SD.", OS_PAD, 76);

    os_.makeHeading(panel, "LOGS & REPORTS", OS_PAD, 96);
    makeButton(panel, "Open Logs",        OS_PAD,       122, 120, 40, "SCREEN:LOGS");
    makeButton(panel, "Export Logs",      OS_PAD+128,   122, 130, 40, "UI:LOGS:EXPORT");
    makeButton(panel, "Diagnostics",      OS_PAD+266,   122, 126, 40, "SCREEN:DIAG");
    makeButton(panel, "Back to Settings", OS_PAD+400,   122, 170, 40, "SCREEN:SETTINGS");

    /* Last operation status */
    os_.makeHeading(panel, "LAST OPERATION", OS_PAD, 172);
    maintStatusLabel_ = makeLabel(panel, "No operation run this session", OS_PAD, 192);
    lv_obj_add_style(maintStatusLabel_, &os_.caption, 0);
    lv_obj_set_width(maintStatusLabel_, pW - OS_PAD);
    lv_label_set_long_mode(maintStatusLabel_, LV_LABEL_LONG_WRAP);

    /* Factory reset — confirmed only */
    os_.makeHeading(panel, "CONFIGURATION RESET", OS_PAD, 220);
    os_.makeCaption(panel, "Resets all Director configuration to factory defaults. Stage show is not stopped.", OS_PAD, 240);
    makeButton(panel, "Factory Reset", OS_PAD, 260, 160, 42, "MAINTENANCE:FACTORY:RESET:CONFIRM", true);
  }

  void buildAboutPage() {
    if (aboutScreen) return; /* Guard: prevent duplicate build and LVGL object leak */
    aboutScreen = makeScreen();
    createDock(aboutScreen);
    lv_obj_t *sum = os_.makePageChrome(aboutScreen, "ABOUT");
    makeButton(sum, "Back", OS_CONTENT_FULL_W - 110, 8, 90, 36, "SCREEN:SETTINGS");

    lv_obj_t *panel = os_.makePrimaryPanel(aboutScreen);
    os_.makeHeading(panel, "SHOWDUINO DIRECTOR", 8, 2);

    /* Director name — updated at runtime from DirectorConfig */
    os_.makeCaption(panel, "Director name", OS_PAD, 28);
    aboutNameLabel_ = makeLabel(panel, "Loading\xe2\x80\xa6", OS_PAD + 130, 28);
    lv_obj_add_style(aboutNameLabel_, &os_.body, 0);
    lv_obj_set_width(aboutNameLabel_, OS_CONTENT_FULL_W - OS_PAD - 140);
    lv_label_set_long_mode(aboutNameLabel_, LV_LABEL_LONG_WRAP);

    char info[768];
    snprintf(info, sizeof(info),
             "Product: Showduino Director\n"
             "Role: Operator UI (LVGL touchscreen)\n"
             "Firmware: %s\n"
             "Board: %s\n"
             "Display: 800x480 RGB + GT911 touch\n"
             "LVGL: %d.%d\n"
             "Protocol: %d.%d\n"
             "Show Runtime Owner: IAN / P4 Stage Engine\n"
             "Audio Engine Owner: IAN / P4 (Director is monitor/control only)\n"
             "Comms Path: Director \xe2\x86\x92 SUE bridge \xe2\x86\x92 IAN\n"
             "Build info: see STORAGE_FW_VERSION in firmware\n"
             "Repository: sumkindafreak/new_showduino\n"
             "License/Credits: see repository documentation.",
             STORAGE_FW_VERSION,
             SHOWDUINO_BOARD_NAME,
             LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
             SHOWDUINO_PROTOCOL_VERSION_MAJOR, SHOWDUINO_PROTOCOL_VERSION_MINOR);
    lv_obj_t *label = makeLabel(panel, info, 8, 56);
    lv_obj_add_style(label, &os_.caption, 0);
    lv_obj_set_width(label, OS_CONTENT_FULL_W - 30);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  }

  void buildScreens() {
    Serial.printf("[UI] heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    createSharedOperatorLog();
    uiBuildPump();

    /* ---- DESKTOP (design reference) — full width, no event log ---- */
    Serial.println("[UI] desktop…");
    desktopScreen = makeScreen();
    uiBuildPump("[UI] desktop");
    createDock(desktopScreen);
    const int deskLeftW = OS_CONTENT_LEFT_W;
    const int deskRightW = OS_CONTENT_RIGHT_W;
    const int deskRightX = OS_CONTENT_RIGHT_X;
    const int deskSumH = 218;
    const int deskActY = OS_BODY_Y + deskSumH + OS_GAP;
    const int deskActH = OS_BODY_H - deskSumH - OS_GAP;

    lv_obj_t *summary = makePanel(desktopScreen, OS_MARGIN, OS_BODY_Y, deskLeftW, deskSumH);
    createSystemSummary(summary);

    lv_obj_t *fabric = makePanel(desktopScreen, deskRightX, OS_BODY_Y, deskRightW, 128);
    os_.makeHeading(fabric, "NODE HEALTH", 8, 2);
    deskFabricLabel_ = makeLabel(fabric, "Online nodes: 0", 8, 28);
    lv_obj_add_style(deskFabricLabel_, &os_.caption, 0);
    lv_obj_set_width(deskFabricLabel_, deskRightW - 132);
    lv_label_set_long_mode(deskFabricLabel_, LV_LABEL_LONG_WRAP);
    makeButton(fabric, "Open", deskRightW - 110, 40, 94, 40, "SCREEN:NODES");

    lv_obj_t *audioCard = makePanel(desktopScreen, deskRightX, OS_BODY_Y + 128 + OS_GAP, deskRightW,
                                    deskSumH - 128 - OS_GAP);
    os_.makeHeading(audioCard, "P4 AUDIO", 8, 2);
    deskAudioSummaryLabel_ = makeLabel(audioCard, "P4 AUDIO\nAwaiting status\nP4 link: Unknown", 8, 28);
    lv_obj_add_style(deskAudioSummaryLabel_, &os_.caption, 0);
    lv_obj_set_width(deskAudioSummaryLabel_, deskRightW - 20);
    lv_label_set_long_mode(deskAudioSummaryLabel_, LV_LABEL_LONG_WRAP);
    makeButton(audioCard, "Open Audio", 8, 60, deskRightW - 24, 36, "SCREEN:AUDIO");

    lv_obj_t *actions = makePanel(desktopScreen, OS_MARGIN, deskActY, OS_CONTENT_FULL_W, deskActH);
    createQuickActions(actions);
    os_.makeCaption(actions, "Playback transport is intentionally owned by Live.", 470, 10);
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
    /* Compact P4 audio state (right side of summary bar) */
    os_.makeCaption(liveSum, "P4 Audio", 490, 8);
    liveAudioStateLabel_ = makeLabel(liveSum, "Awaiting P4", 490, 28);
    lv_obj_add_style(liveAudioStateLabel_, &os_.caption, 0);
    lv_obj_set_width(liveAudioStateLabel_, 270);
    lv_label_set_long_mode(liveAudioStateLabel_, LV_LABEL_LONG_WRAP);

    lv_obj_t *live = os_.makePrimaryPanel(liveScreen);
    os_.makeHeading(live, "SHOW CONTROL", 8, 2);
    makeButton(live, "Start Show", 10, 24, 112, 44, "SHOW:START");
    makeButton(live, "Pause", 130, 24, 92, 44, "SHOW:PAUSE");
    makeButton(live, "Resume", 230, 24, 100, 44, "SHOW:RESUME");
    makeButton(live, "Stop Show", 338, 24, 112, 44, "SHOW:STOP", true);

    liveEmergencyDot = lv_obj_create(live);
    lv_obj_remove_style_all(liveEmergencyDot);
    lv_obj_set_size(liveEmergencyDot, 12, 12);
    lv_obj_set_pos(liveEmergencyDot, 442, 6);
    lv_obj_set_style_radius(liveEmergencyDot, 6, 0);
    lv_obj_set_style_bg_color(liveEmergencyDot, lv_color_hex(OsColor::Unknown), 0);
    lv_obj_set_style_bg_opa(liveEmergencyDot, LV_OPA_COVER, 0);

    liveStatusLabel = makeLabel(live, "0%", 10, 78);
    lv_obj_set_width(liveStatusLabel, 60);
    lv_obj_add_style(liveStatusLabel, &os_.body, 0);

    liveProgressBar = lv_bar_create(live);
    lv_obj_set_pos(liveProgressBar, 70, 82);
    lv_obj_set_size(liveProgressBar, 380, 10);
    lv_bar_set_range(liveProgressBar, 0, 100);
    lv_bar_set_value(liveProgressBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(liveProgressBar, lv_color_hex(0x27272A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(liveProgressBar, lv_color_hex(OsColor::DangerBorder), LV_PART_INDICATOR);

    timelineStatusLabel = makeLabel(live, "", 10, 88);
    lv_obj_add_flag(timelineStatusLabel, LV_OBJ_FLAG_HIDDEN);

    os_.makeHeading(live, "MANUAL OUTPUTS", 8, 106);
    for (uint8_t i = 0; i < 8; i++) {
      int row = i / 4;
      int col = i % 4;
      int x = 10 + col * 112;
      int y = 130 + row * 40;
      static const char *relayNames[8] = { "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8" };
      relayButtons[i] = makeButton(live, relayNames[i], x, y, 104, 36, kRelayCmds[i]);
      refreshRelayButton(i);
    }
    makeButton(live, "All Outputs Off", 10, 214, 142, 38, "RELAY:ALL:OFF");
    makeButton(live, "Pulse R1", 160, 214, 104, 38, "RELAY:1:PULSE:1000");
    makeButton(live, "Request Status", 272, 214, 128, 38, "STATUS:REQUEST");
    uiBuildPump();

    /* ---- SHOWS — deliberate package browser ---- */
    Serial.println("[UI] shows…");
    showsScreen = makeScreen();
    uiBuildPump("[UI] shows");
    createDock(showsScreen);
    lv_obj_t *showSum = os_.makePageChrome(showsScreen, "SHOWS");
    os_.makeCaption(showSum, "Show workflow", 10, 8);
    showsSummaryLabel_ = makeLabel(showSum, "Select package → inspect details → load → open Live", 10, 28);
    lv_obj_add_style(showsSummaryLabel_, &os_.body, 0);
    lv_obj_set_width(showsSummaryLabel_, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(showsSummaryLabel_, LV_LABEL_LONG_CLIP);

    showsListPanel = os_.makePrimaryPanel(showsScreen);
    os_.makeHeading(showsListPanel, "SHOW LIBRARY", 8, 2);
    makeButton(showsListPanel, "Refresh Library", 304, 2, 150, 36, "UI:SHOW:REFRESH");
    showsListTitle = makeLabel(showsListPanel, "Discovered: Unknown", 8, 30);
    lv_obj_add_style(showsListTitle, &os_.caption, 0);
    lv_obj_set_width(showsListTitle, OS_CONTENT_FULL_W - 26);
    lv_label_set_long_mode(showsListTitle, LV_LABEL_LONG_CLIP);
    showsStorageLabel_ = makeLabel(showsListPanel, "Storage: Unknown", 8, 48);
    lv_obj_add_style(showsStorageLabel_, &os_.caption, 0);
    lv_obj_set_width(showsStorageLabel_, OS_CONTENT_FULL_W - 26);
    lv_label_set_long_mode(showsStorageLabel_, LV_LABEL_LONG_CLIP);
    showsScanLabel_ = makeLabel(showsListPanel, "Last scan: Not scanned", 8, 66);
    lv_obj_add_style(showsScanLabel_, &os_.caption, 0);
    lv_obj_set_width(showsScanLabel_, OS_CONTENT_FULL_W - 26);
    lv_label_set_long_mode(showsScanLabel_, LV_LABEL_LONG_CLIP);

    showListScroll = lv_obj_create(showsListPanel);
    lv_obj_remove_style_all(showListScroll);
    lv_obj_set_pos(showListScroll, 4, 88);
    lv_obj_set_size(showListScroll, OS_CONTENT_FULL_W - 28, OS_PRIMARY_H - 98);
    lv_obj_set_style_bg_opa(showListScroll, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(showListScroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(showListScroll, LV_SCROLLBAR_MODE_AUTO);
    makeLabel(showListScroll, "Library not scanned yet.", 8, 8);
    uiBuildPump();

    /* ---- SHOW DETAILS ---- */
    Serial.println("[UI] details…");
    showDetailsScreen = makeScreen();
    uiBuildPump("[UI] details");
    createDock(showDetailsScreen);
    lv_obj_t *detSum = os_.makePageChrome(showDetailsScreen, "SHOW DETAILS");
    detailsNameLabel = makeLabel(detSum, "No show selected", 10, 8);
    lv_obj_add_style(detailsNameLabel, &os_.title, 0);
    lv_obj_set_width(detailsNameLabel, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(detailsNameLabel, LV_LABEL_LONG_CLIP);
    detailsSummaryLabel_ = makeLabel(detSum, "Select a package in Shows.", 10, 30);
    lv_obj_add_style(detailsSummaryLabel_, &os_.caption, 0);
    lv_obj_set_width(detailsSummaryLabel_, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(detailsSummaryLabel_, LV_LABEL_LONG_CLIP);

    lv_obj_t *det = os_.makePrimaryPanel(showDetailsScreen);
    detailsIconHost = lv_obj_create(det);
    lv_obj_remove_style_all(detailsIconHost);
    lv_obj_set_pos(detailsIconHost, 8, 8);
    lv_obj_set_size(detailsIconHost, 104, 72);
    ShowduinoShowThumb::makeDefaultIcon(detailsIconHost, 4, 4, 96, 64);
    detailsDescLabel = makeLabel(det, "Description unavailable", 122, 10);
    lv_obj_set_width(detailsDescLabel, 320);
    lv_obj_add_style(detailsDescLabel, &os_.body, 0);
    lv_label_set_long_mode(detailsDescLabel, LV_LABEL_LONG_WRAP);
    detailsMetaLabel = makeLabel(det, "Duration: Unknown", 8, 88);
    lv_obj_set_width(detailsMetaLabel, OS_CONTENT_FULL_W - 26);
    lv_obj_add_style(detailsMetaLabel, &os_.caption, 0);
    lv_label_set_long_mode(detailsMetaLabel, LV_LABEL_LONG_WRAP);
    detailsRequirementsLabel_ = makeLabel(det,
                                          "Requirements\nNodes: Not specified\nAudio: Not specified\nLighting: Not specified\nStorage: Not specified\nMinimum package: Not specified",
                                          8, 126);
    lv_obj_set_width(detailsRequirementsLabel_, OS_CONTENT_FULL_W - 26);
    lv_obj_add_style(detailsRequirementsLabel_, &os_.caption, 0);
    lv_label_set_long_mode(detailsRequirementsLabel_, LV_LABEL_LONG_WRAP);
    detailsValidationLabel_ = makeLabel(det, "Validation: Not validated", 8, 204);
    lv_obj_set_width(detailsValidationLabel_, OS_CONTENT_FULL_W - 26);
    lv_obj_add_style(detailsValidationLabel_, &os_.body, 0);
    lv_label_set_long_mode(detailsValidationLabel_, LV_LABEL_LONG_WRAP);
    timelineDetailLabel = makeLabel(det, "Load state: Ready to load", 8, 222);
    lv_obj_set_width(timelineDetailLabel, OS_CONTENT_FULL_W - 26);
    lv_obj_add_style(timelineDetailLabel, &os_.caption, 0);
    detailsLoadBtn_ = makeButton(det, "Load Show", 8, 236, 130, 34, "UI:SHOW:LOAD");
    makeButton(det, "Back to Shows", 146, 236, 146, 34, "UI:SHOW:BACK");
    detailsOpenLiveBtn_ = makeButton(det, "Open Live", 300, 236, 120, 34, "UI:SHOW:OPENLIVE");
    lv_obj_add_flag(detailsOpenLiveBtn_, LV_OBJ_FLAG_HIDDEN);
    refreshShowDetailsPresentation();
    uiBuildPump();

    /* ---- MORE — stable launcher for secondary destinations ---- */
    Serial.println("[UI] more…");
    moreScreen = makeScreen();
    uiBuildPump("[UI] more");
    createDock(moreScreen);
    lv_obj_t *moreSum = os_.makePageChrome(moreScreen, "MORE");
    os_.makeCaption(moreSum, "System", 10, 8);
    makeLabel(moreSum, "Secondary tools and configuration", 10, 28);

    lv_obj_t *more = os_.makePrimaryPanel(moreScreen);
    os_.makeHeading(more, "DESTINATIONS", 8, 2);
    makeButton(more, "Nodes",       12, 36, 210, 58, "SCREEN:NODES");
    makeButton(more, "Audio",      234, 36, 210, 58, "SCREEN:AUDIO");
    makeButton(more, "Logs",        12, 106, 210, 58, "SCREEN:LOGS");
    makeButton(more, "Settings",   234, 106, 210, 58, "SCREEN:SETTINGS");
    makeButton(more, "Diagnostics", 12, 176, 140, 58, "SCREEN:DIAG");
    makeButton(more, "Maintenance",162, 176, 140, 58, "SCREEN:MAINT");
    makeButton(more, "About",      312, 176, 140, 58, "SCREEN:ABOUT");
    os_.makeCaption(more,
                    "Playback remains in Live. Show selection remains in Shows.",
                    12, 244);
    uiBuildPump();

    /* ---- NODES + NODE DETAILS + DIAGNOSTICS ---- */
    Serial.println("[UI] nodes…");
    buildNodesPage();
    uiBuildPump("[UI] nodes");
    Serial.println("[UI] node details…");
    buildNodeDetailsPage();
    uiBuildPump("[UI] node details");
    Serial.println("[UI] diagnostics…");
    buildDiagnosticsPage();
    uiBuildPump("[UI] diagnostics");
    Serial.println("[UI] maintenance…");
    buildMaintenancePage();
    uiBuildPump("[UI] maintenance");
    Serial.println("[UI] about…");
    buildAboutPage();
    uiBuildPump("[UI] about");

    /* ---- SETTINGS — How is the system configured? ---- */
    Serial.println("[UI] settings…");
    settingsScreen = makeScreen();
    uiBuildPump("[UI] settings");
    createDock(settingsScreen);
    lv_obj_t *setSum = os_.makePageChrome(settingsScreen, "SETTINGS");
    os_.makeCaption(setSum, "Configuration and preferences", 10, 6);
    timeoutLabel = makeLabel(setSum, "Auto backlight: 10 min", 10, 28);
    lv_obj_add_style(timeoutLabel, &os_.body, 0);

    lv_obj_t *settings = os_.makePrimaryPanel(settingsScreen);
    os_.makeHeading(settings, "DISPLAY", OS_PAD, 2);
    makeButton(settings, "Never",  OS_PAD,       24, 68, 36, "SETTINGS:TIMEOUT:0");
    makeButton(settings, "1m",     OS_PAD+ 76,   24, 50, 36, "SETTINGS:TIMEOUT:1");
    makeButton(settings, "3m",     OS_PAD+134,   24, 50, 36, "SETTINGS:TIMEOUT:3");
    makeButton(settings, "5m",     OS_PAD+192,   24, 50, 36, "SETTINGS:TIMEOUT:5");
    makeButton(settings, "10m",    OS_PAD+250,   24, 58, 36, "SETTINGS:TIMEOUT:10");
    makeButton(settings, "30m",    OS_PAD+316,   24, 58, 36, "SETTINGS:TIMEOUT:30");
    makeButton(settings, "Cycle",  OS_PAD+382,   24, 52, 36, "SETTINGS:TIMEOUT:CYCLE");
    os_.makeCaption(settings, "Timeout: dim at half, off at limit.", OS_PAD, 66);

    /* Brightness controls */
    os_.makeCaption(settings, "Brightness", OS_PAD + 470, 4);
    brightnessLabel_ = makeLabel(settings, "255", OS_PAD + 470, 22);
    lv_obj_add_style(brightnessLabel_, &os_.body, 0);
    makeButton(settings, "\xe2\x96\xbd", OS_PAD + 516, 16, 40, 34, "UI:SETTINGS:BRIGHTNESS:DOWN");
    makeButton(settings, "\xe2\x96\xb2", OS_PAD + 562, 16, 40, 34, "UI:SETTINGS:BRIGHTNESS:UP");

    /* System identity */
    os_.makeHeading(settings, "SYSTEM", OS_PAD, 82);
    settingsDirNameLabel_ = makeLabel(settings, "Director: —", OS_PAD, 104);
    lv_obj_add_style(settingsDirNameLabel_, &os_.caption, 0);
    lv_obj_set_width(settingsDirNameLabel_, OS_CONTENT_FULL_W - OS_PAD * 2 - 24);

    /* Navigation */
    os_.makeHeading(settings, "SECTIONS", OS_PAD, 122);
    makeButton(settings, "Audio",       OS_PAD,       148, 120, 44, "SCREEN:AUDIO");
    makeButton(settings, "Logs",        OS_PAD+128,   148, 120, 44, "SCREEN:LOGS");
    makeButton(settings, "Nodes",       OS_PAD+256,   148, 120, 44, "SCREEN:NODES");
    makeButton(settings, "Diagnostics", OS_PAD+384,   148, 140, 44, "SCREEN:DIAG");
    makeButton(settings, "Maintenance", OS_PAD,       200, 160, 44, "SCREEN:MAINT");
    makeButton(settings, "About",       OS_PAD+168,   200, 120, 44, "SCREEN:ABOUT");
    makeButton(settings, "E-Stop Clear",OS_PAD+396,   200, 168, 44, "EMERGENCY:CLEAR");

    refreshTimeoutLabel();
    refreshSettingsDisplayValues_();
    uiBuildPump();

    Serial.println("[UI] about…");
    buildAboutPage();
    uiBuildPump("[UI] about");
    Serial.println("[UI] maintenance…");
    buildMaintenancePage();
    uiBuildPump("[UI] maintenance");
    Serial.println("[UI] logs…");
    buildLogsPage();
    uiBuildPump("[UI] logs");
    Serial.println("[UI] audio…");
    buildAudioPage();
    uiBuildPump("[UI] audio");
    refreshLogsDisplay();
    refreshAudioPresentation();
    refreshDesktopFabric();
    refreshNodesPresentationIfActive();
    refreshDiagnosticsSummary();

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
      if (abortConfirmRoot && !lv_obj_has_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN) &&
          !emergencyOverlayVisible) {
        lv_obj_move_foreground(abortConfirmRoot);
      } else if (emergencyOverlayVisible) {
        hideAbortConfirm();
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
    lv_label_set_text(sub,
        "Stage halted. Clear the emergency, then Resume show or Abort.\n"
        "All outputs are in safe-state pending Stage confirmation.");
    lv_obj_set_style_text_color(sub, lv_color_hex(0xFECACA), 0);
    lv_obj_set_pos(sub, 40, 183);
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

  void buildActionConfirm() {
    if (actionConfirmRoot) return;
    lv_obj_t *top = lv_layer_top();
    actionConfirmRoot = lv_obj_create(top);
    lv_obj_remove_style_all(actionConfirmRoot);
    lv_obj_set_size(actionConfirmRoot, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(actionConfirmRoot, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(actionConfirmRoot, LV_OPA_70, 0);
    lv_obj_add_flag(actionConfirmRoot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(actionConfirmRoot, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *box = lv_obj_create(actionConfirmRoot);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 520, 240);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(OsColor::Panel), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(OsColor::DangerBorder), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 12, 0);

    actionConfirmTitleLabel_ = lv_label_create(box);
    lv_label_set_text(actionConfirmTitleLabel_, "CONFIRM ACTION");
    lv_obj_add_style(actionConfirmTitleLabel_, &os_.title, 0);
    lv_obj_set_style_text_color(actionConfirmTitleLabel_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(actionConfirmTitleLabel_, 20, 18);
    lv_obj_set_width(actionConfirmTitleLabel_, 480);

    actionConfirmBodyLabel_ = lv_label_create(box);
    lv_label_set_text(actionConfirmBodyLabel_, "This action can affect operation.");
    lv_obj_add_style(actionConfirmBodyLabel_, &os_.caption, 0);
    lv_obj_set_style_text_color(actionConfirmBodyLabel_, lv_color_hex(0xD1D5DB), 0);
    lv_obj_set_pos(actionConfirmBodyLabel_, 20, 62);
    lv_obj_set_width(actionConfirmBodyLabel_, 480);
    lv_label_set_long_mode(actionConfirmBodyLabel_, LV_LABEL_LONG_WRAP);

    makeButton(box, "Cancel", 20, 178, 180, 44, "UI:CONFIRM:CANCEL");
    makeButton(box, "Confirm", 320, 178, 180, 44, "UI:CONFIRM:OK", true);
  }

  void hideActionConfirm() {
    if (actionConfirmRoot) lv_obj_add_flag(actionConfirmRoot, LV_OBJ_FLAG_HIDDEN);
    actionConfirmVisible_ = false;
    actionConfirmCommand_[0] = '\0';
  }

  void showActionConfirm(const char *title, const char *body, const char *confirmCommand) {
    if (!confirmCommand || !confirmCommand[0]) return;
    if (emergencyLocked || emergencyOverlayVisible || mirroredState == SHOW_STATE_EMERGENCY_STOP) {
      pushOperatorEvent("Action blocked — emergency active");
      return;
    }
    if (!actionConfirmRoot) buildActionConfirm();
    if (!actionConfirmRoot) return;
    strncpy(actionConfirmCommand_, confirmCommand, sizeof(actionConfirmCommand_) - 1);
    actionConfirmCommand_[sizeof(actionConfirmCommand_) - 1] = '\0';
    if (actionConfirmTitleLabel_) {
      lv_label_set_text(actionConfirmTitleLabel_, title && title[0] ? title : "CONFIRM ACTION");
    }
    if (actionConfirmBodyLabel_) {
      lv_label_set_text(actionConfirmBodyLabel_, body && body[0] ? body : "Confirm this action.");
    }
    lv_obj_clear_flag(actionConfirmRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(actionConfirmRoot);
    actionConfirmVisible_ = true;
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
      lv_bar_set_value(liveProgressBar, liveProgressPct, LV_ANIM_OFF);
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
    hideActionConfirm();
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
      case DeskPage::Details: showDetails(); break;
      case DeskPage::More: showMore(); break;
      case DeskPage::Nodes: showNodes(); break;
      case DeskPage::NodeDetails: showNodeDetails(); break;
      case DeskPage::Diagnostics: showDiagnostics(); break;
      case DeskPage::Settings: showSettings(); break;
      case DeskPage::Maintenance: showMaintenance(); break;
      case DeskPage::About: showAbout(); break;
      case DeskPage::Audio: showAudio(); break;
      case DeskPage::Logs: showLogs(); break;
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
    const char *audioEmergency = (mirroredState == SHOW_STATE_EMERGENCY_STOP || emergencyLocked)
                                   ? "ACTIVE"
                                   : "Not reported";
    const char *safeState = emergencyLocked
                              ? "Awaiting Stage confirmation"
                              : "Stage reported clear";
    char detail[520];
    snprintf(detail, sizeof(detail),
             "Show: %s\n"
             "Source: Not reported\n"
             "Reason: Not reported\n"
             "Playback before stop: %s\n"
             "Current cue: %u / %u\n"
             "Elapsed: %s    Remaining: %s\n"
             "Stage link: %s\n"
             "Audio emergency: %s\n"
             "Output safe-state: %s\n"
             "Emergency occurred: T+%s%s%s%s%s",
             estopShowName,
             estopPlayStateBefore,
             (unsigned)estopCueIndex,
             (unsigned)estopCueTotal,
             clock, rem,
             estopStageConnected ? "CONNECTED" : "DISCONNECTED",
             audioEmergency,
             safeState,
             occurred,
             emergencyAcknowledged ? "\nAcknowledged: YES" : "\nAcknowledged: NO",
             emergencyLocked ? "\nStage: LOCKED — press CLEAR E-STOP" : "\nStage: CLEARED — Resume or Abort",
             pendingClearAwait_ ? "\nClear requested — awaiting Stage confirmation" : "",
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
    refreshSettingsDisplayValues_();
  }

  void refreshSettingsDisplayValues_() {
    if (brightnessLabel_) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%u", (unsigned)backlightBrightness());
      ShowduinoOsTheme::setTextIfChanged(brightnessLabel_, buf);
    }
  }

  void clearShowListChildren() {
    if (showListScroll == nullptr) return;
    lv_obj_clean(showListScroll);
  }

  static const char *validationStateWord(ShowValidationState state) {
    switch (state) {
      case ShowValidationState::Ready: return "Ready";
      case ShowValidationState::Warning: return "Warning";
      case ShowValidationState::Invalid: return "Invalid";
      case ShowValidationState::MissingAssets: return "Missing assets";
      case ShowValidationState::Incompatible: return "Incompatible";
      case ShowValidationState::NotValidated:
      default: return "Not validated";
    }
  }

  static lv_color_t validationStateColor(ShowValidationState state) {
    switch (state) {
      case ShowValidationState::Ready: return lv_color_hex(OsColor::Ok);
      case ShowValidationState::Warning: return lv_color_hex(OsColor::Warn);
      case ShowValidationState::Invalid:
      case ShowValidationState::MissingAssets:
      case ShowValidationState::Incompatible: return lv_color_hex(OsColor::Fault);
      case ShowValidationState::NotValidated:
      default: return lv_color_hex(OsColor::TextMuted);
    }
  }

  static const char *showLoadStateWord(ShowLoadUiState state) {
    switch (state) {
      case ShowLoadUiState::Ready: return "Ready to load";
      case ShowLoadUiState::LoadRequested: return "Load requested";
      case ShowLoadUiState::Loading: return "Loading";
      case ShowLoadUiState::Loaded: return "Show loaded";
      case ShowLoadUiState::Warning: return "Warning";
      case ShowLoadUiState::Failed: return "Failed";
      case ShowLoadUiState::AwaitingStage: return "Awaiting Stage";
      case ShowLoadUiState::TimedOut: return "Timed out";
      default: return "Unknown";
    }
  }

  void updateShowsHeader(uint8_t packageCount) {
    if (showsListTitle) {
      char title[64];
      snprintf(title, sizeof(title), "Discovered: %u package%s",
               (unsigned)packageCount, packageCount == 1 ? "" : "s");
      ShowduinoOsTheme::setTextIfChanged(showsListTitle, title);
    }
    if (showsStorageLabel_) {
      char storage[120];
      if (showStorageRecovery_ || !showStorageMounted_) {
        snprintf(storage, sizeof(storage), "Storage: Unavailable");
      } else if (showStorageCardType_[0]) {
        snprintf(storage, sizeof(storage), "Storage: SD %s", showStorageCardType_);
      } else {
        snprintf(storage, sizeof(storage), "Storage: Mounted");
      }
      ShowduinoOsTheme::setTextIfChanged(showsStorageLabel_, storage);
    }
    if (showsScanLabel_) {
      char scanLine[140];
      if (!showLibraryScanned_) {
        snprintf(scanLine, sizeof(scanLine), "Last scan: Not scanned");
      } else if (showLibraryScanOk_) {
        snprintf(scanLine, sizeof(scanLine), "Last scan: %s",
                 showLibraryScanNote_[0] ? showLibraryScanNote_ : "Complete");
      } else {
        snprintf(scanLine, sizeof(scanLine), "Last scan: Failed — %s",
                 showLibraryScanNote_[0] ? showLibraryScanNote_ : "check SD status");
      }
      ShowduinoOsTheme::setTextIfChanged(showsScanLabel_, scanLine);
    }
  }

  void renderShowsEmptyState(const char *headline, const char *detail) {
    if (!showListScroll) return;
    lv_obj_t *h = makeLabel(showListScroll, headline ? headline : "No packages", 8, 8);
    lv_obj_add_style(h, &os_.title, 0);
    lv_obj_set_width(h, OS_CONTENT_FULL_W - 52);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_obj_t *d = makeLabel(showListScroll, detail ? detail : "", 8, 36);
    lv_obj_add_style(d, &os_.caption, 0);
    lv_obj_set_width(d, OS_CONTENT_FULL_W - 52);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
  }

  bool showNameMatchesSelection(const char *name) const {
    if (!selectedShowValid_ || !name || !name[0]) return false;
    if (strcmp(name, selectedShowIdBuf) == 0) return true;
    if (!selectedShowEntry_.name[0]) return false;
    if (strcmp(name, selectedShowEntry_.name) == 0) return true;
    size_t runtimeLen = strlen(name);
    size_t selectedLen = strlen(selectedShowEntry_.name);
    if (runtimeLen > 0 && runtimeLen < selectedLen &&
        strncmp(selectedShowEntry_.name, name, runtimeLen) == 0) {
      return true;
    }
    return false;
  }

  bool selectedShowAppearsLoaded() const {
    if (!selectedShowValid_) return false;
    if (showLoadState_ == ShowLoadUiState::Loaded &&
        showLoadTargetId_[0] && strcmp(showLoadTargetId_, selectedShowIdBuf) == 0) {
      return true;
    }
    if ((mirroredState == SHOW_STATE_SHOW_LOADED ||
         mirroredState == SHOW_STATE_RUNNING ||
         mirroredState == SHOW_STATE_PAUSED ||
         mirroredState == SHOW_STATE_FINISHED) &&
        showNameMatchesSelection(loadedShowNameBuf)) {
      return true;
    }
    return false;
  }

  void updateShowLoadStateFromRuntime(const ShowRuntime &rt) {
    bool stateChanged = false;
    if (showLoadRequested_) {
      if ((rt.state == SHOW_STATE_SHOW_LOADED || rt.state == SHOW_STATE_RUNNING ||
           rt.state == SHOW_STATE_PAUSED || rt.state == SHOW_STATE_FINISHED) &&
          showNameMatchesSelection(rt.showName)) {
        showLoadState_ = ShowLoadUiState::Loaded;
        showLoadRequested_ = false;
        strncpy(showLoadTargetId_, selectedShowIdBuf, sizeof(showLoadTargetId_) - 1);
        showLoadTargetId_[sizeof(showLoadTargetId_) - 1] = '\0';
        strncpy(showLoadStatusText_, "Show Loaded — open Live when ready",
                sizeof(showLoadStatusText_) - 1);
        showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
        stateChanged = true;
      } else if (rt.state == SHOW_STATE_ERROR) {
        showLoadState_ = ShowLoadUiState::Failed;
        showLoadRequested_ = false;
        if (rt.lastError[0]) strncpy(showLoadStatusText_, rt.lastError, sizeof(showLoadStatusText_) - 1);
        else strncpy(showLoadStatusText_, "Load failed on Stage", sizeof(showLoadStatusText_) - 1);
        showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
        stateChanged = true;
      } else if (showLoadState_ == ShowLoadUiState::LoadRequested) {
        showLoadState_ = ShowLoadUiState::Loading;
        strncpy(showLoadStatusText_, "Loading — waiting for Stage confirmation",
                sizeof(showLoadStatusText_) - 1);
        showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
        stateChanged = true;
      }
    }

    if (!showLoadRequested_ && selectedShowValid_) {
      if ((rt.state == SHOW_STATE_SHOW_LOADED || rt.state == SHOW_STATE_RUNNING ||
           rt.state == SHOW_STATE_PAUSED || rt.state == SHOW_STATE_FINISHED) &&
          showNameMatchesSelection(rt.showName) &&
          showLoadState_ != ShowLoadUiState::Loaded) {
        showLoadState_ = ShowLoadUiState::Loaded;
        strncpy(showLoadTargetId_, selectedShowIdBuf, sizeof(showLoadTargetId_) - 1);
        showLoadTargetId_[sizeof(showLoadTargetId_) - 1] = '\0';
        strncpy(showLoadStatusText_, "Show Loaded — open Live when ready",
                sizeof(showLoadStatusText_) - 1);
        showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
        stateChanged = true;
      }
    }
    if (stateChanged) refreshShowDetailsPresentation();
  }

  void rebuildShowList(const ShowManager &sm) {
    updateShowsHeader(sm.size());
    showListCount = 0;
    clearShowListChildren();

    if (showsSummaryLabel_) {
      if (!showStorageMounted_ || showStorageRecovery_) {
        ShowduinoOsTheme::setTextIfChanged(
            showsSummaryLabel_,
            "SD unavailable. Restore storage, then refresh library.");
      } else if (!showLibraryScanned_) {
        ShowduinoOsTheme::setTextIfChanged(
            showsSummaryLabel_,
            "Library not scanned yet. Tap Refresh Library.");
      } else if (!showLibraryScanOk_) {
        ShowduinoOsTheme::setTextIfChanged(
            showsSummaryLabel_,
            "Library scan failed. Review SD state and retry.");
      } else {
        ShowduinoOsTheme::setTextIfChanged(
            showsSummaryLabel_,
            "Select package → inspect details → load → open Live");
      }
    }

    if (!showStorageMounted_ || showStorageRecovery_) {
      renderShowsEmptyState("No SD card / storage unavailable",
                            showStorageLastError_[0] ? showStorageLastError_ : "Director is in storage recovery mode.");
      return;
    }
    if (!showLibraryScanned_) {
      renderShowsEmptyState("Library not yet scanned",
                            "Tap Refresh Library to discover show packages.");
      return;
    }
    if (!showLibraryScanOk_) {
      renderShowsEmptyState("Library scan failed",
                            showLibraryScanNote_[0] ? showLibraryScanNote_ : "SD scan reported an error.");
      return;
    }
    if (sm.size() == 0) {
      renderShowsEmptyState("No show packages found",
                            "No packages exist under /showduino/shows/packages.");
      return;
    }

    const int cardW = OS_CONTENT_FULL_W - 36;
    const int cardH = 112;
    int y = 4;
    uint8_t validCount = 0;
    for (uint8_t i = 0; i < sm.size() && showListCount < SHOW_INDEX_MAX; i++) {
      const ShowIndexEntry *e = sm.get(i);
      if (!e) continue;
      showListCache[showListCount] = *e;
      sm.validateShowPackage(e->id, showValidationCache_[showListCount]);
      if (showValidationCache_[showListCount].state == ShowValidationState::Ready) validCount++;
      snprintf(showOpenCmds[showListCount], sizeof(showOpenCmds[showListCount]),
               "UI:SHOW:OPEN:%s", e->id);

      const bool isSelected = selectedShowValid_ && strcmp(selectedShowIdBuf, e->id) == 0;
      const bool isLoaded = (loadedShowNameBuf[0] && strcmp(loadedShowNameBuf, e->name) == 0);
      lv_obj_t *card = lv_obj_create(showListScroll);
      lv_obj_remove_style_all(card);
      lv_obj_set_pos(card, 4, y);
      lv_obj_set_size(card, cardW, cardH);
      lv_obj_set_style_bg_color(card,
                                lv_color_hex(isSelected ? 0x10323F : OsColor::Button), 0);
      lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(card,
                                    lv_color_hex(isSelected ? OsColor::Accent : OsColor::PanelBorder), 0);
      lv_obj_set_style_border_width(card, isSelected ? 2 : 1, 0);
      lv_obj_set_style_radius(card, OS_BTN_RADIUS, 0);

      ShowduinoShowThumb::makeDefaultIcon(card, 8, 8, 72, 48);
      if (e->hasThumbnail) {
        lv_obj_t *badge = lv_label_create(card);
        lv_label_set_text(badge, "ART");
        lv_obj_set_style_text_color(badge, lv_color_hex(OsColor::Ok), 0);
        lv_obj_set_pos(badge, 10, 60);
      }

      char dur[16];
      ShowduinoShowThumb::formatDuration(e->durationSeconds, dur, sizeof(dur));

      lv_obj_t *title = lv_label_create(card);
      lv_label_set_text(title, e->name[0] ? e->name : e->id);
      lv_obj_set_style_text_color(title, lv_color_hex(OsColor::Title), 0);
      lv_obj_set_pos(title, 88, 6);
      lv_obj_set_width(title, cardW - 220);
      lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);

      char info[196];
      snprintf(info, sizeof(info), "%s  ·  cues %u  ·  v%s  ·  %s",
               dur,
               (unsigned)e->cueCount,
               e->version[0] ? e->version : "Unknown",
               e->author[0] ? e->author : "Unknown");
      lv_obj_t *meta = lv_label_create(card);
      lv_label_set_text(meta, info);
      lv_obj_set_style_text_color(meta, lv_color_hex(OsColor::TextMuted), 0);
      lv_obj_set_pos(meta, 88, 28);
      lv_obj_set_width(meta, cardW - 220);
      lv_label_set_long_mode(meta, LV_LABEL_LONG_CLIP);

      char desc[120];
      if (e->description[0]) snprintf(desc, sizeof(desc), "%.96s", e->description);
      else snprintf(desc, sizeof(desc), "Description unavailable");
      lv_obj_t *descLabel = lv_label_create(card);
      lv_label_set_text(descLabel, desc);
      lv_obj_set_style_text_color(descLabel, lv_color_hex(OsColor::TextDim), 0);
      lv_obj_set_pos(descLabel, 88, 48);
      lv_obj_set_width(descLabel, cardW - 220);
      lv_label_set_long_mode(descLabel, LV_LABEL_LONG_CLIP);

      const ShowValidationResult &vr = showValidationCache_[showListCount];
      char statusLine[196];
      snprintf(statusLine, sizeof(statusLine), "Validation: %s  ·  Compatibility: %s  ·  Modified: %s",
               validationStateWord(vr.state),
               vr.compatible ? "Compatible" : "Incompatible",
               e->modified[0] ? e->modified : "Unavailable");
      lv_obj_t *stateLab = lv_label_create(card);
      lv_label_set_text(stateLab, statusLine);
      lv_obj_set_style_text_color(stateLab, validationStateColor(vr.state), 0);
      lv_obj_set_pos(stateLab, 88, 68);
      lv_obj_set_width(stateLab, cardW - 220);
      lv_label_set_long_mode(stateLab, LV_LABEL_LONG_CLIP);

      if (isLoaded) {
        lv_obj_t *loaded = lv_label_create(card);
        lv_label_set_text(loaded, "LOADED");
        lv_obj_set_style_text_color(loaded, lv_color_hex(OsColor::Ok), 0);
        lv_obj_set_pos(loaded, cardW - 128, 8);
      }

      lv_obj_t *openBtn = makeButton(card, "View Details", cardW - 126, 66, 116, 36,
                                     showOpenCmds[showListCount]);
      (void)openBtn;

      showListCount++;
      y += cardH + 8;
      yield();
    }

    if (validCount == 0 && showsSummaryLabel_) {
      ShowduinoOsTheme::setTextIfChanged(
          showsSummaryLabel_,
          "Packages discovered, but all failed validation. Open details to inspect issues.");
    }

    const ShowIndexEntry *selected = cachedShow(selectedShowIdBuf);
    if (!selectedShowIdBuf[0] || !selected) {
      selectedShowValid_ = false;
      selectedShowIdBuf[0] = '\0';
      memset(&selectedShowEntry_, 0, sizeof(selectedShowEntry_));
      memset(&selectedShowValidation_, 0, sizeof(selectedShowValidation_));
      showLoadState_ = ShowLoadUiState::Ready;
      showLoadRequested_ = false;
      showLoadTargetId_[0] = '\0';
      strncpy(showLoadStatusText_, "Ready to load", sizeof(showLoadStatusText_) - 1);
      showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
    } else {
      selectedShowValid_ = true;
      selectedShowEntry_ = *selected;
      for (uint8_t i = 0; i < showListCount; i++) {
        if (strcmp(showListCache[i].id, selectedShowEntry_.id) == 0) {
          selectedShowValidation_ = showValidationCache_[i];
          break;
        }
      }
    }
  }

  const ShowIndexEntry *cachedShow(const char *id) const {
    if (!id) return nullptr;
    for (uint8_t i = 0; i < showListCount; i++) {
      if (strcmp(showListCache[i].id, id) == 0) return &showListCache[i];
    }
    return nullptr;
  }

  const ShowValidationResult *cachedValidation(const char *id) const {
    if (!id) return nullptr;
    for (uint8_t i = 0; i < showListCount; i++) {
      if (strcmp(showListCache[i].id, id) == 0) return &showValidationCache_[i];
    }
    return nullptr;
  }

  String requirementFieldFromManifest(const String &manifest, const char *key) const {
    String asString = ShowduinoFileUtil::jsonGetString(manifest, key, "");
    if (asString.length() > 0) return asString;

    String pattern = String("\"") + key + "\"";
    int k = manifest.indexOf(pattern);
    if (k < 0) return "";
    int colon = manifest.indexOf(':', k + pattern.length());
    if (colon < 0) return "";
    int s = colon + 1;
    while (s < (int)manifest.length() && (manifest[s] == ' ' || manifest[s] == '\t')) s++;
    if (s >= (int)manifest.length()) return "";
    if (manifest[s] == '[') {
      int end = manifest.indexOf(']', s + 1);
      if (end < 0) return "";
      String body = manifest.substring(s + 1, end);
      body.trim();
      if (!body.length()) return "";
      body.replace("\"", "");
      body.replace(",", ", ");
      return body;
    }
    return "";
  }

  void refreshShowDetailsPresentation() {
    if (!detailsNameLabel || !detailsDescLabel || !detailsMetaLabel ||
        !detailsRequirementsLabel_ || !detailsValidationLabel_ || !timelineDetailLabel) {
      return;
    }

    if (!selectedShowValid_) {
      lv_label_set_text(detailsNameLabel, "No show selected");
      if (detailsSummaryLabel_) lv_label_set_text(detailsSummaryLabel_, "Select a package from Shows.");
      lv_label_set_text(detailsDescLabel, "Description unavailable");
      lv_label_set_text(detailsMetaLabel,
                        "Duration: Unknown\nCue count: Unknown\nPackage: Unselected\nLoaded: No");
      lv_label_set_text(detailsRequirementsLabel_,
                        "Requirements\nNodes: Not specified\nAudio assets: Not specified\nLighting: Not specified\nStorage: Not specified\nMinimum package: Not specified");
      lv_label_set_text(detailsValidationLabel_, "Validation: Not validated");
      lv_obj_set_style_text_color(detailsValidationLabel_, lv_color_hex(OsColor::TextMuted), 0);
      lv_label_set_text(timelineDetailLabel, "Load state: Ready to load");
      if (detailsLoadBtn_) lv_obj_add_state(detailsLoadBtn_, LV_STATE_DISABLED);
      if (detailsOpenLiveBtn_) lv_obj_add_flag(detailsOpenLiveBtn_, LV_OBJ_FLAG_HIDDEN);
      if (detailsIconHost) {
        if (detailsCanvas) {
          ShowduinoShowThumb::freeCanvasBuffer(detailsCanvas);
          detailsCanvas = nullptr;
        }
        lv_obj_clean(detailsIconHost);
        ShowduinoShowThumb::makeDefaultIcon(detailsIconHost, 4, 4, 96, 64);
      }
      return;
    }

    lv_label_set_text(detailsNameLabel, selectedShowEntry_.name[0] ? selectedShowEntry_.name : selectedShowEntry_.id);
    if (detailsSummaryLabel_) {
      char summary[120];
      snprintf(summary, sizeof(summary), "Package: %s  ·  Selected: YES  ·  Loaded: %s",
               selectedShowEntry_.id,
               selectedShowAppearsLoaded() ? "YES" : "NO");
      lv_label_set_text(detailsSummaryLabel_, summary);
    }
    lv_label_set_text(detailsDescLabel,
                      selectedShowEntry_.description[0] ? selectedShowEntry_.description : "Description unavailable");

    char dur[16];
    ShowduinoShowThumb::formatDuration(selectedShowEntry_.durationSeconds, dur, sizeof(dur));
    char meta[260];
    snprintf(meta, sizeof(meta),
             "Duration: %s\nCue count: %u\nAuthor: %s\nVersion: %s\nPackage ID: %s\nModified: %s",
             dur,
             (unsigned)selectedShowEntry_.cueCount,
             selectedShowEntry_.author[0] ? selectedShowEntry_.author : "Unavailable",
             selectedShowEntry_.version[0] ? selectedShowEntry_.version : "Unavailable",
             selectedShowEntry_.id,
             selectedShowEntry_.modified[0] ? selectedShowEntry_.modified : "Unavailable");
    lv_label_set_text(detailsMetaLabel, meta);

    String manifest;
    String reqNodes = "Not specified";
    String reqAudio = "Not specified";
    String reqLighting = "Not specified";
    String reqStorage = "Not specified";
    String reqMinVersion = "Not specified";
    if (selectedShowEntry_.path[0] && ShowduinoFileUtil::readTextFile(selectedShowEntry_.path, manifest)) {
      String nodes = requirementFieldFromManifest(manifest, "requiredNodes");
      String audio = requirementFieldFromManifest(manifest, "requiredAudio");
      String lighting = requirementFieldFromManifest(manifest, "requiredLighting");
      String storage = requirementFieldFromManifest(manifest, "requiredStorage");
      String minVersion = requirementFieldFromManifest(manifest, "minimumVersion");
      if (!minVersion.length()) minVersion = requirementFieldFromManifest(manifest, "minPackageVersion");
      bool stageReq = ShowduinoFileUtil::jsonGetBool(manifest, "stageControllerRequired", true);
      if (nodes.length()) reqNodes = nodes;
      else if (stageReq) reqNodes = "Stage Controller (Brain)";
      if (audio.length()) reqAudio = audio;
      if (lighting.length()) reqLighting = lighting;
      if (storage.length()) reqStorage = storage;
      if (minVersion.length()) reqMinVersion = minVersion;
    }

    String requirements = String("Requirements\nNodes: ") + reqNodes +
                          "\nAudio assets: " + reqAudio +
                          "\nLighting: " + reqLighting +
                          "\nStorage: " + reqStorage +
                          "\nMinimum package: " + reqMinVersion;
    lv_label_set_text(detailsRequirementsLabel_, requirements.c_str());

    char validation[220];
    snprintf(validation, sizeof(validation),
             "Validation: %s\nCompatibility: %s\nIssue: %s",
             validationStateWord(selectedShowValidation_.state),
             selectedShowValidation_.compatible ? "Compatible" : "Incompatible",
             selectedShowValidation_.detail[0] ? selectedShowValidation_.detail : "None");
    lv_label_set_text(detailsValidationLabel_, validation);
    lv_obj_set_style_text_color(detailsValidationLabel_,
                                validationStateColor(selectedShowValidation_.state), 0);

    char loadLine[220];
    snprintf(loadLine, sizeof(loadLine), "Load state: %s\n%s",
             showLoadStateWord(showLoadState_),
             showLoadStatusText_[0] ? showLoadStatusText_ : "Ready to load");
    lv_label_set_text(timelineDetailLabel, loadLine);

    if (detailsLoadBtn_) {
      const bool busy = (showLoadState_ == ShowLoadUiState::LoadRequested ||
                         showLoadState_ == ShowLoadUiState::Loading ||
                         showLoadState_ == ShowLoadUiState::AwaitingStage);
      const bool validationBlocksLoad =
          (selectedShowValidation_.state == ShowValidationState::Invalid ||
           selectedShowValidation_.state == ShowValidationState::MissingAssets ||
           selectedShowValidation_.state == ShowValidationState::Incompatible);
      if (busy || validationBlocksLoad) lv_obj_add_state(detailsLoadBtn_, LV_STATE_DISABLED);
      else lv_obj_clear_state(detailsLoadBtn_, LV_STATE_DISABLED);
    }
    if (detailsOpenLiveBtn_) {
      if (selectedShowAppearsLoaded()) lv_obj_clear_flag(detailsOpenLiveBtn_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(detailsOpenLiveBtn_, LV_OBJ_FLAG_HIDDEN);
    }

    if (detailsIconHost) {
      if (detailsCanvas) {
        ShowduinoShowThumb::freeCanvasBuffer(detailsCanvas);
        detailsCanvas = nullptr;
      }
      lv_obj_clean(detailsIconHost);
      bool showedBmp = false;
      if (selectedShowEntry_.hasThumbnail) {
        char thumb[STORAGE_MAX_PATH_LEN];
        const int pathLen = snprintf(thumb, sizeof(thumb), "%s/thumbnail.bmp", selectedShowEntry_.folder);
        if (pathLen > 0 && pathLen < (int)sizeof(thumb)) {
          detailsCanvas = lv_canvas_create(detailsIconHost);
          lv_obj_set_pos(detailsCanvas, 4, 4);
          if (ShowduinoShowThumb::loadBmpToCanvas(detailsCanvas, thumb, 96, 64)) {
            showedBmp = true;
          } else {
            ShowduinoShowThumb::freeCanvasBuffer(detailsCanvas);
            lv_obj_delete(detailsCanvas);
            detailsCanvas = nullptr;
          }
        } else {
          pushOperatorEvent("Thumbnail path too long");
        }
      }
      if (!showedBmp) ShowduinoShowThumb::makeDefaultIcon(detailsIconHost, 4, 4, 96, 64);
    }
  }

  void openShowDetails(const char *showId) {
    if (!showId || !showId[0]) return;
    const ShowIndexEntry *e = cachedShow(showId);
    if (!e) return;
    strncpy(selectedShowIdBuf, showId, sizeof(selectedShowIdBuf) - 1);
    selectedShowIdBuf[sizeof(selectedShowIdBuf) - 1] = '\0';
    selectedShowValid_ = true;
    selectedShowEntry_ = *e;
    const ShowValidationResult *vr = cachedValidation(showId);
    if (vr) selectedShowValidation_ = *vr;
    else {
      memset(&selectedShowValidation_, 0, sizeof(selectedShowValidation_));
      selectedShowValidation_.state = ShowValidationState::NotValidated;
      selectedShowValidation_.compatible = true;
      strncpy(selectedShowValidation_.summary, "Not validated",
              sizeof(selectedShowValidation_.summary) - 1);
      strncpy(selectedShowValidation_.detail, "Validation data unavailable",
              sizeof(selectedShowValidation_.detail) - 1);
    }
    if (selectedShowAppearsLoaded()) {
      showLoadState_ = ShowLoadUiState::Loaded;
      strncpy(showLoadTargetId_, selectedShowIdBuf, sizeof(showLoadTargetId_) - 1);
      showLoadTargetId_[sizeof(showLoadTargetId_) - 1] = '\0';
      strncpy(showLoadStatusText_, "Show Loaded — open Live when ready",
              sizeof(showLoadStatusText_) - 1);
    } else {
      showLoadState_ = ShowLoadUiState::Ready;
      if (strcmp(showLoadTargetId_, selectedShowIdBuf) != 0) showLoadTargetId_[0] = '\0';
      strncpy(showLoadStatusText_, "Ready to load", sizeof(showLoadStatusText_) - 1);
    }
    showLoadStatusText_[sizeof(showLoadStatusText_) - 1] = '\0';
    showLoadRequested_ = false;
    refreshShowDetailsPresentation();

    showDetails();
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
  void showDetails() {
    notePage(DeskPage::Details);
    lv_screen_load(showDetailsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showMore() {
    if (!moreScreen) return;
    notePage(DeskPage::More);
    lv_screen_load(moreScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showNodes() {
    notePage(DeskPage::Nodes);
    refreshNodesPresentationIfActive();
    lv_screen_load(nodesScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showNodeDetails() {
    notePage(DeskPage::NodeDetails);
    refreshNodeDetailsPresentation();
    lv_screen_load(nodeDetailsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showDiagnostics() {
    notePage(DeskPage::Diagnostics);
    refreshDiagnosticsSummary();
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
  void showMaintenance() {
    notePage(DeskPage::Maintenance);
    lv_screen_load(maintenanceScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showAbout() {
    notePage(DeskPage::About);
    lv_screen_load(aboutScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showAudio() {
    notePage(DeskPage::Audio);
    lv_screen_load(audioScreen);
    refreshAudioPresentation();
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showLogs() {
    notePage(DeskPage::Logs);
    lv_screen_load(logsScreen);
    refreshLogsDisplay();
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
};

#endif
