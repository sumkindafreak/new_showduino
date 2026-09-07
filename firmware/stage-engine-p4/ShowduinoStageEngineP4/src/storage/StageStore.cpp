#include "StageStore.h"

#include <string.h>
#include "../../BoardConfig.h"
#include "../StageStorage.h"
#include "../StageTime.h"
#include "StageConfig.h"
#include "StageLog.h"

static StageStoreStatus sSt;
static uint32_t sNextPollMs = 0;
static uint32_t sNextSpaceMs = 0;
static const uint32_t kPollMs = 2000;
static const uint32_t kSpaceMs = 10000;

static const char *const kExtraDirs[] = {
  PATH_AUDIO_SYSTEM,
  PATH_ASSETS,
  PATH_IMPORT,
  PATH_EXPORT,
  PATH_RECOVERY,
  PATH_BACKUPS,
  PATH_LOGS,
  PATH_SYSTEM_META,
  PATH_CONFIG,
  PATH_DIAGNOSTICS,
  PATH_PRODUCTIONS,
  nullptr
};

static const char *const kConfigFiles[] = {
  PATH_SYSTEM_CONFIG,
  PATH_NETWORK_CONFIG,
  PATH_E131_CONFIG,
  PATH_PLUGIN_BUS_CONFIG,
  PATH_PIXELS_CONFIG,
  PATH_DIRECTOR_CONFIG,
  nullptr
};

void stageStoreSetError(const char *message) {
  strncpy(sSt.lastError, message ? message : "", sizeof(sSt.lastError) - 1);
  sSt.lastError[sizeof(sSt.lastError) - 1] = '\0';
}

static void copyErr(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = '\0';
}

static void refreshState() {
  const StageStorageStatus &sd = stageStorageStatus();
  if (!sd.mounted || !stageStorageIsReady()) {
    sSt.state = (strstr(sd.message, "exFAT") || strstr(sd.message, "FAULT"))
                    ? SHOWDUINO_STORAGE_FAULT
                    : SHOWDUINO_STORAGE_OFFLINE;
    return;
  }
  if (!sd.writable || sSt.writesStopped) {
    sSt.state = SHOWDUINO_STORAGE_READ_ONLY;
    return;
  }
  if (!sd.folderOk || !sSt.configHealthOk || sSt.formatVersion == 0) {
    sSt.state = SHOWDUINO_STORAGE_DEGRADED;
    return;
  }
  sSt.state = SHOWDUINO_STORAGE_ONLINE;
}

bool stageStoreWritable() {
  return stageStorageIsReady() && stageStorageStatus().writable &&
         !sSt.writesStopped &&
         sSt.state != SHOWDUINO_STORAGE_OFFLINE &&
         sSt.state != SHOWDUINO_STORAGE_FAULT;
}

#if SHOWDUINO_SD_ENABLED

static bool ensureDir(const char *path) {
  if (showduino_storage_path_check(path) != SHOWDUINO_PATH_OK) return false;
  fs::FS &fs = stageStorageFs();
  if (fs.exists(path)) {
    File f = fs.open(path);
    const bool ok = f && f.isDirectory();
    if (f) f.close();
    return ok;
  }
  return fs.mkdir(path);
}

static bool copyFile(const char *from, const char *to) {
  if (!stageStorageIsReady()) return false;
  if (showduino_storage_path_check(from) != SHOWDUINO_PATH_OK) return false;
  if (showduino_storage_path_check(to) != SHOWDUINO_PATH_OK) return false;
  fs::FS &fs = stageStorageFs();
  File src = fs.open(from, FILE_READ);
  if (!src) return false;
  File dst = fs.open(to, FILE_WRITE);
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buf[256];
  bool ok = true;
  while (src.available()) {
    const int n = src.read(buf, sizeof(buf));
    if (n <= 0 || dst.write(buf, (size_t)n) != (size_t)n) {
      ok = false;
      break;
    }
  }
  dst.flush();
  dst.close();
  src.close();
  return ok;
}

static void ensureCanonicalDirs() {
  for (uint8_t i = 0; kExtraDirs[i]; i++) {
    if (!ensureDir(kExtraDirs[i])) {
      sSt.configHealthOk = false;
      stageStoreSetError("mkdir failed");
      Serial.printf("[STORAGE] mkdir failed: %s\n", kExtraDirs[i]);
    }
  }
}

