#ifndef SHOWDUINO_S3_ESPNOW_TRANSPORT_H
#define SHOWDUINO_S3_ESPNOW_TRANSPORT_H

#include <Arduino.h>

typedef void (*ShowduinoDeskCommandFn)(const char *command);

bool espNowTransportBegin();
bool espNowTransportReady();
void espNowTransportSetCommandHandler(ShowduinoDeskCommandFn fn);
bool espNowTransportSendToDirector(const char *command);
bool espNowTransportHaveDirector();
void espNowTransportDirectorMac(uint8_t out[6]);
uint32_t espNowTransportRxCount();
uint32_t espNowTransportTxCount();
uint32_t espNowTransportRejectedCount();
uint32_t espNowTransportLastDirectorMs();
void espNowTransportPrintMac(const uint8_t *mac);
bool espNowTransportReadStaMac(uint8_t out[6]);

#endif
