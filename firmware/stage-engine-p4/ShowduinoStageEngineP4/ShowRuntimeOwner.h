#ifndef SHOW_RUNTIME_OWNER_H
#define SHOW_RUNTIME_OWNER_H

/*
 * Showduino OS — Stage Runtime module (authoritative ShowRuntime).
 * Future OS modules: Timeline, Storage, Communications, Emergency, Logging, Nodes, UI.
 */

#include <Arduino.h>
#include "../../../protocol/showduino_show_runtime.h"
#include "../../../protocol/showduino_os_modules.h"
#include "ShowEngineState.h"
#include "TimelineEngine.h"

typedef void (*ShowRuntimeSendFn)(const char *line);

struct ShowRuntimeOwner {
  ShowRuntime rt;
  TimelineEngine timeline;
  ShowRuntimeSendFn sendFn = nullptr;
  uint32_t lastBroadcastMs = 0;
  bool tlLoadOpen = false;

  void begin(ShowRuntimeSendFn send) {
    sendFn = send;
    showRuntimeClear(&rt);
    rt.state = SHOW_STATE_BOOTING;
    rt.stateEnteredMs = millis();
    rt.revision = 1;
    showRuntimeSyncFlags(&rt);
    timeline.setDispatch(nullptr);
  }

  void bootToIdle() {
    transitionLogged(SHOW_STATE_IDLE, millis());
    syncLegacyShow(nullptr);
    broadcastAll();
  }

  void setDispatch(TimelineDispatchFn fn) { timeline.setDispatch(fn); }

  void syncFromTimeline() {
    rt.elapsedMs = timeline.CurrentTime();
    rt.remainingMs = timeline.remainingMs();
    rt.totalDurationMs = timeline.Duration();
    rt.currentCue = timeline.cuesFired();
    rt.totalCues = timeline.cueTotal();
    if (timeline.currentShowName()[0]) {
      strncpy(rt.showName, timeline.currentShowName(), sizeof(rt.showName) - 1);
      rt.showName[sizeof(rt.showName) - 1] = '\0';
    }
    showRuntimeSyncFlags(&rt);
  }

  bool transitionLogged(ShowState to, uint32_t nowMs) {
    ShowState from = rt.state;
    if (!showRuntimeCanTransition(from, to)) {
      Serial.printf("[Runtime] REJECT %s → %s\n", showStateName(from), showStateName(to));
      return false;
    }
    if (from != to) {
      Serial.printf("[Runtime] %s → %s\n", showStateName(from), showStateName(to));
    }
    return showRuntimeTransition(&rt, to, nowMs) != 0;
  }

  void setError(const char *err, uint32_t nowMs) {
    ShowState from = rt.state;
    showRuntimeSetError(&rt, err, nowMs);
    Serial.printf("[Runtime] %s → ERROR (%s)\n", showStateName(from), err ? err : "");
    broadcastAll();
  }

  void syncLegacyShow(ShowEngineState *legacy) {
    if (!legacy) return;
    switch (rt.state) {
      case SHOW_STATE_RUNNING:
      case SHOW_STATE_PAUSED:
        legacy->show = ShowRuntimeState::Playing;
        break;
      case SHOW_STATE_EMERGENCY_STOP:
        legacy->show = ShowRuntimeState::Emergency;
        break;
      default:
        legacy->show = ShowRuntimeState::Idle;
        break;
    }
  }

  void broadcastState() {
    if (!sendFn) return;
    char buf[48];
    if (showStateSerialize(rt.state, buf, sizeof(buf))) sendFn(buf);
  }

  void broadcastRuntime() {
    if (!sendFn) return;
    char buf[96];
    if (showRuntimeSerialize(&rt, buf, sizeof(buf))) sendFn(buf);
  }

  void broadcastAll() {
    broadcastState();
    broadcastRuntime();
    lastBroadcastMs = millis();
  }

  void onFinished() {
    uint32_t now = millis();
    syncFromTimeline();
    if (transitionLogged(SHOW_STATE_FINISHED, now)) {
      if (sendFn) sendFn(SHOW_FINISHED_WIRE);
      broadcastAll();
    }
  }

