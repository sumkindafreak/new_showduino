#ifndef SHOWDUINO_AUDIO_COMMAND_H
#define SHOWDUINO_AUDIO_COMMAND_H

#include <Arduino.h>
#include "../../../protocol/showduino_audio_node.h"

void audioCommandBegin(uint8_t volume);
void audioCommandApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin);
void audioCommandApply(const char *command, uint32_t sequence, bool fromShow);
void audioCommandLocalTestToggle();
void audioCommandLocalStop();
void audioCommandLocalPrev();
void audioCommandLocalNext();
void audioCommandNudgeVolume(int delta);
void audioCommandAnnounce();
void audioCommandService();
uint8_t audioCommandVolume();
bool audioCommandDucking();
uint32_t audioCommandLastStartLatencyMs();

#endif
