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
bool emergencyPixelsWhiteActive();
/* Diagnostic RGB write. Does not latch emergency or change white-refresh state. */
bool emergencyPixelsWriteRgb(uint8_t r, uint8_t g, uint8_t b);

#else

inline bool emergencyPixelsBegin() { return false; }
inline void emergencyPixelsSetWhite() {}
inline void emergencyPixelsBlackout() {}
inline void emergencyPixelsService() {}
inline bool emergencyPixelsReady() { return false; }
inline bool emergencyPixelsWhiteActive() { return false; }
inline bool emergencyPixelsWriteRgb(uint8_t, uint8_t, uint8_t) { return false; }

#endif

#endif
