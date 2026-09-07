#ifndef SHOWDUINO_SHOW_HTTP_H
#define SHOWDUINO_SHOW_HTTP_H

#include <Arduino.h>

void showHttpBegin();
void showHttpOnAddress(const char *ip);
void showHttpOnLinkLost();
void showHttpLoop();
bool showHttpListening();

#endif