  void service(uint32_t nowMs, ShowEngineState *legacy) {
    timeline.Update();
    syncFromTimeline();

    if (timeline.Finished() && rt.state == SHOW_STATE_RUNNING) {
      onFinished();
      syncLegacyShow(legacy);
      return;
    }

    if (rt.state == SHOW_STATE_RUNNING) {
      if ((nowMs - lastBroadcastMs) >= 1000UL) {
        rt.revision++;
        broadcastRuntime();
        lastBroadcastMs = nowMs;
      }
    }

    syncLegacyShow(legacy);
  }

  bool handleLoadName(const char *name, uint32_t nowMs, ShowEngineState *legacy) {
    if (rt.state == SHOW_STATE_RUNNING || rt.state == SHOW_STATE_PAUSED ||
        rt.state == SHOW_STATE_EMERGENCY_STOP) {
      if (sendFn) sendFn("REJECTED:SHOW:BUSY");
      return false;
    }
    timeline.ClearTimeline();
    timeline.setShowName(name);
    strncpy(rt.showName, name ? name : "", sizeof(rt.showName) - 1);
    rt.showName[sizeof(rt.showName) - 1] = '\0';
    rt.lastError[0] = '\0';
    rt.aborted = 0;
    tlLoadOpen = false;
    if (rt.state == SHOW_STATE_FINISHED || rt.state == SHOW_STATE_ERROR ||
        rt.state == SHOW_STATE_SHOW_LOADED) {
      transitionLogged(SHOW_STATE_IDLE, nowMs);
    }
    syncLegacyShow(legacy);
    if (sendFn) sendFn("ACK:SHOW:LOAD");
    return true;
  }

  bool handleTlBegin() {
    if (!timeline.BeginMemoryLoad()) return false;
    tlLoadOpen = true;
    if (sendFn) sendFn("ACK:SHOW:TL:BEGIN");
    return true;
  }

  bool handleTlCue(uint32_t timeMs, const char *cmd) {
    if (!tlLoadOpen) return false;
    return timeline.AddMemoryCue(timeMs, cmd);
  }

  bool handleTlEnd(uint32_t nowMs, ShowEngineState *legacy) {
    if (!tlLoadOpen) {
      setError("timeline_not_open", nowMs);
      syncLegacyShow(legacy);
      return false;
    }
    tlLoadOpen = false;
    if (!timeline.EndMemoryLoad()) {
      setError("timeline_load_failed", nowMs);
      syncLegacyShow(legacy);
      return false;
    }
    syncFromTimeline();
    if (!transitionLogged(SHOW_STATE_SHOW_LOADED, nowMs) &&
        rt.state != SHOW_STATE_SHOW_LOADED) {
      /* Allow re-load while already SHOW_LOADED via IDLE hop */
      if (transitionLogged(SHOW_STATE_IDLE, nowMs)) {
        transitionLogged(SHOW_STATE_SHOW_LOADED, nowMs);
      }
    }
    syncLegacyShow(legacy);
    if (sendFn) sendFn("ACK:SHOW:TL:END");
    broadcastAll();
    return true;
  }

