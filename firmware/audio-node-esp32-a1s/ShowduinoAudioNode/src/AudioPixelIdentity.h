#ifndef SHOWDUINO_PIXEL_IDENTITY_H
#define SHOWDUINO_PIXEL_IDENTITY_H

#include <Arduino.h>
#include "../../../protocol/showduino_pixel_node.h"

void audioPixelIdentityBegin(const char *macFallback);
const char *audioPixelIdentityId();
const char *audioPixelIdentityName();
bool audioPixelIdentitySetId(const char *id);
bool audioPixelIdentitySetName(const char *name);

#endif
