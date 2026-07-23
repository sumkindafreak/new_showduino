#ifndef SHOWDUINO_STAGE_STORAGE_H
#define SHOWDUINO_STAGE_STORAGE_H

#include <Arduino.h>
#include "../BoardConfig.h"

struct StageStorageStatus {
  bool mounted = false;
  bool writable = false;
  bool folderOk = false;
  bool hasWww = false;
  char cardType[12] = "NONE";
  uint64_t totalBytes = 0;
  uint64_t freeBytes = 0;
  uint32_t spiHz = 0;
  char message[48] = "SD not started";
};

#if SHOWDUINO_SD_ENABLED

bool stageStorageBegin();
void stageStorageLoop();
const StageStorageStatus &stageStorageStatus();
bool stageStorageIsReady();

#else

inline bool stageStorageBegin() { return false; }
inline void stageStorageLoop() {}
inline bool stageStorageIsReady() { return false; }

inline const StageStorageStatus &stageStorageStatus() {
  static StageStorageStatus disabled;
  strncpy(disabled.message, "SD disabled in BoardConfig", sizeof(disabled.message) - 1);
  return disabled;
}

#endif

#endif
