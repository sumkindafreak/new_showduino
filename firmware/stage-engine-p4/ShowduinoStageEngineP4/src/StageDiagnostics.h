#ifndef SHOWDUINO_STAGE_DIAGNOSTICS_H
#define SHOWDUINO_STAGE_DIAGNOSTICS_H

#include <Arduino.h>

/*
 * Local USB commissioning diagnostics (RUN:TEST).
 * Not part of the Director/comms command protocol.
 */

void stageDiagService();
bool stageDiagHandleCommand(const char *cmd);
void stageDiagNoteCommsPong();
void stageDiagNoteCommsLine(const char *line);
bool stageDiagIsActive();

/* Host hooks implemented by ShowduinoStageEngineP4.ino */
bool stageCommsUartReady();
bool stageCommsLinkUp();
bool stageCommsEverUp();
uint32_t stageCommsLastRxMs();
bool stageCommsSawDirectorTraffic();
void stageCommsSendLine(const char *line);
void stageDiagDispatchLocal(const char *cmd);
bool stageEstopDebouncedAsserted();

#endif
