#ifndef SHOWDUINO_MOSFET_IDENTIFIER_PIXELS_H
#define SHOWDUINO_MOSFET_IDENTIFIER_PIXELS_H

#include <Arduino.h>

void mosfetIdentifierPixelsSafeGpio();
void mosfetIdentifierPixelsBegin();
void mosfetIdentifierPixelsService();
void mosfetIdentifierPixelsAllOff();
void mosfetIdentifierPixelsIdentify();
bool mosfetIdentifierPixelsReady();
bool mosfetIdentifierPixelsIdentifying();

#endif