static void refreshCounts() {
  sSt.productionCount = 0;
  sSt.backupCount = 0;
  sSt.logBytes = stageLogUsedBytes();
  if (!stageStorageIsReady()) return;
  fs::FS &fs = stageStorageFs();
  File prod = fs.open(PATH_PRODUCTIONS);
  if (prod && prod.isDirectory()) {
    File e = prod.openNextFile();
    while (e) {
      if (e.isDirectory()) sSt.productionCount++;
      e.close();
      e = prod.openNextFile();
    }
  }
  if (prod) prod.close();

  File backs = fs.open(PATH_BACKUPS);
  if (backs && backs.isDirectory()) {
    File e = backs.openNextFile();
    while (e) {
      if (e.isDirectory() && strncmp(e.name(), "config-", 7) == 0) sSt.backupCount++;
      e.close();
      e = backs.openNextFile();
    }
  }
  if (backs) backs.close();
}

static bool loadStorageVersion() {
  String json;
  if (!stageStoreReadText(PATH_STORAGE_VERSION, 256, json)) {
    const char *def = "{\n  \"formatVersion\": 1\n}\n";
    if (stageStoreAtomicWrite(PATH_STORAGE_VERSION, def, strlen(def))) {
      sSt.formatVersion = SHOWDUINO_STORAGE_FORMAT_VERSION;
      return true;
    }
    sSt.formatVersion = 0;
    stageStoreSetError("storage-version write failed");
    return false;
  }
  const ShowduinoConfigStatus st = showduino_storage_config_check(
      json.c_str(), json.length(), 256, SHOWDUINO_STORAGE_FORMAT_VERSION);
  if (st != SHOWDUINO_CFG_OK) {
    sSt.formatVersion = 0;
    stageStoreSetError("incompatible storage format");
    Serial.printf("[STORAGE] storage-version rejected (%s) — defaults remain in firmware\n",
                  showduino_storage_cfg_name(st));
    return false;
  }
  sSt.formatVersion = SHOWDUINO_STORAGE_FORMAT_VERSION;
  return true;
}

static void loadOrWriteLastBoot() {
  uint32_t count = 1;
  String json;
  if (stageStoreReadText(PATH_LAST_BOOT, 256, json)) {
    uint32_t prev = 0;
    if (showduino_storage_json_u32(json.c_str(), json.length(), "bootCount", &prev) &&
        prev < 1000000u) {
      count = prev + 1;
    }
  }
  sSt.bootCount = count;
  snprintf(sSt.sessionStamp, sizeof(sSt.sessionStamp), "boot-%lu",
           (unsigned long)count);

  char body[192];
  snprintf(body, sizeof(body),
           "{\n  \"formatVersion\": 1,\n  \"bootCount\": %lu,\n"
           "  \"storageState\": \"%s\",\n  \"uptimeAtWriteMs\": %lu\n}\n",
           (unsigned long)count,
           showduino_storage_state_name(sSt.state),
           (unsigned long)millis());
  (void)stageStoreAtomicWrite(PATH_LAST_BOOT, body, strlen(body));
}

bool stageStoreReadText(const char *path, size_t maxBytes, String &out) {
  out = "";
  if (!stageStorageIsReady()) return false;
  if (showduino_storage_path_check(path) != SHOWDUINO_PATH_OK) return false;
  fs::FS &fs = stageStorageFs();
  if (!fs.exists(path)) return false;
  File f = fs.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return false;
  }
  const size_t sz = f.size();
  if (sz > maxBytes) {
    f.close();
    stageStoreSetError("file too large");
    return false;
  }
  out.reserve(sz + 1);
  while (f.available()) {
    const int c = f.read();
    if (c < 0) break;
    out += (char)c;
  }
  f.close();
  return true;
}

