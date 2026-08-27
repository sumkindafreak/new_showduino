#ifndef SHOWDUINO_S3_PROTOCOL_BRIDGE_H
#define SHOWDUINO_S3_PROTOCOL_BRIDGE_H

#include <Arduino.h>

void protocolBridgeBegin();
void protocolBridgeLoop();
bool protocolBridgePingP4();
bool protocolBridgePingPending();
bool protocolBridgeLastPingOk();
bool protocolBridgeP4Alive();
bool protocolBridgeDirectorOnline();

#endif
