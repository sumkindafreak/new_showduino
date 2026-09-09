#ifndef SHOWDUINO_LAMP_NODE_STATE_H
#define SHOWDUINO_LAMP_NODE_STATE_H

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

#endif
