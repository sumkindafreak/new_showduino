#ifndef SHOWDUINO_WEB_API_HANDLER_H
#define SHOWDUINO_WEB_API_HANDLER_H

#include <Arduino.h>

void webApiBegin(unsigned long bootMs);
bool webApiHandleTunnelRequest(const String &command);

#endif
