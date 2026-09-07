#ifndef SHOWDUINO_AUDIO_NODE_STATE_H
#define SHOWDUINO_AUDIO_NODE_STATE_H

#include <Arduino.h>
#include "../../../protocol/showduino_audio_node.h"

void audioNodeStateBegin(ShowduinoAudioNodeState initial);
void audioNodeStateSet(ShowduinoAudioNodeState st);
ShowduinoAudioNodeState audioNodeState();
void audioNodeStateSetFault(ShowduinoAudioFail fault);
ShowduinoAudioFail audioNodeStateFault();
void audioNodeStateClearFault();
void audioNodeStateSetShowControlled(bool on);
bool audioNodeStateShowControlled();
void audioNodeStateNoteComms();
uint32_t audioNodeStateLastCommsMs();
bool audioNodeStateAuthorityFresh(uint32_t timeoutMs);
const char *audioNodeStateName();

#endif
