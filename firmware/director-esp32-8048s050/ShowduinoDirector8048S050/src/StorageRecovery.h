#ifndef SHOWDUINO_STORAGE_RECOVERY_H
#define SHOWDUINO_STORAGE_RECOVERY_H

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include "StorageConfig.h"
#include "FileUtil.h"
#include "SDManager.h"

class StorageRecovery {
public:
  void beginBootMarker() {
    prefs.begin("showduino", false);
    bootAttempts = prefs.getUInt("bootAttempts", 0) + 1;
    prefs.putUInt("bootAttempts", bootAttempts);
    prefs.putBool("startupOk", false);
    dirtyShutdown = !prefs.getBool("cleanExit", true);
    prefs.putBool("cleanExit", false);
    lastResetReason = (int)esp_reset_reason();
    Serial.printf("[Recovery] bootAttempts=%u dirtyShutdown=%d reset=%d\n",
                  bootAttempts, (int)dirtyShutdown, lastResetReason);
  }

  void markStartupComplete() {
    prefs.putBool("startupOk", true);
    prefs.putUInt("bootAttempts", 0);
    Serial.println("[Recovery] startupComplete=true");
  }

  void markCleanShutdown() {
    prefs.putBool("cleanExit", true);
    prefs.end();
  }

  bool interruptedStartup() const {
    return bootAttempts > 2 || dirtyShutdown;
  }

  bool recoverPendingWrites() {
    Serial.println("[Storage] Recovering temporary files");
    bool ok = true;
    ok &= ShowduinoFileUtil::recoverAtomicFile(PATH_DIRECTOR_JSON);
    ok &= ShowduinoFileUtil::recoverAtomicFile(PATH_SETTINGS_JSON);
    ok &= ShowduinoFileUtil::recoverAtomicFile(PATH_NETWORK_JSON);
    ok &= ShowduinoFileUtil::recoverAtomicFile(PATH_PAIRED_DEVICES);
    ok &= ShowduinoFileUtil::recoverAtomicFile(PATH_SHOW_INDEX);
    ok &= ShowduinoFileUtil::recoverAtomicFile(PATH_STARTUP_JSON);
    ok &= ShowduinoFileUtil::recoverAtomicFile(PATH_STORAGE_JSON);
    return ok;
  }

  bool writeCrashReport(SDManager &sd, const char *lastPage, const char *lastShow,
                        const char *lastCommand) {
    if (!sd.getStatus().mounted) return false;

    char stamp[40];
    ShowduinoFileUtil::formatIsoTimestamp(stamp, sizeof(stamp));
    char path[STORAGE_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "/showduino/logs/crashes/crash_%lu.json", millis());

    String json = "{\n";
    json += "  \"schemaVersion\": 1,\n";
    json += "  \"stamp\": \"" + String(stamp) + "\",\n";
    json += "  \"resetReason\": " + String(lastResetReason) + ",\n";
    json += "  \"bootAttempts\": " + String(bootAttempts) + ",\n";
    json += "  \"dirtyShutdown\": " + String(dirtyShutdown ? "true" : "false") + ",\n";
    json += "  \"lastPage\": \"" + ShowduinoFileUtil::jsonEscape(String(lastPage ? lastPage : "")) + "\",\n";
    json += "  \"lastShow\": \"" + ShowduinoFileUtil::jsonEscape(String(lastShow ? lastShow : "")) + "\",\n";
    json += "  \"lastCommand\": \"" + ShowduinoFileUtil::jsonEscape(String(lastCommand ? lastCommand : "")) + "\",\n";
    json += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM)
    json += "  \"psramSize\": " + String(ESP.getPsramSize()) + ",\n";
    json += "  \"freePsram\": " + String(ESP.getFreePsram()) + ",\n";
#endif
    json += "  \"largestFreeBlock\": " + String(ESP.getMaxAllocHeap()) + ",\n";
    json += "  \"firmware\": \"" + String(STORAGE_FW_VERSION) + "\",\n";
    json += "  \"sdMounted\": " + String(sd.getStatus().mounted ? "true" : "false") + ",\n";
    json += "  \"cardType\": \"" + String(sd.getStatus().cardType) + "\",\n";
    json += "  \"freeBytes\": " + String((unsigned long)sd.getStatus().freeBytes) + "\n";
    json += "}\n";

    bool ok = ShowduinoFileUtil::atomicWriteTextFile(path, json);
    Serial.printf("[Recovery] crash report %s -> %s\n", path, ok ? "OK" : "FAIL");
    return ok;
  }

  uint32_t getBootAttempts() const { return bootAttempts; }
  int getResetReason() const { return lastResetReason; }

private:
  Preferences prefs;
  uint32_t bootAttempts = 0;
  bool dirtyShutdown = false;
  int lastResetReason = 0;
};

#endif
