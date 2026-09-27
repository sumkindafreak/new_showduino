#ifndef SHOWDUINO_EMERGENCY_PROTOCOL_H
#define SHOWDUINO_EMERGENCY_PROTOCOL_H

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"

extern ShowduinoEmergencyMachine gEmergencyMachine;

void emergencyProtocolBegin();
void emergencyProtocolApply(const char *command, uint32_t sequence);
void emergencyProtocolService();
void emergencyProtocolAnnounce();
void emergencyProtocolTryLocalRearm();
ShowduinoEmergencyNodeState emergencyProtocolState();
/* Authoritative P4 system Emergency state; independent of the local assert latch. */
bool emergencyProtocolSystemEmergencyActive();

#endif
