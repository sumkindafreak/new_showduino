#ifndef SHOWDUINO_ESTOP_PIXEL_PROTOCOL_H
#define SHOWDUINO_ESTOP_PIXEL_PROTOCOL_H

#include <Arduino.h>

void estopPixelProtocolBegin();
void estopPixelProtocolService();
bool estopPixelProtocolApply(const char *command, uint32_t sequence);
void estopPixelProtocolOnEmergencyLatch(bool latched);
void estopPixelProtocolOnGlobalClearNormal();
void estopPixelProtocolOnRadioLost();
void estopPixelProtocolOnRadioRestored();
bool estopPixelShowControlled();

#endif