bool stageStoreAtomicWrite(const char *path, const char *data, size_t len) {
  ShowduinoAtomicStep step = showduino_storage_atomic_next(
      SHOWDUINO_ATOMIC_IDLE, SHOWDUINO_ATOMIC_EV_START);

  if (!data || !stageStoreWritable()) {
    if (!sSt.writesStopped) stageStoreSetError("storage not writable");
    return false;
  }
  if (len > SHOWDUINO_STORAGE_CONFIG_MAX) {
    stageStoreSetError("config too large");
    return false;
  }

  const ShowduinoPathStatus pathSt = showduino_storage_path_check(path);
  step = showduino_storage_atomic_next(
      step, pathSt == SHOWDUINO_PATH_OK ? SHOWDUINO_ATOMIC_EV_PATH_OK
                                        : SHOWDUINO_ATOMIC_EV_PATH_BAD);
  if (step == SHOWDUINO_ATOMIC_FAIL) {
    stageStoreSetError("unsafe path");
    return false;
  }

  char tmpPath[SHOWDUINO_STORAGE_PATH_MAX + 1];
  char prevPath[SHOWDUINO_STORAGE_PATH_MAX + 1];
  if (showduino_storage_temp_path(path, tmpPath, sizeof(tmpPath)) != SHOWDUINO_PATH_OK) {
    stageStoreSetError("temp path rejected");
    return false;
  }
  if (showduino_storage_path_join(PATH_RECOVERY,
                                 showduino_storage_basename(path),
                                 prevPath, sizeof(prevPath)) != SHOWDUINO_PATH_OK) {
    copyErr(prevPath, sizeof(prevPath), "");
  } else {
    const size_t n = strlen(prevPath);
    if (n + 5 < sizeof(prevPath)) memcpy(prevPath + n, ".prev", 6);
  }

  fs::FS &fs = stageStorageFs();
  bool preserved = false;
  if (prevPath[0] && fs.exists(path)) {
    preserved = copyFile(path, prevPath);
  }
  step = showduino_storage_atomic_next(
      step, preserved ? SHOWDUINO_ATOMIC_EV_PRESERVE_OK
                      : SHOWDUINO_ATOMIC_EV_PRESERVE_FAIL);

  File f = fs.open(tmpPath, FILE_WRITE);
  const bool wrote = f && f.write((const uint8_t *)data, len) == len;
  step = showduino_storage_atomic_next(
      step, wrote ? SHOWDUINO_ATOMIC_EV_WRITE_OK : SHOWDUINO_ATOMIC_EV_WRITE_FAIL);
  if (!wrote) {
    if (f) f.close();
    fs.remove(tmpPath);
    sSt.writesStopped = true;
    stageStoreSetError("temp write failed");
    if (!stageStoragePollPresence()) {
      Serial.println("[STORAGE] Write failed — stopping further writes");
    }
    refreshState();
    return false;
  }
  f.flush();
  f.close();
  step = showduino_storage_atomic_next(step, SHOWDUINO_ATOMIC_EV_FLUSH_OK);

  File v = fs.open(tmpPath, FILE_READ);
  const bool valid = v && v.size() == len;
  if (v) v.close();
  step = showduino_storage_atomic_next(
      step, valid ? SHOWDUINO_ATOMIC_EV_VALIDATE_OK : SHOWDUINO_ATOMIC_EV_VALIDATE_FAIL);
  if (step == SHOWDUINO_ATOMIC_FAIL) {
    fs.remove(tmpPath);
    stageStoreSetError("temp validate failed");
    return false;
  }

  if (fs.exists(path)) fs.remove(path);
  const bool renamed = fs.rename(tmpPath, path);
  if (!renamed) {
    if (prevPath[0] && fs.exists(prevPath) && !fs.exists(path)) {
      (void)copyFile(prevPath, path);
    }
    fs.remove(tmpPath);
    stageStoreSetError("replace failed");
    sSt.configHealthOk = false;
    refreshState();
    return false;
  }
  step = showduino_storage_atomic_next(step, SHOWDUINO_ATOMIC_EV_REPLACE_OK);
  return step == SHOWDUINO_ATOMIC_DONE;
}

