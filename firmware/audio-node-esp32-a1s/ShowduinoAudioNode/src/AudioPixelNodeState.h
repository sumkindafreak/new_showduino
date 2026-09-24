#ifndef SHOWDUINO_PIXEL_NODE_STATE_H
#define SHOWDUINO_PIXEL_NODE_STATE_H

#include <Arduino.h>
#include "../../../protocol/showduino_pixel_node.h"

void audioPixelNodeStateBegin(ShowduinoPixelNodeState initial);
void audioPixelNodeStateSet(ShowduinoPixelNodeState st);
ShowduinoPixelNodeState audioPixelNodeState();
void audioPixelNodeStateSetFault(const char *reason);
const char *audioPixelNodeStateFault();
void audioPixelNodeStateClearFault();
void audioPixelNodeStateSetLastResult(const char *result);
const char *audioPixelNodeStateLastResult();
void audioPixelNodeStateSetLastCommand(const char *cmd);
const char *audioPixelNodeStateLastCommand();
void audioPixelNodeStateNoteComms();
uint32_t audioPixelNodeStateLastCommsMs();
const char *audioPixelNodeStateName();
ShowduinoPixelNodeState audioPixelNodeStateDisplay();

void audioPixelOwnerTick();
void audioPixelOwnerApplyEvent(ShowduinoOwnerEvent ev);
ShowduinoNodeOwnerMode audioPixelOwnerMode();
const char *audioPixelOwnerModeName();
bool audioPixelOwnerGranted();
bool audioPixelOwnerLostAuthority();
bool audioPixelOwnerEnteredStandalone();
bool audioPixelOwnerEnteredShow();
bool audioPixelNodeStateShowControlled();

#endif
