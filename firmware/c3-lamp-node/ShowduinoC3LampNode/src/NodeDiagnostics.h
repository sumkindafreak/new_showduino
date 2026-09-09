#ifndef SHOWDUINO_LAMP_NODE_DIAG_H
#define SHOWDUINO_LAMP_NODE_DIAG_H

#include <Arduino.h>

void nodeDiagBegin();
void nodeDiagPrintBootBanner();
void nodeDiagPrintHelp();
void nodeDiagPrintStatus();
void nodeDiagPrintEspNow();
void nodeDiagPrintLamp();
void nodeDiagPrintOled();
void nodeDiagPrintPins();
void nodeDiagPrintRunTest();
void nodeDiagPixelTest();
void nodeDiagMarkLoop(uint32_t elapsedUs);
bool nodeDiagHandleLine(const char *line);

#endif
