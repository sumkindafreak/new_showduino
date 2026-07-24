#ifndef SHOWDUINO_CONFIG_MANAGER_H
#define SHOWDUINO_CONFIG_MANAGER_H

#include <Arduino.h>
#include "StorageConfig.h"
#include "StorageTypes.h"
#include "FileUtil.h"
#include "SDManager.h"

class ConfigManager {
public:
  void setDefaults(DirectorConfig &cfg) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.schemaVersion = STORAGE_SCHEMA_VERSION;
    strncpy(cfg.directorId, "DIR-01", sizeof(cfg.directorId));
    strncpy(cfg.directorName, "Main Director", sizeof(cfg.directorName));
    cfg.brightness = 180;
    cfg.volume = 70;
    strncpy(cfg.lastPage, "desktop", sizeof(cfg.lastPage));
    cfg.lastShow[0] = '\0';
    cfg.autoConnect = true;
    cfg.autoSave = true;
    cfg.saveIntervalSeconds = 15;
    strncpy(cfg.logLevel, "info", sizeof(cfg.logLevel));
    cfg.screenTimeoutMinutes = 10;
    cfg.emergencyResetRequiresPin = true;
    cfg.saveLogMode = SaveLogMode::Normal;
    cfg.commsLogMode = CommsLogMode::CommandsAndAcks;
    cfg.maxLogAgeDays = STORAGE_LOG_MAX_AGE_DAYS;
    cfg.maxLogStorageMb = 100;
    cfg.confirmBeforeStart    = false;
    cfg.confirmBeforeStop     = false;
    cfg.autoOpenLiveAfterLoad = false;
  }

  bool load(DirectorConfig &cfg) {
    setDefaults(cfg);
    if (!ShowduinoFileUtil::recoverAtomicFile(PATH_DIRECTOR_JSON)) {
      Serial.println("[Config] no director.json — writing defaults");
      return save(cfg);
    }

    String json;
    if (!ShowduinoFileUtil::readTextFile(PATH_DIRECTOR_JSON, json)) return false;

    cfg.schemaVersion = (uint16_t)ShowduinoFileUtil::jsonGetLong(json, "schemaVersion", STORAGE_SCHEMA_VERSION);
    strncpy(cfg.directorId, ShowduinoFileUtil::jsonGetString(json, "directorId", "DIR-01").c_str(), sizeof(cfg.directorId) - 1);
    strncpy(cfg.directorName, ShowduinoFileUtil::jsonGetString(json, "directorName", "Main Director").c_str(), sizeof(cfg.directorName) - 1);
    cfg.brightness = (uint8_t)ShowduinoFileUtil::jsonGetLong(json, "brightness", 180);
    cfg.volume = (uint8_t)ShowduinoFileUtil::jsonGetLong(json, "volume", 70);
    strncpy(cfg.lastPage, ShowduinoFileUtil::jsonGetString(json, "lastPage", "desktop").c_str(), sizeof(cfg.lastPage) - 1);
    strncpy(cfg.lastShow, ShowduinoFileUtil::jsonGetString(json, "lastShow", "").c_str(), sizeof(cfg.lastShow) - 1);
    cfg.autoConnect = ShowduinoFileUtil::jsonGetBool(json, "autoConnect", true);
    cfg.autoSave = ShowduinoFileUtil::jsonGetBool(json, "autoSave", true);
    cfg.saveIntervalSeconds = (uint16_t)ShowduinoFileUtil::jsonGetLong(json, "saveIntervalSeconds", 15);
    strncpy(cfg.logLevel, ShowduinoFileUtil::jsonGetString(json, "logLevel", "info").c_str(), sizeof(cfg.logLevel) - 1);
    cfg.screenTimeoutMinutes = (uint8_t)ShowduinoFileUtil::jsonGetLong(json, "screenTimeoutMinutes", 10);
    cfg.emergencyResetRequiresPin = ShowduinoFileUtil::jsonGetBool(json, "emergencyResetRequiresPin", true);
    cfg.saveLogMode = (SaveLogMode)ShowduinoFileUtil::jsonGetLong(json, "saveLogMode", (long)SaveLogMode::Normal);
    cfg.commsLogMode = (CommsLogMode)ShowduinoFileUtil::jsonGetLong(json, "commsLogMode", (long)CommsLogMode::CommandsAndAcks);
    cfg.maxLogAgeDays = (uint16_t)ShowduinoFileUtil::jsonGetLong(json, "maxLogAgeDays", STORAGE_LOG_MAX_AGE_DAYS);
    cfg.maxLogStorageMb = (uint32_t)ShowduinoFileUtil::jsonGetLong(json, "maxLogStorageMb", 100);
    /* Show-operation preferences — safe defaults if absent (backward-compatible) */
    cfg.confirmBeforeStart    = ShowduinoFileUtil::jsonGetBool(json, "confirmBeforeStart",    false);
    cfg.confirmBeforeStop     = ShowduinoFileUtil::jsonGetBool(json, "confirmBeforeStop",     false);
    cfg.autoOpenLiveAfterLoad = ShowduinoFileUtil::jsonGetBool(json, "autoOpenLiveAfterLoad", false);

    if (cfg.schemaVersion < STORAGE_SCHEMA_VERSION) {
      Serial.printf("[Config] migrating schema %u -> %u\n", cfg.schemaVersion, STORAGE_SCHEMA_VERSION);
      cfg.schemaVersion = STORAGE_SCHEMA_VERSION;
      createConfigBackup("migration");
      return save(cfg);
    }

    // Mirror settings.json + version.json for the required tree.
    ensureSidecars(cfg);
    Serial.println("[Config] Director configuration loaded");
    dirty = false;
    return true;
  }

  bool save(const DirectorConfig &cfg) {
    String json = serialize(cfg);
    bool ok = ShowduinoFileUtil::atomicWriteTextFile(PATH_DIRECTOR_JSON, json);
    if (ok) {
      ensureSidecars(cfg);
      dirty = false;
      Serial.println("[Config] Director configuration saved");
    } else {
      Serial.println("[Config] save FAILED");
    }
    return ok;
  }

  bool resetToDefaults(DirectorConfig &cfg) {
    createConfigBackup("reset");
    setDefaults(cfg);
    return save(cfg);
  }

  bool createConfigBackup(const char *reason) {
    char stamp[40];
    ShowduinoFileUtil::formatIsoTimestamp(stamp, sizeof(stamp));
    char path[STORAGE_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "/showduino/backups/automatic/config_%s_%s.json",
             reason ? reason : "auto", stamp);
    for (size_t i = 0; i < strlen(path); i++) {
      if (path[i] == ':' || path[i] == '+') path[i] = '-';
    }

    String body;
    if (!ShowduinoFileUtil::readTextFile(PATH_DIRECTOR_JSON, body)) {
      body = "{}";
    }
    return ShowduinoFileUtil::atomicWriteTextFile(path, body);
  }

  bool restoreConfigBackup(const char *backupPath, DirectorConfig &cfg) {
    if (!ShowduinoFileUtil::pathLooksSafe(backupPath)) return false;
    String body;
    if (!ShowduinoFileUtil::readTextFile(backupPath, body)) return false;
    createConfigBackup("prerestore");
    if (!ShowduinoFileUtil::atomicWriteTextFile(PATH_DIRECTOR_JSON, body)) return false;
    return load(cfg);
  }

  void markDirty() { dirty = true; dirtySinceMs = millis(); }
  bool isDirty() const { return dirty; }
  unsigned long dirtyAgeMs() const { return dirty ? (millis() - dirtySinceMs) : 0; }

