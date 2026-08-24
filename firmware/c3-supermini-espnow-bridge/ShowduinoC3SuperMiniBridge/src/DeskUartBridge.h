#ifndef SHOWDUINO_DESK_UART_BRIDGE_H
#define SHOWDUINO_DESK_UART_BRIDGE_H

#include <Arduino.h>

/* Director ESP-NOW desk packets <-> UART lines to the P4 Stage Controller. */
void deskUartBridgeBegin();
void deskUartBridgeOnP4Line(const String &line);

/*
 * Shared SUE -> P4 command path.
 * Web Studio and the Director both use the same UART transport so the P4
 * remains the authoritative Stage Runtime and emergency authority.
 */
void deskUartBridgeSendToP4(const char *command);

#endif
