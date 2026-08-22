#ifndef SHOWDUINO_STAGE_AUDIO_H
#define SHOWDUINO_STAGE_AUDIO_H

#include <Arduino.h>
#include "../BoardConfig.h"

enum class StageAudioMode : uint8_t {
  Idle = 0,
  Show,
  Emergency
};

struct StageAudioStatus {
  bool i2sReady = false;
  bool wavPresent = false;
  bool mp3Present = false;
  bool emergencyPlaying = false;
  char wavPath[40] = "";
  char mp3Path[40] = "";
  char selectedPath[40] = "";
  char lastError[48] = "audio not started";
};

bool stageAudioBegin();
void stageAudioLoop();
const StageAudioStatus &stageAudioStatus();

/* Stop any non-emergency (show) playback. Safe if nothing is playing. */
void stageAudioStopShow();

/*
 * Start looping emergency audio from SD. Stops show audio first.
 * Returns false if the file cannot be opened or I2S is unavailable;
 * the caller must still keep emergency latched.
 */
bool stageAudioStartEmergency();

/* Stop emergency looping. Does not clear emergency state. */
void stageAudioStopEmergency();

bool stageAudioIsEmergencyPlaying();
bool stageAudioIsShowPlaying();

/*
 * Optional show playback (WAV from SD). Rejected by the caller while
 * emergency is active. Returns false on failure without affecting emergency.
 */
bool stageAudioStartShow(const char *path);

#endif
