#ifndef SHOWDUINO_AUDIO_CODEC_H
#define SHOWDUINO_AUDIO_CODEC_H

#include <Arduino.h>

bool audioCodecBegin();
bool audioCodecReady();
bool audioCodecSetVolume(uint8_t percent);
void audioCodecMute(bool mute);
void audioCodecSetPa(bool on);
void audioCodecApplyOutput(const char *mode);
const char *audioCodecOutputName();
bool audioCodecHpInserted();
uint8_t audioCodecI2cAddress();
const char *audioCodecLastError();
bool audioCodecEnableInput();

#endif
