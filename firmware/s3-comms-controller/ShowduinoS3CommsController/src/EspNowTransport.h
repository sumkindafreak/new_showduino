#ifndef SHOWDUINO_S3_ESPNOW_TRANSPORT_H
#define SHOWDUINO_S3_ESPNOW_TRANSPORT_H

#include <Arduino.h>

typedef void (*ShowduinoDeskCommandFn)(const char *command);
typedef void (*ShowduinoNodeCommandFn)(const char *nodeType, const char *command, uint32_t sequence);

bool espNowTransportBegin();
bool espNowTransportReady();
void espNowTransportSetCommandHandler(ShowduinoDeskCommandFn fn);
void espNowTransportSetNodeHandler(ShowduinoNodeCommandFn fn);
bool espNowTransportSendToDirector(const char *command);
bool espNowTransportSendToAudioNode(const char *command, uint32_t sequence);
bool espNowTransportHaveDirector();
bool espNowTransportHaveAudioNode();
void espNowTransportDirectorMac(uint8_t out[6]);
void espNowTransportAudioNodeMac(uint8_t out[6]);
uint32_t espNowTransportRxCount();
uint32_t espNowTransportTxCount();
uint32_t espNowTransportRejectedCount();
uint32_t espNowTransportLastDirectorMs();
uint32_t espNowTransportLastAudioNodeMs();
void espNowTransportPrintMac(const uint8_t *mac);
bool espNowTransportReadStaMac(uint8_t out[6]);

#endif
