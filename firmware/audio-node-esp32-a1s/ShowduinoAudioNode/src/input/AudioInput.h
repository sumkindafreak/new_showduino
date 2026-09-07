#ifndef SHOWDUINO_AUDIO_INPUT_H
#define SHOWDUINO_AUDIO_INPUT_H

#include <Arduino.h>
#include "../../../protocol/showduino_sound_input.h"

bool audioInputBegin();
void audioInputService();
void audioInputOnPlaybackStart();
void audioInputOnPlaybackStop();
void audioInputSetEmergency(bool on);
bool audioInputReady();
bool audioInputFault();
uint8_t audioInputLevel();
uint8_t audioInputPeak();
uint8_t audioInputNoiseFloor();
const char *audioInputLastError();
void audioInputFormatStatus(char *out, size_t n);
int audioInputTakeEventLine(char *out, size_t n);
void audioInputCalibrate();
void audioInputEnable(bool on);
void audioInputSetThreshold(uint8_t threshold);
void audioInputTriggerTest();
void audioInputSetMonitor(bool on);
bool audioInputMonitor();
void audioInputPrintStatus();
bool audioInputStartRecordTest();
bool audioInputRecording();
void audioInputSetLocalTest(bool on);

#endif
