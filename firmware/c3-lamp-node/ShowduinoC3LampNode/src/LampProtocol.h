#ifndef SHOWDUINO_LAMP_PROTOCOL_H
#define SHOWDUINO_LAMP_PROTOCOL_H

#include <Arduino.h>

void lampProtocolBegin();
void lampProtocolApply(const char *command, uint32_t sequence, bool fromShow);
void lampProtocolLocalTest();
void lampProtocolLocalStop();
void lampProtocolAnnounce();
void lampProtocolService();
void lampProtocolFormatStatus(char *out, size_t n);

#endif
