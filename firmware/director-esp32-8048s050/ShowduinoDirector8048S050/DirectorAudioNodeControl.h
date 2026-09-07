#ifndef SHOWDUINO_DIRECTOR_AUDIO_NODE_CONTROL_H
#define SHOWDUINO_DIRECTOR_AUDIO_NODE_CONTROL_H

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../../protocol/showduino_state_wire.h"

#define DIRECTOR_AUDIO_PENDING_MS 5000u
#define DIRECTOR_AUDIO_VOL_THROTTLE_MS 400u

enum DirectorAudioPending : uint8_t {
  DIRECTOR_AUDIO_PEND_NONE = 0,
  DIRECTOR_AUDIO_PEND_PLAY,
  DIRECTOR_AUDIO_PEND_LOOP,
  DIRECTOR_AUDIO_PEND_PAUSE,
  DIRECTOR_AUDIO_PEND_RESUME,
  DIRECTOR_AUDIO_PEND_STOP,
  DIRECTOR_AUDIO_PEND_VOLUME,
  DIRECTOR_AUDIO_PEND_TEST
};

struct DirectorAudioNodeControl {
  bool seen;
  bool online;
  bool emergency;
  bool showRunning;
  char state[12];
  char asset[SHOWDUINO_AUDIO_DETAIL_ASSET_MAX + 1];
  char storage[12];
  char codec[16];
  char codecHealth[8];
  char fault[8];
  char firmware[12];
  char mac[18];
  char caps[48];
  char selectedAsset[SHOWDUINO_AUDIO_DETAIL_ASSET_MAX + 1];
  char inventory[SHOWDUINO_AUDIO_INV_WIRE_MAX][21];
  char feedback[48];
  char lastErrorText[40];
  uint16_t inventoryTotal;
  uint16_t inventoryPage;
  uint8_t volume;
  uint8_t pendingVolume;
  uint32_t pendingSinceMs;
  uint32_t pendingSeq;
  uint32_t lastVolSendMs;
  DirectorAudioPending pending;
  bool soundReady;
  bool soundArmed;
  bool soundCalibrated;
  bool soundEnabled;
  uint8_t soundLevel;
  uint8_t soundPeak;
  uint8_t soundFloor;
  uint8_t soundThreshold;
  uint16_t soundCooldown;
  char soundReadyTok[6];
  char soundLastEvent[8];
};

static inline void director_audio_node_clear(DirectorAudioNodeControl *m) {
  if (!m) return;
  memset(m, 0, sizeof(*m));
  strncpy(m->state, "OFFLINE", sizeof(m->state) - 1);
  strncpy(m->storage, "UNKNOWN", sizeof(m->storage) - 1);
  strncpy(m->codec, "ES8388", sizeof(m->codec) - 1);
  strncpy(m->codecHealth, "-", sizeof(m->codecHealth) - 1);
  strncpy(m->fault, "-", sizeof(m->fault) - 1);
  strncpy(m->selectedAsset, "system-test.wav", sizeof(m->selectedAsset) - 1);
  strncpy(m->soundReadyTok, "OFF", sizeof(m->soundReadyTok) - 1);
  strncpy(m->soundLastEvent, "NONE", sizeof(m->soundLastEvent) - 1);
  m->volume = 80;
  m->pendingVolume = 80;
  m->soundThreshold = 70;
}

static inline const char *director_audio_fault_text(const char *code) {
  if (!code || !code[0] || !strcmp(code, "-") || !strcmp(code, "NONE")) return "";
  if (!strcmp(code, "FILE")) return "File not found";
  if (!strcmp(code, "PATH")) return "Invalid path";
  if (!strcmp(code, "CODEC")) return "Codec fault";
  if (!strcmp(code, "STOR")) return "No storage";
  if (!strcmp(code, "TO")) return "Comms timeout";
  if (!strcmp(code, "UNSUP")) return "Not supported";
  if (!strcmp(code, "CFG")) return "Config fault";
  if (!strcmp(code, "CMD")) return "Command rejected";
  if (!strcmp(code, "FLT")) return "Audio Node fault";
  return "Command failed";
}

static inline const char *director_audio_state_text(const char *st) {
  if (!st || !st[0]) return "OFFLINE";
  if (!strcmp(st, "PLAY")) return "PLAYING";
  if (!strcmp(st, "LOOP")) return "LOOPING";
  if (!strcmp(st, "PAUS")) return "PAUSED";
  if (!strcmp(st, "FLT")) return "FAULT";
  if (!strcmp(st, "ESTP")) return "EMERGENCY";
  if (!strcmp(st, "OFF")) return "OFFLINE";
  if (!strcmp(st, "NSD")) return "NO STORAGE";
  if (!strcmp(st, "LOAD")) return "LOADING";
  if (!strcmp(st, "STOP")) return "STOPPING";
  if (!strcmp(st, "IDLE")) return "IDLE";
  return st;
}

static inline const char *director_audio_storage_text(const char *st) {
  if (!st) return "UNKNOWN";
  if (!strcmp(st, "ON")) return "ONLINE";
  if (!strcmp(st, "OFF")) return "NO SD";
  if (!strcmp(st, "FLT")) return "FAULT";
  return st;
}

