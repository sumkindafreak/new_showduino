#ifndef SHOWDUINO_STORAGE_API_H
#define SHOWDUINO_STORAGE_API_H

#include <Arduino.h>
#include "StorageConfig.h"
#include "StorageTypes.h"
#include "SDManager.h"
#include "StorageRecovery.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "AssetManager.h"
#include "ShowManager.h"
#include "DeviceDatabase.h"

// =========================================================
// Central Showduino Director storage API
// =========================================================

class ShowduinoStorage {
public:
  bool begin(StorageStatusCallback cb = nullptr) {
    statusCb = cb;
    recovery.beginBootMarker();

    if (recovery.interruptedStartup()) {
      Serial.println("[Storage] Interrupted startup detected");
    }

    if (!sd.begin(cb)) {
      recoveryMode = true;
      logs.begin(nullptr, SaveLogMode::ErrorsOnly);
      // Always clear the boot marker in recovery — otherwise a crash before UI
      // leaves NVS bootAttempts climbing and "interrupted startup" forever.
      recovery.markStartupComplete();
      Serial.println("[Storage] RECOVERY MODE ACTIVE — emergency controls remain available");
      return false;
    }

    sd.setStage(StorageBootStage::RecoverWrites, "Recovering temporary files");
    sd.getStatus().recoveryRequired = recovery.interruptedStartup();
    recovery.recoverPendingWrites();

    if (recovery.interruptedStartup()) {
      recovery.writeCrashReport(sd, config.lastPage, config.lastShow, lastCommand);
    }

    sd.setStage(StorageBootStage::LoadDirectorConfig, "Loading Director configuration");
    configMgr.load(config);

    sd.setStage(StorageBootStage::LoadAssetManifest, "Loading UI assets");
    assets.loadManifest();

    sd.setStage(StorageBootStage::LoadDevices, "Loading paired devices");
    devices.load();

    sd.setStage(StorageBootStage::LoadShowIndex, "Loading show library");
    shows.loadIndex();

    sd.setStage(StorageBootStage::StartLogging, "Logging started");
    logs.begin(&sd, config.saveLogMode);
    logs.setCommsMode(config.commsLogMode);
    logs.flushRamQueueToSd();
    logs.logEvent(LogLevel::Info, LogCategory::System, "Storage", "Storage ready");

    sd.getStatus().loggingEnabled = true;
    sd.setStage(StorageBootStage::Ready, "Director ready");
    recovery.markStartupComplete();
    recoveryMode = false;
    bootOk = true;
    return true;
  }

  void loop() {
    unsigned long now = millis();

    if (recoveryMode) {
      if (now - lastMountRetryMs >= STORAGE_MOUNT_RETRY_MS) {
        lastMountRetryMs = now;
        if (retryMountSD()) {
          logs.logEvent(LogLevel::Info, LogCategory::Storage, "Storage", "SD restored — leaving recovery mode");
        }
      }
      return;
    }

    if (!sd.getStatus().mounted) return;

    // Keep SD work short — long SPI stalls make LVGL/touch feel dead.
    if (now - lastSpaceRefreshMs >= 5000UL) {
      lastSpaceRefreshMs = now;
      sd.refreshSpace();
    }
    if (now - lastMaintenanceMs >= 250UL) {
      lastMaintenanceMs = now;
      logs.process(sd.getStatus().criticalSpace);
      devices.processPeriodicSave(60000UL);
      processAutoSave();
    }

    if (statusCb && now - lastStatusPushMs > 2000UL) {
      lastStatusPushMs = now;
      statusCb(sd.getStatus());
    }
  }

  bool shutdown() {
    Serial.println("[Storage] SAVING DATA");
    sd.markSaveState(SaveUiState::Saving);
    unsigned long t0 = millis();
    saveAllDirtyData();
    logs.logEvent(LogLevel::Info, LogCategory::System, "Storage", "Shutdown flush");
    logs.flush();
    while (millis() - t0 < STORAGE_SHUTDOWN_TIMEOUT_MS) {
      // Bounded wait for SD settle.
      delay(10);
      break;
    }
    Serial.println("[Storage] CLOSING STORAGE");
    recovery.markCleanShutdown();
    sd.safelyUnmount();
    Serial.println("[Storage] SAFE TO RESTART");
    return true;
  }

