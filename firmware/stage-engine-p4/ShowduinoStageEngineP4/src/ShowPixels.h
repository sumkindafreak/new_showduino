#ifndef SHOWDUINO_SHOW_PIXELS_H
#define SHOWDUINO_SHOW_PIXELS_H

#include <Arduino.h>
#include "../BoardConfig.h"
#include "../../../../protocol/showduino_pixel_fx.h"

#if SHOWDUINO_SHOW_PIXEL_ENABLED

bool showPixelsBegin();
void showPixelsService();
void showPixelsOnEmergency(bool active);
void showPixelsBlackout();
bool showPixelsReady();
bool showPixelsEmergencyOverride();
uint16_t showPixelsCount();
uint8_t showPixelsGlobalBrightness();
void showPixelsPrintStatus();
bool showPixelsHandleCommand(const char *command, char *reply, size_t replyLen);

#else

inline bool showPixelsBegin() { return false; }
inline void showPixelsService() {}
inline void showPixelsOnEmergency(bool) {}
inline void showPixelsBlackout() {}
inline bool showPixelsReady() { return false; }
inline bool showPixelsEmergencyOverride() { return false; }
inline uint16_t showPixelsCount() { return 0; }
inline uint8_t showPixelsGlobalBrightness() { return 0; }
inline void showPixelsPrintStatus() {}
inline bool showPixelsHandleCommand(const char *, char *, size_t) { return false; }

#endif

#endif /* SHOWDUINO_SHOW_PIXELS_H */
