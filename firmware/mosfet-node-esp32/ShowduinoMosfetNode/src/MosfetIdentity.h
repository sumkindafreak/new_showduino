#ifndef SHOWDUINO_MOSFET_IDENTITY_H
#define SHOWDUINO_MOSFET_IDENTITY_H

#include "../../../protocol/showduino_mosfet_node.h"

void mosfetIdentityBegin(const char *macFallback);
const char *mosfetIdentityId();
const char *mosfetIdentityName();
const char *mosfetIdentityOutName(uint8_t ch1to4);
bool mosfetIdentitySetId(const char *id);
bool mosfetIdentitySetName(const char *name);
bool mosfetIdentitySetOutName(uint8_t ch1to4, const char *name);

#endif
