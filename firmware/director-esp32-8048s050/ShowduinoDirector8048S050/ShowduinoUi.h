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
#include "DirectorAudioModel.h"
#include "ShowduinoOsUi.h"
#include "DisplayManager.h"
#include "DirectorEmergencyScreen.h"
#include "touch_lvgl.h"
#include "page_01_home.h"
#include "page_02_productions.h"
#include "showduino_theme.h"
#include "showduino_capabilities.h"

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
    displaySelf_ = this;
    gDirectorEmergencyScreen.setClearRequestHandler(emergencyClearThunk);
    gDirectorEmergencyScreen.setFinishedHandler(emergencyFinishedThunk);
    displayManager_.begin();
    displayManager_.setCommandHandler(displayCommandThunk);
    touchLvglSetHook(displayTouchHook);
    Serial.println("[UI] building screens…");
    buildScreens();
    Serial.println("[UI] loading desktop…");
    showDesktop();
    syncStatusBarHealth();
    Serial.println("[UI] ready");
  }

  void setBootTime(unsigned long startedAt) { bootMs = startedAt; }
  void setLinkState(uint8_t state) {
    const uint8_t prev = linkState;
    if (linkState == state) return;
    linkState = state;
    statusDirty = true;
    if (state == LINK_DISCONNECTED) statusBar_.noteLinkDown();
    else statusBar_.noteWaitingForSue();
    syncStatusBarHealth();
    if (state == LINK_DISCONNECTED && prev == LINK_READY &&
        !emergencyOverlayVisible && !completeOverlayVisible) {
      showConnectionLost();
    }
  }
  uint8_t getLinkState() const { return linkState; }
  void setEmergencyLocked(bool locked) {
    const bool wasLocked = emergencyLocked;
    if (wasLocked == locked) {
      if (locked) {
        refreshEmergencyOverlayContent();
        if (!gDirectorEmergencyScreen.isVisible() && !emergencyOverlayDismissed) {
          showEmergencyOverlay();
        }
      }
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
      if (pageBeforeEmergency == PAGE_NONE || pageBeforeEmergency == PAGE_EMERGENCY) {
        pageBeforeEmergency = displayManager_.currentPage();
      }
      captureEmergencySnapshot();
      gDirectorEmergencyScreen.setSource(emergencyTriggeredByDirector_
                                             ? DirectorEmergencyScreen::Source::Director
                                             : DirectorEmergencyScreen::Source::Physical);
      emergencyTriggeredByDirector_ = false;
      gDirectorEmergencyScreen.setShowName(estopShowName);
      gDirectorEmergencyScreen.setActiveSince(emergencyActiveSinceMs);
      gDirectorEmergencyScreen.setLatchActive(true, emergencyActiveSinceMs);
      showEmergencyOverlay();
      pushOperatorEvent("Emergency Activated");
      emergencyAlarmOnHook();
    } else if (!locked && wasLocked) {
      pushOperatorEvent("Emergency Cleared");
      emergencyAlarmOffHook();
      pendingAbortAwait = false;
      pendingResumeAwait = false;
      hideAbortConfirm();
      gDirectorEmergencyScreen.setLatchActive(false, millis());
      /* Stay on the emergency screen through the short CLEARED hold.
         Finished handler restores a safe page. Do not resume the show. */
      if (!gDirectorEmergencyScreen.isVisible()) {
        finishEmergencyScreenReturn();
      }
    }
    updatePersistentBanner();
    syncStatusBarHealth();
  }

  void noteEmergencyTriggeredByDirector() { emergencyTriggeredByDirector_ = true; }

  void noteEmergencyClearRejected() {
    gDirectorEmergencyScreen.noteClearRejected(millis());
    emergencyOverlayDismissed = false;
    emergencyOverlayVisible = true;
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
    /* Persistent banner pulse while runtime emergency and the full screen is not up. */
    if (persistentBannerRoot && !lv_obj_has_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN)) {
      bool flashOn = ((nowMs / 500UL) % 2UL) == 0;
      lv_obj_set_style_bg_opa(persistentBannerRoot, flashOn ? LV_OPA_COVER : LV_OPA_80, 0);
    }

    gDirectorEmergencyScreen.tick(nowMs, linkState);
    emergencyOverlayVisible = gDirectorEmergencyScreen.isVisible();

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
    /* Lightweight keyword filters — safe placeholders until typed log channels exist. */
    if (!msg) return false;
    if (logsFilter_ == 1) return true; /* System */
    if (logsFilter_ == 2) return (strstr(msg, "Show") || strstr(msg, "Cue") || strstr(msg, "Runtime"));
    if (logsFilter_ == 3) return (strstr(msg, "Audio") || strstr(msg, "AUDIO"));
    if (logsFilter_ == 4) return (strstr(msg, "ESP-NOW") || strstr(msg, "Comms") || strstr(msg, "Node") || strstr(msg, "LINK"));
    if (logsFilter_ == 5) return (strstr(msg, "Emergency") || strstr(msg, "E-STOP") || strstr(msg, "EMERGENCY"));
    return true;
  }

  void refreshLogsDisplay() {
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
    refreshLogsDisplay();
    pushOperatorEvent("Logs cleared");
  }

  void refreshAudioPresentation() {
    if (audioLocalStatusLabel_) {
      char line[96];
      snprintf(line, sizeof(line), "Status: %s%s",
               DeskAudioModel::playWord(audioModel_.local.play),
               audioModel_.local.muted ? " (MUTED)" : "");
      ShowduinoOsTheme::setTextIfChanged(audioLocalStatusLabel_, line);
    }
    if (audioLocalDetailLabel_) {
      char et[16], rt[16];
      formatClock(audioModel_.local.elapsedMs, et, sizeof(et));
      formatClock(audioModel_.local.remainMs, rt, sizeof(rt));
      char detail[320];
      snprintf(detail, sizeof(detail),
               "%s\nAsset: %s\nVol: %u  Loop: %s\nElapsed: %s  Remain: %s\nSD: %s  I2S: %s\n"
               "Commands only — files play from local SD (no ESP-NOW audio stream).",
               audioModel_.local.outputName,
               audioModel_.local.assetName,
               (unsigned)audioModel_.local.volume,
               audioModel_.local.loop ? "ON" : "OFF",
               et, rt,
               DeskAudioModel::sdWord(audioModel_.local.sd),
               DeskAudioModel::i2sWord(audioModel_.local.i2s));
      ShowduinoOsTheme::setTextIfChanged(audioLocalDetailLabel_, detail);
    }
    if (audioNodesLabel_) {
      if (audioModel_.nodeCount == 0) {
        ShowduinoOsTheme::setTextIfChanged(
            audioNodesLabel_,
            "No audio nodes discovered.\n"
            "Remote nodes = ESP32 + I2S + SD.\n"
            "P4 sends PLAY/STOP/VOLUME over ESP-NOW (commands only).");
      } else {
        String body;
        for (uint8_t i = 0; i < audioModel_.nodeCount && i < SHOWDUINO_AUDIO_NODE_MAX; i++) {
          const DeskRemoteAudioNode &n = audioModel_.nodes[i];
          if (!n.present) continue;
          char row[220];
          snprintf(row, sizeof(row),
                   "%s (%s)\nESP-NOW:%s SD:%s I2S:%s\nASSET:%s STATE:%s SYNC:%s VOL:%u\n\n",
                   n.name[0] ? n.name : "AUDIO NODE",
                   n.nodeId[0] ? n.nodeId : "?",
                   n.online ? "ONLINE" : "OFFLINE",
                   DeskAudioModel::sdWord(n.sd),
                   DeskAudioModel::i2sWord(n.i2s),
                   n.assetName,
                   DeskAudioModel::playWord(n.play),
                   DeskAudioModel::syncWord(n.sync),
                   (unsigned)n.volume);
          body += row;
        }
        ShowduinoOsTheme::setTextIfChanged(audioNodesLabel_, body.c_str());
      }
    }
    if (audioRoutingLabel_) {
      String body = "Asset source = target device SD (not streamed).\n\n";
      for (uint8_t i = 0; i < 8; i++) {
        if (!audioModel_.routes[i].used) continue;
        char row[96];
        snprintf(row, sizeof(row), "%-14s → %s\n",
                 audioModel_.routes[i].zone, audioModel_.routes[i].target);
        body += row;
      }
      ShowduinoOsTheme::setTextIfChanged(audioRoutingLabel_, body.c_str());
    }
    if (audioCmdStatusLabel_) {
      bool any = false;
      String body;
      for (uint8_t i = 0; i < 6; i++) {
        if (!audioModel_.recentCmds[i].used) continue;
        any = true;
        char row[96];
        snprintf(row, sizeof(row), "%s  %s  [%s]\n",
                 audioModel_.recentCmds[i].commandId[0] ? audioModel_.recentCmds[i].commandId : "-",
                 DeskAudioModel::cmdWord(audioModel_.recentCmds[i].phase),
                 audioModel_.recentCmds[i].summary);
        body += row;
      }
      if (!any) body = "No command status available.\n(Acks appear when Stage/nodes report them.)";
      ShowduinoOsTheme::setTextIfChanged(audioCmdStatusLabel_, body.c_str());
    }
    if (deskAudioSummaryLabel_) {
      char sum[128];
      snprintf(sum, sizeof(sum),
               "AUDIO\nLocal: %s\nNodes: %u ONLINE\nPlaying: %u",
               DeskAudioModel::playWord(audioModel_.local.play),
               (unsigned)audioModel_.onlineNodeCount(),
               (unsigned)audioModel_.playingNodeCount());
      ShowduinoOsTheme::setTextIfChanged(deskAudioSummaryLabel_, sum);
    }
  }

  void refreshDesktopFabric() {
    if (!deskFabricLabel_) return;
    const char *esp = "UNKNOWN";
    if (linkState == LINK_READY) esp = "ONLINE";
    else if (linkState == LINK_SEARCHING) esp = "SEARCHING";
    else if (linkState == LINK_DISCONNECTED) esp = "LOST";
    const char *ian = liveStageConnected ? "LINKED" : "NOT AVAILABLE";
    const char *em = (emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) ? "ACTIVE" : "CLEAR";
    char buf[220];
    snprintf(buf, sizeof(buf),
             "ESP-NOW: %s\nIAN / P4: %s\nNodes: %u / %u\nEmergency: %s\nTraffic: TX %lu / RX %lu",
             esp, ian,
             (unsigned)nodeCount, (unsigned)SHOWDUINO_EXPECTED_NODES,
             em,
             (unsigned long)txCount, (unsigned long)rxCount);
    ShowduinoOsTheme::setTextIfChanged(deskFabricLabel_, buf);
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
    if (statusBar_.root()) {
      if (displayManager_.isPhase2PageActive()) {
        lv_obj_add_flag(statusBar_.root(), LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(statusBar_.root(), LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (displayManager_.isPhase2PageActive()) {
      pushDisplaySnapshot();
      if (displayManager_.currentPage() == PAGE_LIVE) {
        refreshLiveStatusPanel();
      }
    }

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

  bool showThemedSystem(DisplayPageId page) {
    if (!displayManager_.showPage(page)) return false;
    pushDisplaySnapshot();
    return true;
  }
  void showConnectionLost() { showThemedSystem(PAGE_CONNECTION_LOST); }
  void showNoNetwork() { showThemedSystem(PAGE_NO_NETWORK); }
  void showNoSd() { showThemedSystem(PAGE_NO_SD); }
  void showLocked() { showThemedSystem(PAGE_LOCKED); }
  void showBackup() { showThemedSystem(PAGE_BACKUP); }
  void showRecovery() { showThemedSystem(PAGE_RECOVERY); }
  void showFirmwareUpdate() { showThemedSystem(PAGE_FIRMWARE_UPDATE); }

private:
  ShowduinoCommandCallback commandCallback = nullptr;
  DirectorStatusBar statusBar_;
  DisplayManager displayManager_;
  static inline ShowduinoUi *statusBarSelf_ = nullptr;
  static inline ShowduinoUi *displaySelf_ = nullptr;
  static void statusBarLogThunk(const char *msg) {
    if (statusBarSelf_) statusBarSelf_->pushOperatorEvent(msg);
  }
  static void displayCommandThunk(const char *command) {
    if (displaySelf_ && command) displaySelf_->runCommand(String(command));
  }
  static void displayTouchHook(int32_t x, int32_t y, bool pressed) {
    if (displaySelf_) displaySelf_->displayManager_.onTouch(x, y, pressed);
  }
  static void emergencyClearThunk() {
    if (displaySelf_) displaySelf_->runCommand("EMERGENCY:CLEAR");
  }
  static void emergencyFinishedThunk() {
    if (displaySelf_) displaySelf_->finishEmergencyScreenReturn();
  }
  void pushDisplaySnapshot() {
    DisplaySnapshot snap;
    displaySnapshotClear(snap);
    snap.page = displayManager_.currentPage();

    const char *tod = statusBar_.timeOfDay();
    strncpy(snap.clock, tod ? tod : "--:--:--", sizeof(snap.clock) - 1);
    const char *dod = statusBar_.dateOfDay();
    strncpy(snap.date, dod ? dod : "--- -- --- ----", sizeof(snap.date) - 1);

    const char *showVal = loadedShowNameBuf[0] ? loadedShowNameBuf : "No Show Loaded";
    strncpy(snap.currentShow, showVal, sizeof(snap.currentShow) - 1);
    strncpy(snap.runtimeState, deskRuntimeWord(), sizeof(snap.runtimeState) - 1);

    const char *safetyVal = "CLEAR";
    if (emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) safetyVal = "E-STOP";
    else if (mirroredState == SHOW_STATE_ERROR) safetyVal = "FAULT";
    strncpy(snap.safetyState, safetyVal, sizeof(snap.safetyState) - 1);

    const char *linkVal = "LINK ?";
    if (linkState == LINK_READY) linkVal = liveStageConnected ? "LINK OK" : "LINK NO STAGE";
    else if (linkState == LINK_SEARCHING) linkVal = "LINK SEARCH";
    else if (linkState == LINK_DISCONNECTED) linkVal = "LINK LOST";
    strncpy(snap.linkState, linkVal, sizeof(snap.linkState) - 1);

    snprintf(snap.cue, sizeof(snap.cue), "%lu / %lu",
             (unsigned long)liveCue, (unsigned long)liveCueTotal);
    formatClock(liveElapsedMs, snap.elapsed, sizeof(snap.elapsed));
    formatClock(liveRemainMs, snap.remain, sizeof(snap.remain));
    snprintf(snap.footer, sizeof(snap.footer), "%s | %s | Nodes %u/%u",
             snap.linkState, snap.safetyState,
             (unsigned)nodeCount, (unsigned)SHOWDUINO_EXPECTED_NODES);

    if (eventLogCount > 0 && eventLog) {
      strncpy(snap.notification, eventSlot(0), sizeof(snap.notification) - 1);
      snap.notification[sizeof(snap.notification) - 1] = '\0';
    }

    snap.nodeCount = nodeCount;
    snap.progressPct = liveProgressPct;
    displayManager_.updateWidgets(snap);

    /* Page 01 Home — refresh header / footer from real status only (no invented values). */
    if (page_01_home_is_active() && snap.page == PAGE_DESKTOP) {
      page_01_home_set_production(snap.currentShow[0] ? snap.currentShow : "NO PRODUCTION");
      if (linkState == LINK_READY) {
        page_01_home_set_link_text(liveStageConnected ? "LINK OK" : "NO STAGE");
      } else if (linkState == LINK_SEARCHING) {
        page_01_home_set_link_text("SEARCHING");
      } else {
        page_01_home_set_link_text("OFFLINE");
      }
      page_01_home_set_clock_text(snap.clock[0] ? snap.clock : "--:--");
      page_01_home_set_footer_sue(snap.runtimeState[0] ? snap.runtimeState : "—");
      page_01_home_set_footer_notify(snap.notification[0] ? snap.notification : "—");
    }
  }

  void applyThemedLiveLayout(bool themed) {
    if (!liveScreen) return;
    if (themed) {
      if (liveChromeRoot_) lv_obj_add_flag(liveChromeRoot_, LV_OBJ_FLAG_HIDDEN);
      if (liveTitleBar_) lv_obj_add_flag(liveTitleBar_, LV_OBJ_FLAG_HIDDEN);
      if (livePrimaryPanel_) {
        lv_obj_set_pos(livePrimaryPanel_, 208, 132);
        lv_obj_set_size(livePrimaryPanel_, 576, 260);
        lv_obj_set_style_bg_opa(livePrimaryPanel_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(livePrimaryPanel_, LV_OPA_TRANSP, 0);
      }
    } else {
      if (liveChromeRoot_) lv_obj_clear_flag(liveChromeRoot_, LV_OBJ_FLAG_HIDDEN);
      if (liveTitleBar_) lv_obj_clear_flag(liveTitleBar_, LV_OBJ_FLAG_HIDDEN);
      if (livePrimaryPanel_) {
        lv_obj_set_pos(livePrimaryPanel_, OS_MARGIN, OS_PRIMARY_Y);
        lv_obj_set_size(livePrimaryPanel_, OS_CONTENT_FULL_W, OS_PRIMARY_H);
        lv_obj_set_style_bg_opa(livePrimaryPanel_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_opa(livePrimaryPanel_, LV_OPA_COVER, 0);
      }
    }
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
  lv_obj_t *liveChromeRoot_ = nullptr;
  lv_obj_t *liveTitleBar_ = nullptr;
  lv_obj_t *livePrimaryPanel_ = nullptr;
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

  DisplayPageId pageBeforeEmergency = PAGE_DESKTOP;

  lv_obj_t *persistentBannerRoot = nullptr;
  lv_obj_t *persistentBannerLabel = nullptr;
  lv_obj_t *abortConfirmRoot = nullptr;
  lv_obj_t *completeOverlayRoot = nullptr;
  lv_obj_t *completeDetailLabel = nullptr;
  bool emergencyOverlayVisible = false;
  bool emergencyOverlayDismissed = false;
  bool emergencyVisitingDiag = false;
  bool emergencyAcknowledged = false;
  bool emergencyTriggeredByDirector_ = false;
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
  bool logsLivePaused_ = false;
  uint8_t logsFilter_ = 0; /* 0=All placeholder */
  DeskAudioModel audioModel_;
  lv_obj_t *audioLocalStatusLabel_ = nullptr;
  lv_obj_t *audioLocalDetailLabel_ = nullptr;
  lv_obj_t *audioNodesLabel_ = nullptr;
  lv_obj_t *audioRoutingLabel_ = nullptr;
  lv_obj_t *audioCmdStatusLabel_ = nullptr;
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
      if (emergencyLocked || gDirectorEmergencyScreen.isVisible()) {
        pushOperatorEvent("Emergency screen remains until Stage clears the latch");
        return;
      }
      emergencyVisitingDiag = true;
      hideEmergencyOverlay();
      showDiagnostics();
      if (commandCallback) commandCallback("UI:ESTOP:DIAG");
      return;
    }
    if (command == "UI:ESTOP:DESK") {
      if (emergencyLocked || gDirectorEmergencyScreen.isVisible()) {
        pushOperatorEvent("Emergency latch still active — CLEAR required");
        return;
      }
      emergencyOverlayDismissed = true;
      emergencyVisitingDiag = false;
      hideEmergencyOverlay();
      hideAbortConfirm();
      showDesktop();
      emergencySessionOpen = false;
      setShowView(DeskShowView::Idle);
      pushOperatorEvent("Returned to Desktop");
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

    if (command == "UI:NET:RETRY") {
      pushOperatorEvent("Network retry requested");
      if (commandCallback) commandCallback("UI:NET:RETRY");
      return;
    }
    if (command == "UI:NET:SCAN") {
      pushOperatorEvent("Network scan requested");
      if (commandCallback) commandCallback("UI:NET:SCAN");
      return;
    }
    if (command == "UI:LOCK:UNLOCK") {
      if (displayManager_.showPage(PAGE_UNLOCK)) pushDisplaySnapshot();
      return;
    }
    if (command == "UI:LOCK:CONFIRM") {
      pushOperatorEvent("Director unlocked");
      showDesktop();
      return;
    }
    if (command == "UI:LOCK:CANCEL") {
      if (displayManager_.showPage(PAGE_LOCKED)) pushDisplaySnapshot();
      return;
    }
    if (command == "UI:DISCOVERY:SCAN") {
      if (!displayManager_.showPage(PAGE_DISCOVERY)) {
        pushOperatorEvent("Discovery scan requested");
        if (commandCallback) commandCallback("UI:DISCOVERY:SCAN");
      } else {
        pushDisplaySnapshot();
      }
      return;
    }
    if (command == "UI:SYSTEM:REBOOT") {
      pushOperatorEvent("Reboot requested");
      if (displayManager_.showPage(PAGE_REBOOT)) pushDisplaySnapshot();
      if (commandCallback) commandCallback("UI:SYSTEM:REBOOT");
      return;
    }
    if (command == "UI:DIAG:PERF") {
      pushOperatorEvent("Performance metrics — coming soon");
      return;
    }
    if (command == "UI:DIAG:ERRORS") {
      showLogs();
      return;
    }
    if (command == "UI:DIAG:TOOLS") {
      showSettings();
      return;
    }

    if (command == "SCREEN:DESKTOP") {
      showDesktop();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE01_CMD_PRODUCTIONS || command == "HOME:PRODUCTIONS") {
      Serial.println("[UI] Page01 → Productions (Page 02)");
      showShows();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE02_CMD_BACK || command == "PAGE02:BACK") {
      Serial.println("[UI] Page02 → Home");
      showDesktop();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE02_CMD_OPEN || command == "PAGE02:OPEN") {
      const char *name = page_02_productions_selected_name();
      if (!name || !name[0]) name = "(none)";
      Serial.printf("[UI] Page02 Open → toast (%s)\n", name);
      char msg[96];
      snprintf(msg, sizeof(msg), "Open \"%s\" — editor coming soon", name);
      pushOperatorEvent(msg);
      return;
    }
    if (command == PAGE02_CMD_NEW || command == "PAGE02:NEW") {
      pushOperatorEvent("New production (local model only)");
      return;
    }
    if (command == PAGE02_CMD_DUPLICATE || command == "PAGE02:DUPLICATE") {
      pushOperatorEvent("Duplicated (local model only)");
      return;
    }
    if (command == PAGE01_CMD_RUN_SHOW || command == "HOME:RUN_SHOW") {
      Serial.println("[UI] Page01 → Run Show");
      showLive();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE01_CMD_CUE_LIBRARY || command == "HOME:CUE_LIBRARY") {
      Serial.println("[UI] Page01 → Cue Library (not built yet)");
      pushOperatorEvent("Cue Library — coming soon");
      return;
    }
    if (command == PAGE01_CMD_NODES || command == "HOME:NODES") {
      Serial.println("[UI] Page01 → Nodes");
      showDiagnostics();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE01_CMD_OUTPUTS || command == "HOME:OUTPUTS") {
      Serial.println("[UI] Page01 → Outputs (Page 05 not built yet)");
      pushOperatorEvent("Outputs — coming soon");
      return;
    }
    if (command == PAGE01_CMD_SETTINGS || command == "HOME:SETTINGS") {
      Serial.println("[UI] Page01 → Settings");
      showSettings();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE01_CMD_DIAGNOSTICS || command == "HOME:DIAGNOSTICS") {
      Serial.println("[UI] Page01 → Diagnostics");
      showDiagnostics();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command.startsWith("THEME:TEST:")) {
      const String name = command.substring(strlen("THEME:TEST:"));
      if (name.equalsIgnoreCase("NEXT")) {
        showduino_theme_test_next();
      } else {
        showduino_theme_test_apply_named(name.c_str());
      }
      if (page_01_home_is_active()) {
        page_01_home_apply_theme();
      }
      if (page_02_productions_is_active()) {
        page_02_productions_apply_theme();
      }
      return;
    }
    if (command == "SCREEN:LIVE") {
      showLive();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:SHOWS") {
      showShows();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:DIAG" || command == "SCREEN:NODES") {
      showDiagnostics();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:SETTINGS") {
      showSettings();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:AUDIO") {
      showAudio();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:LOGS") {
      showLogs();
      maybeRestoreEmergencyOverlay();
      return;
    }

    if (command.startsWith("UI:LOGS:FILTER:")) {
      logsFilter_ = (uint8_t)command.substring(strlen("UI:LOGS:FILTER:")).toInt();
      static const char *names[] = {"All", "System", "Show", "Audio", "Network", "Emergency"};
      if (logsFilterLabel_ && logsFilter_ < 6) {
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
      clearOperatorLogs();
      return;
    }
    if (command == "UI:LOGS:EXPORT") {
      pushOperatorEvent("Export Logs — placeholder (not available)");
      return;
    }
    if (command == "UI:LOGS:PAUSE") {
      logsLivePaused_ = true;
      pushOperatorEvent("Log live updates paused");
      return;
    }
    if (command == "UI:LOGS:RESUME") {
      logsLivePaused_ = false;
      refreshLogsDisplay();
      pushOperatorEvent("Log live updates resumed");
      return;
    }

    if (command == "UI:SHOW:BACK") {
      showShows();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command.startsWith("UI:SHOW:OPEN:")) {
      openShowDetails(command.substring(strlen("UI:SHOW:OPEN:")).c_str());
      return;
    }

    /* Block desk commands while emergency overlay is up (except E-STOP actions). */
    if (emergencyOverlayVisible && command != "EMERGENCY:STOP" && command != "EMERGENCY:CLEAR" &&
        !command.startsWith("UI:ESTOP:")) {
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
    } else if (command == "AUDIO:LOCAL:VOLUME:-") {
      outbound = "AUDIO:LOCAL:VOLUME:-";
    } else if (command.startsWith("AUDIO:LOCAL:") || command.startsWith("AUDIO:NODE:")) {
      outbound = command; /* colon-text to Stage; no PCM over ESP-NOW */
      {
        char note[96];
        snprintf(note, sizeof(note), "Audio cmd %.80s", command.c_str());
        pushOperatorEvent(note);
      }
    } else if (command == "EMERGENCY:STOP") {
      emergencyActivating = true;
      emergencyTriggeredByDirector_ = true;
      pageBeforeEmergency = displayManager_.currentPage();
      if (pageBeforeEmergency == PAGE_NONE) pageBeforeEmergency = PAGE_DESKTOP;
      for (uint8_t i = 0; i < 8; i++) {
        relayView[i] = DeskRelayView::PendingOff;
        refreshRelayButton(i);
      }
      Serial.println("[E-Stop] E-STOP pressed — sending EMERGENCY:STOP");
    } else if (command == "EMERGENCY:CLEAR") {
      /* Do not unlock until STATE:EMERGENCY:CLEAR. */
      gDirectorEmergencyScreen.noteClearRequested(millis());
      appendLog("E-CLEAR requested…");
      pushOperatorEvent("E-CLEAR → Stage (await STATE:EMERGENCY:CLEAR)");
      Serial.println("[E-Stop] CLEAR E-STOP pressed — sending EMERGENCY:CLEAR");
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
    showduino_theme_init();
    /* Legacy style aliases — kept so existing overlay code continues to compile. */
    styleScreen = os_.screen;
    stylePanel = os_.panel;
    styleButton = os_.button;
    styleDangerButton = os_.buttonDanger;
    styleTitle = os_.title;
    styleSmall = os_.caption;
  }

  lv_obj_t *makeScreen() { return os_.makeScreen(); }

  lv_obj_t *makePagePanel(DisplayPageId page) {
    lv_obj_t *panel = displayManager_.createPagePanel(page);
    if (!panel) return nullptr;
    lv_obj_set_style_text_color(panel, lv_color_hex(OsColor::Text), 0);
    return panel;
  }

  lv_obj_t *makePanel(lv_obj_t *parent, int x, int y, int w, int h) {
    return os_.makePanel(parent, x, y, w, h);
  }

  lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y) {
    return os_.makeLabel(parent, text, x, y);
  }

  lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                       const char *command, bool danger = false, bool scrollChain = true) {
    return os_.makeButton(parent, text, x, y, w, h, staticEventHandler, this, command, danger,
                          scrollChain);
  }

  void createTopBar(lv_obj_t *screen, const char *title) {
    os_.makePageChrome(screen, title);
  }

  void createDock(lv_obj_t *screen) {
    /* Child page panels use DisplayManager's one persistent dock. */
    if (screen && lv_obj_get_parent(screen) == nullptr) {
      os_.makeDock(screen, staticEventHandler, this);
    }
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
    logsScreen = makePagePanel(PAGE_LOGS);
    createDock(logsScreen);
    lv_obj_t *sum = os_.makePageChrome(logsScreen, "SYSTEM LOGS");
    logsCountLabel_ = makeLabel(sum, "Events: 0", 10, 8);
    lv_obj_add_style(logsCountLabel_, &os_.body, 0);
    logsNewestLabel_ = makeLabel(sum, "Newest: —", 10, 28);
    lv_obj_add_style(logsNewestLabel_, &os_.caption, 0);
    lv_obj_set_width(logsNewestLabel_, OS_CONTENT_FULL_W - 24);
    lv_label_set_long_mode(logsNewestLabel_, LV_LABEL_LONG_CLIP);

    lv_obj_t *panel = os_.makePrimaryPanel(logsScreen);
    os_.makeHeading(panel, "FILTERS", 8, 2);
    makeButton(panel, "All", 8, 28, 70, 36, "UI:LOGS:FILTER:0");
    makeButton(panel, "System", 84, 28, 86, 36, "UI:LOGS:FILTER:1");
    makeButton(panel, "Show", 176, 28, 70, 36, "UI:LOGS:FILTER:2");
    makeButton(panel, "Audio", 252, 28, 70, 36, "UI:LOGS:FILTER:3");
    makeButton(panel, "Net", 328, 28, 64, 36, "UI:LOGS:FILTER:4");
    makeButton(panel, "E-Stop", 398, 28, 80, 36, "UI:LOGS:FILTER:5");
    logsFilterLabel_ = makeLabel(panel, "Filter: All", 490, 34);
    lv_obj_add_style(logsFilterLabel_, &os_.caption, 0);

    makeButton(panel, "Clear", 8, 72, 90, 36, "UI:LOGS:CLEAR", true);
    makeButton(panel, "Export", 106, 72, 90, 36, "UI:LOGS:EXPORT");
    makeButton(panel, "Pause", 204, 72, 90, 36, "UI:LOGS:PAUSE");
    makeButton(panel, "Resume", 302, 72, 90, 36, "UI:LOGS:RESUME");
    makeButton(panel, "Back", 400, 72, 90, 36, "SCREEN:SETTINGS");

    operatorLogRoot = panel;
    operatorLogScroll = lv_obj_create(panel);
    lv_obj_remove_style_all(operatorLogScroll);
    lv_obj_set_pos(operatorLogScroll, 8, 118);
    lv_obj_set_size(operatorLogScroll, OS_CONTENT_FULL_W - 28, OS_PRIMARY_H - 130);
    lv_obj_set_style_bg_opa(operatorLogScroll, LV_OPA_TRANSP, 0);
    ShowduinoOsTheme::enableVerticalScroll(operatorLogScroll);
    operatorLogLabel = lv_label_create(operatorLogScroll);
    lv_obj_set_pos(operatorLogLabel, 2, 0);
    lv_obj_set_width(operatorLogLabel, OS_CONTENT_FULL_W - 40);
    lv_obj_add_style(operatorLogLabel, &os_.body, 0);
    lv_label_set_long_mode(operatorLogLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(operatorLogLabel, "(no events)\n");
  }

  void buildAudioPage() {
    audioScreen = makePagePanel(PAGE_AUDIO);
    createDock(audioScreen);
    lv_obj_t *sum = os_.makePageChrome(audioScreen, "AUDIO SYSTEM");
    makeLabel(sum, "1× local P4 output  ·  remote zones = ESP-NOW command nodes", 10, 10);
    makeButton(sum, "Back", OS_CONTENT_FULL_W - 110, 8, 90, 36, "SCREEN:SETTINGS");

    lv_obj_t *panel = os_.makePrimaryPanel(audioScreen);
    /* Content extends past OS_PRIMARY_H (~y=466); makePanel clears SCROLLABLE — restore it. */
    ShowduinoOsTheme::enableVerticalScroll(panel);

    os_.makeHeading(panel, "LOCAL OUTPUT — IAN / P4 AUDIO 1", 8, 2);
    audioLocalStatusLabel_ = makeLabel(panel, "Status: UNKNOWN", 8, 28);
    lv_obj_add_style(audioLocalStatusLabel_, &os_.title, 0);
    audioLocalDetailLabel_ = makeLabel(panel, "NOT AVAILABLE", 8, 54);
    lv_obj_add_style(audioLocalDetailLabel_, &os_.caption, 0);
    lv_obj_set_width(audioLocalDetailLabel_, OS_CONTENT_FULL_W - 40);
    lv_label_set_long_mode(audioLocalDetailLabel_, LV_LABEL_LONG_WRAP);

    makeButton(panel, "Play", 8, 150, 70, 36, "AUDIO:LOCAL:PLAY");
    makeButton(panel, "Pause", 84, 150, 70, 36, "AUDIO:LOCAL:PAUSE");
    makeButton(panel, "Stop", 160, 150, 70, 36, "AUDIO:LOCAL:STOP", true);
    makeButton(panel, "Mute", 236, 150, 70, 36, "AUDIO:LOCAL:MUTE");
    makeButton(panel, "Vol -", 312, 150, 64, 36, "AUDIO:LOCAL:VOLUME:-");
    makeButton(panel, "Vol +", 382, 150, 64, 36, "AUDIO:LOCAL:VOLUME:+");
    makeButton(panel, "Test", 452, 150, 70, 36, "AUDIO:LOCAL:TEST");

    os_.makeHeading(panel, "REMOTE AUDIO NODES", 8, 198);
    audioNodesLabel_ = makeLabel(panel, "Scanning…", 8, 224);
    lv_obj_add_style(audioNodesLabel_, &os_.caption, 0);
    lv_obj_set_width(audioNodesLabel_, OS_CONTENT_FULL_W - 40);
    lv_label_set_long_mode(audioNodesLabel_, LV_LABEL_LONG_WRAP);
    makeButton(panel, "Refresh", 8, 300, 100, 36, "AUDIO:NODE:STATUS");
    makeButton(panel, "Stop All", 116, 300, 100, 36, "AUDIO:NODE:STOP", true);
    makeButton(panel, "Mute All", 224, 300, 100, 36, "AUDIO:NODE:MUTE");
    makeButton(panel, "Test", 332, 300, 80, 36, "AUDIO:NODE:TEST");

    os_.makeHeading(panel, "AUDIO ROUTING", 8, 348);
    audioRoutingLabel_ = makeLabel(panel, "NOT AVAILABLE", 8, 374);
    lv_obj_add_style(audioRoutingLabel_, &os_.caption, 0);
    lv_obj_set_width(audioRoutingLabel_, OS_CONTENT_FULL_W - 40);
    lv_label_set_long_mode(audioRoutingLabel_, LV_LABEL_LONG_WRAP);

    os_.makeHeading(panel, "COMMAND STATUS", 8, 440);
    audioCmdStatusLabel_ = makeLabel(panel, "NOT AVAILABLE", 8, 466);
    lv_obj_add_style(audioCmdStatusLabel_, &os_.caption, 0);
    lv_obj_set_width(audioCmdStatusLabel_, OS_CONTENT_FULL_W - 40);
    lv_label_set_long_mode(audioCmdStatusLabel_, LV_LABEL_LONG_WRAP);
  }

  void buildScreens() {
    Serial.printf("[UI] heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    createSharedOperatorLog();
    uiBuildPump();

    /* ---- PAGE 01 HOME (LVGL tiles) ---- */
    Serial.println("[UI] Page 01 Home…");
    lv_obj_t *homePanel = makePagePanel(PAGE_DESKTOP);
    if (homePanel != nullptr) {
      page_01_home_create(homePanel, displayCommandThunk);
      ShowduinoCapabilities caps = showduino_capabilities_defaults();
      page_01_home_set_capabilities(&caps);
      page_01_home_set_footer_p4("—");
      page_01_home_set_footer_relay("—");
      page_01_home_set_footer_mosfet("—");
      page_01_home_set_footer_neopixel("—");
      page_01_home_set_footer_audio("—");
      page_01_home_set_footer_dmx("—");
    } else {
      Serial.println("[UI] Page 01 panel missing (theme/hybrid gate)");
    }
    uiBuildPump("[UI] Page 01");

    /* ---- DESKTOP legacy fallback — full width, no event log ---- */
    Serial.println("[UI] desktop legacy…");
    desktopScreen = makeScreen();
    uiBuildPump("[UI] desktop");
    createDock(desktopScreen);
    const int deskLeftW = OS_CONTENT_LEFT_W;
    const int deskRightW = OS_CONTENT_RIGHT_W;
    const int deskRightX = OS_CONTENT_RIGHT_X;
    const int deskSumH = 240;
    const int deskActY = OS_BODY_Y + deskSumH + OS_GAP;
    const int deskActH = OS_BODY_H - deskSumH - OS_GAP;

    lv_obj_t *summary = makePanel(desktopScreen, OS_MARGIN, OS_BODY_Y, deskLeftW, deskSumH);
    createSystemSummary(summary);
    makeButton(summary, "Start", 10, 204, 90, 36, "SHOW:START");
    makeButton(summary, "Pause", 108, 204, 90, 36, "SHOW:PAUSE");
    makeButton(summary, "Stop", 206, 204, 90, 36, "SHOW:STOP", true);

    lv_obj_t *fabric = makePanel(desktopScreen, deskRightX, OS_BODY_Y, deskRightW, 128);
    os_.makeHeading(fabric, "FABRIC", 8, 2);
    deskFabricLabel_ = makeLabel(fabric, "ESP-NOW: UNKNOWN", 8, 28);
    lv_obj_add_style(deskFabricLabel_, &os_.caption, 0);
    lv_obj_set_width(deskFabricLabel_, deskRightW - 20);
    lv_label_set_long_mode(deskFabricLabel_, LV_LABEL_LONG_WRAP);

    lv_obj_t *audioCard = makePanel(desktopScreen, deskRightX, OS_BODY_Y + 128 + OS_GAP, deskRightW,
                                    deskSumH - 128 - OS_GAP);
    os_.makeHeading(audioCard, "AUDIO", 8, 2);
    deskAudioSummaryLabel_ = makeLabel(audioCard, "Local: UNKNOWN\nNodes: 0 ONLINE\nPlaying: 0", 8, 28);
    lv_obj_add_style(deskAudioSummaryLabel_, &os_.caption, 0);
    lv_obj_set_width(deskAudioSummaryLabel_, deskRightW - 20);
    lv_label_set_long_mode(deskAudioSummaryLabel_, LV_LABEL_LONG_WRAP);
    makeButton(audioCard, "Open", 8, 60, deskRightW - 24, 36, "SCREEN:AUDIO");

    lv_obj_t *actions = makePanel(desktopScreen, OS_MARGIN, deskActY, OS_CONTENT_FULL_W, deskActH);
    createQuickActions(actions);
    uiBuildPump();

    /* ---- LIVE — What is happening right now? ---- */
    Serial.println("[UI] live…");
    liveScreen = makePagePanel(PAGE_LIVE);
    uiBuildPump("[UI] live");
    createDock(liveScreen);
    liveChromeRoot_ = os_.makePageChrome(liveScreen, "LIVE", &liveTitleBar_);
    os_.makeCaption(liveChromeRoot_, "Cue", 10, 8);
    liveCueLabel_ = makeLabel(liveChromeRoot_, "0 / 0", 10, 28);
    lv_obj_add_style(liveCueLabel_, &os_.title, 0);
    os_.makeCaption(liveChromeRoot_, "Elapsed", 160, 8);
    liveElapsedLabel_ = makeLabel(liveChromeRoot_, "0:00", 160, 28);
    lv_obj_add_style(liveElapsedLabel_, &os_.body, 0);
    os_.makeCaption(liveChromeRoot_, "Remaining", 300, 8);
    liveRemainLabel_ = makeLabel(liveChromeRoot_, "0:00", 300, 28);
    lv_obj_add_style(liveRemainLabel_, &os_.body, 0);
    /* Reserved icon/scene slots — no placeholder text */

    livePrimaryPanel_ = os_.makePrimaryPanel(liveScreen);
    lv_obj_t *live = livePrimaryPanel_;
    /* Do not scroll the whole Live panel — E-Clear taps must not become drag-scroll. */
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

    os_.makeHeading(live, "RELAYS", 8, 88);
    for (uint8_t i = 0; i < 8; i++) {
      int row = i / 4;
      int col = i % 4;
      int x = 10 + col * 112;
      int y = 110 + row * 38;
      static const char *relayNames[8] = { "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8" };
      relayButtons[i] = makeButton(live, relayNames[i], x, y, 104, 34, kRelayCmds[i]);
      refreshRelayButton(i);
    }
    /* Bottom row fits in OS_PRIMARY_H (~216). E-Clear must not scroll-chain. */
    makeButton(live, "All Off", 10, 188, 100, 36, "RELAY:ALL:OFF");
    makeButton(live, "Pulse R1", 118, 188, 100, 36, "RELAY:1:PULSE:1000");
    makeButton(live, "E-Clear", 226, 188, 100, 36, "EMERGENCY:CLEAR", false, false);
    uiBuildPump();

    /* ---- PAGE 02 PRODUCTIONS (LVGL library shell) ---- */
    Serial.println("[UI] Page 02 Productions…");
    showsScreen = makePagePanel(PAGE_SHOWS);
    uiBuildPump("[UI] Page 02");
    if (showsScreen != nullptr) {
      page_02_productions_create(showsScreen, displayCommandThunk);
    } else {
      Serial.println("[UI] Page 02 panel missing");
    }
    /* Legacy show-list widgets stay null — rebuildShowList is guarded. */
    showListScroll = nullptr;
    showsSummaryLabel_ = nullptr;
    showsListTitle = nullptr;
    showsListPanel = nullptr;
    uiBuildPump();

    /* ---- SHOW DETAILS ---- */
    Serial.println("[UI] details…");
    showDetailsScreen = makePagePanel(PAGE_SHOW_DETAILS);
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
    diagnosticsScreen = makePagePanel(PAGE_DIAGNOSTICS);
    if (!diagnosticsScreen) diagnosticsScreen = makePagePanel(PAGE_NODES);
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
    settingsScreen = makePagePanel(PAGE_SETTINGS);
    uiBuildPump("[UI] settings");
    createDock(settingsScreen);
    lv_obj_t *setSum = os_.makePageChrome(settingsScreen, "SETTINGS");
    os_.makeCaption(setSum, "Display", 10, 8);
    timeoutLabel = makeLabel(setSum, "Auto backlight: 10 min", 10, 28);
    lv_obj_add_style(timeoutLabel, &os_.body, 0);
    /* Always-visible Clear — not inside the scrollable primary panel. */
    makeButton(setSum, "Clear E-Stop", OS_CONTENT_FULL_W - 160, 16, 140, 40, "EMERGENCY:CLEAR",
               false, false);

    lv_obj_t *settings = os_.makePrimaryPanel(settingsScreen);
    /* SYSTEM row sits below OS_PRIMARY_H — panel must be scrollable. */
    ShowduinoOsTheme::enableVerticalScroll(settings);
    os_.makeHeading(settings, "MODULES", 8, 2);
    makeButton(settings, "Audio System", 8, 32, 230, 48, "SCREEN:AUDIO");
    makeButton(settings, "System Logs", 248, 32, 220, 48, "SCREEN:LOGS");
    os_.makeCaption(settings, "Audio: local P4 + remote nodes   ·   Logs: operator event history", 8, 86);

    os_.makeHeading(settings, "DISPLAY", 8, 112);
    makeButton(settings, "Never", 8, 140, 70, 40, "SETTINGS:TIMEOUT:0");
    makeButton(settings, "1m", 86, 140, 54, 40, "SETTINGS:TIMEOUT:1");
    makeButton(settings, "3m", 148, 140, 54, 40, "SETTINGS:TIMEOUT:3");
    makeButton(settings, "5m", 210, 140, 54, 40, "SETTINGS:TIMEOUT:5");
    makeButton(settings, "10m", 272, 140, 62, 40, "SETTINGS:TIMEOUT:10");
    makeButton(settings, "30m", 342, 140, 62, 40, "SETTINGS:TIMEOUT:30");
    makeButton(settings, "Cycle", 412, 140, 48, 40, "SETTINGS:TIMEOUT:CYCLE");
    os_.makeCaption(settings, "Dim at half timeout, then off. Touch wakes.", 8, 186);

    os_.makeHeading(settings, "SYSTEM", 8, 210);
    makeButton(settings, "Backup", 8, 236, 140, 44, "STORAGE:BACKUP");
    makeButton(settings, "Export", 156, 236, 120, 44, "STORAGE:EXPORT");
    makeButton(settings, "About", 284, 236, 120, 44, "SETTINGS:ABOUT");

    refreshTimeoutLabel();
    uiBuildPump();

    Serial.println("[UI] logs…");
    buildLogsPage();
    uiBuildPump("[UI] logs");
    Serial.println("[UI] audio…");
    buildAudioPage();
    uiBuildPump("[UI] audio");
    refreshLogsDisplay();
    refreshAudioPresentation();
    refreshDesktopFabric();

    Serial.println("[UI] overlays…");
    buildPersistentBanner();
    uiBuildPump();
    buildAbortConfirm();
    uiBuildPump();
    buildCompleteOverlay();
    if (statusBar_.root()) lv_obj_move_foreground(statusBar_.root());
    pushOperatorEvent("Showduino ready");
    Serial.printf("[UI] screens built heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  }
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
    const bool show = (mirroredState == SHOW_STATE_EMERGENCY_STOP) &&
                      !gDirectorEmergencyScreen.isVisible();
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
      if (abortConfirmRoot && !lv_obj_has_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(abortConfirmRoot);
      }
    } else {
      lv_obj_add_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN);
    }
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
    if (emergencyOverlayVisible) return; /* emergency wins */
    if (displayManager_.assetsReadyForPage(PAGE_COMPLETE) &&
        displayManager_.showPage(PAGE_COMPLETE)) {
      completeOverlayVisible = true;
      pushDisplaySnapshot();
      return;
    }
    if (!completeOverlayRoot) buildCompleteOverlay();
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
    if (displayManager_.currentPage() == PAGE_COMPLETE) {
      showDesktop();
    }
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
    if (emergencyOverlayDismissed && !emergencyLocked) return;
    emergencySessionOpen = true;
    emergencyVisitingDiag = false;
    emergencyOverlayDismissed = false;
    gDirectorEmergencyScreen.setShowName(estopShowName);
    gDirectorEmergencyScreen.show(millis());
    emergencyOverlayVisible = gDirectorEmergencyScreen.isVisible();
    updatePersistentBanner();
  }

  void hideEmergencyOverlay() {
    if (displayManager_.currentPage() == PAGE_EMERGENCY) {
      DisplayPageId restore = pageBeforeEmergency;
      if (restore == PAGE_NONE || restore == PAGE_EMERGENCY) restore = PAGE_DESKTOP;
      displayManager_.showPage(restore);
      pushDisplaySnapshot();
    }
    gDirectorEmergencyScreen.hide();
    emergencyOverlayVisible = false;
  }

  void maybeRestoreEmergencyOverlay() {
    if (!emergencyVisitingDiag) return;
    emergencyVisitingDiag = false;
    if (emergencySessionOpen && emergencyLocked && !emergencyOverlayDismissed) {
      showEmergencyOverlay();
    }
  }

  void restorePageAfterEmergency() {
    switch (pageBeforeEmergency) {
      case PAGE_LIVE: showLive(); break;
      case PAGE_SHOWS: showShows(); break;
      case PAGE_SHOW_DETAILS: showShows(); break;
      case PAGE_NODES:
      case PAGE_DIAGNOSTICS: showDiagnostics(); break;
      case PAGE_SETTINGS: showSettings(); break;
      case PAGE_AUDIO: showAudio(); break;
      case PAGE_LOGS: showLogs(); break;
      default: showDesktop(); break;
    }
  }

  void finishEmergencyScreenReturn() {
    emergencySessionOpen = false;
    emergencyOverlayDismissed = true;
    pendingAbortAwait = false;
    pendingResumeAwait = false;
    hideAbortConfirm();
    hideEmergencyOverlay();
    restorePageAfterEmergency();
    setShowView(DeskShowView::Idle);
    updatePersistentBanner();
  }

  void updateEmergencyResumeButton() {}

  void refreshEmergencyOverlayContent() {
    gDirectorEmergencyScreen.setShowName(estopShowName);
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
    /* Page 02 owns the production library shell. Legacy list widgets are unused. */
    if (showListScroll == nullptr) {
      Serial.printf("[UI] ShowManager index size=%u (Page 02 local model active)\n",
                    (unsigned)sm.size());
      return;
    }
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
      ShowduinoOsTheme::disableNestedScroll(row);
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
      /* Drag on the hit target must chain to showListScroll. */
      lv_obj_add_flag(hit, LV_OBJ_FLAG_SCROLL_CHAIN);
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

    if (!displayManager_.showPage(PAGE_SHOW_DETAILS)) {
      Serial.println("[UI] show details panel unavailable");
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }

  void showDesktop() {
    if (displayManager_.showPage(PAGE_DESKTOP)) {
      Serial.println("[UI] Page 01 Home (LVGL)");
      pushDisplaySnapshot();
    } else {
      displayManager_.releasePage();
      lv_screen_load(desktopScreen);
      Serial.println("[UI] Desktop legacy LVGL (asset gate)");
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showLive() {
    if (displayManager_.showPage(PAGE_LIVE)) {
      applyThemedLiveLayout(false);
      pushDisplaySnapshot();
    } else {
      displayManager_.releasePage();
      applyThemedLiveLayout(false);
      lv_screen_load(liveScreen);
      Serial.println("[UI] Live legacy LVGL");
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showShows() {
    if (displayManager_.showPage(PAGE_SHOWS)) {
      Serial.println("[UI] Page 02 Productions (LVGL)");
      if (page_02_productions_is_active()) {
        page_02_productions_apply_theme();
      }
      pushDisplaySnapshot();
    } else {
      displayManager_.releasePage();
      if (showsScreen) {
        lv_screen_load(showsScreen);
        Serial.println("[UI] Shows legacy LVGL");
      } else {
        Serial.println("[UI] Productions page unavailable");
        pushOperatorEvent("Productions unavailable");
        showDesktop();
      }
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showDiagnostics() {
    if (displayManager_.showPage(PAGE_DIAGNOSTICS)) {
      pushDisplaySnapshot();
    } else if (displayManager_.showPage(PAGE_NODES)) {
      pushDisplaySnapshot();
    } else {
      displayManager_.releasePage();
      lv_screen_load(diagnosticsScreen);
      Serial.println("[UI] Nodes legacy LVGL");
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showSettings() {
    if (!displayManager_.showPage(PAGE_SETTINGS)) {
      displayManager_.releasePage();
      lv_screen_load(settingsScreen);
      Serial.println("[UI] Settings legacy LVGL");
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showAudio() {
    if (!displayManager_.showPage(PAGE_AUDIO)) {
      displayManager_.releasePage();
      lv_screen_load(audioScreen);
      Serial.println("[UI] Audio legacy LVGL");
    }
    refreshAudioPresentation();
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showLogs() {
    if (!displayManager_.showPage(PAGE_LOGS)) {
      displayManager_.releasePage();
      lv_screen_load(logsScreen);
      Serial.println("[UI] Logs legacy LVGL");
    }
    const bool wasPaused = logsLivePaused_;
    logsLivePaused_ = false;
    refreshLogsDisplay();
    logsLivePaused_ = wasPaused;
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
};

#endif