bool stageStoreBackupConfig(char *sessionOut, size_t sessionLen) {
  if (sessionOut && sessionLen) sessionOut[0] = '\0';
  if (!stageStoreWritable()) {
    stageStoreSetError("backup requires writable SD");
    return false;
  }
  char stamp[24];
  stageStoreSessionStamp(stamp, sizeof(stamp));
  char folderName[40];
  snprintf(folderName, sizeof(folderName), "config-%s", stamp);
  char dest[SHOWDUINO_STORAGE_PATH_MAX + 1];
  if (showduino_storage_path_join(PATH_BACKUPS, folderName, dest, sizeof(dest)) !=
      SHOWDUINO_PATH_OK) {
    stageStoreSetError("backup path rejected");
    return false;
  }
  if (!ensureDir(PATH_BACKUPS) || !ensureDir(dest)) {
    stageStoreSetError("backup mkdir failed");
    return false;
  }

  uint8_t copied = 0;
  for (uint8_t i = 0; kConfigFiles[i]; i++) {
    if (!stageStorageFs().exists(kConfigFiles[i])) continue;
    char to[SHOWDUINO_STORAGE_PATH_MAX + 1];
    if (showduino_storage_path_join(dest, showduino_storage_basename(kConfigFiles[i]),
                                    to, sizeof(to)) != SHOWDUINO_PATH_OK) {
      continue;
    }
    if (copyFile(kConfigFiles[i], to)) copied++;
  }
  if (sessionOut && sessionLen) copyErr(sessionOut, sessionLen, folderName);
  Serial.printf("[STORAGE] Backup %s files=%u\n", dest, (unsigned)copied);
  stageLogWrite(StageLogChannel::System, "INFO", "config backup");
  refreshCounts();
  (void)copied;
  return true;
}

bool stageStoreWriteDiagnostics(const char *basename, const char *body) {
  if (!body) return false;
  if (!stageStoreWritable()) return false;
  if (!showduino_storage_name_ok(basename)) return false;
  if (!ensureDir(PATH_DIAGNOSTICS)) return false;
  char path[SHOWDUINO_STORAGE_PATH_MAX + 1];
  if (showduino_storage_path_join(PATH_DIAGNOSTICS, basename, path, sizeof(path)) !=
      SHOWDUINO_PATH_OK) {
    return false;
  }
  return stageStoreAtomicWrite(path, body, strlen(body));
}

void stageStoreSessionStamp(char *out, size_t n) {
  if (!out || n == 0) return;
  if (stageTimeSynced()) {
    char date[16];
    stageTimeDate(date, sizeof(date));
    snprintf(out, n, "%s-b%lu", date[0] ? date : "session",
             (unsigned long)sSt.bootCount);
  } else {
    snprintf(out, n, "boot-%lu", (unsigned long)(sSt.bootCount ? sSt.bootCount : 1));
  }
}

void stageStoreBegin() {
  sSt = StageStoreStatus();
  strncpy(sSt.lastError, "", sizeof(sSt.lastError) - 1);
  if (!stageStorageIsReady()) {
    sSt.state = SHOWDUINO_STORAGE_OFFLINE;
    stageStoreSetError(stageStorageStatus().message[0] ? stageStorageStatus().message
                                                       : "SD OFFLINE");
    Serial.println("[STORAGE] SD OFFLINE — firmware defaults active");
    Serial.println("[STORAGE] Boot, Comms, Director, and emergency remain available");
    return;
  }

  ensureCanonicalDirs();
  refreshState();
  stageLogBegin();
  (void)loadStorageVersion();
  stageConfigBegin();
  loadOrWriteLastBoot();
  refreshCounts();
  sSt.configHealthOk = stageConfigFileHealthy(PATH_SYSTEM_CONFIG, SHOWDUINO_STORAGE_SYSTEM_MAX) &&
                       (sSt.formatVersion == SHOWDUINO_STORAGE_FORMAT_VERSION);
  refreshState();
  sNextPollMs = millis() + kPollMs;
  sNextSpaceMs = millis() + kSpaceMs;
  Serial.printf("[STORAGE] state=%s format=%u boot=%lu writable=%s\n",
                stageStoreStateName(),
                (unsigned)sSt.formatVersion,
                (unsigned long)sSt.bootCount,
                stageStoreWritable() ? "yes" : "no");
  stageLogWrite(StageLogChannel::System, "INFO", "storage online");
}

