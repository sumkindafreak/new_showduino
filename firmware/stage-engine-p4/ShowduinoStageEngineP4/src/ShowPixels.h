#ifndef SHOWDUINO_SHOW_PIXELS_H
#define SHOWDUINO_SHOW_PIXELS_H

#include <Arduino.h>
#include <algorithm>
#include "../BoardConfig.h"
#include "../../../protocol/showduino_pixel_fx.h"

/* Arduino's legacy min/max macros do not expand for explicit template calls
 * such as min<uint32_t>(). Make the standard templates visible as well. */
using std::min;
using std::max;

#if SHOWDUINO_SHOW_PIXEL_ENABLED

bool showPixelsBegin();
void showPixelsApplyPersisted();
void showPixelsService();
void showPixelsOnEmergency(bool active);
void showPixelsBlackout();
bool showPixelsReady();
bool showPixelsEmergencyOverride();
uint16_t showPixelsCount();
uint16_t showPixelsConfiguredCount();
uint16_t showPixelsMax();
uint8_t showPixelsGlobalBrightness();
void showPixelsPrintStatus();
bool showPixelsHandleCommand(const char *command, char *reply, size_t replyLen);

#else

inline bool showPixelsBegin() { return false; }
inline void showPixelsApplyPersisted() {}
inline void showPixelsService() {}
inline void showPixelsOnEmergency(bool) {}
inline void showPixelsBlackout() {}
inline bool showPixelsReady() { return false; }
inline bool showPixelsEmergencyOverride() { return false; }
inline uint16_t showPixelsCount() { return 0; }
inline uint16_t showPixelsConfiguredCount() { return 0; }
inline uint16_t showPixelsMax() { return 0; }
inline uint8_t showPixelsGlobalBrightness() { return 0; }
inline void showPixelsPrintStatus() {}
inline bool showPixelsHandleCommand(const char *, char *, size_t) { return false; }

#endif

#endif /* SHOWDUINO_SHOW_PIXELS_H */
