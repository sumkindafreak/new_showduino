#ifndef SHOWDUINO_TIMELINE_ENGINE_H
#define SHOWDUINO_TIMELINE_ENGINE_H

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

/**
 * Stage Engine Timeline (Stage 6) — RAM cues only.
 * Director uploads cues; Stage owns playback clock and dispatch.
 */

#ifndef TIMELINE_MAX_CUES
#define TIMELINE_MAX_CUES 2048
#endif
#ifndef TIMELINE_CMD_LEN
#define TIMELINE_CMD_LEN 64
#endif

enum class TimelinePlayState : uint8_t {
  Stopped = 0,
  Running,
  Paused,
  Finished
};

struct TimelineCue {
  uint32_t timeMs;
  char command[TIMELINE_CMD_LEN];
};

typedef void (*TimelineDispatchFn)(const char *command);

class TimelineEngine {
public:
  bool BeginMemoryLoad() {
    discardLoading();
    memLoadActive = true;
    memLoadCount = 0;
    return true;
  }

  bool AddMemoryCue(uint32_t timeMs, const char *command) {
    if (!memLoadActive || !command || !command[0]) return false;
    if (memLoadCount >= TIMELINE_MAX_CUES) return false;
    if (!ensureLoadingCapacity(memLoadCount + 1)) return false;
    loadingCues[memLoadCount].timeMs = timeMs;
    strncpy(loadingCues[memLoadCount].command, command, TIMELINE_CMD_LEN - 1);
    loadingCues[memLoadCount].command[TIMELINE_CMD_LEN - 1] = '\0';
    memLoadCount++;
    return true;
  }

  bool EndMemoryLoad() {
    if (!memLoadActive || !loadingCues || memLoadCount == 0) return false;
    if (memLoadCount > 1) stableSortByTime(loadingCues, memLoadCount);
    uint32_t newDurationMs = 0;
    for (uint16_t i = 0; i < memLoadCount; i++) {
      if (loadingCues[i].timeMs > newDurationMs) newDurationMs = loadingCues[i].timeMs;
    }

    /* Atomically replace the active timeline after the complete load succeeds. */
    Stop();
    freeCues();
    cues = loadingCues;
    cueCount = memLoadCount;
    cueCapacity = loadingCapacity;
    durationMs = newDurationMs;
    loadingCues = nullptr;
    loadingCapacity = 0;
    memLoadActive = false;
    memLoadCount = 0;
    Serial.println("Timeline loaded");
    Serial.printf("Cue count: %u\n", (unsigned)cueCount);
    return true;
  }

  void AbortMemoryLoad() {
    discardLoading();
    memLoadActive = false;
    memLoadCount = 0;
  }

  void ClearTimeline() {
    Stop();
    freeCues();
    AbortMemoryLoad();
  }

  void Start() {
    if (cueCount == 0 && durationMs == 0) {
      Serial.println("[Timeline] start ignored — nothing loaded");
      return;
    }
    playState = TimelinePlayState::Running;
    nextCue = 0;
    elapsedMs = 0;
    anchorMs = millis();
    pausedAccumMs = 0;
    Serial.println("Playback started");
  }

  void Stop() {
    if (playState != TimelinePlayState::Stopped) {
      Serial.println("Playback stopped");
    }
    playState = TimelinePlayState::Stopped;
    nextCue = 0;
    elapsedMs = 0;
    pausedAccumMs = 0;
    anchorMs = 0;
  }

  void Pause() {
    if (playState != TimelinePlayState::Running) return;
    elapsedMs = CurrentTime();
    playState = TimelinePlayState::Paused;
    Serial.println("Playback paused");
  }

  void Resume() {
    if (playState != TimelinePlayState::Paused) return;
    anchorMs = millis();
    pausedAccumMs = elapsedMs;
    playState = TimelinePlayState::Running;
    Serial.println("Playback resumed");
  }

