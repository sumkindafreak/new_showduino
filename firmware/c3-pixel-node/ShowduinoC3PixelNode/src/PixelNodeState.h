#ifndef SHOWDUINO_PIXEL_NODE_STATE_H
#define SHOWDUINO_PIXEL_NODE_STATE_H

#include <Arduino.h>
#include "../../../protocol/showduino_pixel_node.h"

void pixelNodeStateBegin(ShowduinoPixelNodeState initial);
void pixelNodeStateSet(ShowduinoPixelNodeState st);
ShowduinoPixelNodeState pixelNodeState();
void pixelNodeStateSetFault(const char *reason);
const char *pixelNodeStateFault();
void pixelNodeStateClearFault();
void pixelNodeStateSetLastResult(const char *result);
const char *pixelNodeStateLastResult();
void pixelNodeStateSetLastCommand(const char *cmd);
const char *pixelNodeStateLastCommand();
void pixelNodeStateNoteComms();
uint32_t pixelNodeStateLastCommsMs();
const char *pixelNodeStateName();
ShowduinoPixelNodeState pixelNodeStateDisplay();

void pixelOwnerTick();
void pixelOwnerApplyEvent(ShowduinoOwnerEvent ev);
ShowduinoNodeOwnerMode pixelOwnerMode();
const char *pixelOwnerModeName();
bool pixelOwnerGranted();
bool pixelOwnerLostAuthority();
bool pixelOwnerEnteredStandalone();
bool pixelOwnerEnteredShow();
bool pixelNodeStateShowControlled();

#endif
