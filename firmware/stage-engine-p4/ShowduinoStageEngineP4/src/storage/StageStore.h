#ifndef SHOWDUINO_STAGE_STORE_H
#define SHOWDUINO_STAGE_STORE_H

#include <Arduino.h>
#include "../../../protocol/showduino_storage.h"

struct StageStoreStatus {
  ShowduinoStorageState state = SHOWDUINO_STORAGE_OFFLINE;
  uint8_t formatVersion = 0;
  uint32_t bootCount = 0;
  uint16_t productionCount = 0;
  uint16_t backupCount = 0;
  uint32_t logBytes = 0;
  bool configHealthOk = true;
  bool writesStopped = false;
  char lastError[64] = "";
  char sessionStamp[24] = "boot-0";
};

void stageStoreBegin();
void stageStoreLoop();
const StageStoreStatus &stageStoreStatus();
ShowduinoStorageState stageStoreState();
const char *stageStoreStateName();
bool stageStoreWritable();
bool stageStoreAtomicWrite(const char *path, const char *data, size_t len);
bool stageStoreReadText(const char *path, size_t maxBytes, String &out);
bool stageStoreBackupConfig(char *sessionOut, size_t sessionLen);
bool stageStoreWriteDiagnostics(const char *basename, const char *body);
void stageStoreSessionStamp(char *out, size_t n);
void stageStoreNoteLastProduction(const char *id);
void stageStorePrintStatus();
void stageStorePrintList();
void stageStorePrintCheck();
bool stageStoreHandleCommand(const String &command);
void stageStoreAppendJson(String &json);
void stageStoreSetError(const char *message);

#endif
