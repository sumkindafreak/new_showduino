#ifndef SHOWDUINO_PIXEL_DISPLAY_H
#define SHOWDUINO_PIXEL_DISPLAY_H

#include <Arduino.h>

bool pixelDisplayBegin();
bool pixelDisplayReady();
void pixelDisplayService();
void pixelDisplayForce();
void pixelDisplayNextPage();
uint8_t pixelDisplayPage();
void pixelDisplayOledTest();
const char *pixelDisplayController();

#endif
