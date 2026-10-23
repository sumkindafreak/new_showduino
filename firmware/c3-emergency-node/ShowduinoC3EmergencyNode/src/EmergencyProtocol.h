#ifndef SHOWDUINO_EMERGENCY_PROTOCOL_H
#define SHOWDUINO_EMERGENCY_PROTOCOL_H

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"

extern ShowduinoEmergencyMachine gEmergencyMachine;

void emergencyProtocolBegin();
/* MISSION 001J: where a command came from decides what it may do.
 *   Radio    : from the S3 relay (untrusted). CHAL/CMD go to EmergencyAuth; plain
 *              ACK / GLOBAL:OBSERVED / EMERGENCY:CLEAR / ID / NAME / REARM refused.
 *   LocalUsb : this board's USB console (physical access).
 *   Web      : this board's SoftAP page. PIXEL commands only. */
enum class EmergencyCmdSource : uint8_t { Radio = 0, LocalUsb, Web };
void emergencyProtocolApply(const char *command, uint32_t sequence, EmergencyCmdSource source);
/* Authenticated GCL accepted (or local USB maintenance): observe the P4 clear. */
void emergencyProtocolOnGlobalClear();
void emergencyProtocolService();
void emergencyProtocolAnnounce();
void emergencyProtocolTryLocalRearm();
ShowduinoEmergencyNodeState emergencyProtocolState();
/* Authoritative P4 system Emergency state; independent of the local assert latch. */
bool emergencyProtocolSystemEmergencyActive();

#endif
