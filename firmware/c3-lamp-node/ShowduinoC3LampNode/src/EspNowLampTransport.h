#ifndef SHOWDUINO_LAMP_ESPNOW_TRANSPORT_H
#define SHOWDUINO_LAMP_ESPNOW_TRANSPORT_H

#include <Arduino.h>

typedef void (*LampNodeRxFn)(const char *command, uint32_t sequence);

bool lampEspNowBegin();
bool lampEspNowReady();
void lampEspNowService();
void lampEspNowSetHandler(LampNodeRxFn fn);
bool lampEspNowSend(const char *command, uint32_t sequence);
void lampEspNowMacString(char *out, size_t n);
uint32_t lampEspNowRxCount();
uint32_t lampEspNowTxCount();
uint32_t lampEspNowRejected();
uint32_t lampEspNowLastRxMs();
bool lampEspNowHaveComms();

#endif
