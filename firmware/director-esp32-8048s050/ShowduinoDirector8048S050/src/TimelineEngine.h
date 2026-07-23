#ifndef SHOWDUINO_TIMELINE_ENGINE_H
#define SHOWDUINO_TIMELINE_ENGINE_H

#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include "StorageConfig.h"
#include "FileUtil.h"

/**
 * Data-driven Timeline Engine (Stage 5).
 * Loads timeline.json once, plays with millis(), dispatches raw command strings.
 * Does not interpret commands — dispatch callback sends them as-is to Stage.
 */

#ifndef TIMELINE_MAX_CUES
#define TIMELINE_MAX_CUES 4096
#endif
#ifndef TIMELINE_CMD_LEN
#define TIMELINE_CMD_LEN 72
#endif
#ifndef TIMELINE_MAX_JSON_BYTES
#define TIMELINE_MAX_JSON_BYTES (256UL * 1024UL)
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
  bool LoadTimeline(const char *timelineJsonPath) {
    Stop();
    freeCues();

    if (!timelineJsonPath || !ShowduinoFileUtil::pathLooksSafe(timelineJsonPath)) {
      Serial.println("[Timeline] load failed — bad path");
      return false;
    }
    if (!SD.exists(timelineJsonPath)) {
      Serial.printf("[Timeline] load failed — missing %s\n", timelineJsonPath);
      return false;
    }

    String json;
    if (!ShowduinoFileUtil::readTextFile(timelineJsonPath, json, TIMELINE_MAX_JSON_BYTES)) {
      Serial.println("[Timeline] load failed — read error");
      return false;
    }
    bool ok = loadFromJsonString(json);
    json = "";
    if (ok) {
      strncpy(loadedPath, timelineJsonPath, sizeof(loadedPath) - 1);
      loadedPath[sizeof(loadedPath) - 1] = '\0';
    }
    return ok;
  }

  /** Stage 6: build cues in RAM without SD (Director uploads cues). */
  bool BeginMemoryLoad() {
    Stop();
    freeCues();
    memLoadActive = true;
    memLoadCount = 0;
    return true;
  }

  bool AddMemoryCue(uint32_t timeMs, const char *command) {
    if (!memLoadActive || !command || !command[0]) return false;
    if (memLoadCount >= TIMELINE_MAX_CUES) return false;
    if (!ensureCueCapacity(memLoadCount + 1)) return false;
    cues[memLoadCount].timeMs = timeMs;
    strncpy(cues[memLoadCount].command, command, TIMELINE_CMD_LEN - 1);
    cues[memLoadCount].command[TIMELINE_CMD_LEN - 1] = '\0';
    memLoadCount++;
    return true;
  }

  bool EndMemoryLoad() {
    if (!memLoadActive) return false;
    memLoadActive = false;
    cueCount = memLoadCount;
    memLoadCount = 0;
    if (cueCount > 1) stableSortByTime();
    durationMs = 0;
    for (uint16_t i = 0; i < cueCount; i++) {
      if (cues[i].timeMs > durationMs) durationMs = cues[i].timeMs;
    }
    Serial.println("Timeline loaded");
    Serial.printf("Cue count: %u\n", (unsigned)cueCount);
    return true;
  }

  void ClearTimeline() {
    Stop();
    freeCues();
    memLoadActive = false;
    memLoadCount = 0;
  }

  void Start() {
    if (cueCount == 0 && durationMs == 0 && !loadedPath[0]) {
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
  const char *path() const { return loadedPath; }
  const TimelineCue *cueAt(uint16_t index) const {
    if (!cues || index >= cueCount) return nullptr;
    return &cues[index];
  }

  uint8_t progressPercent() const {
    if (durationMs == 0) {
      return (playState == TimelinePlayState::Finished) ? 100 : 0;
    }
    uint32_t t = CurrentTime();
    if (t >= durationMs) return 100;
    return (uint8_t)((t * 100UL) / durationMs);
  }

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

  static const char *stateText(TimelinePlayState s) {
    switch (s) {
      case TimelinePlayState::Running:  return "Running";
      case TimelinePlayState::Paused:   return "Paused";
      case TimelinePlayState::Finished: return "Finished";
      default:                          return "Stopped";
    }
  }

private:
  TimelineCue *cues = nullptr;
  uint16_t cueCount = 0;
  uint16_t cueCapacity = 0;
  uint16_t nextCue = 0;
  uint16_t memLoadCount = 0;
  bool memLoadActive = false;
  uint32_t durationMs = 0;
  uint32_t elapsedMs = 0;
  uint32_t anchorMs = 0;
  uint32_t pausedAccumMs = 0;
  TimelinePlayState playState = TimelinePlayState::Stopped;
  TimelineDispatchFn dispatchFn = nullptr;
  char loadedPath[STORAGE_MAX_PATH_LEN] = {};
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
    loadedPath[0] = '\0';
  }

  bool ensureCueCapacity(uint16_t need) {
    if (need <= cueCapacity && cues) return true;
    uint16_t cap = cueCapacity ? cueCapacity : 16;
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
    uint16_t keep = cueCount;
    if (memLoadActive && memLoadCount > keep) keep = memLoadCount;
    if (cues && keep) {
      memcpy(nbuf, cues, sizeof(TimelineCue) * keep);
      heap_caps_free(cues);
    } else if (cues) {
      heap_caps_free(cues);
    }
    cues = nbuf;
    cueCapacity = cap;
    return true;
  }

  bool loadFromJsonString(const String &json) {
    int arr = json.indexOf("\"timeline\"");
    if (arr < 0) arr = json.indexOf("\"events\"");
    if (arr < 0) {
      Serial.println("[Timeline] load failed — no timeline/events array");
      return false;
    }
    int bracket = json.indexOf('[', arr);
    int end = json.indexOf(']', bracket);
    if (bracket < 0 || end <= bracket) {
      Serial.println("[Timeline] load failed — malformed array");
      return false;
    }

    uint16_t need = 0;
    int pos = bracket + 1;
    while (pos < end && need < TIMELINE_MAX_CUES) {
      int o1 = json.indexOf('{', pos);
      if (o1 < 0 || o1 >= end) break;
      int o2 = json.indexOf('}', o1);
      if (o2 < 0 || o2 > end) break;
      need++;
      pos = o2 + 1;
    }

    if (need == 0) {
      cueCount = 0;
      durationMs = 0;
      Serial.println("Timeline loaded");
      Serial.printf("Cue count: %u\n", (unsigned)cueCount);
      return true;
    }

    if (!ensureCueCapacity(need)) {
      Serial.println("[Timeline] load failed — OOM");
      return false;
    }

    uint16_t n = 0;
    pos = bracket + 1;
    while (pos < end && n < need) {
      int o1 = json.indexOf('{', pos);
      if (o1 < 0 || o1 >= end) break;
      int o2 = json.indexOf('}', o1);
      if (o2 < 0 || o2 > end) break;
      String obj = json.substring(o1, o2 + 1);

      long t = ShowduinoFileUtil::jsonGetLong(obj, "time", -1);
      if (t < 0) t = ShowduinoFileUtil::jsonGetLong(obj, "timeMs", -1);
      String cmd = ShowduinoFileUtil::jsonGetString(obj, "command", "");
      if (t >= 0 && cmd.length() > 0) {
        cues[n].timeMs = (uint32_t)t;
        strncpy(cues[n].command, cmd.c_str(), TIMELINE_CMD_LEN - 1);
        cues[n].command[TIMELINE_CMD_LEN - 1] = '\0';
        n++;
      }
      pos = o2 + 1;
      yield();
    }

    cueCount = n;
    stableSortByTime();

    durationMs = 0;
    for (uint16_t i = 0; i < cueCount; i++) {
      if (cues[i].timeMs > durationMs) durationMs = cues[i].timeMs;
    }

    Serial.println("Timeline loaded");
    Serial.printf("Cue count: %u\n", (unsigned)cueCount);
    Serial.printf("[Timeline] duration=%lu ms\n", (unsigned long)durationMs);
    return true;
  }

  void stableSortByTime() {
    /* Insertion sort — stable, fine for a few thousand cues once at load. */
    for (uint16_t i = 1; i < cueCount; i++) {
      TimelineCue key = cues[i];
      int j = (int)i - 1;
      while (j >= 0 && cues[j].timeMs > key.timeMs) {
        cues[j + 1] = cues[j];
        j--;
      }
      cues[j + 1] = key;
      if ((i & 31) == 0) yield();
    }
  }
};

#endif
