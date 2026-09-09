#ifndef SHOWDUINO_S3_COMMS_UART_H
#define SHOWDUINO_S3_COMMS_UART_H

#include <Arduino.h>

void commsUartBegin();
bool commsUartReady();
void commsUartWriteLine(const char *line);
void commsUartWriteBytes(const uint8_t *data, size_t len);
bool commsUartReadLine(char *out, size_t outSize);
int commsUartAvailable();
int commsUartRead();
uint32_t commsUartLastRxMs();
uint32_t commsUartRxCount();
uint32_t commsUartTxCount();
uint32_t commsUartDroppedCount();
bool commsUartEverRx();

#endif
