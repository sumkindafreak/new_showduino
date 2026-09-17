#ifndef SHOWDUINO_UI_H
#define SHOWDUINO_UI_H

#include <Arduino.h>
#include <stdlib.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "backlight.h"
#include "src/ShowManager.h"
#include "src/ShowThumb.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_show_runtime.h"
#include "../../../protocol/showduino_version.h"
#include "../../../protocol/showduino_gateway_wire.h"
#include "../../../protocol/showduino_emergency_director_desk.h"
#include "src/StorageConfig.h"
#include "DirectorStatusBar.h"
#include "DirectorAudioModel.h"
#include "ShowduinoOsUi.h"
#include "DisplayManager.h"
#include "DisplayPages.h"
#include "DirectorEmergencyScreen.h"
#include "DirectorEmergencyClearDialog.h"
#include "DirectorLocateScreen.h"
#include "DirectorTouchCalibrationScreen.h"
#include "DirectorUnlockScreen.h"
#include "DirectorUiMotion.h"
#include "DirectorAmbientPixels.h"
#include "touch_lvgl.h"
#include "page_01_home.h"
#include "page_02_productions.h"
#include "page_04_nodes.h"
#include "page_05_audio_node.h"
#include "page_06_diagnostics.h"
#include "page_08_settings.h"
#include "page_10_live.h"
#include "page_logs.h"
#include "page_audio_system.h"
#include "DirectorDiagnostics.h"
#include "DirectorAudioNodeControl.h"
#include "showduino_theme.h"
#include "showduino_capabilities.h"
#include "DirectorUiText.h"

// =========================================================
// Showduino OS - LVGL Director shell (Stage 7.9 design system)
// Desktop is the canonical visual reference for all pages.
// Presentation only - no protocol / runtime / comms changes.
// =========================================================

