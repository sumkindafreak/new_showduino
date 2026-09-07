#ifndef SHOWDUINO_STAGE_AUDIO_H
#define SHOWDUINO_STAGE_AUDIO_H

#include <Arduino.h>
#include "../BoardConfig.h"

/*
 * Showduino P4 Local System Audio.
 *
 * P4 onboard ES8311 + NS4150B speaker = system / safety audio ONLY.
 * Attraction / programme audio is exclusively the specialist Audio Node.
 * There is no Audio Node → P4 speaker fallback.
 *
 * SystemSound::Shutdown is reserved. Do not trigger it in this generation.
 */

enum class SystemSound : uint8_t {
  None = 0,
  Boot,
  Emergency,
  Beep,
  Tone,
  Error,
  Accepted,
  Complete,
  Shutdown
};

enum class SystemAudioPlayState : uint8_t {
  Idle = 0,
  Playing,
  Emergency,
  Failed
};

enum class SystemAudioHealth : uint8_t {
  Ready = 0,
  NoSd,
  CodecFault,
  I2sFault,
  FileError,
  Playing,
  Emergency
};

struct StageAudioStatus {
  bool codecDetected = false;
  bool codecReady = false;
  bool i2sReady = false;
  bool amplifierEnabled = false;
  bool storageReady = false;
  bool wavPresent = false;
  bool mp3Present = false;
  bool emergencyPlaying = false;
  bool playbackStarted = false;
  bool playbackActive = false;
  bool playbackFailed = false;
  uint8_t activeAssets = 0;
  bool shutdownAssetPresent = false;
  SystemSound current = SystemSound::None;
  SystemAudioPlayState playState = SystemAudioPlayState::Idle;
  SystemAudioHealth health = SystemAudioHealth::CodecFault;
  char currentName[16] = "NONE";
  char healthName[16] = "CODEC_FAULT";
  char wavPath[48] = "";
  char mp3Path[48] = "";
  char selectedPath[48] = "";
  char lastError[48] = "audio not started";
};

struct StageWavInfo {
  bool valid = false;
  bool pcm = false;
  bool engineSupported = false;
  bool dataNonZero = false;
  uint16_t bits = 0;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint32_t dataBytes = 0;
  char error[48] = "";
};

bool stageAudioBegin();
bool stageAudioInitHardware();
void stageAudioLoop();
void stageAudioRequestBoot();
/* Queue BOOT after a Director desk power-on HELLO. Not for USB HELLO. */
void stageAudioOnDirectorHello();
const StageAudioStatus &stageAudioStatus();

bool stageAudioPlay(SystemSound sound);
bool stageAudioPlayTest(SystemSound sound);
void stageAudioStopNotifications();
void stageAudioStop();

bool stageAudioStartEmergency();
void stageAudioStopEmergency();
bool stageAudioIsEmergencyPlaying();
bool stageAudioIsPlaying();

const char *systemSoundName(SystemSound sound);
const char *systemSoundPath(SystemSound sound);
const char *stageAudioResolvedPath(SystemSound sound);
void stageAudioRefreshAssets();
SystemSound systemSoundFromName(const char *name);
bool systemSoundIsReserved(SystemSound sound);
bool systemSoundIsActive(SystemSound sound);

bool stageAudioInspectWav(const char *path, StageWavInfo *out);
bool stageAudioI2sStarted();
uint32_t stageAudioI2sBytesWritten();
uint32_t stageAudioEmergencyLoopCount();
void stageAudioResetDiagCounters();
void stageAudioPrintStatus();
bool stageAudioHandleCommand(const char *command, char *reply, size_t replyLen);

/* Compatibility aliases — these are NOT attraction/show playback. */
inline void stageAudioStopShow() { stageAudioStopNotifications(); }
inline bool stageAudioIsShowPlaying() { return stageAudioIsPlaying() && !stageAudioIsEmergencyPlaying(); }

#endif
