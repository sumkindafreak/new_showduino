#ifndef SHOWDUINO_PIXEL_ESPNOW_TRANSPORT_H
#define SHOWDUINO_PIXEL_ESPNOW_TRANSPORT_H

#include <Arduino.h>

typedef void (*PixelNodeRxFn)(const char *command, uint32_t sequence);

bool pixelEspNowBegin();
bool pixelEspNowReady();
void pixelEspNowSetHandler(PixelNodeRxFn fn);
bool pixelEspNowSend(const char *command, uint32_t sequence);
void pixelEspNowMacString(char *out, size_t n);
void pixelEspNowMacBytes(uint8_t out[6]);
void pixelEspNowReassert();
void pixelEspNowRecover();
void pixelEspNowService();
uint32_t pixelEspNowRxCount();
uint32_t pixelEspNowTxCount();
uint32_t pixelEspNowRejected();
uint32_t pixelEspNowLastRxMs();
bool pixelEspNowHaveComms();
uint8_t pixelEspNowChannel();
int8_t pixelEspNowRssi();

#endif