private:
  bool dirty = false;
  unsigned long dirtySinceMs = 0;

  String serialize(const DirectorConfig &cfg) {
    String j = "{\n";
    j += "  \"schemaVersion\": " + String(cfg.schemaVersion) + ",\n";
    j += "  \"directorId\": \"" + ShowduinoFileUtil::jsonEscape(cfg.directorId) + "\",\n";
    j += "  \"directorName\": \"" + ShowduinoFileUtil::jsonEscape(cfg.directorName) + "\",\n";
    j += "  \"brightness\": " + String(cfg.brightness) + ",\n";
    j += "  \"volume\": " + String(cfg.volume) + ",\n";
    j += "  \"lastPage\": \"" + ShowduinoFileUtil::jsonEscape(cfg.lastPage) + "\",\n";
    j += "  \"lastShow\": \"" + ShowduinoFileUtil::jsonEscape(cfg.lastShow) + "\",\n";
    j += "  \"autoConnect\": " + String(cfg.autoConnect ? "true" : "false") + ",\n";
    j += "  \"autoSave\": " + String(cfg.autoSave ? "true" : "false") + ",\n";
    j += "  \"saveIntervalSeconds\": " + String(cfg.saveIntervalSeconds) + ",\n";
    j += "  \"logLevel\": \"" + ShowduinoFileUtil::jsonEscape(cfg.logLevel) + "\",\n";
    j += "  \"screenTimeoutMinutes\": " + String(cfg.screenTimeoutMinutes) + ",\n";
    j += "  \"emergencyResetRequiresPin\": " + String(cfg.emergencyResetRequiresPin ? "true" : "false") + ",\n";
    j += "  \"saveLogMode\": " + String((int)cfg.saveLogMode) + ",\n";
    j += "  \"commsLogMode\": " + String((int)cfg.commsLogMode) + ",\n";
    j += "  \"maxLogAgeDays\": " + String(cfg.maxLogAgeDays) + ",\n";
    j += "  \"maxLogStorageMb\": " + String(cfg.maxLogStorageMb) + ",\n";
    j += "  \"confirmBeforeStart\": " + String(cfg.confirmBeforeStart ? "true" : "false") + ",\n";
    j += "  \"confirmBeforeStop\": " + String(cfg.confirmBeforeStop ? "true" : "false") + ",\n";
    j += "  \"autoOpenLiveAfterLoad\": " + String(cfg.autoOpenLiveAfterLoad ? "true" : "false") + "\n";
    j += "}\n";
    return j;
  }

  void ensureSidecars(const DirectorConfig &cfg) {
    String settings = "{\n  \"schemaVersion\": 1,\n  \"saveLogMode\": " + String((int)cfg.saveLogMode) +
                      ",\n  \"autoSave\": " + String(cfg.autoSave ? "true" : "false") +
                      ",\n  \"saveIntervalSeconds\": " + String(cfg.saveIntervalSeconds) + "\n}\n";
    ShowduinoFileUtil::atomicWriteTextFile(PATH_SETTINGS_JSON, settings);

    String version = "{\n  \"schemaVersion\": 1,\n  \"firmware\": \"" + String(STORAGE_FW_VERSION) +
                     "\",\n  \"storageSchema\": " + String(STORAGE_SCHEMA_VERSION) + "\n}\n";
    ShowduinoFileUtil::atomicWriteTextFile(PATH_VERSION_JSON, version);

    if (!SD.exists(PATH_NETWORK_JSON)) {
      ShowduinoFileUtil::atomicWriteTextFile(PATH_NETWORK_JSON,
        "{\n  \"schemaVersion\": 1,\n  \"mode\": \"espnow\",\n  \"channel\": 1\n}\n");
    }
    if (!SD.exists(PATH_STARTUP_JSON)) {
      ShowduinoFileUtil::atomicWriteTextFile(PATH_STARTUP_JSON,
        "{\n  \"schemaVersion\": 1,\n  \"lastBootOk\": true\n}\n");
    }
    if (!SD.exists(PATH_STORAGE_JSON)) {
      ShowduinoFileUtil::atomicWriteTextFile(PATH_STORAGE_JSON,
        "{\n  \"schemaVersion\": 1,\n  \"root\": \"/showduino\"\n}\n");
    }
  }
};

#endif
