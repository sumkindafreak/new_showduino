#ifndef SHOWDUINO_PIXEL_IDENTITY_H
#define SHOWDUINO_PIXEL_IDENTITY_H

#include <Arduino.h>
#include "../../../protocol/showduino_pixel_node.h"

void pixelIdentityBegin(const char *macFallback);
const char *pixelIdentityId();
const char *pixelIdentityName();
bool pixelIdentitySetId(const char *id);
bool pixelIdentitySetName(const char *name);

#endif