void stageStoreLoop() {
  const uint32_t now = millis();
  stageLogLoop();
  if (!stageStorageIsReady()) {
    if (sSt.state == SHOWDUINO_STORAGE_ONLINE ||
        sSt.state == SHOWDUINO_STORAGE_DEGRADED ||
        sSt.state == SHOWDUINO_STORAGE_READ_ONLY) {
      sSt.writesStopped = true;
      sSt.state = SHOWDUINO_STORAGE_OFFLINE;
      stageStoreSetError("SD OFFLINE");
      Serial.println("[STORAGE] SD OFFLINE — RAM timeline and emergency continue");
    }
    return;
  }
  if ((int32_t)(now - sNextPollMs) >= 0) {
    sNextPollMs = now + kPollMs;
    if (!stageStoragePollPresence()) {
      sSt.writesStopped = true;
      refreshState();
    }
  }
  if (stageStorageIsReady() && (int32_t)(now - sNextSpaceMs) >= 0) {
    sNextSpaceMs = now + kSpaceMs;
    stageStorageRefreshSpace();
    refreshState();
  }
}

void stageStorePrintStatus() {
  const StageStorageStatus &sd = stageStorageStatus();
  Serial.println("[STORAGE]");
  Serial.printf("State: %s\n", stageStoreStateName());
  Serial.printf("Mounted: %s writable=%s\n",
                sd.mounted ? "yes" : "no",
                stageStoreWritable() ? "yes" : "no");
  Serial.printf("Card: %s total=%llu MB free=%llu MB\n",
                sd.cardType,
                (unsigned long long)(sd.totalBytes / (1024ULL * 1024ULL)),
                (unsigned long long)(sd.freeBytes / (1024ULL * 1024ULL)));
  Serial.printf("Format: %u boot=%lu productions=%u backups=%u\n",
                (unsigned)sSt.formatVersion,
                (unsigned long)sSt.bootCount,
                (unsigned)sSt.productionCount,
                (unsigned)sSt.backupCount);
  Serial.printf("Logs: %lu bytes config=%s\n",
                (unsigned long)sSt.logBytes,
                sSt.configHealthOk ? "ok" : "degraded");
  if (sSt.lastError[0]) Serial.printf("Last error: %s\n", sSt.lastError);
}

void stageStorePrintList() {
  Serial.println("[STORAGE] LIST");
  if (!stageStorageIsReady()) {
    Serial.println("SD OFFLINE");
    return;
  }
  fs::FS &fs = stageStorageFs();
  for (uint8_t i = 0; kConfigFiles[i]; i++) {
    Serial.printf("CONFIG %s %s\n",
                  showduino_storage_basename(kConfigFiles[i]),
                  fs.exists(kConfigFiles[i]) ? "PRESENT" : "ABSENT");
  }
  File prod = fs.open(PATH_PRODUCTIONS);
  if (prod && prod.isDirectory()) {
    File e = prod.openNextFile();
    uint8_t n = 0;
    while (e && n < 32) {
      if (e.isDirectory()) {
        Serial.printf("PRODUCTION %s\n", e.name());
        n++;
      }
      e.close();
      e = prod.openNextFile();
    }
  }
  if (prod) prod.close();
  Serial.printf("BACKUPS %u\n", (unsigned)sSt.backupCount);
}

void stageStorePrintCheck() {
  Serial.println("[STORAGE] CHECK");
  if (!stageStorageIsReady()) {
    Serial.println("RESULT OFFLINE");
    return;
  }
  bool ok = sSt.formatVersion == SHOWDUINO_STORAGE_FORMAT_VERSION;
  Serial.printf("FORMAT %s\n", ok ? "OK" : "REJECTED");
  for (uint8_t i = 0; kConfigFiles[i]; i++) {
    if (!stageStorageFs().exists(kConfigFiles[i])) {
      Serial.printf("FILE %s ABSENT (defaults)\n",
                    showduino_storage_basename(kConfigFiles[i]));
      continue;
    }
    const bool healthy = stageConfigFileHealthy(kConfigFiles[i], SHOWDUINO_STORAGE_CONFIG_MAX);
    Serial.printf("FILE %s %s\n",
                  showduino_storage_basename(kConfigFiles[i]),
                  healthy ? "OK" : "REJECTED");
    if (!healthy) ok = false;
  }
  sSt.configHealthOk = ok;
  refreshState();
  Serial.printf("RESULT %s\n", ok ? "OK" : "DEGRADED");
}

