#ifndef SHOWDUINO_MOSFET_ESPNOW_TRANSPORT_H
#define SHOWDUINO_MOSFET_ESPNOW_TRANSPORT_H

#include <Arduino.h>

typedef void (*MosfetNodeRxFn)(const char *command, uint32_t sequence);

bool mosfetEspNowBegin();
bool mosfetEspNowReady();
void mosfetEspNowSetHandler(MosfetNodeRxFn fn);
bool mosfetEspNowSend(const char *command, uint32_t sequence);
void mosfetEspNowMacString(char *out, size_t n);
void mosfetEspNowMacBytes(uint8_t out[6]);
void mosfetEspNowReassert();
void mosfetEspNowRecover();
void mosfetEspNowService();
uint32_t mosfetEspNowRxCount();
uint32_t mosfetEspNowTxCount();
uint32_t mosfetEspNowRejected();
uint32_t mosfetEspNowLastRxMs();
bool mosfetEspNowHaveComms();
uint8_t mosfetEspNowChannel();
int8_t mosfetEspNowRssi();

#endif
