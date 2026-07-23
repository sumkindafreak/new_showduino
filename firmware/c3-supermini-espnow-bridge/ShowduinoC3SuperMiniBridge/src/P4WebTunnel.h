#ifndef SHOWDUINO_P4_WEB_TUNNEL_H
#define SHOWDUINO_P4_WEB_TUNNEL_H

#include <Arduino.h>

typedef void (*P4LinkPumpFn)();

void p4WebTunnelBegin();
void p4WebTunnelSetPump(P4LinkPumpFn fn);

bool p4WebTunnelConsumingBytes();
void p4WebTunnelOnByte(char c);
bool p4WebTunnelOnLine(const String &line);

bool p4WebTunnelGet(const char *path, String &bodyOut, int &statusOut, uint32_t timeoutMs = 800);

#endif
