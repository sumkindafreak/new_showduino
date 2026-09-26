#ifndef SHOWDUINO_MOSFET_PROTOCOL_H
#define SHOWDUINO_MOSFET_PROTOCOL_H

#include "../../../protocol/showduino_node_ownership.h"

void mosfetProtocolBegin();
void mosfetProtocolLoop();
void mosfetProtocolAnnounce();
bool mosfetProtocolApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin);
void mosfetProtocolLocalTest();

#endif
