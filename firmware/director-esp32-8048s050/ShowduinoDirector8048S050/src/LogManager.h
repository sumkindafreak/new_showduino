#ifndef SHOWDUINO_LOG_MANAGER_H
#define SHOWDUINO_LOG_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include "StorageConfig.h"
#include "StorageTypes.h"
#include "FileUtil.h"
#include "SDManager.h"

class LogManager {
public:
  void begin(SDManager *sdIn, SaveLogMode modeIn) {
    sd = sdIn;
    mode = modeIn;
    enabled = sd && sd->getStatus().mounted;
    openSystemLog();
    if (sd) sd->getStatus().loggingEnabled = enabled;
  }

  void setMode(SaveLogMode m) { mode = m; }
  void setCommsMode(CommsLogMode m) { commsMode = m; }
  SaveLogMode getMode() const { return mode; }
  CommsLogMode getCommsMode() const { return commsMode; }

  void logEvent(LogLevel level, LogCategory category, const char *source, const char *message) {
    if (!passesFilter(level, category)) {
      // Still print critical/errors to Serial.
      if (level >= LogLevel::Error) {
        Serial.printf("[LOG] %s | %s\n", source ? source : "?", message ? message : "");
      }
      return;
    }

    char stamp[40];
    ShowduinoFileUtil::formatIsoTimestamp(stamp, sizeof(stamp));
    char line[STORAGE_MAX_LOG_LINE];
    snprintf(line, sizeof(line), "%s | %s | %s | %s | %s",
             stamp, levelName(level), categoryName(category),
             source ? source : "-", message ? message : "");

    Serial.println(line);

    if (!enabled || !sd || !sd->getStatus().mounted) {
      enqueueRam(line);
      return;
    }

    const char *path = pathFor(category);
    appendLine(path, line, level >= LogLevel::Error || level == LogLevel::Critical ||
                               category == LogCategory::Emergency);
  }

  void logEmergency(const char *source, const char *message,
                    const char *activeShow = nullptr, uint16_t currentCue = 0) {
    char stamp[40];
    ShowduinoFileUtil::formatIsoTimestamp(stamp, sizeof(stamp));
    char path[STORAGE_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "/showduino/logs/emergency/emergency_%lu.json", millis());

    String body = "{\n";
    body += "  \"schemaVersion\": 1,\n";
    body += "  \"stamp\": \"" + String(stamp) + "\",\n";
    body += "  \"triggerSource\": \"" + ShowduinoFileUtil::jsonEscape(String(source ? source : "UI")) + "\",\n";
    body += "  \"message\": \"" + ShowduinoFileUtil::jsonEscape(String(message ? message : "EMERGENCY:STOP")) + "\",\n";
    body += "  \"activeShow\": \"" + ShowduinoFileUtil::jsonEscape(String(activeShow ? activeShow : activeShowId)) + "\",\n";
    body += "  \"currentCue\": " + String(currentCue) + ",\n";
    body += "  \"outputsShutdown\": true\n";
    body += "}\n";

    ShowduinoFileUtil::atomicWriteTextFile(path, body);
    if (showLogOpen) noteShowEStop();
    logEvent(LogLevel::Critical, LogCategory::Emergency, source, message);
  }

  bool startShowLog(const char *showId) {
    if (!enabled || !showId) return false;
    char stamp[40];
    ShowduinoFileUtil::formatIsoTimestamp(stamp, sizeof(stamp));
    for (size_t i = 0; i < strlen(stamp); i++) {
      if (stamp[i] == ':' || stamp[i] == '+') stamp[i] = '-';
    }
    snprintf(activeShowLog, sizeof(activeShowLog),
             "/showduino/logs/shows/%s_%s.log", showId, stamp);
    showLogOpen = true;
    showStartMs = millis();
    cuesTriggered = warnings = errors = overrides = eStops = 0;
    strncpy(activeShowId, showId, sizeof(activeShowId) - 1);

    String hdr = String("SHOW_SESSION_START id=") + showId + "\n";
    appendLine(activeShowLog, hdr.c_str(), true);
    logEvent(LogLevel::Event, LogCategory::Show, showId, "Show started");
    return true;
  }

  void noteShowCue() { cuesTriggered++; }
  void noteShowWarning() { warnings++; }
  void noteShowError() { errors++; }
  void noteShowOverride() { overrides++; }
  void noteShowEStop() { eStops++; }

  void endShowLog(const char *result) {
    if (!showLogOpen) return;
    unsigned long dur = (millis() - showStartMs) / 1000UL;
    String summary = "{\n";
    summary += "  \"showId\": \"" + ShowduinoFileUtil::jsonEscape(activeShowId) + "\",\n";
    summary += "  \"result\": \"" + ShowduinoFileUtil::jsonEscape(result ? result : "unknown") + "\",\n";
    summary += "  \"durationSeconds\": " + String(dur) + ",\n";
    summary += "  \"cuesTriggered\": " + String(cuesTriggered) + ",\n";
    summary += "  \"warnings\": " + String(warnings) + ",\n";
    summary += "  \"errors\": " + String(errors) + ",\n";
    summary += "  \"manualOverrides\": " + String(overrides) + ",\n";
    summary += "  \"emergencyStops\": " + String(eStops) + "\n";
    summary += "}\n";
    appendLine(activeShowLog, summary.c_str(), true);
    logEvent(LogLevel::Event, LogCategory::Show, activeShowId, result ? result : "ended");
    showLogOpen = false;
  }

  void flush() {
    // FILE_APPEND writes are flushed on critical; normal path uses periodic flush marker.
    lastFlushMs = millis();
  }

