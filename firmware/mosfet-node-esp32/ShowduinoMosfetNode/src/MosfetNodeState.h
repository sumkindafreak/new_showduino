#ifndef SHOWDUINO_MOSFET_NODE_STATE_H
#define SHOWDUINO_MOSFET_NODE_STATE_H

#include "../../../protocol/showduino_mosfet_node.h"
#include "../../../protocol/showduino_node_ownership.h"

void mosfetNodeStateBegin();
void mosfetNodeStateLoop();
ShowduinoMosfetNodeState mosfetNodeStateGet();
ShowduinoNodeOwnerMode mosfetNodeOwnerMode();
void mosfetNodeStateOnGrant(uint32_t nowMs);
void mosfetNodeStateOnEmergencyStop();
void mosfetNodeStateOnEmergencyClear();
void mosfetNodeStateSetFault(const char *reason);
bool mosfetNodeStateOwned();
bool mosfetNodeStateEmergency();
bool mosfetNodeStateEnteredShow(); /* true once on first grant transition */

#endif
