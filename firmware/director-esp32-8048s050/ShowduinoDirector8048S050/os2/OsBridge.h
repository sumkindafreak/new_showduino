#pragma once

/**
 * OsBridge — communication layer → services.
 *
 * The only place Director firmware may push Stage/link/catalogue truth into OS 2.0.
 * Apps never call this. Shell never calls this.
 */

#include "services/StubServices.h"
#include "models/Production.h"
#include "BoardConfig.h"

namespace Os2 {

inline ShowService::Playback playbackFromShowState(uint8_t state) {
  switch (state) {
    case 0: return ShowService::Playback::Booting;
    case 1: return ShowService::Playback::Idle;
    case 2: return ShowService::Playback::Loaded;
    case 3: return ShowService::Playback::Running;
    case 4: return ShowService::Playback::Paused;
    case 5: return ShowService::Playback::Emergency;
    case 6: return ShowService::Playback::Finished;
    case 7: return ShowService::Playback::Error;
    default: return ShowService::Playback::Error;
  }
}

inline NetworkService::Link linkFromDirector(uint8_t linkState) {
  switch (linkState) {
    case LINK_READY:        return NetworkService::Link::Ready;
    case LINK_DISCONNECTED: return NetworkService::Link::Disconnected;
    case LINK_SEARCHING:
    default:                return NetworkService::Link::Searching;
  }
}

inline void publishShow(const char *showName,
                        uint8_t showState,
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
  showService().apply(showName,
                      playbackFromShowState(showState),
                      elapsedMs, remainingMs, totalDurationMs,
                      currentCue, totalCues,
                      loaded, running, paused, emergency, finished);
}

inline void publishLink(uint8_t linkState,
                        uint8_t nodesOnline,
                        uint8_t nodesExpected,
                        uint32_t lastHeartbeatMs,
                        uint32_t nowMs,
                        bool espNowReady,
                        uint32_t txCount,
                        uint32_t rxCount) {
  NetworkService &net = networkService();
  net.applyLink(linkFromDirector(linkState));
  net.applyNodes(nodesOnline, nodesExpected);
  net.applyHeartbeat(lastHeartbeatMs, nowMs);
  net.applyTransport(espNowReady);
  net.applyTraffic(txCount, rxCount);

  DeviceService &dev = deviceService();
  StatusLevel stageStatus = StatusLevel::Inactive;
  if (linkState == LINK_READY) stageStatus = StatusLevel::Healthy;
  else if (linkState == LINK_SEARCHING) stageStatus = StatusLevel::Working;
  else stageStatus = StatusLevel::Critical;
  const bool stageUp = (linkState == LINK_READY);
  dev.setStage(stageUp, stageStatus);
  dev.setSue(stageUp, stageStatus);
  if (nodesExpected > 0) {
    for (uint8_t i = 1; i <= nodesExpected && i <= 8; ++i) {
      bool present = (i <= nodesOnline) && stageUp;
      StatusLevel st = present ? StatusLevel::Healthy
                               : (stageUp ? StatusLevel::Warning : StatusLevel::Inactive);
      dev.setNode(i, present, st);
    }
  }
}

inline void publishSafety(bool emergencyLocked) {
  (void)emergencyLocked;
}

/** Push production catalogue — no paths exposed to apps. */
inline void publishCatalogue(const ProductionManifest *items, int count) {
  assetService().setCatalogue(items, count);
}

}  // namespace Os2