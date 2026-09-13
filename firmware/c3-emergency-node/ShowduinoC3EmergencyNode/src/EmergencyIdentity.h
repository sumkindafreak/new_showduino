#ifndef SHOWDUINO_EMERGENCY_IDENTITY_H
#define SHOWDUINO_EMERGENCY_IDENTITY_H

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"

void emergencyIdentityBegin(const char *macFallback);
const char *emergencyIdentityId();
const char *emergencyIdentityName();
bool emergencyIdentitySetId(const char *id);
bool emergencyIdentitySetName(const char *name);

#endif