  void process(bool criticalSpace) {
    if (criticalSpace && mode > SaveLogMode::ErrorsOnly) {
      mode = SaveLogMode::ErrorsOnly;
    }
    if (millis() - lastFlushMs >= STORAGE_LOG_FLUSH_NORMAL_MS) flush();
    if (commsDebugUntilMs && millis() > commsDebugUntilMs) {
      commsMode = CommsLogMode::CommandsAndAcks;
      commsDebugUntilMs = 0;
      logEvent(LogLevel::Info, LogCategory::Communication, "LogManager", "Full packet debug auto-disabled");
    }
  }

  void enableFullPacketDebug(uint32_t durationMs = STORAGE_PACKET_DEBUG_MS) {
    commsMode = CommsLogMode::FullPacketDebug;
    commsDebugUntilMs = millis() + durationMs;
  }

  void flushRamQueueToSd() {
    if (!sd || !sd->getStatus().mounted) return;
    for (uint8_t i = 0; i < ramCount; i++) {
      appendLine(pathFor(LogCategory::System), ramQueue[i].c_str(), true);
    }
    ramCount = 0;
  }

  const char *activeSystemLogPath() const { return systemLogPath; }

private:
  SDManager *sd = nullptr;
  SaveLogMode mode = SaveLogMode::Normal;
  CommsLogMode commsMode = CommsLogMode::CommandsAndAcks;
  bool enabled = false;
  char systemLogPath[STORAGE_MAX_PATH_LEN] = {};
  char activeShowLog[STORAGE_MAX_PATH_LEN] = {};
  char activeShowId[64] = {};
  bool showLogOpen = false;
  unsigned long showStartMs = 0;
  unsigned long lastFlushMs = 0;
  unsigned long commsDebugUntilMs = 0;
  uint16_t cuesTriggered = 0, warnings = 0, errors = 0, overrides = 0, eStops = 0;
  String ramQueue[STORAGE_RAM_LOG_QUEUE];
  uint8_t ramCount = 0;

  void openSystemLog() {
    snprintf(systemLogPath, sizeof(systemLogPath),
             "/showduino/logs/system/system_%lu.log", millis() / 1000UL);
    if (sd) {
      strncpy(sd->getStatus().activeSystemLog, systemLogPath, sizeof(sd->getStatus().activeSystemLog) - 1);
    }
  }

  bool passesFilter(LogLevel level, LogCategory category) {
    if (mode == SaveLogMode::Off) return false;
    if (mode == SaveLogMode::ErrorsOnly) return level >= LogLevel::Error;
    if (mode == SaveLogMode::ShowEvents)
      return category == LogCategory::Show || category == LogCategory::Cue ||
             category == LogCategory::Emergency || level >= LogLevel::Warning;
    if (mode == SaveLogMode::Normal) return level >= LogLevel::Info;
    if (mode == SaveLogMode::Detailed) return level >= LogLevel::Debug;
    return true;  // DebugMode
  }

  const char *pathFor(LogCategory c) {
    switch (c) {
      case LogCategory::Communication: return "/showduino/logs/communication/comms.log";
      case LogCategory::Show:
      case LogCategory::Cue: return showLogOpen ? activeShowLog : "/showduino/logs/shows/shows.log";
      case LogCategory::WarningCat: return "/showduino/logs/warnings/warnings.log";
      case LogCategory::Emergency: return "/showduino/logs/emergency/emergency.log";
      case LogCategory::Crash: return "/showduino/logs/crashes/crashes.log";
      default: return systemLogPath[0] ? systemLogPath : "/showduino/logs/system/system.log";
    }
  }

  void appendLine(const char *path, const char *line, bool forceFlush) {
    if (!ShowduinoFileUtil::pathLooksSafe(path)) return;
    ShowduinoFileUtil::ensureParentDirs(path);

    // Rotate if oversized.
    if (SD.exists(path)) {
      File probe = SD.open(path, FILE_READ);
      size_t sz = probe ? probe.size() : 0;
      if (probe) probe.close();
      if (sz > STORAGE_LOG_MAX_FILE_BYTES) {
        String rotated = String(path) + "." + String(millis());
        SD.rename(path, rotated.c_str());
      }
    }

    File f = SD.open(path, FILE_APPEND);
    if (!f) {
      enqueueRam(line);
      return;
    }
    f.println(line);
    if (forceFlush) f.flush();
    f.close();
  }

  void enqueueRam(const char *line) {
    if (ramCount >= STORAGE_RAM_LOG_QUEUE) {
      // Drop oldest.
      for (uint8_t i = 1; i < STORAGE_RAM_LOG_QUEUE; i++) ramQueue[i - 1] = ramQueue[i];
      ramCount = STORAGE_RAM_LOG_QUEUE - 1;
    }
    ramQueue[ramCount++] = String(line);
  }

  static const char *levelName(LogLevel l) {
    switch (l) {
      case LogLevel::Debug: return "DEBUG";
      case LogLevel::Info: return "INFO";
      case LogLevel::Event: return "EVENT";
      case LogLevel::Warning: return "WARNING";
      case LogLevel::Error: return "ERROR";
      case LogLevel::Critical: return "CRITICAL";
    }
    return "INFO";
  }

  static const char *categoryName(LogCategory c) {
    switch (c) {
      case LogCategory::System: return "SYSTEM";
      case LogCategory::Storage: return "STORAGE";
      case LogCategory::Communication: return "COMMUNICATION";
      case LogCategory::Device: return "DEVICE";
      case LogCategory::Show: return "SHOW";
      case LogCategory::Cue: return "CUE";
      case LogCategory::UserAction: return "USER";
      case LogCategory::WarningCat: return "WARNING";
      case LogCategory::Emergency: return "EMERGENCY";
      case LogCategory::Crash: return "CRASH";
      case LogCategory::Update: return "UPDATE";
    }
    return "SYSTEM";
  }
};

#endif
