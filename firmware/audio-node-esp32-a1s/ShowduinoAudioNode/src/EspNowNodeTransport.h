#ifndef SHOWDUINO_AUDIO_ESPNOW_TRANSPORT_H
#define SHOWDUINO_AUDIO_ESPNOW_TRANSPORT_H

#include <Arduino.h>

typedef void (*AudioNodeRxFn)(const char *command, uint32_t sequence);

bool audioEspNowBegin();
bool audioEspNowReady();
void audioEspNowSetHandler(AudioNodeRxFn fn);
bool audioEspNowSend(const char *command, uint32_t sequence);
void audioEspNowMacString(char *out, size_t n);
uint32_t audioEspNowRxCount();
uint32_t audioEspNowTxCount();
uint32_t audioEspNowRejected();
uint32_t audioEspNowLastRxMs();
bool audioEspNowHaveComms();

#endif
