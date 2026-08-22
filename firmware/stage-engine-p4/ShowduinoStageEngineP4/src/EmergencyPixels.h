#ifndef SHOWDUINO_EMERGENCY_PIXELS_H
#define SHOWDUINO_EMERGENCY_PIXELS_H

#include <Arduino.h>
#include "../BoardConfig.h"

#if SHOWDUINO_EMERGENCY_PIXEL_ENABLED

bool emergencyPixelsBegin();
void emergencyPixelsSetWhite();
void emergencyPixelsBlackout();
void emergencyPixelsService();
bool emergencyPixelsReady();

#else

inline bool emergencyPixelsBegin() { return false; }
inline void emergencyPixelsSetWhite() {}
inline void emergencyPixelsBlackout() {}
inline void emergencyPixelsService() {}
inline bool emergencyPixelsReady() { return false; }

#endif

#endif
