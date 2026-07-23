#ifndef SHOWDUINO_DIRECTOR_AUDIO_MODEL_H
#define SHOWDUINO_DIRECTOR_AUDIO_MODEL_H

#include <Arduino.h>
#include <string.h>

/* Director presentation model — IAN/P4 owns local audio; remote nodes own SD/I2S. */
#ifndef SHOWDUINO_AUDIO_NODE_MAX
#define SHOWDUINO_AUDIO_NODE_MAX 8
#endif

enum class DeskAudioPlayState : uint8_t {
  Unknown = 0, Offline, Ready, Playing, Paused, Muted, Error
};
enum class DeskAudioSdState : uint8_t { Unknown = 0, Ready, Missing, Error };
enum class DeskAudioI2sState : uint8_t { Unknown = 0, Ready, Error };
enum class DeskAudioSyncState : uint8_t { Unknown = 0, Locked, Drift, Lost };
enum class DeskAudioCmdPhase : uint8_t {
  None = 0, Queued, Sent, Acked, Started, Completed, Failed, TimedOut
};

struct DeskLocalAudio {
  bool available = false;
  DeskAudioPlayState play = DeskAudioPlayState::Unknown;
  DeskAudioSdState sd = DeskAudioSdState::Unknown;
  DeskAudioI2sState i2s = DeskAudioI2sState::Unknown;
  bool muted = false;
  bool loop = false;
  uint8_t volume = 0;
  char assetName[48];
  char outputName[32];
  uint32_t elapsedMs = 0;
  uint32_t remainMs = 0;
  char lastError[48];
  DeskLocalAudio() {
    memset(assetName, 0, sizeof(assetName));
    memset(outputName, 0, sizeof(outputName));
    memset(lastError, 0, sizeof(lastError));
    strncpy(outputName, "IAN / P4 AUDIO 1", sizeof(outputName) - 1);
    strncpy(assetName, "NOT AVAILABLE", sizeof(assetName) - 1);
  }
};

struct DeskRemoteAudioNode {
  bool present = false;
  char nodeId[24];
  char name[32];
  bool online = false;
  DeskAudioSdState sd = DeskAudioSdState::Unknown;
  DeskAudioI2sState i2s = DeskAudioI2sState::Unknown;
  DeskAudioPlayState play = DeskAudioPlayState::Unknown;
  DeskAudioSyncState sync = DeskAudioSyncState::Unknown;
  bool muted = false;
  uint8_t volume = 0;
  char assetName[48];
  char zoneName[32];
  uint32_t lastSeenMs = 0;
  DeskAudioCmdPhase lastCmd = DeskAudioCmdPhase::None;
  char lastCmdId[20];
  DeskRemoteAudioNode() {
    memset(nodeId, 0, sizeof(nodeId));
    memset(name, 0, sizeof(name));
    memset(assetName, 0, sizeof(assetName));
    memset(zoneName, 0, sizeof(zoneName));
    memset(lastCmdId, 0, sizeof(lastCmdId));
    strncpy(assetName, "NOT AVAILABLE", sizeof(assetName) - 1);
  }
};

struct DeskAudioRouteRow {
  bool used = false;
  char zone[28];
  char target[40];
  DeskAudioRouteRow() { memset(zone, 0, sizeof(zone)); memset(target, 0, sizeof(target)); }
};

struct DeskAudioCommandStatus {
  bool used = false;
  char commandId[20];
  char summary[48];
  DeskAudioCmdPhase phase = DeskAudioCmdPhase::None;
  DeskAudioCommandStatus() {
    memset(commandId, 0, sizeof(commandId));
    memset(summary, 0, sizeof(summary));
  }
};

struct DeskAudioModel {
  DeskLocalAudio local;
  DeskRemoteAudioNode nodes[SHOWDUINO_AUDIO_NODE_MAX];
  uint8_t nodeCount = 0;
  DeskAudioRouteRow routes[8];
  DeskAudioCommandStatus recentCmds[6];