  bool handleRun(uint32_t nowMs, ShowEngineState *legacy) {
    if (legacy && legacy->emergency == EmergencyState::Active) {
      if (sendFn) sendFn("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return false;
    }
    if (rt.state != SHOW_STATE_SHOW_LOADED && rt.state != SHOW_STATE_PAUSED &&
        rt.state != SHOW_STATE_FINISHED) {
      if (rt.totalCues == 0 && timeline.cueTotal() == 0) {
        setError("no_timeline", nowMs);
        syncLegacyShow(legacy);
        return false;
      }
    }
    if (rt.state == SHOW_STATE_FINISHED) {
      transitionLogged(SHOW_STATE_SHOW_LOADED, nowMs);
    }
    if (rt.state == SHOW_STATE_PAUSED) {
      timeline.Resume();
    } else {
      timeline.Start();
    }
    if (!transitionLogged(SHOW_STATE_RUNNING, nowMs)) {
      if (sendFn) sendFn("REJECTED:SHOW:INVALID_TRANSITION");
      return false;
    }
    syncFromTimeline();
    syncLegacyShow(legacy);
    if (sendFn) sendFn(SHOWDUINO_LEGACY_ACK_SHOW_START);
    broadcastAll();
    return true;
  }

  bool handlePause(uint32_t nowMs, ShowEngineState *legacy) {
    if (rt.state != SHOW_STATE_RUNNING) {
      if (sendFn) sendFn("REJECTED:SHOW:INVALID_TRANSITION");
      return false;
    }
    timeline.Pause();
    syncFromTimeline();
    if (!transitionLogged(SHOW_STATE_PAUSED, nowMs)) return false;
    syncLegacyShow(legacy);
    if (sendFn) sendFn("ACK:SHOW:PAUSE");
    broadcastAll();
    return true;
  }

  bool handleResume(uint32_t nowMs, ShowEngineState *legacy) {
    if (legacy && legacy->emergency == EmergencyState::Active) {
      if (sendFn) sendFn("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return false;
    }
    if (rt.state == SHOW_STATE_EMERGENCY_STOP) {
      /* Operator resume after E-stop clear path uses SHOW:RESUME once emergency flag clear */
    }
    if (rt.state != SHOW_STATE_PAUSED && rt.state != SHOW_STATE_EMERGENCY_STOP) {
      if (sendFn) sendFn("REJECTED:SHOW:INVALID_TRANSITION");
      return false;
    }
    if (timeline.state() == TimelinePlayState::Paused) {
      timeline.Resume();
    } else if (rt.state == SHOW_STATE_EMERGENCY_STOP) {
      /* Was paused by E-stop — Resume clock */
      timeline.Resume();
    }
    if (!transitionLogged(SHOW_STATE_RUNNING, nowMs)) return false;
    syncFromTimeline();
    syncLegacyShow(legacy);
    if (sendFn) sendFn("ACK:SHOW:RESUME");
    broadcastAll();
    return true;
  }

  bool handleStop(uint32_t nowMs, ShowEngineState *legacy) {
    /* Abort Show must work during emergency — STOP is always a safe request.
       (RESUME remains blocked while emergency is active.) */
    const bool abortFromEstop =
        (rt.state == SHOW_STATE_EMERGENCY_STOP) ||
        (legacy && legacy->emergency == EmergencyState::Active);

    timeline.Stop();
    syncFromTimeline();
    rt.aborted = (rt.state == SHOW_STATE_RUNNING || rt.state == SHOW_STATE_PAUSED ||
                  rt.state == SHOW_STATE_EMERGENCY_STOP) ? 1 : 0;
    if (rt.state == SHOW_STATE_FINISHED) {
      transitionLogged(SHOW_STATE_IDLE, nowMs);
    } else if (rt.state != SHOW_STATE_IDLE) {
      if (!transitionLogged(SHOW_STATE_IDLE, nowMs)) {
        rt.state = SHOW_STATE_IDLE;
        rt.stateEnteredMs = nowMs;
        rt.revision++;
        showRuntimeSyncFlags(&rt);
        Serial.println("[Runtime] forced → IDLE");
      }
    }

    /* Abort Show ends the emergency show session — outputs already forced safe. */
    if (abortFromEstop && legacy) {
      legacy->emergency = EmergencyState::Clear;
    }

    syncLegacyShow(legacy);
    if (sendFn) sendFn(SHOWDUINO_LEGACY_ACK_SHOW_STOP);
    broadcastAll();
    return true;
  }

  void onEmergencyStop(uint32_t nowMs, ShowEngineState *legacy) {
    /* Preserve cue + elapsed — pause only. Safety routing stays in .ino. */
    if (timeline.state() == TimelinePlayState::Running) {
      timeline.Pause();
    }
    syncFromTimeline();
    transitionLogged(SHOW_STATE_EMERGENCY_STOP, nowMs);
    syncLegacyShow(legacy);
    broadcastAll();
  }

  void onEmergencyCleared(uint32_t nowMs, ShowEngineState *legacy) {
    /* Mid-show E-stop left the timeline Paused → stay PAUSED (operator RESUME).
       E-stop with no active playback → IDLE / SHOW_LOADED so Director can leave safe mode. */
    if (rt.state == SHOW_STATE_EMERGENCY_STOP) {
      if (timeline.state() == TimelinePlayState::Paused) {
        transitionLogged(SHOW_STATE_PAUSED, nowMs);
      } else if (rt.showName[0] && (rt.totalCues > 0 || rt.loaded)) {
        transitionLogged(SHOW_STATE_SHOW_LOADED, nowMs);
      } else {
        transitionLogged(SHOW_STATE_IDLE, nowMs);
      }
    }
    syncFromTimeline();
    syncLegacyShow(legacy);
    broadcastAll();
  }

  void handleStateQuery() { broadcastAll(); }
};

#endif /* SHOW_RUNTIME_OWNER_H */
