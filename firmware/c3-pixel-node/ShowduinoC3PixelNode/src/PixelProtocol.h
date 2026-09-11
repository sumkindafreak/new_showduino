#ifndef SHOWDUINO_PIXEL_PROTOCOL_H
#define SHOWDUINO_PIXEL_PROTOCOL_H

#include <Arduino.h>
#include "../../../protocol/showduino_node_ownership.h"

void pixelProtocolBegin();
void pixelProtocolApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin);
void pixelProtocolLocalTest();
void pixelProtocolAnnounce();
void pixelProtocolService();
void pixelProtocolFormatStatus(char *out, size_t n);

#endif
