#ifndef SHOWDUINO_PIXEL_PROTOCOL_H
#define SHOWDUINO_PIXEL_PROTOCOL_H

#include <Arduino.h>
#include "../../../protocol/showduino_node_ownership.h"

void audioPixelProtocolBegin();
void audioPixelProtocolApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin);
void audioPixelProtocolLocalTest();
void audioPixelProtocolAnnounce();
void audioPixelProtocolService();
void audioPixelProtocolFormatStatus(char *out, size_t n);

#endif