  void Update() {
    if (playState != TimelinePlayState::Running) return;

    elapsedMs = CurrentTime();

    while (nextCue < cueCount && cues[nextCue].timeMs <= elapsedMs) {
      const char *cmd = cues[nextCue].command;
      Serial.printf("Cue executed @%lu: %s\n",
                    (unsigned long)cues[nextCue].timeMs, cmd);
      if (dispatchFn && cmd[0]) {
        dispatchFn(cmd);
      }
      nextCue++;
      yield();
    }

    if (nextCue >= cueCount && elapsedMs >= durationMs) {
      playState = TimelinePlayState::Finished;
      Serial.println("Playback finished");
    }
  }

  bool Finished() const { return playState == TimelinePlayState::Finished; }

  uint32_t CurrentTime() const {
    if (playState == TimelinePlayState::Running) {
      return pausedAccumMs + (millis() - anchorMs);
    }
    return elapsedMs;
  }

  uint32_t Duration() const { return durationMs; }
  TimelinePlayState state() const { return playState; }
  bool isActive() const {
    return playState == TimelinePlayState::Running || playState == TimelinePlayState::Paused;
  }
  uint16_t cueTotal() const { return cueCount; }
  uint16_t cuesFired() const { return nextCue; }

  uint32_t remainingMs() const {
    uint32_t t = CurrentTime();
    if (t >= durationMs) return 0;
    return durationMs - t;
  }

  void setDispatch(TimelineDispatchFn fn) { dispatchFn = fn; }

  void setShowName(const char *name) {
    strncpy(showName, name ? name : "", sizeof(showName) - 1);
    showName[sizeof(showName) - 1] = '\0';
  }
  const char *currentShowName() const { return showName; }

private:
  TimelineCue *cues = nullptr;
  TimelineCue *loadingCues = nullptr;
  uint16_t cueCount = 0;
  uint16_t cueCapacity = 0;
  uint16_t loadingCapacity = 0;
  uint16_t nextCue = 0;
  uint16_t memLoadCount = 0;
  bool memLoadActive = false;
  uint32_t durationMs = 0;
  uint32_t elapsedMs = 0;
  uint32_t anchorMs = 0;
  uint32_t pausedAccumMs = 0;
  TimelinePlayState playState = TimelinePlayState::Stopped;
  TimelineDispatchFn dispatchFn = nullptr;
  char showName[64] = {};

  void freeCues() {
    if (cues) {
      heap_caps_free(cues);
      cues = nullptr;
    }
    cueCount = 0;
    cueCapacity = 0;
    nextCue = 0;
    durationMs = 0;
  }

  void discardLoading() {
    if (loadingCues) {
      heap_caps_free(loadingCues);
      loadingCues = nullptr;
    }
    loadingCapacity = 0;
  }

  bool ensureLoadingCapacity(uint16_t need) {
    if (need <= loadingCapacity && loadingCues) return true;
    uint16_t cap = loadingCapacity ? loadingCapacity : 16;
    while (cap < need) {
      uint32_t next = (uint32_t)cap * 2u;
      if (next > TIMELINE_MAX_CUES) next = TIMELINE_MAX_CUES;
      if ((uint16_t)next <= cap) return false;
      cap = (uint16_t)next;
    }
    TimelineCue *nbuf = (TimelineCue *)heap_caps_malloc(sizeof(TimelineCue) * cap,
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!nbuf) {
      nbuf = (TimelineCue *)heap_caps_malloc(sizeof(TimelineCue) * cap,
                                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!nbuf) return false;
    memset(nbuf, 0, sizeof(TimelineCue) * cap);
    if (loadingCues && memLoadCount) {
      memcpy(nbuf, loadingCues, sizeof(TimelineCue) * memLoadCount);
      heap_caps_free(loadingCues);
    }
    loadingCues = nbuf;
    loadingCapacity = cap;
    return true;
  }

  static void stableSortByTime(TimelineCue *items, uint16_t count) {
    for (uint16_t i = 1; i < count; i++) {
      TimelineCue key = items[i];
      int j = (int)i - 1;
      while (j >= 0 && items[j].timeMs > key.timeMs) {
        items[j + 1] = items[j];
        j--;
      }
      items[j + 1] = key;
      if ((i & 31) == 0) yield();
    }
  }
};

#endif
