#ifndef SHOWDUINO_S3_LAMP_PROTOCOL_H
#define SHOWDUINO_S3_LAMP_PROTOCOL_H

#include <Arduino.h>
#include "../../../protocol/showduino_node_ownership.h"

void lampProtocolBegin();
void lampProtocolApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin);
void lampProtocolLocalIgnite();
void lampProtocolAnnounce();
void lampProtocolService();
void lampProtocolFormatStatus(char *out, size_t n);

#endif
