#ifndef SHOWDUINO_C3_WEB_SERVER_MANAGER_H
#define SHOWDUINO_C3_WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include "../BoardConfig.h"

#if SHOWDUINO_WEBUI_ENABLED

void webServerBegin(unsigned long bootMs);
void webServerLoop();

#else

inline void webServerBegin(unsigned long bootMs) { (void)bootMs; }
inline void webServerLoop() {}

#endif

#endif