/* Stage 7.9 - layout aliases to Showduino OS design system */
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
    director_audio_node_clear(&audioNodeCtrl_);
    Serial.println("[UI] Showduino OS theme...");
    initTheme();
    Serial.println("[UI] status bar...");
    statusBar_.setLogCallback(statusBarLogThunk);
    statusBarSelf_ = this;
    statusBar_.begin();
    if (statusBar_.root()) {
      lv_obj_add_flag(statusBar_.root(), LV_OBJ_FLAG_HIDDEN);
    }
    displaySelf_ = this;
    gDirectorEmergencyScreen.setClearRequestHandler(emergencyClearThunk);
    gDirectorEmergencyScreen.setFinishedHandler(emergencyFinishedThunk);
    displayManager_.begin();
    displayManager_.setCommandHandler(displayCommandThunk);
    touchLvglSetHook(displayTouchHook);
    Serial.println("[UI] building screens...");
    buildScreens();
    if (!gDirectorUnlockScreen.ownsDisplay()) {
      Serial.println("[UI] loading desktop...");
      showDesktop();
    } else {
      Serial.println("[UI] boot screen owns display - desktop deferred");
    }
    syncStatusBarHealth();
    Serial.println("[UI] ready");
  }

  void onBootPresentationFinished() {
    directorAmbientHoldPresentation(false);
    directorUiMotionSetEmergency(emergencyLocked);
    DisplayPageId page = displayManager_.takeDeferredPage();
    if (emergencyLocked) {
      if (page == PAGE_NONE) page = PAGE_DESKTOP;
      displayManager_.showPage(page);
      showEmergencyOverlay();
      updateStatusWidgets(true);
      return;
    }
    if (page == PAGE_NONE) page = PAGE_DESKTOP;
    if (page == PAGE_CONNECTION_LOST && linkState == LINK_READY) {
      page = PAGE_DESKTOP;
    }
    if (page == PAGE_DESKTOP) showDesktop();
    else displayManager_.showPage(page);
    updateStatusWidgets(true);
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
      const DisplayPageId cur = displayManager_.currentPage();
      if (cur != PAGE_CONNECTION_LOST && cur != PAGE_NO_NETWORK) {
        pageBeforeLinkLost = cur;
      }
      showConnectionLost();
    } else if (state == LINK_READY && prev != LINK_READY &&
               !emergencyOverlayVisible) {
      const DisplayPageId cur = displayManager_.currentPage();
      if (cur == PAGE_CONNECTION_LOST || cur == PAGE_NO_NETWORK) {
        restoreAfterLinkLost();
        pushOperatorEvent("Stage link restored");
      }
    }
  }
  uint8_t getLinkState() const { return linkState; }
  void setEmergencyLocked(bool locked) {
    directorUiMotionSetEmergency(locked);
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
      applyEmergencyScreenSource();
      emergencyTriggeredByDirector_ = false;
      gDirectorEmergencyScreen.setShowName(estopShowName);
      gDirectorEmergencyScreen.setActiveSince(emergencyActiveSinceMs);
      gDirectorEmergencyScreen.setLatchActive(true, emergencyActiveSinceMs);
      if (gDirectorUnlockScreen.ownsDisplay()) {
        gDirectorUnlockScreen.abortForEmergency();
      } else {
        showEmergencyOverlay();
      }
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
    if (page_04_nodes_is_active()) refreshNodesPage();
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void markEmergencyStateFromStage() { emergencyStateKnown_ = true; }

  bool applyStageCapabilityLine(const char *line) {
    const bool hit = director_diag_apply_line(&diagCaps_, line);
    if (hit) {
      statusDirty = true;
      if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
    }
    return hit;
  }

  void noteStageReply(unsigned long ms) {
    lastStageReplyMs_ = ms;
    stageReplySeen_ = true;
  }

  void setEspNowReady(bool ready) {
    if (espNowReadyUi_ == ready) return;
    espNowReadyUi_ = ready;
    statusDirty = true;
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void noteEmergencyTriggeredByDirector() { emergencyTriggeredByDirector_ = true; }

  void applyEmergencyScreenSource() {
    if (emergencyTriggeredByDirector_) {
      gDirectorEmergencyScreen.setSource(DirectorEmergencyScreen::Source::Director);
      return;
    }
    if (!strcmp(emergencySourceKind_, "HARDWIRED") ||
        !strcmp(emergencySourceKind_, "physical")) {
      gDirectorEmergencyScreen.setSource(DirectorEmergencyScreen::Source::Physical);
      return;
    }
    if (!strcmp(emergencySourceKind_, "WIRELESS") ||
        !strcmp(emergencySourceKind_, "wireless")) {
      gDirectorEmergencyScreen.setSource(DirectorEmergencyScreen::Source::Wireless);
      gDirectorEmergencyScreen.setWirelessStation(emergencySourceId_, emergencySourceName_);
      return;
    }
    if (!strcmp(emergencySourceKind_, "REMOTE") ||
        !strcmp(emergencySourceKind_, "director")) {
      gDirectorEmergencyScreen.setSource(DirectorEmergencyScreen::Source::Director);
      return;
    }
    gDirectorEmergencyScreen.setSource(DirectorEmergencyScreen::Source::Unknown);
  }

  void applyEmergencySourceWire(const ShowduinoEmergencySourceWire &src) {
    strncpy(emergencySourceKind_, src.kind, sizeof(emergencySourceKind_) - 1);
    strncpy(emergencySourceId_, src.id, sizeof(emergencySourceId_) - 1);
    strncpy(emergencySourceName_, src.name, sizeof(emergencySourceName_) - 1);
    if (emergencyLocked) applyEmergencyScreenSource();
  }

  void setSafetyEstopFault(bool fault) {
    if (safetyEstopFault_ == fault) return;
    safetyEstopFault_ = fault;
    estopSheet_.safety_fault = fault ? 1 : 0;
    showduino_emergency_desk_rebuild(&estopSheet_);
    updatePersistentBanner();
    if (page_04_nodes_is_active()) refreshNodesPage();
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void applyEmergencyNodeWire(ShowduinoEmergencyNodeWire wire) {
    emergencyNodeWire_ = wire;
    recountSpecialistNodes();
    if (page_04_nodes_is_active()) refreshNodesPage();
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void applyEmergencyNodeDetail(const ShowduinoEmergencyDetailWire &d) {
    showduino_emergency_desk_apply_detail(&estopSheet_, &d);
    estopSheet_.global_emergency = emergencyLocked ? 1 : 0;
    showduino_emergency_desk_rebuild(&estopSheet_);
    recountSpecialistNodes();
    if (page_04_nodes_is_active()) refreshNodesPage();
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void applyEmergencyStationWire(const ShowduinoEmergencyStationWire &st) {
    showduino_emergency_desk_apply_station(&estopSheet_, &st);
    estopSheet_.global_emergency = emergencyLocked ? 1 : 0;
    if (page_04_nodes_is_active()) refreshNodesPage();
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void noteEmergencyClearRejected() {
    gDirectorEmergencyScreen.noteClearRejected(millis());
    emergencyOverlayDismissed = false;
    emergencyOverlayVisible = true;
  }

  void noteEmergencyClearCancelled() {
    gDirectorEmergencyScreen.noteClearCancelled();
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
    const uint8_t prev = nodeCount;
    nodeCount = n;
    liveStatusDirty = true;
    syncStatusBarHealth();
    refreshNodesPage();
    if (n > prev) {
      directorAmbientPulse(DIRECTOR_AMBIENT_EVT_NODE_DISCOVERED);
    }
  }

  uint8_t getNodeCount() const { return nodeCount; }

  void setLampNodeAvail(ShowduinoLampNodeWire raw) {
    const ShowduinoNodeAvailWire wire = showduino_lamp_wire_to_avail(raw);
    if (lampNodeRaw_ == raw && lampNodeWire_ == wire) return;
    lampNodeRaw_ = raw;
    lampNodeWire_ = wire;
    const bool present = (wire == SHOWDUINO_NODE_WIRE_ONLINE ||
                          wire == SHOWDUINO_NODE_WIRE_FAULT);
    ShowduinoCapabilities caps = page_01_home_get_capabilities();
    if (caps.lamp != present) {
      caps.lamp = present;
      page_01_home_set_capabilities(&caps);
    }
    page_01_home_set_footer_lamp(present
        ? (wire == SHOWDUINO_NODE_WIRE_FAULT ? "FAULT" : "ONLINE")
        : "-");
    recountSpecialistNodes();
    refreshNodesPage();
    statusDirty = true;
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void setLampNodeDetail(const ShowduinoLampDetailWire &d) {
    if (lampDetailValid_ && showduino_lamp_detail_equal(&lampDetail_, &d)) return;
    lampDetail_ = d;
    lampDetailValid_ = true;
    if (page_04_nodes_is_active()) refreshLampSheet();
  }

  void setPixelNodeAvail(ShowduinoPixelNodeWire wire) {
    if (pixelNodeRaw_ == wire) return;
    pixelNodeRaw_ = wire;
    pixelNodeWire_ = showduino_pixel_wire_to_avail(wire);
    const bool present = (wire == SHOWDUINO_PIXEL_NODE_WIRE_ONLINE ||
                          wire == SHOWDUINO_PIXEL_NODE_WIRE_FAULT ||
                          wire == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY);
    ShowduinoCapabilities caps = page_01_home_get_capabilities();
    if (caps.neopixel != present) {
      caps.neopixel = present;
      page_01_home_set_capabilities(&caps);
    }
    const char *footer = "-";
    if (wire == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY) footer = "EMERG";
    else if (wire == SHOWDUINO_PIXEL_NODE_WIRE_FAULT) footer = "FAULT";
    else if (wire == SHOWDUINO_PIXEL_NODE_WIRE_ONLINE) footer = "ONLINE";
    page_01_home_set_footer_neopixel(present ? footer : "-");
    recountSpecialistNodes();
    refreshNodesPage();
    statusDirty = true;
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void setPixelNodeDetail(const ShowduinoPixelDetailWire &d) {
    if (memcmp(&pixelDetail_, &d, sizeof(d)) == 0) return;
    pixelDetail_ = d;
    if (page_04_nodes_is_active()) refreshNodesPage();
  }

  void setAudioNodeWire(ShowduinoAudioNodeWire wire) {
    if (audioNodeWire_ == wire) return;
    audioNodeWire_ = wire;
    director_audio_apply_coarse(&audioNodeCtrl_, wire);
    const bool present = (wire != SHOWDUINO_AUDIO_NODE_WIRE_OFFLINE &&
                          wire != SHOWDUINO_AUDIO_NODE_WIRE_INVALID);
    ShowduinoCapabilities caps = page_01_home_get_capabilities();
    if (caps.audio != present) {
      caps.audio = present;
      page_01_home_set_capabilities(&caps);
    }
    page_01_home_set_footer_audio(present ? audioNodeStatusWord() : "-");
    recountSpecialistNodes();
    refreshNodesPage();
    refreshAudioNodePage();
    statusDirty = true;
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void applyAudioNodeDetail(const ShowduinoAudioDetailWire &d) {
    director_audio_apply_detail(&audioNodeCtrl_, &d);
    refreshAudioNodePage();
    statusDirty = true;
  }

  void applyAudioNodeSound(const ShowduinoAudioSoundWire &s) {
    director_audio_apply_sound(&audioNodeCtrl_, &s);
    refreshAudioNodePage();
  }

  void applyAudioNodeCaps(const char *caps) {
    if (!caps) return;
    strncpy(audioNodeCtrl_.caps, caps, sizeof(audioNodeCtrl_.caps) - 1);
    refreshAudioNodePage();
  }

  void applyAudioNodeMeta(const char *fw, const char *mac) {
    if (fw) strncpy(audioNodeCtrl_.firmware, fw, sizeof(audioNodeCtrl_.firmware) - 1);
    if (mac) strncpy(audioNodeCtrl_.mac, mac, sizeof(audioNodeCtrl_.mac) - 1);
    refreshAudioNodePage();
    if (page_06_diagnostics_is_active()) refreshDiagnosticsPage();
  }

  void applyAudioNodeInv(uint16_t page, uint16_t total, const char names[][21], uint8_t count) {
    audioNodeCtrl_.inventoryPage = page;
    audioNodeCtrl_.inventoryTotal = total;
    for (uint8_t i = 0; i < SHOWDUINO_AUDIO_INV_WIRE_MAX; i++) {
      audioNodeCtrl_.inventory[i][0] = '\0';
      if (i < count && names) strncpy(audioNodeCtrl_.inventory[i], names[i],
                                     sizeof(audioNodeCtrl_.inventory[i]) - 1);
    }
    refreshAudioNodePage();
  }

  /**
   * Stage 7: mirror ShowRuntime into operator UX (overlay / banner / LIVE / complete).
   * Does not mutate runtime - display + pending-confirm handling only.
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
      /* Left EMERGENCY_STOP via Stage CLEAR / STOP - unlock from runtime alone. */
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
        case SHOW_STATE_SHOW_LOADED:
          if (prev == SHOW_STATE_RUNNING || prev == SHOW_STATE_PAUSED) {
            pushOperatorEvent("Show Stopped");
          } else {
            pushOperatorEvent("Show Loaded");
          }
          break;
        case SHOW_STATE_RUNNING:
          if (prev == SHOW_STATE_PAUSED && !justConfirmedResume) {
            pushOperatorEvent("Resumed");
          } else if (prev != SHOW_STATE_EMERGENCY_STOP && prev != SHOW_STATE_PAUSED) {
            pushOperatorEvent("Show Started");
          }
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
          if (prev == SHOW_STATE_RUNNING || prev == SHOW_STATE_PAUSED) {
            pushOperatorEvent("Show Stopped");
          }
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
    directorTouchCalTick(nowMs);
    directorLocateTick(nowMs);
    gDirectorEmergencyClearDialog.raise();

    if (liveProgressBar && liveStatusDirty) {
      refreshLiveStatusPanel();
    }
  }

  bool emergencyOverlayIsVisible() const { return emergencyOverlayVisible; }
  bool completeOverlayIsVisible() const { return completeOverlayVisible; }

  /** Future alarm sound hooks - no audio yet. */
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

  bool nodesRequiredMissing() const {
    return SHOWDUINO_EXPECTED_NODES > 0 && nodeCount < SHOWDUINO_EXPECTED_NODES;
  }

  static const char *healthSystemWord(DirectorStatusBar::SystemState st) {
    switch (st) {
      case DirectorStatusBar::SystemState::Booting: return "BOOTING";
      case DirectorStatusBar::SystemState::Discovery: return "DISCOVERY";
      case DirectorStatusBar::SystemState::Ready: return "READY";
      case DirectorStatusBar::SystemState::Running: return "RUNNING";
      case DirectorStatusBar::SystemState::Paused: return "PAUSED";
      case DirectorStatusBar::SystemState::Stopped: return "STOPPED";
      case DirectorStatusBar::SystemState::Emergency: return "EMERGENCY";
      case DirectorStatusBar::SystemState::Ota: return "OTA";
      case DirectorStatusBar::SystemState::Error: return "FAULT";
      default: return "UNKNOWN";
    }
  }

  static const char *healthNetworkWord(DirectorStatusBar::NetworkState st) {
    switch (st) {
      case DirectorStatusBar::NetworkState::Online: return "ONLINE";
      case DirectorStatusBar::NetworkState::Degraded: return "DEGRADED";
      case DirectorStatusBar::NetworkState::Offline: return "OFFLINE";
      case DirectorStatusBar::NetworkState::Lost: return "LOST";
      default: return "UNKNOWN";
    }
  }

  const char *healthLinkWord() const {
    if (linkState == LINK_READY) return "READY";
    if (linkState == LINK_SEARCHING) return "SEARCHING";
    return "LOST";
  }

  const char *healthDegradedReason() const {
    if (linkState == LINK_DISCONNECTED) return "LINK_LOST";
    if (linkState == LINK_SEARCHING) return "LINK_SEARCHING";
    if (synchronising) return "SYNCHRONISING";
    if (nodesRequiredMissing()) return "MISSING_NODES";
    return "UNKNOWN";
  }

  void emitHealthDiagnostic(DirectorStatusBar::SystemState sys,
                            DirectorStatusBar::NetworkState net,
                            bool force) {
    const bool stageOnline = liveStageConnected || (linkState == LINK_READY);
    if (!force && healthLogReady_ &&
        lastHealthLink_ == linkState &&
        lastHealthSys_ == (uint8_t)sys &&
        lastHealthNet_ == (uint8_t)net &&
        lastHealthNodes_ == nodeCount &&
        lastHealthSync_ == synchronising &&
        lastHealthStage_ == stageOnline) {
      return;
    }
    lastHealthLink_ = linkState;
    lastHealthSys_ = (uint8_t)sys;
    lastHealthNet_ = (uint8_t)net;
    lastHealthNodes_ = nodeCount;
    lastHealthSync_ = synchronising;
    lastHealthStage_ = stageOnline;
    healthLogReady_ = true;

    SD_LOGD("HEALTH", "link=%s stage=%s sync=%s nodes=%u/%u net=%s sys=%s",
            healthLinkWord(), stageOnline ? "ONLINE" : "OFFLINE",
            synchronising ? "true" : "false",
            (unsigned)nodeCount, (unsigned)SHOWDUINO_EXPECTED_NODES,
            healthNetworkWord(net), healthSystemWord(sys));
    if (net == DirectorStatusBar::NetworkState::Degraded) {
      SD_LOGW("HEALTH", "DEGRADED reason=%s", healthDegradedReason());
    }
  }

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

  /* Legacy helpers used by older call sites - map to confirmed only */
  void setRelayState(uint8_t channel, bool on) { applyConfirmedRelay(channel, on); }

  void setAllRelaysOff() {
    for (uint8_t i = 1; i <= 8; i++) applyConfirmedRelay(i, false);
  }

  void appendLog(const String &line) {
    if (line.startsWith("RX <- Stage:") || line.startsWith("TX -> Stage")) {
      SD_LOGT("DIRECTOR", "%s", line.c_str());
    } else if (line.startsWith("WARN:") || line.indexOf("rejected") >= 0 ||
               line.indexOf("FAILED") >= 0 || line.indexOf("failed") >= 0 ||
               line.indexOf("DISCONNECTED") >= 0) {
      SD_LOGW("DIRECTOR", "%s", line.c_str());
    } else {
      SD_LOGI("DIRECTOR", "%s", line.c_str());
    }
    pushOperatorEvent(line.c_str());
  }

  void pushOperatorEvent(const char *msg) {
    if (!msg || !msg[0]) return;
    if (!eventLog) ensureEventLogStorage();
    if (!eventLog) {
      Serial.println(msg);
      return;
    }
    /* Duplicate flood (UNKNOWN_COMMAND, STATE echoes) must not rebuild LVGL. */
    if (eventLogCount > 0 &&
        strncmp(eventSlot(0), msg, OPERATOR_EVENT_LINE_LEN - 1) == 0) {
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

    logsDirty_ = true;
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
    /* Lightweight keyword filters - safe placeholders until typed log channels exist. */
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
      char shownLine[OPERATOR_EVENT_LINE_LEN];
      director_ui_sanitize_copy(shownLine, sizeof(shownLine), line);
      uiLogText += "[";
      uiLogText += logSeverityTag(line);
      uiLogText += "] ";
      uiLogText += shownLine;
      uiLogText += "\n";
      shown++;
    }
    if (operatorLogLabel != nullptr) {
      lv_label_set_text(operatorLogLabel, uiLogText.length() ? uiLogText.c_str() : "(no events)\n");
      if (operatorLogScroll != nullptr) lv_obj_scroll_to_y(operatorLogScroll, 0, LV_ANIM_OFF);
    }
    page_logs_set_body(uiLogText.length() ? uiLogText.c_str() : "(no events)\n");
    if (logsCountLabel_) {
      char buf[48];
      snprintf(buf, sizeof(buf), "Events: %u", (unsigned)eventLogCount);
      ShowduinoOsTheme::setTextIfChanged(logsCountLabel_, buf);
      page_logs_set_count(buf);
    } else {
      char buf[48];
      snprintf(buf, sizeof(buf), "Events: %u", (unsigned)eventLogCount);
      page_logs_set_count(buf);
    }
    if (logsNewestLabel_) {
      const char *newest = (eventLogCount > 0) ? eventSlot(0) : "-";
      char shown[OPERATOR_EVENT_LINE_LEN];
      director_ui_sanitize_copy(shown, sizeof(shown), newest);
      char buf[96];
      snprintf(buf, sizeof(buf), "Newest: %.70s", shown);
      ShowduinoOsTheme::setTextIfChanged(logsNewestLabel_, buf);
      page_logs_set_newest(buf);
    } else {
      const char *newest = (eventLogCount > 0) ? eventSlot(0) : "-";
      char shown[OPERATOR_EVENT_LINE_LEN];
      director_ui_sanitize_copy(shown, sizeof(shown), newest);
      char buf[96];
      snprintf(buf, sizeof(buf), "Newest: %.70s", shown);
      page_logs_set_newest(buf);
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
    char line[96];
    snprintf(line, sizeof(line), "Status: %s%s",
             DeskAudioModel::playWord(audioModel_.local.play),
             audioModel_.local.muted ? " (MUTED)" : "");
    ShowduinoOsTheme::setTextIfChanged(audioLocalStatusLabel_, line);
    page_audio_system_set_local_status(line);
    page_audio_system_set_header(DeskAudioModel::playWord(audioModel_.local.play), OsColor::Accent);

    char et[16], rt[16];
    formatClock(audioModel_.local.elapsedMs, et, sizeof(et));
    formatClock(audioModel_.local.remainMs, rt, sizeof(rt));
    char detail[320];
    snprintf(detail, sizeof(detail),
             "%s\nAsset: %s\nVol: %u  Loop: %s\nElapsed: %s  Remain: %s\nSD: %s  I2S: %s\n"
             "Commands only - files play from local SD (no ESP-NOW audio stream).",
             audioModel_.local.outputName,
             audioModel_.local.assetName,
             (unsigned)audioModel_.local.volume,
             audioModel_.local.loop ? "ON" : "OFF",
             et, rt,
             DeskAudioModel::sdWord(audioModel_.local.sd),
             DeskAudioModel::i2sWord(audioModel_.local.i2s));
    ShowduinoOsTheme::setTextIfChanged(audioLocalDetailLabel_, detail);
    page_audio_system_set_local_detail(detail);

    if (audioModel_.nodeCount == 0) {
      const char *none =
          "No audio nodes discovered.\n"
          "Remote nodes = ESP32 + I2S + SD.\n"
          "P4 sends PLAY/STOP/VOLUME over ESP-NOW (commands only).";
      ShowduinoOsTheme::setTextIfChanged(audioNodesLabel_, none);
      page_audio_system_set_nodes(none);
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
      page_audio_system_set_nodes(body.c_str());
    }

    {
      String body = "Asset source = target device SD (not streamed).\n\n";
      for (uint8_t i = 0; i < 8; i++) {
        if (!audioModel_.routes[i].used) continue;
        char row[96];
        snprintf(row, sizeof(row), "%-14s -> %s\n",
                 audioModel_.routes[i].zone, audioModel_.routes[i].target);
        body += row;
      }
      ShowduinoOsTheme::setTextIfChanged(audioRoutingLabel_, body.c_str());
      page_audio_system_set_routing(body.c_str());
    }
    {
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
      page_audio_system_set_command_status(body.c_str());
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

  /** Timeline / runtime readout (Stage 5-7). */
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

  void applyAtmosphereSettings(bool ledsOn, uint8_t ledBri, uint8_t animMode) {
    directorAmbientSetEnabled(ledsOn);
    directorAmbientSetBrightness(ledBri);
    directorUiMotionSetMode((DirectorUiAnimMode)animMode);
    refreshAtmosphereLabel();
  }

  void refreshAtmospherePresentation() { refreshAtmosphereLabel(); }

  /** SUE TimeService wire (TIME:...) - display only, no local clock. */
  bool applySueTimeWire(const char *line) { return statusBar_.applyTimeWire(line); }

  bool applyGatewayWire(const char *line) {
    if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_GATEWAY_PREFIX,
                         strlen(SHOWDUINO_WIRE_STATE_GATEWAY_PREFIX)) != 0) {
      return false;
    }
    const char *p = line + strlen(SHOWDUINO_WIRE_STATE_GATEWAY_PREFIX);
    gwAp_ = strstr(p, "AP=ON") != nullptr;
    gwSta_ = strstr(p, "STA=ON") != nullptr;
    gwInet_ = strstr(p, "INET=ON") != nullptr;
    const char *ch = strstr(p, "CH=");
    if (ch) gwCh_ = (uint8_t)atoi(ch + 3);
    statusBar_.setHomeWifi(gwSta_);
    return true;
  }

  bool commsUpdatingVisible() const {
    if (maintenanceWire_) return true;
    if (expectCommsReconnect_ && linkState != LINK_READY) return true;
    if (!commsOtaActive_) return false;
    return strcmp(commsOtaState_, "COMPLETE") != 0 &&
           strcmp(commsOtaState_, "ROLLED_BACK") != 0 &&
           strcmp(commsOtaState_, "FAILED") != 0 &&
           strcmp(commsOtaState_, "IDLE") != 0;
  }

  bool applyUpdateWire(const char *line) {
    if (!line || strncmp(line, SHOWDUINO_WIRE_STATE_UPDATE_PREFIX,
                         strlen(SHOWDUINO_WIRE_STATE_UPDATE_PREFIX)) != 0) {
      return false;
    }
    const char *p = line + strlen(SHOWDUINO_WIRE_STATE_UPDATE_PREFIX);
    strncpy(updateStatus_, p, sizeof(updateStatus_) - 1);
    updateStatus_[sizeof(updateStatus_) - 1] = '\0';
    updateLatest_[0] = '\0';
    commsOtaActive_ = (strncmp(p, "COMMS:", 6) == 0);
    maintenanceWire_ = (strcmp(p, "MAINTENANCE") == 0);
    if (commsOtaActive_) {
      strncpy(commsOtaState_, p + 6, sizeof(commsOtaState_) - 1);
      commsOtaState_[sizeof(commsOtaState_) - 1] = '\0';
      if (!strcmp(commsOtaState_, "REBOOT_REQUIRED") ||
          !strcmp(commsOtaState_, "PENDING_VALIDATION")) {
        expectCommsReconnect_ = true;
      }
      if (!strcmp(commsOtaState_, "COMPLETE") ||
          !strcmp(commsOtaState_, "ROLLED_BACK") ||
          !strcmp(commsOtaState_, "FAILED") ||
          !strcmp(commsOtaState_, "IDLE")) {
        expectCommsReconnect_ = false;
      }
      statusBar_.setUpdateHint(false, nullptr);
      return true;
    }
    char *colon = strchr(updateStatus_, ':');
    if (colon) {
      *colon = '\0';
      strncpy(updateLatest_, colon + 1, sizeof(updateLatest_) - 1);
    }
    const bool avail = strcmp(updateStatus_, SHOWDUINO_UPDATE_AVAILABLE) == 0;
    statusBar_.setUpdateHint(avail, updateLatest_);
    return true;
  }

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
    if (em != SB::EmergencyState::EmergencyStop && commsUpdatingVisible()) {
      sys = SB::SystemState::Ota;
    }
    statusBar_.setSystemState(sys);

    SB::NetworkState net = SB::NetworkState::Offline;
    const bool missingNodes = nodesRequiredMissing();
    if (linkState == LINK_DISCONNECTED) {
      net = SB::NetworkState::Lost;
    } else if (linkState == LINK_SEARCHING) {
      net = SB::NetworkState::Offline;
    } else if (synchronising || missingNodes) {
      net = SB::NetworkState::Degraded;
    } else {
      net = SB::NetworkState::Online;
    }
    statusBar_.setNetworkState(net);
    statusBar_.setNodeCounts(nodeCount, (uint8_t)SHOWDUINO_EXPECTED_NODES);
    emitHealthDiagnostic(sys, net, false);
  }

  /** USB / Serial health dump. Prints on demand even if unchanged. */
  void printHealthDiagnostic() {
    using SB = DirectorStatusBar;
    SB::SystemState sys = SB::SystemState::Booting;
    SB::EmergencyState em = SB::EmergencyState::Normal;
    if (emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) {
      em = SB::EmergencyState::EmergencyStop;
    } else if (mirroredState == SHOW_STATE_ERROR) {
      em = SB::EmergencyState::Fault;
    }
    if (em == SB::EmergencyState::EmergencyStop) sys = SB::SystemState::Emergency;
    else if (mirroredState == SHOW_STATE_ERROR) sys = SB::SystemState::Error;
    else if (mirroredState == SHOW_STATE_BOOTING && linkState != LINK_READY) {
      sys = SB::SystemState::Booting;
    } else if (linkState != LINK_READY || synchronising) {
      sys = SB::SystemState::Discovery;
    } else {
      sys = SB::SystemState::Ready;
    }
    SB::NetworkState net = SB::NetworkState::Offline;
    if (linkState == LINK_DISCONNECTED) net = SB::NetworkState::Lost;
    else if (linkState == LINK_SEARCHING) net = SB::NetworkState::Offline;
    else if (synchronising || nodesRequiredMissing()) net = SB::NetworkState::Degraded;
    else net = SB::NetworkState::Online;
    emitHealthDiagnostic(sys, net, true);
  }

  // Call often from loop. Only touches LVGL when something actually changed.
  void updateStatusWidgets(bool refreshTrafficAndUptime = false) {
    if (logsDirty_) {
      logsDirty_ = false;
      refreshLogsDisplay();
    }
    syncStatusBarHealth();
    statusBar_.update(millis());
    if (statusBar_.root()) {
      const bool cover =
          gDirectorUnlockScreen.ownsDisplay() ||
          gDirectorUnlockScreen.isVisible() ||
          gDirectorEmergencyScreen.isVisible() ||
          gDirectorEmergencyClearDialog.isVisible() ||
          directorTouchCalActive() ||
          displayPageIsSystemModal(displayManager_.currentPage()) ||
          (abortConfirmRoot && !lv_obj_has_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN)) ||
          (aboutRoot_ && !lv_obj_has_flag(aboutRoot_, LV_OBJ_FLAG_HIDDEN)) ||
          (networkRoot_ && !lv_obj_has_flag(networkRoot_, LV_OBJ_FLAG_HIDDEN)) ||
          (completeOverlayRoot && !lv_obj_has_flag(completeOverlayRoot, LV_OBJ_FLAG_HIDDEN));
      /* Hide/show once. move_foreground every loop invalidates layer_top
       * and tears the RGB panel. */
      if (cover != statusBarCovered_) {
        statusBarCovered_ = cover;
        if (cover) {
          lv_obj_add_flag(statusBar_.root(), LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_clear_flag(statusBar_.root(), LV_OBJ_FLAG_HIDDEN);
          lv_obj_move_foreground(statusBar_.root());
        }
      }
    }
    if (displayManager_.isPhase2PageActive() &&
        (statusDirty || refreshTrafficAndUptime)) {
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
      lv_color_t runtimeColor = lv_color_hex(OsColor::Text);
      if (mirroredState == SHOW_STATE_RUNNING) runtimeColor = lv_color_hex(OsColor::Ok);
      else if (mirroredState == SHOW_STATE_PAUSED) runtimeColor = lv_color_hex(OsColor::Warn);
      else if (mirroredState == SHOW_STATE_EMERGENCY_STOP || mirroredState == SHOW_STATE_ERROR)
        runtimeColor = lv_color_hex(OsColor::Fault);
      if (sumRuntimeValue_) {
        const char *cur = lv_label_get_text(sumRuntimeValue_);
        if (!cur || strcmp(cur, rt) != 0) {
          lv_label_set_text(sumRuntimeValue_, rt);
          lv_obj_set_style_text_color(sumRuntimeValue_, runtimeColor, 0);
        }
      }

      const char *safetyVal = "CLEAR";
      lv_color_t safetyColor = lv_color_hex(OsColor::Ok);
      if (emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) {
        safetyVal = "E-STOP";
        safetyColor = lv_color_hex(OsColor::Fault);
      } else if (mirroredState == SHOW_STATE_ERROR) {
        safetyVal = "FAULT";
        safetyColor = lv_color_hex(OsColor::Fault);
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

    if (page_06_diagnostics_is_active() &&
        (statusDirty || refreshTrafficAndUptime)) {
      refreshDiagnosticsPage();
    }
    if (page_08_settings_is_active() && statusDirty) {
      refreshSettingsPage();
    }
    if (page_10_live_is_active() && (statusDirty || liveStatusDirty)) {
      refreshLiveStatusPanel();
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
  static bool displayTouchHook(int32_t x, int32_t y, bool pressed) {
    if (directorLocateOnTouch(x, y, pressed)) return true;
    if (directorTouchCalOnTouch(x, y, pressed)) return true;
    if (displaySelf_) displaySelf_->displayManager_.onTouch(x, y, pressed);
    return false;
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

    if (page_04_nodes_is_active() && snap.page == PAGE_NODES) {
      refreshNodesPage();
    }

    /* Page 01 Home - refresh header / footer from real status only (no invented values). */
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
      page_01_home_set_footer_sue(snap.linkState[0] ? snap.linkState : "-");
      page_01_home_set_footer_notify(snap.notification[0] ? snap.notification : "-");
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

  const char *audioNodeStatusWord() const {
    switch (audioNodeWire_) {
      case SHOWDUINO_AUDIO_NODE_WIRE_ONLINE: return "ONLINE";
      case SHOWDUINO_AUDIO_NODE_WIRE_PLAYING: return "PLAYING";
      case SHOWDUINO_AUDIO_NODE_WIRE_PAUSED: return "PAUSED";
      case SHOWDUINO_AUDIO_NODE_WIRE_FAULT: return "FAULT";
      case SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY: return "EMERGENCY";
      default: return "NOT DETECTED";
    }
  }

  void recountSpecialistNodes() {
    uint8_t n = 0;
    if (lampNodeWire_ == SHOWDUINO_NODE_WIRE_ONLINE ||
        lampNodeWire_ == SHOWDUINO_NODE_WIRE_FAULT) n++;
    if (audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_ONLINE ||
        audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_PLAYING ||
        audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_PAUSED ||
        audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_LOOPING ||
        audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_FAULT ||
        audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY) n++;
    if (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_ONLINE ||
        pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_FAULT ||
        pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY) {
      n = (uint8_t)(n + (pixelDetail_.online ? pixelDetail_.online : 1));
    }
    if (estopSheet_.online) n = (uint8_t)(n + estopSheet_.online);
    setNodeCount(n);
  }

  void sendAudioNodeCmd(const String &cmd) {
    if (commandCallback) commandCallback(cmd);
  }

  void refreshAudioNodePage() {
    audioNodeCtrl_.emergency = emergencyLocked ||
        audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY;
    audioNodeCtrl_.showRunning = (mirroredState == SHOW_STATE_RUNNING ||
                                  mirroredState == SHOW_STATE_PAUSED);
    if (page_05_audio_node_is_active()) {
      page_05_audio_node_set_model(&audioNodeCtrl_);
    }
  }

  void refreshLampSheet() {
    if (!page_04_nodes_is_active()) return;
    ShowduinoLampDirectorInput in;
    ShowduinoLampDirectorSheet sh;
    memset(&in, 0, sizeof(in));
    const bool lampOn = (lampNodeWire_ == SHOWDUINO_NODE_WIRE_ONLINE ||
                         lampNodeWire_ == SHOWDUINO_NODE_WIRE_FAULT);
    in.present = lampOn ? 1 : 0;
    in.offline = lampOn ? 0 : 1;
    in.emergency = (emergencyLocked ||
                    lampNodeRaw_ == SHOWDUINO_LAMP_NODE_WIRE_EMERGENCY) ? 1 : 0;
    in.detail_valid = lampDetailValid_ ? 1 : 0;
    strncpy(in.logical_id, lampLogicalId_, sizeof(in.logical_id) - 1);
    in.detail = lampDetail_;
    showduino_lamp_director_build_sheet(&in, &sh);
    page_04_nodes_set_lamp_sheet(&sh);
  }

  void sendLampDesk(ShowduinoLampDeskVerb verb) {
    char cmd[48];
    showduino_lamp_director_format_cmd(lampLogicalId_, verb, cmd, sizeof(cmd));
    if (commandCallback) commandCallback(cmd);
  }

  void refreshNodesPage() {
    if (!page_04_nodes_is_active()) return;
    char sum[64];
    snprintf(sum, sizeof(sum), "FABRIC  |  %u ONLINE  |  5 SPECIALISTS",
             (unsigned)nodeCount);
    page_04_nodes_set_summary(sum);

    const bool audioOn = (audioNodeWire_ != SHOWDUINO_AUDIO_NODE_WIRE_OFFLINE &&
                          audioNodeWire_ != SHOWDUINO_AUDIO_NODE_WIRE_INVALID);
    uint32_t audioCol = ShowduinoPalette::Disabled;
    const char *audioDetail = "No compatible node detected.\nProgramme WAV stays on this role.";
    if (audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_ONLINE) {
      audioCol = ShowduinoPalette::Accent;
      audioDetail = "ESP32-A1S  |  ES8388\nProgramme audio ready.";
    } else if (audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_PLAYING) {
      audioCol = ShowduinoPalette::AccentBright;
      audioDetail = "Playing from node SD.\nP4 is authoritative.";
    } else if (audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_PAUSED) {
      audioCol = ShowduinoPalette::Warn;
      audioDetail = "Playback paused on the node.";
    } else if (audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_FAULT) {
      audioCol = ShowduinoPalette::Danger;
      audioDetail = "Audio Node fault reported.";
    } else if (audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY) {
      audioCol = ShowduinoPalette::Danger;
      audioDetail = "Emergency - programme audio stopped.";
    }
    page_04_nodes_set_card(PAGE04_ROLE_AUDIO, audioOn, audioNodeStatusWord(),
                           audioDetail, audioCol);
    page_04_nodes_set_lock(emergencyLocked);
    estopSheet_.global_emergency = emergencyLocked ? 1 : 0;
    showduino_emergency_desk_rebuild(&estopSheet_);
    page_04_nodes_set_emergency_sheet(&estopSheet_);
    refreshAudioNodePage();

    const bool lampOn = (lampNodeWire_ == SHOWDUINO_NODE_WIRE_ONLINE ||
                         lampNodeWire_ == SHOWDUINO_NODE_WIRE_FAULT);
    uint32_t lampCol = ShowduinoPalette::Disabled;
    const char *lampSt = "NOT DETECTED";
    char lampDet[96];
    snprintf(lampDet, sizeof(lampDet),
             "No compatible node detected.\nCarbide / theatrical lamp FX.");
    if (lampNodeRaw_ == SHOWDUINO_LAMP_NODE_WIRE_EMERGENCY) {
      lampCol = ShowduinoPalette::Danger;
      lampSt = "EMERGENCY";
      snprintf(lampDet, sizeof(lampDet),
               "%s\nShowduino emergency. Controls locked.",
               lampLogicalId_);
    } else if (lampNodeWire_ == SHOWDUINO_NODE_WIRE_ONLINE) {
      lampCol = ShowduinoPalette::Accent;
      lampSt = "ONLINE";
      snprintf(lampDet, sizeof(lampDet), "%s present.\nS3 carbide lamp.",
               lampLogicalId_);
    } else if (lampNodeWire_ == SHOWDUINO_NODE_WIRE_FAULT) {
      lampCol = ShowduinoPalette::Danger;
      lampSt = "FAULT";
      snprintf(lampDet, sizeof(lampDet), "%s\nLamp Node fault reported.",
               lampLogicalId_);
    }
    page_04_nodes_set_card(PAGE04_ROLE_LAMP, lampOn, lampSt, lampDet, lampCol);
    refreshLampSheet();

    page_04_nodes_set_card(PAGE04_ROLE_MOSFET, false, "NOT DETECTED",
                           "No compatible node detected.\nPWM / dimming outputs.",
                           ShowduinoPalette::Disabled);
    {
      const bool pixOn = (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_ONLINE ||
                          pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_FAULT ||
                          pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY);
      uint32_t pixCol = ShowduinoPalette::Disabled;
      const char *pixSt = "NOT DETECTED";
      char pixDet[96];
      snprintf(pixDet, sizeof(pixDet),
               "No compatible node detected.\nRemote Show Pixel Line.");
      if (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_ONLINE) {
        pixCol = ShowduinoPalette::Accent;
        pixSt = "ONLINE";
        snprintf(pixDet, sizeof(pixDet), "%u of %u online\n%s %s",
                 (unsigned)pixelDetail_.online,
                 (unsigned)(pixelDetail_.seen ? pixelDetail_.seen : pixelDetail_.online),
                 pixelDetail_.firstId[0] ? pixelDetail_.firstId : "PIXEL",
                 pixelDetail_.firstState[0] ? pixelDetail_.firstState : "");
      } else if (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY) {
        pixCol = ShowduinoPalette::Danger;
        pixSt = "EMERGENCY";
        snprintf(pixDet, sizeof(pixDet), "Showduino emergency.\n%s all-white",
                 pixelDetail_.firstId[0] ? pixelDetail_.firstId : "PIXEL");
      } else if (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_FAULT) {
        pixCol = ShowduinoPalette::Danger;
        pixSt = "FAULT";
        snprintf(pixDet, sizeof(pixDet), "Pixel Node fault reported.\n%s",
                 pixelDetail_.firstId[0] ? pixelDetail_.firstId : "");
      } else if (pixelDetail_.seen) {
        pixSt = "OFFLINE";
        snprintf(pixDet, sizeof(pixDet), "%u seen, none online.",
                 (unsigned)pixelDetail_.seen);
      }
      page_04_nodes_set_card(PAGE04_ROLE_NEOPIXEL, pixOn, pixSt, pixDet, pixCol);
    }
    page_04_nodes_set_card(PAGE04_ROLE_DMX, false, "NOT DETECTED",
                           "No compatible node detected.\nDedicated universe output.",
                           ShowduinoPalette::Disabled);

    const bool stageOk = (linkState == LINK_READY);
    uint32_t stageCol = ShowduinoPalette::Warn;
    const char *stageSt = "SEARCHING";
    const char *stageDet = "Director  ->  Comms S3  ->  P4.\nWaiting for Stage.";
    if (linkState == LINK_READY) {
      stageCol = ShowduinoPalette::Accent;
      stageSt = liveStageConnected ? "LINK OK" : "NO STAGE";
      stageDet = liveStageConnected
                     ? "Stage Controller linked.\nNodes report through the P4."
                     : "Comms up. Stage not confirmed.";
    } else if (linkState == LINK_DISCONNECTED) {
      stageCol = ShowduinoPalette::Danger;
      stageSt = "LINK LOST";
      stageDet = "Stage link lost.\nNode status may be stale.";
    }
    page_04_nodes_set_card(PAGE04_ROLE_STAGE, stageOk, stageSt, stageDet, stageCol);
    (void)stageOk;
  }

  bool audioNodePresent() const {
    return audioNodeWire_ != SHOWDUINO_AUDIO_NODE_WIRE_OFFLINE &&
           audioNodeWire_ != SHOWDUINO_AUDIO_NODE_WIRE_INVALID;
  }

  bool lampNodePresent() const {
    return lampNodeWire_ == SHOWDUINO_NODE_WIRE_ONLINE ||
           lampNodeWire_ == SHOWDUINO_NODE_WIRE_FAULT;
  }

  bool pixelNodePresent() const {
    return pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_ONLINE ||
           pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_FAULT ||
           pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY;
  }

  bool emergencyNodePresent() const {
    return emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_ONLINE ||
           emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_FAULT ||
           emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_ACTIVE ||
           estopSheet_.online > 0;
  }

  DirectorDiagSafetySync diagnosticsSafetySync() const {
    if (emergencyLocked) {
      if (audioNodePresent() && audioNodeWire_ != SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY) {
        return DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT;
      }
      if (lampNodePresent() && lampNodeRaw_ != SHOWDUINO_LAMP_NODE_WIRE_EMERGENCY) {
        return DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT;
      }
      if (pixelNodePresent() && pixelNodeRaw_ != SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY) {
        return DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT;
      }
      return DIRECTOR_DIAG_SYNC_OK;
    }
    if (audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY ||
        lampNodeRaw_ == SHOWDUINO_LAMP_NODE_WIRE_EMERGENCY ||
        pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY) {
      return DIRECTOR_DIAG_SYNC_STATE_MISMATCH;
    }
    return DIRECTOR_DIAG_SYNC_OK;
  }

  void refreshDiagnosticsPage() {
    if (!page_06_diagnostics_is_active()) return;

    const uint32_t now = millis();
    const DirectorDiagSafetySync sync = diagnosticsSafetySync();
    const bool stageOnline = (linkState == LINK_READY);
    const bool nodeFault =
        audioNodeWire_ == SHOWDUINO_AUDIO_NODE_WIRE_FAULT ||
        lampNodeWire_ == SHOWDUINO_NODE_WIRE_FAULT ||
        pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_FAULT ||
        emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_FAULT;
    const bool storageFault = director_diag_sd_fault(&diagCaps_);
    const DirectorDiagTri sdTri = director_diag_sd_tri(&diagCaps_);
    const bool timeWarn = (diagCaps_.time == DIRECTOR_DIAG_UNSYNCED);

    DirectorDiagHealth health = DIRECTOR_DIAG_HEALTH_READY;
    if (emergencyLocked) {
      health = DIRECTOR_DIAG_HEALTH_EMERGENCY;
    } else if (!stageOnline) {
      health = DIRECTOR_DIAG_HEALTH_STAGE_OFFLINE;
    } else if (sync == DIRECTOR_DIAG_SYNC_STATE_MISMATCH ||
               sync == DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT ||
               storageFault ||
               diagCaps_.audio == DIRECTOR_DIAG_FAULT ||
               diagCaps_.pixels == DIRECTOR_DIAG_FAULT ||
               nodeFault ||
               safetyEstopFault_) {
      health = DIRECTOR_DIAG_HEALTH_FAULT;
    } else if (timeWarn || sdTri == DIRECTOR_DIAG_DEGRADED ||
               diagCaps_.ethernet == DIRECTOR_DIAG_OFFLINE) {
      health = DIRECTOR_DIAG_HEALTH_DEGRADED;
    }

    char strip[96];
    if (health == DIRECTOR_DIAG_HEALTH_EMERGENCY) {
      director_diag_copy(strip, sizeof(strip), "P4 emergency latch active");
    } else if (health == DIRECTOR_DIAG_HEALTH_STAGE_OFFLINE) {
      director_diag_copy(strip, sizeof(strip), "Stage link not ready");
    } else if (sync == DIRECTOR_DIAG_SYNC_STATE_MISMATCH) {
      director_diag_copy(strip, sizeof(strip), "STATE MISMATCH - node emergency vs P4 CLEAR");
    } else if (sync == DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT) {
      director_diag_copy(strip, sizeof(strip), "SAFETY SYNC FAULT - node did not follow P4");
    } else if (timeWarn) {
      director_diag_copy(strip, sizeof(strip), "P4 clock TIME UNSYNCED");
    } else if (storageFault) {
      snprintf(strip, sizeof(strip), "P4 storage %s",
               diagCaps_.sdSeen ? diagCaps_.sdState : "FAULT");
    } else if (diagCaps_.audio == DIRECTOR_DIAG_FAULT) {
      director_diag_copy(strip, sizeof(strip), "P4 system audio fault");
    } else if (diagCaps_.pixels == DIRECTOR_DIAG_FAULT) {
      director_diag_copy(strip, sizeof(strip), "P4 pixel fault");
    } else {
      director_diag_copy(strip, sizeof(strip), "Director / P4 / nodes");
    }
    page_06_diagnostics_set_health(director_diag_health_word(health), strip,
                                   director_diag_health_color(health));

    /* DIRECTOR */
    {
      char det[96];
      snprintf(det, sizeof(det), "FW %s", STORAGE_FW_VERSION);
      page_06_diagnostics_set_card(PAGE06_CARD_DIRECTOR, true, "ONLINE", det,
                                   ShowduinoPalette::Accent);
      char up[16], heap[16], psram[16];
      director_diag_format_uptime(up, sizeof(up), (now - bootMs) / 1000UL);
      director_diag_format_heap(heap, sizeof(heap), ESP.getFreeHeap());
      director_diag_format_heap(psram, sizeof(psram), ESP.getFreePsram());
      char body[400];
      snprintf(body, sizeof(body),
               "Firmware     %s\n"
               "Uptime       %s\n"
               "Heap         %s\n"
               "PSRAM        %s\n"
               "ESP-NOW      %s\n"
               "TX / RX      %lu / %lu\n"
               "Touch        %s\n"
               "Calibration  %s\n"
               "Display      RGB 800x480",
               STORAGE_FW_VERSION, up, heap, psram,
               espNowReadyUi_ ? "READY" : "NOT READY",
               (unsigned long)txCount, (unsigned long)rxCount,
               touchLvglReady() ? "GT911 READY" : "NOT READY",
               touchLvglCalibrationIsNvs() ? "NVS v1" : "Factory fallback");
      page_06_diagnostics_set_sheet_body(PAGE06_CARD_DIRECTOR, body);
    }

    /* COMMS */
    {
      const bool ready = espNowReadyUi_;
      char det[96];
      if (ready) {
        snprintf(det, sizeof(det), "ESP-NOW | TX %lu / RX %lu",
                 (unsigned long)txCount, (unsigned long)rxCount);
      } else {
        director_diag_copy(det, sizeof(det), "ESP-NOW not ready");
      }
      page_06_diagnostics_set_card(PAGE06_CARD_COMMS, ready,
                                   ready ? "ONLINE" : "NOT READY", det,
                                   ready ? ShowduinoPalette::Accent : ShowduinoPalette::Warn);
      char age[24];
      director_diag_format_age(age, sizeof(age), stageReplySeen_, now, lastStageReplyMs_);
      char body[320];
      snprintf(body, sizeof(body),
               "ESP-NOW      %s\n"
               "TX           %lu\n"
               "RX           %lu\n"
               "Stage reply  %s\n"
               "Link         %s",
               ready ? "READY" : "NOT READY",
               (unsigned long)txCount, (unsigned long)rxCount, age,
               stageOnline ? "READY" : (linkState == LINK_SEARCHING ? "SEARCHING" : "DISCONNECTED"));
      page_06_diagnostics_set_sheet_body(PAGE06_CARD_COMMS, body);
    }

    /* P4 */
    {
      const char *st = "STAGE OFFLINE";
      uint32_t col = ShowduinoPalette::Danger;
      bool present = false;
      char det[96];
      director_diag_copy(det, sizeof(det), "NOT REPORTED");
      if (stageOnline) {
        present = true;
        st = "ONLINE";
        col = ShowduinoPalette::Accent;
        if (diagCaps_.firmwareSeen) {
          snprintf(det, sizeof(det), "FW %s", diagCaps_.firmware);
        }
      } else if (linkState == LINK_SEARCHING) {
        st = "SEARCHING";
        col = ShowduinoPalette::Warn;
        director_diag_copy(det, sizeof(det), "Awaiting Stage HELLO");
      }
      page_06_diagnostics_set_card(PAGE06_CARD_P4, present, st, det, col);
      char age[24], eth[20], clk[20];
      director_diag_format_age(age, sizeof(age), stageReplySeen_, now, lastStageReplyMs_);
      if (diagCaps_.ethernet == DIRECTOR_DIAG_ONLINE) director_diag_copy(eth, sizeof(eth), "ONLINE");
      else if (diagCaps_.ethernet == DIRECTOR_DIAG_OFFLINE) director_diag_copy(eth, sizeof(eth), "OFFLINE");
      else director_diag_copy(eth, sizeof(eth), "NOT REPORTED");
      if (diagCaps_.time == DIRECTOR_DIAG_READY) director_diag_copy(clk, sizeof(clk), "READY");
      else if (diagCaps_.time == DIRECTOR_DIAG_UNSYNCED) director_diag_copy(clk, sizeof(clk), "UNSYNCED");
      else director_diag_copy(clk, sizeof(clk), "NOT REPORTED");
      char body[400];
      snprintf(body, sizeof(body),
               "Firmware     %s\n"
               "Last contact %s\n"
               "Runtime      %s\n"
               "Clock        %s\n"
               "Ethernet     %s\n"
               "HELLO        %s",
               diagCaps_.firmwareSeen ? diagCaps_.firmware : "NOT REPORTED",
               age,
               mirroredRevision ? liveStateName : "NOT REPORTED",
               clk, eth,
               diagCaps_.helloComplete ? "COMPLETE" : (diagCaps_.identitySeen ? "PARTIAL" : "NOT REPORTED"));
      page_06_diagnostics_set_sheet_body(PAGE06_CARD_P4, body);
    }

    /* SAFETY */
    {
      const char *st = "NOT REPORTED";
      const char *det = "NOT REPORTED";
      uint32_t col = ShowduinoPalette::Disabled;
      bool present = emergencyLocked || emergencyStateKnown_;
      if (emergencyLocked) {
        st = "EMERGENCY";
        det = "P4 latch active";
        col = ShowduinoPalette::Danger;
      } else if (sync == DIRECTOR_DIAG_SYNC_STATE_MISMATCH) {
        st = "FAULT";
        det = "STATE MISMATCH";
        col = ShowduinoPalette::Danger;
        present = true;
      } else if (sync == DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT) {
        st = "FAULT";
        det = "SAFETY SYNC FAULT";
        col = ShowduinoPalette::Danger;
        present = true;
      } else if (safetyEstopFault_) {
        st = "FAULT";
        det = "Safety station fault";
        col = ShowduinoPalette::Danger;
        present = true;
      } else if (emergencyStateKnown_) {
        st = "CLEAR";
        det = emergencyNodePresent() ? "Physical loop healthy" : "P4 emergency CLEAR";
        col = ShowduinoPalette::Accent;
      }
      page_06_diagnostics_set_card(PAGE06_CARD_SAFETY, present, st, det, col);

      const char *loop = "NOT REPORTED";
      if (safetyEstopFault_) loop = "SAFETY STATION FAULT";
      else if (emergencyLocked &&
               (!strcmp(emergencySourceKind_, "HARDWIRED") ||
                !strcmp(emergencySourceKind_, "physical"))) {
        loop = "Physical loop asserted";
      } else if (emergencyStateKnown_ && !emergencyLocked && emergencyNodePresent()) {
        loop = "Physical loop healthy";
      }
      const char *en = "NOT DETECTED";
      if (emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_ONLINE) en = "ONLINE";
      else if (emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_ACTIVE) en = "EMERGENCY";
      else if (emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_FAULT) en = "FAULT";
      else if (emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_OFFLINE) en = "OFFLINE";
      char src[48];
      if (emergencySourceKind_[0]) {
        snprintf(src, sizeof(src), "%s %s", emergencySourceKind_,
                 emergencySourceId_[0] ? emergencySourceId_ : "");
      } else {
        director_diag_copy(src, sizeof(src), "NOT REPORTED");
      }
      char body[420];
      snprintf(body, sizeof(body),
               "Global       %s\n"
               "Physical     %s\n"
               "Emergency Node %s\n"
               "Last source  %s\n"
               "Sync         %s\n"
               "Diagnostics cannot clear or override emergency.",
               emergencyLocked ? "EMERGENCY" : (emergencyStateKnown_ ? "CLEAR" : "NOT REPORTED"),
               loop, en, src,
               sync == DIRECTOR_DIAG_SYNC_STATE_MISMATCH ? "STATE MISMATCH" :
               (sync == DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT ? "SAFETY SYNC FAULT" : "OK"));
      page_06_diagnostics_set_sheet_body(PAGE06_CARD_SAFETY, body);
    }

    /* STORAGE */
    {
      const char *st = "NOT REPORTED";
      const char *det = "NOT REPORTED";
      uint32_t col = ShowduinoPalette::Disabled;
      bool present = diagCaps_.sdSeen;
      if (sdTri == DIRECTOR_DIAG_ONLINE) {
        st = "ONLINE";
        det = "P4 SD ready";
        col = ShowduinoPalette::Accent;
      } else if (sdTri == DIRECTOR_DIAG_DEGRADED) {
        st = diagCaps_.sdState;
        det = "P4 SD degraded";
        col = ShowduinoPalette::Warn;
      } else if (sdTri == DIRECTOR_DIAG_OFFLINE || sdTri == DIRECTOR_DIAG_FAULT) {
        st = diagCaps_.sdState[0] ? diagCaps_.sdState : "FAULT";
        det = "P4 SD unavailable";
        col = ShowduinoPalette::Danger;
      }
      page_06_diagnostics_set_card(PAGE06_CARD_STORAGE, present, st, det, col);
      char body[200];
      snprintf(body, sizeof(body),
               "Reported P4 SD  %s\n"
               "Source          HELLO / capabilities",
               diagCaps_.sdSeen ? diagCaps_.sdState : "NOT REPORTED");
      page_06_diagnostics_set_sheet_body(PAGE06_CARD_STORAGE, body);
    }

    /* SYSTEM AUDIO (P4) */
    {
      const char *st = "NOT REPORTED";
      const char *det = "NOT REPORTED";
      uint32_t col = ShowduinoPalette::Disabled;
      bool present = (diagCaps_.audio != DIRECTOR_DIAG_UNREPORTED);
      if (diagCaps_.audio == DIRECTOR_DIAG_READY) {
        st = "READY";
        det = "ES8311";
        col = ShowduinoPalette::Accent;
      } else if (diagCaps_.audio == DIRECTOR_DIAG_FAULT) {
        st = "FAULT";
        det = "P4 system audio fault";
        col = ShowduinoPalette::Danger;
      }
      page_06_diagnostics_set_card(PAGE06_CARD_AUDIO, present, st, det, col);
      const char *nodeSt = audioNodeStatusWord();
      char body[280];
      snprintf(body, sizeof(body),
               "P4 system     %s\n"
               "P4 codec      %s\n"
               "Audio Node    %s\n"
               "Node firmware %s",
               diagCaps_.audio == DIRECTOR_DIAG_READY ? "READY" :
               (diagCaps_.audio == DIRECTOR_DIAG_FAULT ? "FAULT" : "NOT REPORTED"),
               present ? "ES8311" : "NOT REPORTED",
               nodeSt,
               audioNodeCtrl_.firmware[0] ? audioNodeCtrl_.firmware : "NOT REPORTED");
      page_06_diagnostics_set_sheet_body(PAGE06_CARD_AUDIO, body);
    }

    /* PIXELS (P4 local lines) */
    {
      const char *st = "NOT REPORTED";
      const char *det = "NOT REPORTED";
      uint32_t col = ShowduinoPalette::Disabled;
      bool present = (diagCaps_.pixels != DIRECTOR_DIAG_UNREPORTED);
      if (diagCaps_.pixels == DIRECTOR_DIAG_READY) {
        st = "READY";
        det = "Show + emergency lines";
        col = ShowduinoPalette::Accent;
      } else if (diagCaps_.pixels == DIRECTOR_DIAG_FAULT) {
        st = "FAULT";
        det = "P4 pixel fault";
        col = ShowduinoPalette::Danger;
      }
      page_06_diagnostics_set_card(PAGE06_CARD_PIXELS, present, st, det, col);
      char body[240];
      snprintf(body, sizeof(body),
               "P4 pixels     %s\n"
               "Pixel Node    %s\n"
               "Detail from HELLO PIXELS:READY / PIXELS:FAULT.",
               diagCaps_.pixels == DIRECTOR_DIAG_READY ? "READY" :
               (diagCaps_.pixels == DIRECTOR_DIAG_FAULT ? "FAULT" : "NOT REPORTED"),
               pixelNodePresent() ?
                 (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY ? "EMERGENCY" :
                  (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_FAULT ? "FAULT" : "ONLINE"))
                 : "NOT DETECTED");
      page_06_diagnostics_set_sheet_body(PAGE06_CARD_PIXELS, body);
    }

    /* NODES summary */
    {
      uint8_t online = 0;
      if (audioNodePresent()) online++;
      if (lampNodePresent()) online++;
      if (pixelNodePresent()) online++;
      if (emergencyNodePresent()) online++;
      char st[28];
      char det[96];
      uint32_t col = ShowduinoPalette::Disabled;
      bool present = (online > 0);
      if (sync == DIRECTOR_DIAG_SYNC_STATE_MISMATCH ||
          sync == DIRECTOR_DIAG_SYNC_SAFETY_SYNC_FAULT) {
        director_diag_copy(st, sizeof(st), "FAULT");
        director_diag_copy(det, sizeof(det),
                           sync == DIRECTOR_DIAG_SYNC_STATE_MISMATCH
                               ? "STATE MISMATCH"
                               : "SAFETY SYNC FAULT");
        col = ShowduinoPalette::Danger;
        present = true;
      } else if (online == 0) {
        director_diag_copy(st, sizeof(st), "NOT DETECTED");
        director_diag_copy(det, sizeof(det), "No specialist nodes");
      } else {
        snprintf(st, sizeof(st), "%u ONLINE", (unsigned)online);
        col = ShowduinoPalette::Accent;
        if (audioNodePresent() && audioNodeCtrl_.firmware[0]) {
          snprintf(det, sizeof(det), "Audio Node %s", audioNodeCtrl_.firmware);
        } else if (audioNodePresent()) {
          director_diag_copy(det, sizeof(det), "Audio Node");
        } else if (lampNodePresent()) {
          director_diag_copy(det, sizeof(det), "Lamp Node");
        } else if (pixelNodePresent()) {
          director_diag_copy(det, sizeof(det), "Pixel Node");
        } else {
          director_diag_copy(det, sizeof(det), "Emergency Node");
        }
      }
      page_06_diagnostics_set_card(PAGE06_CARD_NODES, present, st, det, col);
      char body[400];
      snprintf(body, sizeof(body),
               "Audio        %s%s%s\n"
               "Lamp         %s\n"
               "Pixel        %s\n"
               "Emergency    %s\n"
               "Full setup remains on Nodes / Audio Node.",
               audioNodeStatusWord(),
               audioNodeCtrl_.firmware[0] ? "  FW " : "",
               audioNodeCtrl_.firmware[0] ? audioNodeCtrl_.firmware : "",
               lampNodePresent()
                   ? (lampNodeRaw_ == SHOWDUINO_LAMP_NODE_WIRE_EMERGENCY ? "EMERGENCY"
                      : (lampNodeWire_ == SHOWDUINO_NODE_WIRE_FAULT ? "FAULT" : "ONLINE"))
                   : "NOT DETECTED",
               pixelNodePresent()
                   ? (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_EMERGENCY ? "EMERGENCY"
                      : (pixelNodeRaw_ == SHOWDUINO_PIXEL_NODE_WIRE_FAULT ? "FAULT" : "ONLINE"))
                   : "NOT DETECTED",
               emergencyNodePresent()
                   ? (emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_ACTIVE ? "EMERGENCY"
                      : (emergencyNodeWire_ == SHOWDUINO_EMERGENCY_NODE_WIRE_FAULT ? "FAULT" : "ONLINE"))
                   : "NOT DETECTED");
      page_06_diagnostics_set_sheet_body(PAGE06_CARD_NODES, body);
    }
  }


  /** Desktop SYSTEM SUMMARY - consistent operator vocabulary. */
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
  lv_obj_t *atmosphereLabel_ = nullptr;
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
  DisplayPageId pageBeforeLinkLost = PAGE_DESKTOP;

  lv_obj_t *persistentBannerRoot = nullptr;
  lv_obj_t *persistentBannerLabel = nullptr;
  lv_obj_t *abortConfirmRoot = nullptr;
  lv_obj_t *completeOverlayRoot = nullptr;
  lv_obj_t *completeDetailLabel = nullptr;
  lv_obj_t *aboutRoot_ = nullptr;
  lv_obj_t *aboutBody_ = nullptr;
  lv_obj_t *networkRoot_ = nullptr;
  lv_obj_t *networkBody_ = nullptr;
  bool gwAp_ = true;
  bool gwSta_ = false;
  bool gwInet_ = false;
  uint8_t gwCh_ = 1;
  char updateStatus_[40] = "NONE";
  char commsOtaState_[32] = "";
  bool commsOtaActive_ = false;
  bool expectCommsReconnect_ = false;
  bool maintenanceWire_ = false;
  char updateLatest_[32] = "";
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
  bool logsDirty_ = false;
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
  ShowduinoNodeAvailWire lampNodeWire_ = SHOWDUINO_NODE_WIRE_UNKNOWN;
  ShowduinoLampNodeWire lampNodeRaw_ = SHOWDUINO_LAMP_NODE_WIRE_INVALID;
  ShowduinoLampDetailWire lampDetail_{};
  bool lampDetailValid_ = false;
  char lampLogicalId_[16] = SHOWDUINO_CARBIDE_LOGICAL_DEFAULT;
  ShowduinoNodeAvailWire pixelNodeWire_ = SHOWDUINO_NODE_WIRE_UNKNOWN;
  ShowduinoPixelNodeWire pixelNodeRaw_ = SHOWDUINO_PIXEL_NODE_WIRE_INVALID;
  ShowduinoPixelDetailWire pixelDetail_{};
  ShowduinoAudioNodeWire audioNodeWire_ = SHOWDUINO_AUDIO_NODE_WIRE_OFFLINE;
  DirectorAudioNodeControl audioNodeCtrl_;
  DirectorDiagnosticsCaps diagCaps_{};
  bool emergencyStateKnown_ = false;
  bool stageReplySeen_ = false;
  uint32_t lastStageReplyMs_ = 0;
  bool espNowReadyUi_ = false;
  ShowduinoEmergencyNodeWire emergencyNodeWire_ = SHOWDUINO_EMERGENCY_NODE_WIRE_INVALID;
  ShowduinoEmergencyDirectorSheet estopSheet_{};
  bool safetyEstopFault_ = false;
  char emergencySourceKind_[12] = "";
  char emergencySourceId_[16] = "";
  char emergencySourceName_[20] = "";
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
  lv_obj_t *livePendingLabel_ = nullptr;
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
  bool healthLogReady_ = false;
  uint8_t lastHealthLink_ = 0xFF;
  uint8_t lastHealthSys_ = 0xFF;
  uint8_t lastHealthNet_ = 0xFF;
  uint8_t lastHealthNodes_ = 0xFF;
  bool lastHealthSync_ = true;
  bool lastHealthStage_ = false;
  bool statusBarCovered_ = true;
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

    /* Emergency overlay actions - handled even while overlay blocks the desk. */
    if (command == "UI:ESTOP:RESUME") {
      if (emergencyLocked) {
        pushOperatorEvent("Resume blocked - clear emergency on Stage first");
        return;
      }
      pendingResumeAwait = true;
      pushOperatorEvent("Resume requested - awaiting Stage");
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
      pushOperatorEvent("Abort confirmed - awaiting Stage");
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
        pushOperatorEvent("Emergency latch still active - CLEAR required");
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
      return;
    }

    if (command == "UI:NET:RETRY") {
      pushOperatorEvent("Retrying Stage link");
      if (commandCallback) commandCallback("UI:NET:RETRY");
      return;
    }
    if (command == "UI:NET:SCAN") {
      pushOperatorEvent("Retrying Stage link");
      if (commandCallback) commandCallback("UI:NET:RETRY");
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
      if (commandCallback) commandCallback("UI:NET:RETRY");
      return;
    }
    if (command == "UI:SYSTEM:REBOOT") {
      pushOperatorEvent("Rebooting Director");
      if (displayManager_.showPage(PAGE_REBOOT)) pushDisplaySnapshot();
      if (commandCallback) commandCallback("UI:SYSTEM:REBOOT");
      return;
    }
    if (command == "UI:DIAG:PERF") {
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
      Serial.println("[UI] Page01 -> Productions (Page 02)");
      showShows();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE02_CMD_BACK || command == "PAGE02:BACK") {
      Serial.println("[UI] Page02 -> Home");
      showDesktop();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE02_CMD_OPEN || command == "PAGE02:OPEN") {
      const char *id = page_02_productions_selected_id();
      if (!id || !id[0]) {
        pushOperatorEvent("No production selected");
        return;
      }
      openShowDetails(id);
      return;
    }
    if (command == PAGE02_CMD_LOAD || command == "PAGE02:LOAD") {
      const char *id = page_02_productions_selected_id();
      if (!id || !id[0]) {
        pushOperatorEvent("No production selected");
        return;
      }
      strncpy(selectedShowIdBuf, id, sizeof(selectedShowIdBuf) - 1);
      selectedShowIdBuf[sizeof(selectedShowIdBuf) - 1] = '\0';
      if (commandCallback) commandCallback("UI:SHOW:LOAD");
      return;
    }
    if (command == PAGE02_CMD_RUN || command == "PAGE02:RUN") {
      const char *id = page_02_productions_selected_id();
      if (!id || !id[0]) {
        pushOperatorEvent("No production selected");
        return;
      }
      strncpy(selectedShowIdBuf, id, sizeof(selectedShowIdBuf) - 1);
      selectedShowIdBuf[sizeof(selectedShowIdBuf) - 1] = '\0';
      showLive();
      if (commandCallback) commandCallback("UI:SHOW:RUN");
      return;
    }
    if (command == PAGE01_CMD_RUN_SHOW || command == "HOME:RUN_SHOW") {
      Serial.println("[UI] Page01 -> RUN SHOW");
      const char *id = page_02_productions_selected_id();
      if (id && id[0]) {
        strncpy(selectedShowIdBuf, id, sizeof(selectedShowIdBuf) - 1);
        selectedShowIdBuf[sizeof(selectedShowIdBuf) - 1] = '\0';
      }
      showLive();
      maybeRestoreEmergencyOverlay();
      if (commandCallback) commandCallback("UI:SHOW:RUN");
      return;
    }
    if (command == PAGE01_CMD_CUE_LIBRARY || command == "HOME:CUE_LIBRARY" ||
        command == PAGE01_CMD_OUTPUTS || command == "HOME:OUTPUTS") {
      return;
    }
    if (command == PAGE01_CMD_NODES || command == "HOME:NODES") {
      Serial.println("[UI] Page01 -> Nodes");
      showNodes();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE01_CMD_SETTINGS || command == "HOME:SETTINGS") {
      Serial.println("[UI] Page01 -> Settings");
      showSettings();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE01_CMD_DIAGNOSTICS || command == "HOME:DIAGNOSTICS") {
      Serial.println("[UI] Page01 -> Diagnostics");
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
      if (page_04_nodes_is_active()) {
        page_04_nodes_apply_theme();
      }
      if (page_05_audio_node_is_active()) {
        page_05_audio_node_apply_theme();
      }
      if (page_06_diagnostics_is_active()) {
        page_06_diagnostics_apply_theme();
      }
      if (page_08_settings_is_active()) {
        page_08_settings_apply_theme();
      }
      if (page_10_live_is_active()) {
        page_10_live_apply_theme();
      }
      if (page_logs_is_active()) {
        page_logs_apply_theme();
      }
      if (page_audio_system_is_active()) {
        page_audio_system_apply_theme();
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
    if (command == "SCREEN:DIAG") {
      showDiagnostics();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == "SCREEN:NODES") {
      showNodes();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE04_CMD_BACK) {
      showDesktop();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE04_CMD_AUDIO) {
      showAudioNode();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE04_CMD_CLOSE) {
      page_04_nodes_close_sheet();
      return;
    }
    if (command == PAGE04_CMD_STATUS) {
      if (commandCallback) commandCallback("STATUS:REQUEST");
      return;
    }
    if (command == PAGE04_CMD_AUDIO_TEST) {
      sendAudioNodeCmd("AUDIO:NODE:TEST");
      return;
    }
    if (command == PAGE04_CMD_AUDIO_STOP) {
      sendAudioNodeCmd("AUDIO:NODE:STOP");
      return;
    }
    if (command == PAGE04_CMD_LAMP_IGNITE) {
      sendLampDesk(SHOWDUINO_LAMP_DESK_CMD_IGNITE);
      return;
    }
    if (command == PAGE04_CMD_LAMP_EXTINGUISH) {
      sendLampDesk(SHOWDUINO_LAMP_DESK_CMD_EXTINGUISH);
      return;
    }
    if (command == PAGE04_CMD_LAMP_FLARE) {
      sendLampDesk(SHOWDUINO_LAMP_DESK_CMD_FLARE);
      return;
    }
    if (command == PAGE04_CMD_LAMP_STATUS) {
      sendLampDesk(SHOWDUINO_LAMP_DESK_CMD_STATUS);
      return;
    }
    if (command == PAGE04_CMD_EMERGENCY || command == PAGE04_CMD_EMERGENCY_STATUS) {
      if (commandCallback) commandCallback("ESTOP:STATUS");
      return;
    }
    if (command == PAGE06_CMD_BACK || command == PAGE08_CMD_BACK ||
        command == PAGE10_CMD_BACK) {
      showDesktop();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE08_CMD_CLOSE) {
      page_08_settings_close_sheet();
      return;
    }
    if (command == PAGE06_CMD_CLOSE) {
      page_06_diagnostics_close_sheet();
      return;
    }
    if (command == PAGE06_CMD_TOOLS) {
      return;
    }
    if (command == PAGE06_CMD_REFRESH) {
      if (commandCallback) {
        commandCallback("HELLO");
        commandCallback("STATUS:REQUEST");
      }
      refreshDiagnosticsPage();
      pushOperatorEvent("Diagnostics refresh requested");
      return;
    }
    if (command == PAGE06_CMD_STAGE_STATUS) {
      if (commandCallback) commandCallback("STATUS:REQUEST");
      return;
    }
    if (command == PAGE06_CMD_SD_STATUS) {
      if (commandCallback) commandCallback("STORAGE:STATUS");
      return;
    }
    if (command == PAGE06_CMD_BACKUP) {
      if (commandCallback) commandCallback("STORAGE:BACKUP");
      return;
    }
    if (command == PAGE06_CMD_REPAIR) {
      if (commandCallback) commandCallback("STORAGE:REPAIR");
      return;
    }
    if (command == PAGE06_CMD_LOGS) {
      showLogs();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE05_CMD_BACK) {
      showNodes();
      maybeRestoreEmergencyOverlay();
      return;
    }
    if (command == PAGE05_CMD_PLAY) {
      sendAudioNodeCmd(String("AUDIO:NODE:PLAY:") + page_05_audio_node_selected_asset());
      return;
    }
    if (command == PAGE05_CMD_LOOP) {
      sendAudioNodeCmd(String("AUDIO:NODE:LOOP:") + page_05_audio_node_selected_asset());
      return;
    }
    if (command == PAGE05_CMD_PAUSE) {
      sendAudioNodeCmd("AUDIO:NODE:PAUSE");
      return;
    }
    if (command == PAGE05_CMD_RESUME) {
      sendAudioNodeCmd("AUDIO:NODE:RESUME");
      return;
    }
    if (command == PAGE05_CMD_STOP) {
      sendAudioNodeCmd("AUDIO:NODE:STOP");
      return;
    }
    if (command == PAGE05_CMD_VOL_DOWN || command == PAGE05_CMD_VOL_UP) {
      int v = (int)audioNodeCtrl_.volume + (command == PAGE05_CMD_VOL_UP ? 5 : -5);
      if (v < 0) v = 0;
      if (v > 100) v = 100;
      audioNodeCtrl_.pending = DIRECTOR_AUDIO_PEND_VOLUME;
      audioNodeCtrl_.pendingVolume = (uint8_t)v;
      sendAudioNodeCmd(String("AUDIO:NODE:VOLUME:") + String(v));
      refreshAudioNodePage();
      return;
    }
    if (command == PAGE05_CMD_TEST) {
      sendAudioNodeCmd("AUDIO:NODE:TEST");
      return;
    }
    if (command == PAGE05_CMD_SELECT) {
      page_05_audio_node_show_select(true);
      return;
    }
    if (command == PAGE05_CMD_DETAILS) {
      page_05_audio_node_show_details(true);
      return;
    }
    if (command == PAGE05_CMD_CLOSE) {
      page_05_audio_node_show_details(false);
      page_05_audio_node_show_select(false);
      return;
    }
    if (command == PAGE05_CMD_INV_NEXT) {
      sendAudioNodeCmd(String("AUDIO:NODE:INVENTORY:") +
                       String((unsigned)(audioNodeCtrl_.inventoryPage + 1)));
      return;
    }
    if (command.startsWith("PAGE05:ASSET:")) {
      strncpy(audioNodeCtrl_.selectedAsset, command.c_str() + 13,
              sizeof(audioNodeCtrl_.selectedAsset) - 1);
      page_05_audio_node_show_select(false);
      refreshAudioNodePage();
      return;
    }
    if (command == PAGE05_CMD_CALIBRATE) {
      sendAudioNodeCmd("AUDIO:NODE:SOUND:CALIBRATE");
      return;
    }
    if (command == PAGE05_CMD_SND_EN || command == PAGE05_CMD_SND_DIS) {
      sendAudioNodeCmd(audioNodeCtrl_.soundEnabled ? "AUDIO:NODE:SOUND:DISABLE"
                                                   : "AUDIO:NODE:SOUND:ENABLE");
      return;
    }
    if (command == PAGE05_CMD_TH_UP || command == PAGE05_CMD_TH_DN) {
      int t = (int)audioNodeCtrl_.soundThreshold + (command == PAGE05_CMD_TH_UP ? 5 : -5);
      if (t < 0) t = 0;
      if (t > 100) t = 100;
      audioNodeCtrl_.soundThreshold = (uint8_t)t;
      sendAudioNodeCmd(String("AUDIO:NODE:SOUND:THRESHOLD:") + String(t));
      refreshAudioNodePage();
      return;
    }
    if (command == PAGE05_CMD_SND_TEST) {
      sendAudioNodeCmd("AUDIO:NODE:SOUND:TRIGGER:TEST");
      return;
    }
    if (command == "SETTINGS:ABOUT" || command == "SETTINGS:SOFTWARE") {
      showAboutDialog();
      return;
    }
    if (command == "SETTINGS:NETWORK") {
      showNetworkDialog();
      return;
    }
    if (command == "UI:ABOUT:CLOSE") {
      hideAboutDialog();
      return;
    }
    if (command == "UI:NETWORK:CLOSE") {
      hideNetworkDialog();
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
    if (command == PAGE08_CMD_TOUCH_CAL || command == "TOUCH:CALIBRATE") {
      directorTouchCalStart();
      return;
    }
    if (command == PAGE08_CMD_TOUCH_RESET) {
      directorTouchCalStartReset();
      return;
    }

    if (command.startsWith("UI:LOGS:FILTER:")) {
      logsFilter_ = (uint8_t)command.substring(strlen("UI:LOGS:FILTER:")).toInt();
      static const char *names[] = {"All", "System", "Show", "Audio", "Network", "Emergency"};
      if (logsFilter_ < 6) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Filter: %s", names[logsFilter_]);
        ShowduinoOsTheme::setTextIfChanged(logsFilterLabel_, buf);
        page_logs_set_filter(buf);
        page_logs_set_header(names[logsFilter_], OsColor::Accent);
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
    if (emergencyOverlayVisible && command != "EMERGENCY:STOP" &&
        command != "EMERGENCY:CLEAR" && command != "EMERGENCY:CLEAR_CONFIRM" &&
        command != "EMERGENCY:CLEAR_CANCEL" &&
        !command.startsWith("UI:ESTOP:")) {
      return;
    }
    if (completeOverlayVisible &&
        command != "UI:COMPLETE:RUN" && command != "UI:COMPLETE:MENU" &&
        command != "UI:COMPLETE:EXPORT" && command != "EMERGENCY:STOP") {
      return;
    }

    String outbound = command;

    if (command.startsWith("UI:RELAY:") || command == "RELAY:ALL:OFF" ||
        command.startsWith("RELAY:")) {
      return;
    }

    /* Absolute relay request from channel tap - never send TOGGLE */
    if (command == "STOP:ALL" || command == "SHOW:STOP" || command == "UI:SHOW:STOP") {
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
    } else if (command == "AUDIO:LOCAL:STOP" || command == "AUDIO:STOP") {
      outbound = "AUDIO:LOCAL:STOP";
    } else if (command.startsWith("AUDIO:")) {
      return;
    } else if (command == "EMERGENCY:STOP") {
      emergencyActivating = true;
      emergencyTriggeredByDirector_ = true;
      pageBeforeEmergency = displayManager_.currentPage();
      if (pageBeforeEmergency == PAGE_NONE) pageBeforeEmergency = PAGE_DESKTOP;
      for (uint8_t i = 0; i < 8; i++) {
        relayView[i] = DeskRelayView::PendingOff;
        refreshRelayButton(i);
      }
      Serial.println("[E-Stop] E-STOP pressed - sending EMERGENCY:STOP");
    } else if (command == "EMERGENCY:CLEAR") {
      /* Request the P4 clear workflow. Do not unlock until STATE:EMERGENCY:CLEAR. */
      gDirectorEmergencyScreen.noteClearRequested(millis());
      appendLog("E-CLEAR request sent to Stage");
      pushOperatorEvent("E-CLEAR request -> Stage");
      Serial.println("[E-Stop] CLEAR EMERGENCY - sending EMERGENCY:CLEAR request");
      outbound = "EMERGENCY:CLEAR";
    } else if (command == "EMERGENCY:CLEAR_CONFIRM") {
      appendLog("E-CLEAR confirm requested...");
      pushOperatorEvent("E-CLEAR confirm -> Stage (await STATE:EMERGENCY:CLEAR)");
      Serial.println("[E-Stop] CONFIRM CLEAR - sending EMERGENCY:CLEAR_CONFIRM");
      outbound = "EMERGENCY:CLEAR_CONFIRM";
    }

    statusDirty = true;
    updateStatusWidgets(true);
    if (commandCallback != nullptr) commandCallback(outbound);
  }

  void refreshRelayButton(uint8_t idx) {
    if (idx >= 8 || relayButtons[idx] == nullptr) return;
    DeskRelayView v = relayView[idx];
    uint32_t bg = ShowduinoPalette::PanelRaised;
    uint32_t border = ShowduinoPalette::AccentDark;
    switch (v) {
      case DeskRelayView::ConfirmedOn:
        bg = ShowduinoPalette::AccentDim; border = ShowduinoPalette::Accent; break;
      case DeskRelayView::ConfirmedOff:
        bg = ShowduinoPalette::PanelRaised; border = ShowduinoPalette::AccentDark; break;
      case DeskRelayView::PendingOn:
      case DeskRelayView::PendingOff:
        bg = ShowduinoPalette::Panel; border = ShowduinoPalette::Pending; break;
      case DeskRelayView::Fault:
        bg = ShowduinoPalette::DangerPanel; border = ShowduinoPalette::Warn; break;
      case DeskRelayView::Unknown:
      default:
        bg = ShowduinoPalette::Panel; border = ShowduinoPalette::AccentDark; break;
    }
    lv_obj_set_style_bg_color(relayButtons[idx], lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(relayButtons[idx], lv_color_hex(border), 0);
  }

  void initTheme() {
    os_.begin();
    showduino_theme_init();
    /* Legacy style aliases - kept so existing overlay code continues to compile. */
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
    lv_obj_set_style_bg_color(deskProgressBar_, lv_color_hex(OsColor::ScanLine), LV_PART_MAIN);
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
    /* Operator log moved to Settings -> Logs (no layer-top panel). */
  }

  void createLogPanel(lv_obj_t *screen) { (void)screen; }

  void buildLogsPage() {
    logsScreen = makePagePanel(PAGE_LOGS);
    if (logsScreen != nullptr) {
      page_logs_create(logsScreen, displayCommandThunk);
      operatorLogLabel = page_logs_body_label();
      operatorLogScroll = page_logs_scroll();
      operatorLogRoot = logsScreen;
    }
  }

  void buildAudioPage() {
    audioScreen = makePagePanel(PAGE_AUDIO);
    if (audioScreen != nullptr) {
      page_audio_system_create(audioScreen, displayCommandThunk);
    }
  }

  void buildScreens() {
    Serial.printf("[UI] heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    createSharedOperatorLog();
    uiBuildPump();

    /* ---- PAGE 01 HOME (LVGL tiles) ---- */
    Serial.println("[UI] Page 01 Home...");
    lv_obj_t *homePanel = makePagePanel(PAGE_DESKTOP);
    if (homePanel != nullptr) {
      page_01_home_create(homePanel, displayCommandThunk);
      ShowduinoCapabilities caps = showduino_capabilities_defaults();
      page_01_home_set_capabilities(&caps);
      page_01_home_set_footer_p4("-");
      page_01_home_set_footer_lamp("-");
      page_01_home_set_footer_mosfet("-");
      page_01_home_set_footer_neopixel("-");
      page_01_home_set_footer_audio("-");
      page_01_home_set_footer_dmx("-");
    } else {
      Serial.println("[UI] Page 01 panel missing (theme/hybrid gate)");
    }
    uiBuildPump("[UI] Page 01");

    /* Legacy full-screen desktop is retired. Page 01 is the operator home. */

    /* ---- LIVE - What is happening right now? ---- */
    Serial.println("[UI] live...");
    liveScreen = makePagePanel(PAGE_LIVE);
    uiBuildPump("[UI] live");
    if (liveScreen != nullptr) {
      page_10_live_create(liveScreen, displayCommandThunk);
    }
    liveChromeRoot_ = nullptr;
    liveTitleBar_ = nullptr;
    livePrimaryPanel_ = nullptr;
    liveCueLabel_ = nullptr;
    liveElapsedLabel_ = nullptr;
    liveRemainLabel_ = nullptr;
    livePendingLabel_ = nullptr;
    liveStatusLabel = nullptr;
    liveProgressBar = nullptr;
    liveEmergencyDot = nullptr;
    timelineStatusLabel = nullptr;
    uiBuildPump();

    /* ---- PAGE 02 PRODUCTIONS (LVGL library shell) ---- */
    Serial.println("[UI] Page 02 Productions...");
    showsScreen = makePagePanel(PAGE_SHOWS);
    uiBuildPump("[UI] Page 02");
    if (showsScreen != nullptr) {
      page_02_productions_create(showsScreen, displayCommandThunk);
    } else {
      Serial.println("[UI] Page 02 panel missing");
    }
    /* Page 02 is filled from ShowManager by refreshShowLibrary(). */
    showListScroll = nullptr;
    showsSummaryLabel_ = nullptr;
    showsListTitle = nullptr;
    showsListPanel = nullptr;
    uiBuildPump();

    /* ---- SHOW DETAILS ---- */
    Serial.println("[UI] details...");
    showDetailsScreen = makePagePanel(PAGE_SHOW_DETAILS);
    uiBuildPump("[UI] details");
    if (showDetailsScreen != nullptr) {
      ShowduinoOsTheme::AppHeader detHdr = os_.makeAppHeader(
          showDetailsScreen, "SHOW DETAILS", staticEventHandler, this, "UI:SHOW:BACK", 168);
      detailsNameLabel = detHdr.status;
      lv_label_set_text(detailsNameLabel, "NO PRODUCTION");
      lv_obj_set_style_text_color(detailsNameLabel, lv_color_hex(OsColor::Accent), 0);

      lv_obj_t *det = os_.makeRaisedCard(showDetailsScreen, 16, OS_SUMMARY_Y,
                                        OS_CONTENT_FULL_W,
                                        (int)(OS_DOCK_Y - OS_SUMMARY_Y - OS_GAP), true);
      detailsIconHost = lv_obj_create(det);
      lv_obj_remove_style_all(detailsIconHost);
      lv_obj_set_pos(detailsIconHost, 16, 16);
      lv_obj_set_size(detailsIconHost, 96, 64);
      ShowduinoShowThumb::makeDefaultIcon(detailsIconHost, 0, 0, 96, 64);
      detailsDescLabel = makeLabel(det, "Description", 128, 18);
      lv_obj_set_width(detailsDescLabel, OS_CONTENT_FULL_W - 160);
      lv_obj_add_style(detailsDescLabel, &os_.body, 0);
      lv_label_set_long_mode(detailsDescLabel, LV_LABEL_LONG_WRAP);
      detailsMetaLabel = makeLabel(det, "Duration / Version / Author", 16, 92);
      lv_obj_set_width(detailsMetaLabel, OS_CONTENT_FULL_W - 40);
      lv_obj_add_style(detailsMetaLabel, &os_.caption, 0);
      lv_label_set_long_mode(detailsMetaLabel, LV_LABEL_LONG_WRAP);
      timelineDetailLabel = makeLabel(det, "Playback: STOPPED", 16, 140);
      lv_obj_set_width(timelineDetailLabel, OS_CONTENT_FULL_W - 40);
      lv_obj_add_style(timelineDetailLabel, &os_.body, 0);
      makeButton(det, "LOAD", 16, 210, 112, 48, "UI:SHOW:LOAD");
      makeButton(det, "RUN", 136, 210, 112, 48, "UI:SHOW:RUN");
      makeButton(det, "PAUSE", 256, 210, 112, 48, "SHOW:PAUSE");
      makeButton(det, "RESUME", 376, 210, 120, 48, "SHOW:RESUME");
      makeButton(det, "STOP", 504, 210, 112, 48, "UI:SHOW:STOP", true);
    }
    uiBuildPump();

    /* ---- PAGE 04 NODES (fabric inventory) ---- */
    Serial.println("[UI] Page 04 Nodes...");
    lv_obj_t *nodesPanel = makePagePanel(PAGE_NODES);
    uiBuildPump("[UI] Page 04");
    if (nodesPanel != nullptr) {
      page_04_nodes_create(nodesPanel, displayCommandThunk);
      refreshNodesPage();
    } else {
      Serial.println("[UI] Page 04 panel missing");
    }
    uiBuildPump();

    Serial.println("[UI] Page 05 Audio Node...");
    lv_obj_t *audioNodePanel = makePagePanel(PAGE_AUDIO_NODE);
    uiBuildPump("[UI] Page 05");
    if (audioNodePanel != nullptr) {
      page_05_audio_node_create(audioNodePanel, displayCommandThunk);
      page_05_audio_node_set_model(&audioNodeCtrl_);
    } else {
      Serial.println("[UI] Page 05 panel missing");
    }
    uiBuildPump();

    /* ---- PAGE 06 DIAGNOSTICS ---- */
    Serial.println("[UI] Page 06 Diagnostics...");
    diagnosticsScreen = makePagePanel(PAGE_DIAGNOSTICS);
    uiBuildPump("[UI] Page 06");
    if (diagnosticsScreen != nullptr) {
      page_06_diagnostics_create(diagnosticsScreen, displayCommandThunk);
      refreshDiagnosticsPage();
    } else {
      Serial.println("[UI] Page 06 panel missing");
    }
    uiBuildPump();

    /* ---- PAGE 08 SETTINGS ---- */
    Serial.println("[UI] Page 08 Settings...");
    settingsScreen = makePagePanel(PAGE_SETTINGS);
    uiBuildPump("[UI] settings");
    if (settingsScreen != nullptr) {
      page_08_settings_create(settingsScreen, displayCommandThunk);
      refreshSettingsPage();
    } else {
      Serial.println("[UI] Settings page missing");
    }
    timeoutLabel = nullptr;
    atmosphereLabel_ = nullptr;
    uiBuildPump();

    Serial.println("[UI] logs...");
    buildLogsPage();
    uiBuildPump("[UI] logs");
    Serial.println("[UI] audio...");
    buildAudioPage();
    uiBuildPump("[UI] audio");
    refreshLogsDisplay();
    refreshAudioPresentation();
    refreshDesktopFabric();

    Serial.println("[UI] overlays...");
    buildPersistentBanner();
    uiBuildPump();
    buildAbortConfirm();
    uiBuildPump();
    buildCompleteOverlay();
    if (statusBar_.root()) lv_obj_add_flag(statusBar_.root(), LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_set_style_bg_color(persistentBannerRoot, lv_color_hex(ShowduinoPalette::DangerPanel), 0);
    lv_obj_set_style_bg_opa(persistentBannerRoot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(persistentBannerRoot, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(persistentBannerRoot, 1, 0);
    lv_obj_set_style_border_color(persistentBannerRoot, lv_color_hex(ShowduinoPalette::Danger), 0);
    lv_obj_add_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(persistentBannerRoot, LV_OBJ_FLAG_CLICKABLE);
    persistentBannerLabel = lv_label_create(persistentBannerRoot);
    lv_label_set_text(persistentBannerLabel, "EMERGENCY STOP ACTIVE");
    lv_obj_set_style_text_color(persistentBannerLabel, lv_color_hex(ShowduinoPalette::DangerText), 0);
    lv_obj_set_style_text_font(persistentBannerLabel, &lv_font_montserrat_14, 0);
    lv_obj_center(persistentBannerLabel);
  }

  void updatePersistentBanner() {
    if (!persistentBannerRoot) buildPersistentBanner();
    /* Banner follows Stage runtime state - hides when state leaves EMERGENCY_STOP. */
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
      if (persistentBannerLabel) {
        lv_label_set_text(persistentBannerLabel, line);
        lv_obj_set_style_text_color(persistentBannerLabel,
                                    lv_color_hex(ShowduinoPalette::DangerText), 0);
      }
      lv_obj_set_style_border_color(persistentBannerRoot, lv_color_hex(ShowduinoPalette::Danger), 0);
      lv_obj_clear_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(persistentBannerRoot);
      if (statusBar_.root()) lv_obj_move_foreground(statusBar_.root());
      if (abortConfirmRoot && !lv_obj_has_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(abortConfirmRoot);
      }
      gDirectorEmergencyClearDialog.raise();
    } else if (safetyEstopFault_ && !emergencyLocked) {
      if (persistentBannerLabel) {
        lv_label_set_text(persistentBannerLabel,
                          estopSheet_.warning[0] ? estopSheet_.warning
                          : "SAFETY NODE FAULT  EMERGENCY STATION OFFLINE");
        lv_obj_set_style_text_color(persistentBannerLabel,
                                    lv_color_hex(ShowduinoPalette::Warn), 0);
      }
      lv_obj_set_style_border_color(persistentBannerRoot, lv_color_hex(ShowduinoPalette::Warn), 0);
      lv_obj_clear_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(persistentBannerRoot);
    } else {
      lv_obj_set_style_border_color(persistentBannerRoot, lv_color_hex(ShowduinoPalette::Danger), 0);
      if (persistentBannerLabel) {
        lv_obj_set_style_text_color(persistentBannerLabel,
                                    lv_color_hex(ShowduinoPalette::DangerText), 0);
      }
      lv_obj_add_flag(persistentBannerRoot, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void buildAbortConfirm() {
    if (abortConfirmRoot) return;
    lv_obj_t *top = lv_layer_top();
    abortConfirmRoot = os_.makeDialogScrim(top);
    lv_obj_add_flag(abortConfirmRoot, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *box = os_.makeDialogBox(abortConfirmRoot, 440, 210, true);

    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, "ABORT SHOW?");
    lv_obj_set_style_text_color(t, lv_color_hex(ShowduinoPalette::Text), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(t, 24, 20);

    lv_obj_t *m = lv_label_create(box);
    lv_label_set_text(m, "Request Stage to stop playback and return to Desktop.\nThe production remains loaded. Awaiting Stage confirmation.");
    lv_obj_set_style_text_color(m, lv_color_hex(ShowduinoPalette::Muted), 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(m, 24, 58);
    lv_obj_set_width(m, 390);
    lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);

    makeButton(box, "CONFIRM ABORT", 24, 140, 190, OS_BTN_H, "UI:ESTOP:ABORT:YES", true, false);
    makeButton(box, "CANCEL", 230, 140, 170, OS_BTN_H, "UI:ESTOP:ABORT:NO", false, false);
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
    lv_obj_set_style_bg_color(completeOverlayRoot, lv_color_hex(ShowduinoPalette::Background), 0);
    lv_obj_set_style_bg_opa(completeOverlayRoot, LV_OPA_COVER, 0);
    lv_obj_add_flag(completeOverlayRoot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(completeOverlayRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(completeOverlayRoot, LV_OBJ_FLAG_SCROLLABLE);

    os_.paintChassis(completeOverlayRoot);

    lv_obj_t *kicker = lv_label_create(completeOverlayRoot);
    lv_label_set_text(kicker, "///  PRODUCTION");
    lv_obj_set_style_text_color(kicker, lv_color_hex(ShowduinoPalette::Accent), 0);
    lv_obj_set_style_text_font(kicker, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(kicker, 34, OS_BODY_Y);

    lv_obj_t *title = lv_label_create(completeOverlayRoot);
    lv_label_set_text(title, "SHOW COMPLETE");
    lv_obj_set_style_text_color(title, lv_color_hex(ShowduinoPalette::Text), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(title, 34, OS_BODY_Y + 32);

    lv_obj_t *subtitle = lv_label_create(completeOverlayRoot);
    lv_label_set_text(subtitle, "STAGE REPORTS FINISHED");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(ShowduinoPalette::Accent), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(subtitle, 36, OS_BODY_Y + 68);

    const int actionY = SCREEN_HEIGHT - 28 - OS_BTN_H;
    const int boxY = OS_BODY_Y + 96;
    int boxH = actionY - OS_GAP - boxY;
    if (boxH < 120) boxH = 120;

    lv_obj_t *report = os_.makeRaisedCard(completeOverlayRoot, 34, boxY, 732, boxH, true);
    lv_obj_set_style_pad_all(report, 16, 0);
    os_.enableVerticalScroll(report);

    completeDetailLabel = lv_label_create(report);
    lv_label_set_text(completeDetailLabel, "Show: -");
    lv_obj_set_style_text_color(completeDetailLabel, lv_color_hex(ShowduinoPalette::Text), 0);
    lv_obj_set_style_text_font(completeDetailLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_width(completeDetailLabel, 700);
    lv_label_set_long_mode(completeDetailLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(completeDetailLabel, 0, 0);

    makeButton(completeOverlayRoot, "RUN AGAIN", 34, actionY, 350, OS_BTN_H,
               "UI:COMPLETE:RUN");
    makeButton(completeOverlayRoot, "RETURN HOME", 400, actionY, 366, OS_BTN_H,
               "UI:COMPLETE:MENU");
  }

  void showCompleteScreen(const ShowRuntime &rt) {
    if (emergencyOverlayVisible) return; /* emergency wins */
    char et[16], done[16];
    formatClock(rt.elapsedMs ? rt.elapsedMs : rt.totalDurationMs, et, sizeof(et));
    formatClock(millis() - bootMs, done, sizeof(done));
    char detail[320];
    snprintf(detail, sizeof(detail),
             "Show: %s\nTotal runtime: %s\nCues executed: %lu / %lu\nWarnings: %u\nErrors: %u\nEmergency count: %u\nCompletion time: T+%s\nStage reports finished. The production remains loaded.",
             rt.showName[0] ? rt.showName : "-",
             et,
             (unsigned long)rt.currentCue,
             (unsigned long)rt.totalCues,
             (unsigned)sessionWarningCount,
             (unsigned)sessionErrorCount,
             (unsigned)sessionEmergencyCount,
             done);
    if (displayManager_.showPage(PAGE_COMPLETE)) {
      displayManager_.setSystemDetail(detail);
      completeOverlayVisible = true;
      pushDisplaySnapshot();
      return;
    }
    if (!completeOverlayRoot) buildCompleteOverlay();
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
    page_10_live_set_cue(cueBuf);
    page_10_live_set_elapsed(et);
    page_10_live_set_remain(rt);

    const char *pending = "";
    uint32_t col = OsColor::TextMuted;
    if (pendingResumeAwait) {
      pending = "RESUME PENDING - awaiting Stage";
      col = OsColor::Pending;
    } else if (pendingAbortAwait) {
      pending = "ABORT PENDING - awaiting Stage";
      col = OsColor::Pending;
    } else if (emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) {
      pending = "EMERGENCY - Stage latch active";
      col = OsColor::Fault;
    }
    page_10_live_set_pending(pending, col);
    page_10_live_set_progress(liveProgressPct);
    page_10_live_set_emergency((mirroredState == SHOW_STATE_EMERGENCY_STOP) || emergencyLocked);

    uint32_t headerCol = OsColor::Accent;
    if (emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP) headerCol = OsColor::Fault;
    else if (mirroredState == SHOW_STATE_ERROR) headerCol = OsColor::Fault;
    else if (mirroredState == SHOW_STATE_RUNNING) headerCol = OsColor::Ok;
    page_10_live_set_header(deskRuntimeWord(), headerCol);
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

  void hideAboutDialog() {
    if (aboutRoot_) lv_obj_add_flag(aboutRoot_, LV_OBJ_FLAG_HIDDEN);
  }

  void hideNetworkDialog() {
    if (networkRoot_) lv_obj_add_flag(networkRoot_, LV_OBJ_FLAG_HIDDEN);
  }

  void fillAboutText(char *text, size_t n) {
    const char *otaLine = "COMMS ONLY — not system-wide";
    const char *phaseLine = "READY";
    if (maintenanceWire_ && !commsOtaActive_) phaseLine = "SYSTEM MAINTENANCE";
    if (commsOtaActive_) {
      if (!strcmp(commsOtaState_, "DOWNLOADING") ||
          !strcmp(commsOtaState_, "VERIFYING") ||
          !strcmp(commsOtaState_, "INSTALLING")) {
        phaseLine = "UPDATING COMMS";
      } else if (!strcmp(commsOtaState_, "REBOOT_REQUIRED")) {
        phaseLine = "COMMS RESTARTING";
      } else if (!strcmp(commsOtaState_, "PENDING_VALIDATION")) {
        phaseLine = "PENDING VALIDATION";
      } else if (!strcmp(commsOtaState_, "COMPLETE")) {
        phaseLine = "UPDATE COMPLETE";
      } else if (!strcmp(commsOtaState_, "ROLLED_BACK")) {
        phaseLine = "UPDATE ROLLED BACK";
      } else if (!strcmp(commsOtaState_, "FAILED") ||
                 !strcmp(commsOtaState_, "INTERRUPTED_BY_EMERGENCY")) {
        phaseLine = "UPDATE FAILED";
      } else {
        phaseLine = commsOtaState_;
      }
    }
    if (expectCommsReconnect_ && linkState != LINK_READY) {
      phaseLine = "RECONNECTING";
    }
    snprintf(text, n,
             "Showduino  %s\n"
             "Director   %s\n"
             "Board      %s\n"
             "Protocol   %d.%d\n"
             "Update     %s\n"
             "OTA        %s\n"
             "E-stop     ESTOP-01 then healthy+linked\n"
             "Role       control surface\n"
             "P4 is the show authority.",
             SHOWDUINO_PLATFORM_VERSION,
             STORAGE_FW_VERSION,
             SHOWDUINO_BOARD_NAME,
             SHOWDUINO_PROTOCOL_VERSION_MAJOR,
             SHOWDUINO_PROTOCOL_VERSION_MINOR,
             phaseLine,
             otaLine);
  }

  void showAboutDialog() {
    if (!aboutRoot_) {
      aboutRoot_ = os_.makeDialogScrim(lv_layer_top());
      lv_obj_t *box = os_.makeDialogBox(aboutRoot_, 520, 360, false);

      lv_obj_t *title = lv_label_create(box);
      lv_label_set_text(title, "SOFTWARE");
      lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(title, lv_color_hex(ShowduinoPalette::Text), 0);
      lv_obj_set_pos(title, 24, 20);

      aboutBody_ = lv_label_create(box);
      lv_obj_set_style_text_font(aboutBody_, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(aboutBody_, lv_color_hex(ShowduinoPalette::Muted), 0);
      lv_obj_set_pos(aboutBody_, 24, 64);
      lv_obj_set_width(aboutBody_, 470);
      lv_label_set_long_mode(aboutBody_, LV_LABEL_LONG_WRAP);

      makeButton(box, "CLOSE", 180, 290, 160, OS_BTN_H, "UI:ABOUT:CLOSE");
    }
    if (aboutBody_) {
      char text[420];
      fillAboutText(text, sizeof(text));
      lv_label_set_text(aboutBody_, text);
    }
    lv_obj_clear_flag(aboutRoot_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(aboutRoot_);
  }

  void showNetworkDialog() {
    if (!networkRoot_) {
      networkRoot_ = os_.makeDialogScrim(lv_layer_top());
      lv_obj_t *box = os_.makeDialogBox(networkRoot_, 520, 300, false);
      lv_obj_t *title = lv_label_create(box);
      lv_label_set_text(title, "NETWORK");
      lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(title, lv_color_hex(ShowduinoPalette::Text), 0);
      lv_obj_set_pos(title, 24, 20);
      networkBody_ = lv_label_create(box);
      lv_obj_set_style_text_font(networkBody_, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(networkBody_, lv_color_hex(ShowduinoPalette::Muted), 0);
      lv_obj_set_pos(networkBody_, 24, 64);
      lv_obj_set_width(networkBody_, 470);
      lv_label_set_long_mode(networkBody_, LV_LABEL_LONG_WRAP);
      makeButton(box, "CLOSE", 180, 230, 160, OS_BTN_H, "UI:NETWORK:CLOSE");
    }
    if (networkBody_) {
      char text[280];
      snprintf(text, sizeof(text),
               "Showduino AP     %s\n"
               "Home / venue     %s\n"
               "Internet         %s\n"
               "ESP-NOW channel  %u\n"
               "Director link is not home Wi-Fi.\n"
               "Internet loss is not SHOWDUINO lost.",
               gwAp_ ? "ON" : "OFF",
               gwSta_ ? "ON" : "OFF",
               gwInet_ ? "ON" : "OFF",
               (unsigned)gwCh_);
      lv_label_set_text(networkBody_, text);
    }
    lv_obj_clear_flag(networkRoot_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(networkRoot_);
  }

  void restoreAfterLinkLost() {
    DisplayPageId back = pageBeforeLinkLost;
    pageBeforeLinkLost = PAGE_DESKTOP;
    switch (back) {
      case PAGE_LIVE: showLive(); break;
      case PAGE_SHOWS: showShows(); break;
      case PAGE_SHOW_DETAILS: showShows(); break;
      case PAGE_NODES: showNodes(); break;
      case PAGE_AUDIO_NODE: showAudioNode(); break;
      case PAGE_DIAGNOSTICS: showDiagnostics(); break;
      case PAGE_SETTINGS: showSettings(); break;
      case PAGE_AUDIO: showAudio(); break;
      case PAGE_LOGS: showLogs(); break;
      default: showDesktop(); break;
    }
  }

  void restorePageAfterEmergency() {
    switch (pageBeforeEmergency) {
      case PAGE_LIVE: showLive(); break;
      case PAGE_SHOWS: showShows(); break;
      case PAGE_SHOW_DETAILS: showShows(); break;
      case PAGE_NODES: showNodes(); break;
      case PAGE_AUDIO_NODE: showAudioNode(); break;
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

  void refreshSettingsPage() {
    if (!page_08_settings_is_active()) return;
    char dispStatus[28];
    char dispDetail[96];
    if (screenTimeoutMinutes == 0) {
      snprintf(dispStatus, sizeof(dispStatus), "NEVER");
      snprintf(dispDetail, sizeof(dispDetail), "Always on  |  Touch %s",
               touchLvglCalibrationIsNvs() ? "Calibrated" : "Factory");
    } else {
      snprintf(dispStatus, sizeof(dispStatus), "%u MIN", (unsigned)screenTimeoutMinutes);
      snprintf(dispDetail, sizeof(dispDetail), "Dim then off  |  Touch %s",
               touchLvglCalibrationIsNvs() ? "Calibrated" : "Factory");
    }
    page_08_settings_set_card(PAGE08_CARD_DISPLAY, true, dispStatus, dispDetail, OsColor::Accent);

    char atmoStatus[28];
    char atmoDetail[96];
    snprintf(atmoStatus, sizeof(atmoStatus), "%s", directorAmbientEnabled() ? "ON" : "OFF");
    snprintf(atmoDetail, sizeof(atmoDetail), "Bright %u  |  Motion %s",
             (unsigned)directorAmbientBrightness(), directorUiMotionModeName());
    page_08_settings_set_card(PAGE08_CARD_ATMOSPHERE, true, atmoStatus, atmoDetail, OsColor::Accent);
    page_08_settings_set_card(PAGE08_CARD_AUDIO, true, "OPEN",
                              "P4 local Stop is live", OsColor::Accent);
    char logsStatus[28];
    snprintf(logsStatus, sizeof(logsStatus), "%u EVENTS", (unsigned)eventLogCount);
    page_08_settings_set_card(PAGE08_CARD_LOGS, true, logsStatus, "Operator history",
                              OsColor::Accent);
    page_08_settings_set_card(PAGE08_CARD_STORAGE, true, "LOCAL", "Backup / export",
                              OsColor::Accent);
    page_08_settings_set_card(PAGE08_CARD_SYSTEM, true, STORAGE_FW_VERSION,
                              "About / network / software", OsColor::Accent);
    page_08_settings_set_header(STORAGE_FW_VERSION, OsColor::Accent);

    const bool em = emergencyLocked || mirroredState == SHOW_STATE_EMERGENCY_STOP;
    page_08_settings_set_strip(
        "SAFETY",
        em ? "E-STOP ACTIVE  |  CLEAR is a request" : "CLEAR  |  P4 remains the latch.",
        em ? OsColor::Fault : OsColor::Ok);
  }

  void refreshTimeoutLabel() { refreshSettingsPage(); }

  void refreshAtmosphereLabel() { refreshSettingsPage(); }

  void clearShowListChildren() {
    if (showListScroll == nullptr) return;
    lv_obj_clean(showListScroll);
  }

  void rebuildShowList(const ShowManager &sm) {
    showListCount = 0;
    Page02ProductionEntry rows[PAGE02_MAX_PRODUCTIONS];
    memset(rows, 0, sizeof(rows));
    int n = 0;
    for (uint8_t i = 0; i < sm.size() && showListCount < SHOW_INDEX_MAX; i++) {
      const ShowIndexEntry *e = sm.get(i);
      if (!e || !e->id[0]) continue;
      showListCache[showListCount] = *e;
      snprintf(showOpenCmds[showListCount], sizeof(showOpenCmds[showListCount]),
               "UI:SHOW:OPEN:%s", e->id);
      if (n < PAGE02_MAX_PRODUCTIONS) {
        strncpy(rows[n].id, e->id, sizeof(rows[n].id) - 1);
        strncpy(rows[n].name, e->name[0] ? e->name : e->id, sizeof(rows[n].name) - 1);
        strncpy(rows[n].description, e->description, sizeof(rows[n].description) - 1);
        strncpy(rows[n].modified, e->modified[0] ? e->modified : "-", sizeof(rows[n].modified) - 1);
        rows[n].used = true;
        n++;
      }
      showListCount++;
    }
    page_02_productions_set_entries(rows, n);

    /* Page 02 owns the production library shell. Legacy list widgets are unused. */
    if (showListScroll == nullptr) {
      Serial.printf("[UI] ShowManager index size=%u (Page 02 SD library)\n",
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
        lv_obj_set_style_text_color(badge, lv_color_hex(OsColor::Accent), 0);
        lv_obj_set_pos(badge, 14, 48);
      }

      char dur[16];
      ShowduinoShowThumb::formatDuration(e->durationSeconds, dur, sizeof(dur));
      char line1[96];
      snprintf(line1, sizeof(line1), "%s", e->name);
      lv_obj_t *n = lv_label_create(row);
      lv_label_set_text(n, line1);
      lv_obj_set_style_text_color(n, lv_color_hex(OsColor::Text), 0);
      lv_obj_set_pos(n, 72, 6);

      char line2[128];
      snprintf(line2, sizeof(line2), "%s  |  v%s  |  %s",
               dur, e->version[0] ? e->version : "-", e->author[0] ? e->author : "-");
      lv_obj_t *m = lv_label_create(row);
      lv_label_set_text(m, line2);
      lv_obj_set_style_text_color(m, lv_color_hex(OsColor::TextMuted), 0);
      lv_obj_set_pos(m, 72, 28);

      char line3[96];
      if (e->description[0]) {
        snprintf(line3, sizeof(line3), "%.70s", e->description);
      } else {
        snprintf(line3, sizeof(line3), "(no description)");
      }
      lv_obj_t *d = lv_label_create(row);
      lv_label_set_text(d, line3);
      lv_obj_set_style_text_color(d, lv_color_hex(OsColor::TextDim), 0);
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

    char shownName[64];
    char shownDesc[96];
    char shownAuthor[48];
    char shownVersion[24];
    director_ui_sanitize_copy(shownName, sizeof(shownName), name);
    director_ui_sanitize_copy(shownDesc, sizeof(shownDesc), desc);
    director_ui_sanitize_copy(shownAuthor, sizeof(shownAuthor), author);
    director_ui_sanitize_copy(shownVersion, sizeof(shownVersion), version);
    if (detailsNameLabel) lv_label_set_text(detailsNameLabel, shownName);
    if (detailsDescLabel) lv_label_set_text(detailsDescLabel, shownDesc);

    char dur[16];
    ShowduinoShowThumb::formatDuration(durSec, dur, sizeof(dur));
    char meta[160];
    snprintf(meta, sizeof(meta), "Duration  %s\nVersion   %s\nAuthor    %s",
             dur, shownVersion, shownAuthor);
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
      Serial.println("[UI] Home page unavailable");
      pushOperatorEvent("Home page unavailable");
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showLive() {
    if (displayManager_.showPage(PAGE_LIVE)) {
      Serial.println("[UI] Page 10 Live (LVGL)");
      if (page_10_live_is_active()) {
        page_10_live_apply_theme();
        refreshLiveStatusPanel();
      }
      pushDisplaySnapshot();
    } else {
      Serial.println("[UI] Live page unavailable");
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
  void showNodes() {
    if (displayManager_.showPage(PAGE_NODES)) {
      Serial.println("[UI] Page 04 Nodes (LVGL)");
      if (page_04_nodes_is_active()) {
        page_04_nodes_apply_theme();
        refreshNodesPage();
      }
      pushDisplaySnapshot();
      if (commandCallback) commandCallback("STATUS:REQUEST");
    } else {
      Serial.println("[UI] Nodes page unavailable");
      showDesktop();
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showAudioNode() {
    if (displayManager_.showPage(PAGE_AUDIO_NODE)) {
      Serial.println("[UI] Page 05 Audio Node (LVGL)");
      if (page_05_audio_node_is_active()) {
        page_05_audio_node_apply_theme();
        refreshAudioNodePage();
      }
      pushDisplaySnapshot();
      if (commandCallback) commandCallback("STATUS:REQUEST");
    } else {
      Serial.println("[UI] Audio Node page unavailable");
      showNodes();
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showDiagnostics() {
    if (displayManager_.showPage(PAGE_DIAGNOSTICS)) {
      Serial.println("[UI] Page 06 Diagnostics (LVGL)");
      if (page_06_diagnostics_is_active()) {
        page_06_diagnostics_apply_theme();
        refreshDiagnosticsPage();
      }
      pushDisplaySnapshot();
      if (commandCallback) {
        commandCallback("HELLO");
        commandCallback("STATUS:REQUEST");
      }
    } else {
      Serial.println("[UI] Diagnostics page unavailable");
      showDesktop();
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showSettings() {
    if (displayManager_.showPage(PAGE_SETTINGS)) {
      Serial.println("[UI] Page 08 Settings (LVGL)");
      if (page_08_settings_is_active()) {
        page_08_settings_apply_theme();
        refreshSettingsPage();
      }
      pushDisplaySnapshot();
    } else {
      Serial.println("[UI] Settings page unavailable");
      showDesktop();
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showAudio() {
    if (displayManager_.showPage(PAGE_AUDIO)) {
      if (page_audio_system_is_active()) page_audio_system_apply_theme();
      refreshAudioPresentation();
      pushDisplaySnapshot();
    } else {
      Serial.println("[UI] Audio page unavailable");
      showSettings();
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showLogs() {
    if (displayManager_.showPage(PAGE_LOGS)) {
      if (page_logs_is_active()) page_logs_apply_theme();
      const bool wasPaused = logsLivePaused_;
      logsLivePaused_ = false;
      refreshLogsDisplay();
      logsLivePaused_ = wasPaused;
      pushDisplaySnapshot();
    } else {
      Serial.println("[UI] Logs page unavailable");
      showSettings();
    }
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
};

#endif
