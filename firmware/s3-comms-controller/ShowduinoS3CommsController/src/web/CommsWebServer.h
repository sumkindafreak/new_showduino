#ifndef SHOWDUINO_S3_COMMS_WEB_SERVER_H
#define SHOWDUINO_S3_COMMS_WEB_SERVER_H

#include <Arduino.h>

void commsWebBegin();
void commsWebLoop();
bool commsWebReady();
bool commsWebFault();
uint8_t commsWebRadioChannel();
const char *commsWebSsid();
const char *commsWebIp();

#endif