static inline const char *director_audio_pending_text(DirectorAudioPending p) {
  switch (p) {
    case DIRECTOR_AUDIO_PEND_PLAY: return "PLAY PENDING";
    case DIRECTOR_AUDIO_PEND_LOOP: return "LOOP PENDING";
    case DIRECTOR_AUDIO_PEND_PAUSE: return "PAUSE PENDING";
    case DIRECTOR_AUDIO_PEND_RESUME: return "RESUME PENDING";
    case DIRECTOR_AUDIO_PEND_STOP: return "STOP PENDING";
    case DIRECTOR_AUDIO_PEND_VOLUME: return "VOLUME PENDING";
    case DIRECTOR_AUDIO_PEND_TEST: return "TEST PENDING";
    default: return "";
  }
}

static inline bool director_audio_has_cap(const DirectorAudioNodeControl *m, const char *cap) {
  if (!m || !cap) return false;
  if (!m->caps[0]) {
    return strcmp(cap, "MP3") != 0 && strcmp(cap, "MIC") != 0 && strcmp(cap, "RECORD") != 0;
  }
  return strstr(m->caps, cap) != nullptr;
}

static inline bool director_audio_playing(const DirectorAudioNodeControl *m) {
  return m && (!strcmp(m->state, "PLAY") || !strcmp(m->state, "LOOP") || !strcmp(m->state, "LOAD"));
}

static inline bool director_audio_storage_ok(const DirectorAudioNodeControl *m) {
  return m && m->online && strcmp(m->state, "NSD") != 0 && strcmp(m->storage, "OFF") != 0 &&
         strcmp(m->storage, "FLT") != 0;
}

static inline void director_audio_apply_detail(DirectorAudioNodeControl *m,
                                               const ShowduinoAudioDetailWire *d) {
  if (!m || !d) return;
  strncpy(m->state, d->state[0] ? d->state : "OFF", sizeof(m->state) - 1);
  m->volume = d->volume;
  if (m->pending != DIRECTOR_AUDIO_PEND_VOLUME) m->pendingVolume = d->volume;
  strncpy(m->storage, d->storage, sizeof(m->storage) - 1);
  strncpy(m->codecHealth, d->codec, sizeof(m->codecHealth) - 1);
  strncpy(m->fault, d->fault, sizeof(m->fault) - 1);
  if (d->asset[0] && strcmp(d->asset, "-") != 0) {
    strncpy(m->asset, d->asset, sizeof(m->asset) - 1);
  } else {
    m->asset[0] = '\0';
  }
  const char *err = director_audio_fault_text(d->fault);
  strncpy(m->lastErrorText, err, sizeof(m->lastErrorText) - 1);
  if (!strcmp(m->state, "OFF")) {
    m->online = false;
  } else {
    m->online = true;
    m->seen = true;
  }
  m->emergency = !strcmp(m->state, "ESTP");
}

static inline void director_audio_apply_coarse(DirectorAudioNodeControl *m,
                                               ShowduinoAudioNodeWire wire) {
  if (!m) return;
  if (wire == SHOWDUINO_AUDIO_NODE_WIRE_INVALID) return;
  if (wire == SHOWDUINO_AUDIO_NODE_WIRE_OFFLINE) {
    m->online = false;
    strncpy(m->state, "OFF", sizeof(m->state) - 1);
    m->asset[0] = '\0';
    m->pending = DIRECTOR_AUDIO_PEND_NONE;
    m->feedback[0] = '\0';
    return;
  }
  m->seen = true;
  m->online = true;
  m->emergency = (wire == SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY);
  if (wire == SHOWDUINO_AUDIO_NODE_WIRE_PLAYING) strncpy(m->state, "PLAY", sizeof(m->state) - 1);
  else if (wire == SHOWDUINO_AUDIO_NODE_WIRE_LOOPING) strncpy(m->state, "LOOP", sizeof(m->state) - 1);
  else if (wire == SHOWDUINO_AUDIO_NODE_WIRE_PAUSED) strncpy(m->state, "PAUS", sizeof(m->state) - 1);
  else if (wire == SHOWDUINO_AUDIO_NODE_WIRE_FAULT) strncpy(m->state, "FLT", sizeof(m->state) - 1);
  else if (wire == SHOWDUINO_AUDIO_NODE_WIRE_EMERGENCY) strncpy(m->state, "ESTP", sizeof(m->state) - 1);
  else strncpy(m->state, "IDLE", sizeof(m->state) - 1);
}

static inline void director_audio_apply_sound(DirectorAudioNodeControl *m,
                                              const ShowduinoAudioSoundWire *s) {
  if (!m || !s) return;
  strncpy(m->soundReadyTok, s->ready[0] ? s->ready : "OFF", sizeof(m->soundReadyTok) - 1);
  m->soundLevel = s->level;
  m->soundPeak = s->peak;
  m->soundFloor = s->floor;
  m->soundThreshold = s->threshold;
  m->soundArmed = s->armed != 0;
  m->soundCooldown = s->cooldown;
  m->soundCalibrated = s->calibrated != 0;
  m->soundReady = strcmp(m->soundReadyTok, "RDY") == 0 || strcmp(m->soundReadyTok, "CAL") == 0;
  m->soundEnabled = strcmp(m->soundReadyTok, "OFF") != 0;
  strncpy(m->soundLastEvent, s->event[0] ? s->event : "NONE", sizeof(m->soundLastEvent) - 1);
}

#endif
