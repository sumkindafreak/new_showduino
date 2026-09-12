#ifndef SHOWDUINO_S3_LAMP_NODE_STATE_H
#define SHOWDUINO_S3_LAMP_NODE_STATE_H

#include "../../../protocol/showduino_lamp_node.h"

void lampNodeStateBegin(ShowduinoLampNodeState st);
void lampNodeStateSet(ShowduinoLampNodeState st);
ShowduinoLampNodeState lampNodeState();
void lampNodeStateSetFault(const char *reason);
const char *lampNodeStateFault();
void lampNodeStateClearFault();
void lampNodeStateSetShowControlled(bool v);
bool lampNodeStateShowControlled();
void lampNodeStateSetLastResult(const char *result);
const char *lampNodeStateLastResult();
void lampNodeStateSetLastCommand(const char *cmd);
const char *lampNodeStateLastCommand();
void lampNodeStateNoteComms();
uint32_t lampNodeStateLastCommsMs();
bool lampNodeStateAuthorityFresh(uint32_t timeoutMs);
const char *lampNodeStateName();
ShowduinoLampProductMode lampNodeProductMode();
const char *lampNodeProductModeName();

void lampOwnerTick();
void lampOwnerApplyEvent(ShowduinoOwnerEvent ev);
ShowduinoNodeOwnerMode lampOwnerMode();
const char *lampOwnerModeName();
bool lampOwnerGranted();
bool lampOwnerLostAuthority();
bool lampOwnerEnteredStandalone();
bool lampOwnerEnteredShow();
uint32_t lampOwnerLastGrantMs();

#endif