  const StorageStatus &getStorageStatus() const { return sd.getStatus(); }
  StorageStatus &getStorageStatusMut() { return sd.getStatus(); }
  bool isRecoveryMode() const { return recoveryMode; }
  DirectorConfig &getConfig() { return config; }
  const DirectorConfig &getConfig() const { return config; }

  bool loadAllConfiguration() { return configMgr.load(config); }
  bool saveAllConfiguration() {
    sd.markSaveState(SaveUiState::Saving);
    bool ok = configMgr.save(config);
    sd.markSaveState(ok ? SaveUiState::Saved : SaveUiState::SaveError);
    return ok;
  }

  bool saveAllDirtyData() {
    sd.markSaveState(SaveUiState::Saving);
    bool ok = true;
    if (configMgr.isDirty()) ok &= configMgr.save(config);
    if (devices.isDirty()) ok &= devices.save();
    if (shows.isDirty()) {
      if (shows.hasActiveShow()) ok &= shows.saveShow(shows.activeShow());
      else ok &= shows.saveIndex();
    }
    logs.flush();
    sd.markSaveState(ok ? SaveUiState::Saved : SaveUiState::SaveError);
    return ok;
  }

  void markConfigDirty() {
    configMgr.markDirty();
    sd.markSaveState(SaveUiState::Unsaved);
  }
  void markShowDirty() {
    shows.markDirty();
    sd.markSaveState(SaveUiState::Unsaved);
  }
  void markDevicesDirty() {
    devices.markDirty();
    sd.markSaveState(SaveUiState::Unsaved);
  }

  void processAutoSave() {
    if (!config.autoSave) return;
    unsigned long delayMs = constrain((unsigned long)config.saveIntervalSeconds * 1000UL,
                                      STORAGE_AUTOSAVE_MIN_MS, STORAGE_AUTOSAVE_MAX_MS);
    if (configMgr.isDirty() && configMgr.dirtyAgeMs() >= delayMs) saveAllConfiguration();
    if (shows.isDirty() && configMgr.dirtyAgeMs() >= delayMs) {
      if (shows.hasActiveShow()) shows.saveShow(shows.activeShow());
      else shows.saveIndex();
      sd.markSaveState(SaveUiState::Saved);
    }
  }

  bool startShowLog(const char *showId) { return logs.startShowLog(showId); }
  void endShowLog(const char *result) { logs.endShowLog(result); }

  void logEvent(LogLevel level, LogCategory category, const char *source, const char *message) {
    logs.logEvent(level, category, source, message);
  }

  void logEmergency(const char *source, const char *message) {
    logs.logEmergency(source, message);
    saveAllDirtyData();  // critical immediate save
  }

  void setLastCommand(const char *cmd) {
    strncpy(lastCommand, cmd ? cmd : "", sizeof(lastCommand) - 1);
  }

  bool createManualBackup() {
    bool ok = configMgr.createConfigBackup("manual");
    devices.save();
    shows.saveIndex();
    logs.logEvent(LogLevel::Info, LogCategory::Storage, "Backup", ok ? "Manual backup created" : "Manual backup failed");
    return ok;
  }

