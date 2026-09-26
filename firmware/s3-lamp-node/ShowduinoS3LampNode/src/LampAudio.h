#ifndef SHOWDUINO_S3_LAMP_AUDIO_H
#define SHOWDUINO_S3_LAMP_AUDIO_H

#include "../../../protocol/showduino_carbide_lamp.h"

void lampAudioBegin();
void lampAudioService();
void lampAudioPlay(ShowduinoLampSound id);
void lampAudioStop();
bool lampAudioHardwarePresent();
bool lampAudioReady();
bool lampAudioPlaying();
const char *lampAudioStatus();
const char *lampAudioModuleName();
const char *lampAudioCurrentRole();
const char *lampAudioCurrentFile();
const char *lampAudioCurrentFat();
const char *lampAudioFileQueryStatus();
const char *lampAudioExpectedFiles();
void lampAudioSetVolume(uint8_t vol);
uint8_t lampAudioVolume();
const char *lampAudioVolumeNote();
const char *lampAudioLastError();
const char *lampAudioLastRx();
bool lampAudioHeardReply();

#endif
