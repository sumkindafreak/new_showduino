#ifndef SHOWDUINO_S3_COMMS_STATUS_RGB_H
#define SHOWDUINO_S3_COMMS_STATUS_RGB_H

#include <Arduino.h>

enum class CommsRgbState : uint8_t {
  Boot = 0,
  WebuiStarting,
  WebuiReady,
  Synchronising,
  Healthy,
  DirectorOffline,
  P4Offline,
  WebuiFault,
  EspNowFault,
  Emergency,
  Fault
};

void commsStatusRgbBegin();
void commsStatusRgbLoop();
void commsStatusRgbNoteWebStarting();
void commsStatusRgbStartTest();
void commsStatusRgbPrintStatus();

CommsRgbState commsStatusRgbState();
const char *commsStatusRgbStateName();
const char *commsStatusRgbColourName();
uint8_t commsStatusRgbBrightness();
uint8_t commsStatusRgbPin();

#endif