  bool exportDiagnostics() {
    char path[STORAGE_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "/showduino/exports/diagnostics/diag_%lu.json", millis());
    const StorageStatus &st = sd.getStatus();
    String j = "{\n";
    j += "  \"schemaVersion\": 1,\n";
    j += "  \"firmware\": \"" + String(STORAGE_FW_VERSION) + "\",\n";
    j += "  \"directorId\": \"" + ShowduinoFileUtil::jsonEscape(config.directorId) + "\",\n";
    j += "  \"resetReason\": " + String(recovery.getResetReason()) + ",\n";
    j += "  \"sdMounted\": " + String(st.mounted ? "true" : "false") + ",\n";
    j += "  \"cardType\": \"" + String(st.cardType) + "\",\n";
    j += "  \"totalBytes\": " + String((unsigned long)(st.totalBytes)) + ",\n";
    j += "  \"freeBytes\": " + String((unsigned long)(st.freeBytes)) + ",\n";
    j += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
    j += "  \"pairedDevices\": " + String(devices.size()) + ",\n";
    j += "  \"showsIndexed\": " + String(shows.size()) + ",\n";
    j += "  \"lastError\": \"" + ShowduinoFileUtil::jsonEscape(st.lastError) + "\"\n";
    j += "}\n";
    // Intentionally omits passwords / PINs / pairing keys.
    bool ok = ShowduinoFileUtil::atomicWriteTextFile(path, j);
    logs.logEvent(LogLevel::Info, LogCategory::Storage, "Diagnostics", ok ? path : "export failed");
    return ok;
  }

  bool safelyUnmountSD() {
    saveAllDirtyData();
    logs.flush();
    return sd.safelyUnmount();
  }

  bool retryMountSD() {
    if (!sd.retryMount()) return false;
    recovery.recoverPendingWrites();
    configMgr.load(config);
    assets.loadManifest();
    devices.load();
    shows.loadIndex();
    logs.begin(&sd, config.saveLogMode);
    logs.flushRamQueueToSd();
    recoveryMode = false;
    bootOk = true;
    sd.setStage(StorageBootStage::Ready, "Storage restored");
    return true;
  }

  bool repairFolders() { return sd.ensureFolderStructure(); }

  // Subsystem accessors
  LogManager &logger() { return logs; }
  ShowManager &showManager() { return shows; }
  DeviceDatabase &deviceDb() { return devices; }
  AssetManager &assetManager() { return assets; }
  ConfigManager &configManager() { return configMgr; }

private:
  SDManager sd;
  StorageRecovery recovery;
  ConfigManager configMgr;
  LogManager logs;
  AssetManager assets;
  ShowManager shows;
  DeviceDatabase devices;
  DirectorConfig config;
  StorageStatusCallback statusCb = nullptr;

  bool bootOk = false;
  bool recoveryMode = false;
  unsigned long lastMountRetryMs = 0;
  unsigned long lastStatusPushMs = 0;
  unsigned long lastSpaceRefreshMs = 0;
  unsigned long lastMaintenanceMs = 0;
  char lastCommand[96] = {};
};

// ---- Global convenience wrappers (match required public API) ----
extern ShowduinoStorage gStorage;

inline bool storageBegin(StorageStatusCallback cb = nullptr) { return gStorage.begin(cb); }
inline void storageLoop() { gStorage.loop(); }
inline void storageShutdown() { gStorage.shutdown(); }
inline const StorageStatus &getStorageStatus() { return gStorage.getStorageStatus(); }
inline bool loadAllConfiguration() { return gStorage.loadAllConfiguration(); }
inline bool saveAllConfiguration() { return gStorage.saveAllConfiguration(); }
inline bool saveAllDirtyData() { return gStorage.saveAllDirtyData(); }
inline void markConfigDirty() { gStorage.markConfigDirty(); }
inline void markShowDirty() { gStorage.markShowDirty(); }
inline void markDevicesDirty() { gStorage.markDevicesDirty(); }
inline bool startShowLog(const char *showId) { return gStorage.startShowLog(showId); }
inline void endShowLog(const char *result) { gStorage.endShowLog(result); }
inline void logEvent(LogLevel level, LogCategory category, const char *source, const char *message) {
  gStorage.logEvent(level, category, source, message);
}
inline bool createManualBackup() { return gStorage.createManualBackup(); }
inline bool exportDiagnostics() { return gStorage.exportDiagnostics(); }
inline bool safelyUnmountSD() { return gStorage.safelyUnmountSD(); }
inline bool retryMountSD() { return gStorage.retryMountSD(); }

#endif