  void resetPlaceholders() {
    local = DeskLocalAudio();
    nodeCount = 0;
    for (uint8_t i = 0; i < SHOWDUINO_AUDIO_NODE_MAX; i++) nodes[i] = DeskRemoteAudioNode();
    for (uint8_t i = 0; i < 8; i++) routes[i] = DeskAudioRouteRow();
    for (uint8_t i = 0; i < 6; i++) recentCmds[i] = DeskAudioCommandStatus();

    routes[0].used = true;
    strncpy(routes[0].zone, "MAIN PA", sizeof(routes[0].zone) - 1);
    strncpy(routes[0].target, "P4 LOCAL OUTPUT", sizeof(routes[0].target) - 1);
    routes[1].used = true;
    strncpy(routes[1].zone, "FOYER", sizeof(routes[1].zone) - 1);
    strncpy(routes[1].target, "AUDIO NODE (unassigned)", sizeof(routes[1].target) - 1);
    routes[2].used = true;
    strncpy(routes[2].zone, "CORRIDOR", sizeof(routes[2].zone) - 1);
    strncpy(routes[2].target, "AUDIO NODE (unassigned)", sizeof(routes[2].target) - 1);
    routes[3].used = true;
    strncpy(routes[3].zone, "CREATURE FX", sizeof(routes[3].zone) - 1);
    strncpy(routes[3].target, "AUDIO NODE (unassigned)", sizeof(routes[3].target) - 1);
  }

  uint8_t onlineNodeCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < nodeCount && i < SHOWDUINO_AUDIO_NODE_MAX; i++)
      if (nodes[i].present && nodes[i].online) n++;
    return n;
  }
  uint8_t playingNodeCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < nodeCount && i < SHOWDUINO_AUDIO_NODE_MAX; i++)
      if (nodes[i].present && nodes[i].play == DeskAudioPlayState::Playing) n++;
    return n;
  }

  static const char *playWord(DeskAudioPlayState s) {
    switch (s) {
      case DeskAudioPlayState::Ready: return "READY";
      case DeskAudioPlayState::Playing: return "PLAYING";
      case DeskAudioPlayState::Paused: return "PAUSED";
      case DeskAudioPlayState::Muted: return "MUTED";
      case DeskAudioPlayState::Error: return "ERROR";
      case DeskAudioPlayState::Offline: return "OFFLINE";
      default: return "UNKNOWN";
    }
  }
  static const char *sdWord(DeskAudioSdState s) {
    switch (s) {
      case DeskAudioSdState::Ready: return "READY";
      case DeskAudioSdState::Missing: return "MISSING";
      case DeskAudioSdState::Error: return "ERROR";
      default: return "UNKNOWN";
    }
  }
  static const char *i2sWord(DeskAudioI2sState s) {
    switch (s) {
      case DeskAudioI2sState::Ready: return "READY";
      case DeskAudioI2sState::Error: return "ERROR";
      default: return "UNKNOWN";
    }
  }
  static const char *syncWord(DeskAudioSyncState s) {
    switch (s) {
      case DeskAudioSyncState::Locked: return "LOCKED";
      case DeskAudioSyncState::Drift: return "DRIFT";
      case DeskAudioSyncState::Lost: return "LOST";
      default: return "UNKNOWN";
    }
  }
  static const char *cmdWord(DeskAudioCmdPhase s) {
    switch (s) {
      case DeskAudioCmdPhase::Queued: return "QUEUED";
      case DeskAudioCmdPhase::Sent: return "SENT";
      case DeskAudioCmdPhase::Acked: return "ACKED";
      case DeskAudioCmdPhase::Started: return "STARTED";
      case DeskAudioCmdPhase::Completed: return "COMPLETED";
      case DeskAudioCmdPhase::Failed: return "FAILED";
      case DeskAudioCmdPhase::TimedOut: return "TIMED OUT";
      default: return "NONE";
    }
  }
};

#endif