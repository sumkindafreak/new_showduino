#ifndef SHOWDUINO_S3_COMMS_UART_H
#define SHOWDUINO_S3_COMMS_UART_H

#include <Arduino.h>

void commsUartBegin();
bool commsUartReady();
void commsUartWriteLine(const char *line);
bool commsUartReadLine(char *out, size_t outSize);
uint32_t commsUartLastRxMs();
uint32_t commsUartRxCount();
uint32_t commsUartTxCount();
uint32_t commsUartDroppedCount();
bool commsUartEverRx();

#endif
