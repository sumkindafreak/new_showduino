#pragma once

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "Service.h"
#include "../OsTypes.h"
#include "../events/EventBus.h"
#include "../Compatibility.h"

namespace Os2 {

/**
 * ShowService — Compatibility contract v1 (Api::ShowService).
 * Truth only: current production, playback, cues, progress.
 * Intent (load/run/stop) → CommandService.
 */
class ShowService : public IService {
 public:
  static constexpr uint16_t kApiVersion = Api::ShowService;
  enum class Playback : uint8_t {
    Booting = 0,
    Idle,
    Loaded,
    Running,
    Paused,
    Emergency,
    Finished,
    Error
  };

  const char *id() const override { return "show"; }

  void begin() override { clear(); }

  void clear() {
    title_[0] = '\0';
    playback_ = Playback::Idle;
    elapsedMs_ = remainingMs_ = totalDurationMs_ = 0;
    currentCue_ = totalCues_ = 0;
    loaded_ = running_ = paused_ = emergency_ =     finished_ = false;
    revision_ = 0;
    bump();
  }

  void apply(const char *showName,
             Playback playback,
             uint32_t elapsedMs,
             uint32_t remainingMs,
             uint32_t totalDurationMs,
             uint32_t currentCue,
             uint32_t totalCues,
             bool loaded,
             bool running,
             bool paused,
             bool emergency,
             bool finished) {
    const Playback prevPlay = playback_;
    const bool prevRun = running_;
    const bool prevPause = paused_;
    const bool prevEmerg = emergency_;
    const bool prevFin = finished_;
    const bool prevLoaded = loaded_;
    const uint32_t prevCue = currentCue_;
    const uint8_t prevPct = progressPercent();
    bool changed = false;

    if (showName && showName[0]) {
      if (strncmp(title_, showName, sizeof(title_)) != 0) {
        strncpy(title_, showName, sizeof(title_) - 1);
        title_[sizeof(title_) - 1] = '\0';
        changed = true;
      }
    } else if (title_[0]) {
      title_[0] = '\0';
      changed = true;
    }

    if (playback_ != playback) { playback_ = playback; changed = true; }
    if (elapsedMs_ != elapsedMs) { elapsedMs_ = elapsedMs; changed = true; }
    if (remainingMs_ != remainingMs) { remainingMs_ = remainingMs; changed = true; }
    if (totalDurationMs_ != totalDurationMs) { totalDurationMs_ = totalDurationMs; changed = true; }
    if (currentCue_ != currentCue) { currentCue_ = currentCue; changed = true; }
    if (totalCues_ != totalCues) { totalCues_ = totalCues; changed = true; }
    if (loaded_ != loaded) { loaded_ = loaded; changed = true; }
    if (running_ != running) { running_ = running; changed = true; }
    if (paused_ != paused) { paused_ = paused; changed = true; }
    if (emergency_ != emergency) { emergency_ = emergency; changed = true; }
    if (finished_ != finished) { finished_ = finished; changed = true; }

    if (!changed) return;
    bump();

    /* Publish precise events — apps subscribe, never poll. */
    if (loaded_ && !prevLoaded) {
      events().publish(Event::ShowLoaded);
    }
    if (running_ && !prevRun) {
      events().publish(Event::ShowStarted);
    } else if (!running_ && prevRun) {
      events().publish(Event::ShowStopped);
    }
    if (paused_ && !prevPause) {
      events().publish(Event::ShowPaused);
    }
    if (finished_ && !prevFin) {
      events().publish(Event::ShowFinished);
    }
    if (currentCue_ != prevCue) {
      events().publish(Event::CueChanged, currentCue_, totalCues_);
    }
    if (emergency_ != prevEmerg) {
      events().publish(Event::EmergencyChanged, emergency_ ? 1 : 0);
    }
    {
      uint8_t pct = progressPercent();
      if (pct != prevPct) {
        events().publish(Event::ShowProgress, pct);
      }
    }
    if (playback_ != prevPlay) {
      events().publish(Event::SystemStatus);
    }
  }

  bool loaded() const { return loaded_ && title_[0]; }
  const char *title() const { return loaded() ? title_ : "None Loaded"; }
  Playback playback() const { return playback_; }

  const char *playbackLabel() const {
    switch (playback_) {
      case Playback::Booting:   return "BOOTING";
      case Playback::Idle:      return "IDLE";
      case Playback::Loaded:    return "LOADED";
      case Playback::Running:   return "RUNNING";
      case Playback::Paused:    return "PAUSED";
      case Playback::Emergency: return "EMERGENCY";
      case Playback::Finished:  return "FINISHED";
      case Playback::Error:     return "ERROR";
      default:                  return "IDLE";
    }
  }

  uint32_t elapsedMs() const { return elapsedMs_; }
  uint32_t remainingMs() const { return remainingMs_; }
  uint32_t totalDurationMs() const { return totalDurationMs_; }
  uint32_t currentCue() const { return currentCue_; }
  uint32_t totalCues() const { return totalCues_; }
  uint32_t nextCue() const {
    if (totalCues_ == 0) return 0;
    if (currentCue_ >= totalCues_) return totalCues_;
    return currentCue_ + 1;
  }

  uint8_t progressPercent() const {
    if (totalDurationMs_ > 0) {
      if (elapsedMs_ >= totalDurationMs_) return 100;
      return (uint8_t)((elapsedMs_ * 100UL) / totalDurationMs_);
    }
    if (finished_) return 100;
    if (totalCues_ > 0 && currentCue_ > 0) {
      return (uint8_t)((currentCue_ * 100UL) / totalCues_);
    }
    return 0;
  }

  bool running() const { return running_; }
  bool paused() const { return paused_; }
  bool emergency() const { return emergency_; }
  bool finished() const { return finished_; }

  StatusLevel status() const {
    if (emergency_) return StatusLevel::Critical;
    if (playback_ == Playback::Error) return StatusLevel::Critical;
    if (running_) return StatusLevel::Working;
    if (paused_) return StatusLevel::Warning;
    if (loaded_) return StatusLevel::Healthy;
    return StatusLevel::Inactive;
  }

  void formatCue(char *buf, size_t n) const {
    if (!buf || n == 0) return;
    if (totalCues_ == 0) { snprintf(buf, n, "—"); return; }
    snprintf(buf, n, "%lu / %lu", (unsigned long)currentCue_, (unsigned long)totalCues_);
  }

  void formatProgress(char *buf, size_t n) const {
    if (!buf || n == 0) return;
    snprintf(buf, n, "%u%%", (unsigned)progressPercent());
  }

  /* Intent (load/run/stop) lives in CommandService — this service is truth only. */

  uint32_t revision() const { return revision_; }

 private:
  void bump() { ++revision_; }

  char title_[64]{};
  Playback playback_ = Playback::Idle;
  uint32_t elapsedMs_ = 0;
  uint32_t remainingMs_ = 0;
  uint32_t totalDurationMs_ = 0;
  uint32_t currentCue_ = 0;
  uint32_t totalCues_ = 0;
  bool loaded_ = false;
  bool running_ = false;
  bool paused_ = false;
  bool emergency_ = false;
  bool finished_ = false;
  uint32_t revision_ = 0;
};

}  // namespace Os2