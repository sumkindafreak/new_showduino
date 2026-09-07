#ifndef SHOWDUINO_S3_COMMS_WEB_TUNNEL_H
#define SHOWDUINO_S3_COMMS_WEB_TUNNEL_H

#include <Arduino.h>

typedef void (*CommsWebPumpFn)();

void commsWebTunnelBegin();
void commsWebTunnelSetPump(CommsWebPumpFn fn);
bool commsWebTunnelConsumingBytes();
void commsWebTunnelOnByte(char c);
bool commsWebTunnelOnLine(const char *line);

/* Blocking wait with pump so ESP-NOW / UART keep moving. */
bool commsWebTunnelGet(const char *path, String &bodyOut, int &statusOut,
                       String &mimeOut, uint32_t timeoutMs = 1800);
bool commsWebTunnelPost(const char *path, String &bodyOut, int &statusOut,
                        String &mimeOut, uint32_t timeoutMs = 4000);

#endif
