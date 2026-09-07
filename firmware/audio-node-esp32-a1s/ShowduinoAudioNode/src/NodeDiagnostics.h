#ifndef SHOWDUINO_AUDIO_NODE_DIAG_H
#define SHOWDUINO_AUDIO_NODE_DIAG_H

#include <Arduino.h>

void nodeDiagBegin();
void nodeDiagPrintBootBanner();
void nodeDiagPrintHelp();
void nodeDiagPrintStatus();
void nodeDiagPrintAudio();
void nodeDiagPrintEspNow();
void nodeDiagPrintSd();
void nodeDiagPrintCodec();
void nodeDiagPrintAssets();
void nodeDiagPrintMetrics();
void nodeDiagPrintRunTest();
void nodeDiagLedTest();
void nodeDiagMarkLoop(uint32_t elapsedUs);
void nodeDiagServiceLed();
bool nodeDiagHandleLine(const char *line);

#endif
