#ifndef SHOWDUINO_AUDIO_INPUT_CONFIG_H
#define SHOWDUINO_AUDIO_INPUT_CONFIG_H

#include "../../../protocol/showduino_sound_input.h"

#define PATH_AUDIO_CAL        "/showduino/config/audio-input-cal.json"
#define PATH_AUDIO_RECORDINGS "/showduino/recordings"

void audioInputConfigBegin();
const ShowduinoSoundInputConfig &audioInputConfig();
void audioInputConfigSet(const ShowduinoSoundInputConfig *cfg, bool persist);
void audioInputConfigSetEnabled(bool on, bool persist);
void audioInputConfigSetThreshold(uint8_t threshold, bool persist);
void audioInputConfigSetCooldown(uint16_t ms, bool persist);
void audioInputConfigSetInhibit(uint16_t ms, bool persist);
void audioInputConfigSetMode(uint8_t mode, bool persist);
ShowduinoAudioCfgStatus audioInputConfigApplyJson(const char *json);
void audioInputConfigFormatJson(char *out, size_t n);
bool audioInputConfigLoadCal(uint8_t *noiseFloor);
bool audioInputConfigSaveCal(uint8_t noiseFloor);
bool audioInputConfigFault();

#endif
