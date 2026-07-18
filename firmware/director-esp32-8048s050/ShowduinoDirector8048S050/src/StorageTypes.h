#ifndef SHOWDUINO_STORAGE_TYPES_H
#define SHOWDUINO_STORAGE_TYPES_H

#include <Arduino.h>
#include "StorageConfig.h"

enum class LogLevel : uint8_t {
  Debug = 0,
  Info,
  Event,
  Warning,
  Error,
  Critical
};

enum class LogCategory : uint8_t {
  System = 0,
  Storage,
  Communication,
  Device,
  Show,
  Cue,
  UserAction,
  WarningCat,
  Emergency,
  Crash,
  Update
};

enum class SaveLogMode : uint8_t {
  Off = 0,
  ErrorsOnly,
  ShowEvents,
  Normal,
  Detailed,
  DebugMode
};

enum class CommsLogMode : uint8_t {
  CommandsOnly = 0,
  CommandsAndAcks,
  ErrorsOnly,
  FullPacketDebug
};

enum class SaveUiState : uint8_t {
  Saved = 0,
  Saving,
  Unsaved,
  SaveError
};

enum class StorageBootStage : uint8_t {
  Idle = 0,
  InitSpi,
  MountCard,
  DetectCard,
  ReadSize,
  FreeSpace,
  ValidateFolders,
  CreateFolders,
  ValidateConfig,
  RecoverWrites,
  LoadDirectorConfig,
  LoadAssetManifest,
  LoadDevices,
  LoadShowIndex,
  StartLogging,
  Ready,
  RecoveryMode,
  Failed
};

struct StorageStatus {
  bool mounted;
  bool writable;
  bool folderStructureValid;
  bool recoveryRequired;
  bool lowSpace;
  bool criticalSpace;
  bool loggingEnabled;
  bool recoveryMode;

  uint64_t totalBytes;
  uint64_t usedBytes;
  uint64_t freeBytes;

  uint32_t fileCount;
  uint32_t logFileCount;

  char cardType[16];
  char lastError[128];
  char activeSystemLog[STORAGE_MAX_PATH_LEN];
  char bootMessage[96];
  StorageBootStage stage;
  SaveUiState saveState;
  SaveLogMode saveLogMode;
  CommsLogMode commsLogMode;
  unsigned long lastSuccessfulSaveMs;
};

struct DirectorConfig {
  uint16_t schemaVersion;
  char directorId[24];
  char directorName[48];
  uint8_t brightness;
  uint8_t volume;
  char lastPage[32];
  char lastShow[64];
  bool autoConnect;
  bool autoSave;
  uint16_t saveIntervalSeconds;
  char logLevel[16];
  uint8_t screenTimeoutMinutes;
  bool emergencyResetRequiresPin;
  SaveLogMode saveLogMode;
  CommsLogMode commsLogMode;
  uint16_t maxLogAgeDays;
  uint32_t maxLogStorageMb;
};

struct ShowDefinition {
  uint16_t schemaVersion;
  char id[64];
  char name[64];
  char description[128];
  char author[48];
  char version[16];
  uint32_t durationSeconds;
  uint16_t cueCount;
  char created[32];
  char modified[32];
  uint16_t startCue;
  bool stageControllerRequired;
};

struct DeviceRecord {
  char id[32];
  char name[48];
  char type[32];
  char mac[24];
  uint16_t protocolVersion;
  bool paired;
  bool trusted;
  char lastSeen[32];
  char firmware[16];
  char capabilities[96];
};

typedef void (*StorageStatusCallback)(const StorageStatus &status);

#endif
