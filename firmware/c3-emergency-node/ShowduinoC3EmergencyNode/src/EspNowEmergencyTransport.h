#ifndef SHOWDUINO_EMERGENCY_ESPNOW_TRANSPORT_H
#define SHOWDUINO_EMERGENCY_ESPNOW_TRANSPORT_H

#include <Arduino.h>

typedef void (*EmergencyNodeRxFn)(const char *command, uint32_t sequence);

bool emergencyEspNowBegin();
bool emergencyEspNowReady();
void emergencyEspNowSetHandler(EmergencyNodeRxFn fn);
bool emergencyEspNowSend(const char *command, uint32_t sequence);
void emergencyEspNowMacString(char *out, size_t n);
void emergencyEspNowMacBytes(uint8_t out[6]);
void emergencyEspNowReassert();
void emergencyEspNowRecover();
void emergencyEspNowService();
uint32_t emergencyEspNowRxCount();
uint32_t emergencyEspNowTxCount();
uint32_t emergencyEspNowRejected();
uint32_t emergencyEspNowLastRxMs();
bool emergencyEspNowHaveComms();
uint8_t emergencyEspNowChannel();
int8_t emergencyEspNowRssi();

#endif
