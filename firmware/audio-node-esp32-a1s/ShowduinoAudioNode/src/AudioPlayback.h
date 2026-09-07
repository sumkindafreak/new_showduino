#ifndef SHOWDUINO_AUDIO_PLAYBACK_H
#define SHOWDUINO_AUDIO_PLAYBACK_H

#include <Arduino.h>

enum class AudioPlayMode : uint8_t { Once = 0, Loop };

bool audioPlaybackBegin();
bool audioPlaybackStart(const char *absPath, AudioPlayMode mode);
void audioPlaybackStop();
bool audioPlaybackPause();
bool audioPlaybackResume();
void audioPlaybackService();
bool audioPlaybackActive();
bool audioPlaybackPaused();
bool audioPlaybackLooping();
bool audioPlaybackJustCompleted();
bool audioPlaybackJustFailed();
const char *audioPlaybackPath();
const char *audioPlaybackRel();
uint32_t audioPlaybackPosition();
uint32_t audioPlaybackDuration();
uint32_t audioPlaybackUnderruns();
uint32_t audioPlaybackBytesRead();
uint32_t audioPlaybackFilesPlayed();
uint32_t audioPlaybackCompleted();
uint32_t audioPlaybackFailed();
const char *audioPlaybackLastError();

#endif