bool stageStoreHandleCommand(const String &command) {
  if (command == "STORAGE:STATUS") {
    stageStorePrintStatus();
    return true;
  }
  if (command == "STORAGE:LIST") {
    refreshCounts();
    stageStorePrintList();
    return true;
  }
  if (command == "STORAGE:CHECK") {
    stageStorePrintCheck();
    return true;
  }
  if (command == "STORAGE:BACKUP") {
    char session[40] = "";
    if (!stageStoreBackupConfig(session, sizeof(session))) return false;
    Serial.printf("[STORAGE] BACKUP %s\n", session[0] ? session : "ok");
    return true;
  }
  return false;
}

#else /* !SHOWDUINO_SD_ENABLED */

bool stageStoreReadText(const char *, size_t, String &out) {
  out = "";
  return false;
}
bool stageStoreAtomicWrite(const char *, const char *, size_t) { return false; }
bool stageStoreBackupConfig(char *sessionOut, size_t sessionLen) {
  if (sessionOut && sessionLen) sessionOut[0] = '\0';
  return false;
}
bool stageStoreWriteDiagnostics(const char *, const char *) { return false; }
void stageStoreSessionStamp(char *out, size_t n) {
  if (out && n) snprintf(out, n, "boot-0");
}
void stageStoreBegin() {
  sSt.state = SHOWDUINO_STORAGE_OFFLINE;
  stageStoreSetError("SD disabled in BoardConfig");
}
void stageStoreLoop() {}
void stageStorePrintStatus() { Serial.println("[STORAGE] OFFLINE"); }
void stageStorePrintList() { Serial.println("[STORAGE] OFFLINE"); }
void stageStorePrintCheck() { Serial.println("[STORAGE] OFFLINE"); }
bool stageStoreHandleCommand(const String &command) {
  if (command.startsWith("STORAGE:")) {
    Serial.println("[STORAGE] OFFLINE");
    return true;
  }
  return false;
}

#endif

const StageStoreStatus &stageStoreStatus() { return sSt; }
ShowduinoStorageState stageStoreState() { return sSt.state; }
const char *stageStoreStateName() { return showduino_storage_state_name(sSt.state); }

void stageStoreNoteLastProduction(const char *id) {
  stageConfigSetLastProduction(id);
}

void stageStoreAppendJson(String &json) {
  const StageStorageStatus &sd = stageStorageStatus();
  json += "{\n";
  json += "    \"mounted\": ";
  json += sd.mounted ? "true" : "false";
  json += ",\n    \"writable\": ";
  json += stageStoreWritable() ? "true" : "false";
  json += ",\n    \"state\": \"";
  json += stageStoreStateName();
  json += "\",\n    \"formatVersion\": ";
  json += String((unsigned)sSt.formatVersion);
  json += ",\n    \"cardType\": \"";
  json += sd.cardType;
  json += "\",\n    \"totalBytes\": ";
  json += String((unsigned long)sd.totalBytes);
  json += ",\n    \"freeBytes\": ";
  json += String((unsigned long)sd.freeBytes);
  json += ",\n    \"usedBytes\": ";
  json += String((unsigned long)(sd.totalBytes > sd.freeBytes ? sd.totalBytes - sd.freeBytes : 0));
  json += ",\n    \"productionCount\": ";
  json += String((unsigned)sSt.productionCount);
  json += ",\n    \"backupCount\": ";
  json += String((unsigned)sSt.backupCount);
  json += ",\n    \"logBytes\": ";
  json += String((unsigned long)sSt.logBytes);
  json += ",\n    \"configHealth\": \"";
  json += sSt.configHealthOk ? "ok" : "degraded";
  json += "\",\n    \"lastError\": \"";
  json += sSt.lastError;
  json += "\",\n    \"webuiOnSd\": false,\n    \"webuiHost\": \"comms-s3\",\n";
  json += "    \"root\": \"/showduino\",\n";
  json += "    \"note\": \"SD is the persistent backbone, not the safety backbone.\"\n";
  json += "  }";
}
