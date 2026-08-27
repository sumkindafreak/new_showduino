#ifndef SHOWDUINO_WEB_API_HANDLER_H
#define SHOWDUINO_WEB_API_HANDLER_H

#include <Arduino.h>

void webApiBegin(unsigned long bootMs);
bool webApiHandleTunnelRequest(const String &command);
bool webApiOriginReady();
/*
 * Exercise origin URL mapping + SD read without transmitting on Comms UART.
 * url is a public WebUI path ("/" or "/index.html"), not an SD path.
 */
bool webApiProbePublicUrl(const char *url, char *resolved, size_t resolvedLen,
                          char *err, size_t errLen);
bool webApiProbeIndex(char *err, size_t errLen);

#endif
