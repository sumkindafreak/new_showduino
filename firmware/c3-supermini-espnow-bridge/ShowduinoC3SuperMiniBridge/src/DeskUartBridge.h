#ifndef SHOWDUINO_DESK_UART_BRIDGE_H
#define SHOWDUINO_DESK_UART_BRIDGE_H

#include <Arduino.h>

/* Director ESP-NOW desk packets <-> UART lines to the P4 Stage Controller. */
void deskUartBridgeBegin();
void deskUartBridgeOnP4Line(const String &line);

#endif
