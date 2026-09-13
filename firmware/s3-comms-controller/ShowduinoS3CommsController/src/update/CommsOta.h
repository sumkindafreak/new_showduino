#ifndef SHOWDUINO_COMMS_OTA_H
#define SHOWDUINO_COMMS_OTA_H

#include <Arduino.h>
#include "../../../protocol/showduino_update_manager.h"

void commsOtaBegin();
void commsOtaLoop();
bool commsOtaBusy();
const char *commsOtaState();
const char *commsOtaError();
void commsOtaAppendStatusJson(String &json);
bool commsOtaParseApply(const String &body, ShowduinoOtaCandidate *c, String &url);
bool commsOtaRequestApply(const ShowduinoOtaCandidate &c, const char *url, String &err);
bool commsOtaWantsNetJob();
void commsOtaRunNetJob();
void commsOtaPushDirector();

#endif
