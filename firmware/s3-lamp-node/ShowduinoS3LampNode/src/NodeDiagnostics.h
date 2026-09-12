#ifndef SHOWDUINO_S3_LAMP_NODE_DIAG_H
#define SHOWDUINO_S3_LAMP_NODE_DIAG_H

#include <Arduino.h>

void nodeDiagBegin();
void nodeDiagPrintBootBanner();
void nodeDiagPrintHelp();
void nodeDiagPrintStatus();
void nodeDiagPrintPins();
void nodeDiagMarkLoop(uint32_t elapsedUs);
bool nodeDiagHandleLine(const char *line);

#endif
