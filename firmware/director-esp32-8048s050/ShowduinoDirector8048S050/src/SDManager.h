#ifndef SHOWDUINO_SD_MANAGER_H
#define SHOWDUINO_SD_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "StorageTypes.h"
#include "StorageConfig.h"
#include "FileUtil.h"

class SDManager {
public:
  bool begin(StorageStatusCallback cb = nullptr) {
    statusCb = cb;
    setStage(StorageBootStage::InitSpi, "Initialising SD card");

    /* Try progressively slower SPI clocks — cheap cards often fail writes at 10 MHz. */
    static const uint32_t kSpeeds[] = {
      STORAGE_SPI_HZ,
      4000000UL,
      1000000UL
    };

    bool mounted = false;
    for (uint8_t si = 0; si < 3; si++) {
      if (tryMountAtHz(kSpeeds[si])) {
        mounted = true;
        activeSpiHz = kSpeeds[si];
        break;
      }
      Serial.printf("[Storage] Mount/write failed at %lu Hz — retrying slower...\n",
                    (unsigned long)kSpeeds[si]);
      SD.end();
      delay(80);
    }

    if (!mounted) {
      status.mounted = false;
      status.writable = false;
      status.recoveryMode = true;
      setStage(StorageBootStage::RecoveryMode, "SD CARD NOT AVAILABLE");
      Serial.println("[Storage] HINT: reseat card; unlock write-protect; format FAT32 (not exFAT).");
      return false;
    }

    status.mounted = true;
    Serial.printf("[Storage] SD ready at %lu Hz  type=%s  size=%llu MB\n",
                  (unsigned long)activeSpiHz,
                  status.cardType,
                  (unsigned long long)(status.totalBytes / (1024ULL * 1024ULL)));

    setStage(StorageBootStage::ValidateFolders, "Validating folders");
    if (!ensureFolderStructure()) {
      setError("Folder structure repair failed");
      status.folderStructureValid = false;
      /* Still mounted — may be read-only or partially usable */
      if (!status.writable) {
        status.recoveryMode = true;
        setStage(StorageBootStage::RecoveryMode, "SD NOT WRITABLE");
        Serial.println("[Storage] HINT: unlock SD write-protect switch; format card as FAT32.");
        return false;
      }
    } else {
      status.folderStructureValid = true;
      Serial.println("[Storage] Validating folders — OK");
    }

    refreshSpace();
    Serial.printf("[Storage] Card %s  total=%llu MB  free=%llu MB  writable=%s\n",
                  status.cardType,
                  (unsigned long long)(status.totalBytes / (1024ULL * 1024ULL)),
                  (unsigned long long)(status.freeBytes / (1024ULL * 1024ULL)),
                  status.writable ? "yes" : "NO");
    return status.writable;
  }

  bool retryMount() {
    Serial.println("[Storage] Retrying SD mount...");
    SD.end();
    delay(100);
    return begin(nullptr);
  }

  bool safelyUnmount() {
    Serial.println("[Storage] Safely unmounting SD");
    status.mounted = false;
    status.writable = false;
    status.loggingEnabled = false;
    SD.end();
    strncpy(status.bootMessage, "SD unmounted", sizeof(status.bootMessage));
    notify();
    return true;
  }

  void refreshSpace() {
    if (!status.mounted) return;
    status.usedBytes = SD.usedBytes();
    if (status.totalBytes == 0) status.totalBytes = SD.cardSize();
    if (status.usedBytes > status.totalBytes) status.usedBytes = status.totalBytes;
    status.freeBytes = status.totalBytes - status.usedBytes;

    status.lowSpace = false;
    status.criticalSpace = false;
    if (status.totalBytes > 0) {
      uint32_t freePct = (uint32_t)((status.freeBytes * 100ULL) / status.totalBytes);
      status.lowSpace = freePct < STORAGE_LOW_SPACE_PCT;
      status.criticalSpace = freePct < STORAGE_CRITICAL_SPACE_PCT;
    }
  }

  bool ensureFolderStructure() {
    setStage(StorageBootStage::CreateFolders, "Creating missing folders");

    /* Fail fast: if the root app folder cannot be created, do not spam 50+ mkdirs. */
    if (!ShowduinoFileUtil::ensureDir(STORAGE_ROOT)) {
      Serial.printf("[Storage] failed mkdir %s (aborting folder tree)\n", STORAGE_ROOT);
      status.writable = false;
      return false;
    }

    bool allOk = true;
    uint8_t failCount = 0;
    for (uint8_t i = 0; STORAGE_REQUIRED_DIRS[i] != nullptr; i++) {
      if (!ShowduinoFileUtil::ensureDir(STORAGE_REQUIRED_DIRS[i])) {
        Serial.printf("[Storage] failed mkdir %s\n", STORAGE_REQUIRED_DIRS[i]);
        allOk = false;
        failCount++;
        if (failCount >= 3) {
          Serial.println("[Storage] Multiple mkdir failures — stopping folder create.");
          break;
        }
      }
    }
    status.writable = probeWritable();
    return allOk && status.writable;
  }

  StorageStatus &getStatus() { return status; }
  const StorageStatus &getStatus() const { return status; }

  void setStage(StorageBootStage stage, const char *msg) {
    status.stage = stage;
    strncpy(status.bootMessage, msg, sizeof(status.bootMessage) - 1);
    status.bootMessage[sizeof(status.bootMessage) - 1] = '\0';
    Serial.printf("[Storage] %s\n", msg);
    notify();
  }

  void setError(const char *err) {
    strncpy(status.lastError, err, sizeof(status.lastError) - 1);
    status.lastError[sizeof(status.lastError) - 1] = '\0';
    Serial.printf("[Storage] ERROR: %s\n", err);
    notify();
  }

  void markSaveState(SaveUiState s) {
    status.saveState = s;
    if (s == SaveUiState::Saved) status.lastSuccessfulSaveMs = millis();
    notify();
  }

private:
  StorageStatus status = {};
  StorageStatusCallback statusCb = nullptr;
  uint32_t activeSpiHz = STORAGE_SPI_HZ;

  void notify() {
    if (statusCb) statusCb(status);
  }

  bool tryMountAtHz(uint32_t hz) {
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    setStage(StorageBootStage::MountCard, "Mounting SD card");
    if (!SD.begin(SD_CS_PIN, SPI, hz)) {
      setError("SD mount failed");
      return false;
    }

    Serial.println("[Storage] SD card detected");
    setStage(StorageBootStage::DetectCard, "Detecting card type");
    uint8_t type = SD.cardType();
    if (type == CARD_NONE) {
      setError("No SD card attached");
      return false;
    }
    if (type == CARD_MMC) strncpy(status.cardType, "MMC", sizeof(status.cardType));
    else if (type == CARD_SD) strncpy(status.cardType, "SDSC", sizeof(status.cardType));
    else if (type == CARD_SDHC) strncpy(status.cardType, "SDHC", sizeof(status.cardType));
    else strncpy(status.cardType, "UNKNOWN", sizeof(status.cardType));

    setStage(StorageBootStage::ReadSize, "Reading card size");
    status.totalBytes = SD.cardSize();
    Serial.printf("[Storage] type=%s size=%llu MB @ %lu Hz\n",
                  status.cardType,
                  (unsigned long long)(status.totalBytes / (1024ULL * 1024ULL)),
                  (unsigned long)hz);

    /* Write probe BEFORE creating the folder tree. */
    if (!probeWritableRoot()) {
      setError("SD not writable");
      Serial.println("[Storage] Write probe failed (lock switch? exFAT? bad card?)");
      return false;
    }
    status.writable = true;
    return true;
  }

  bool probeWritableRoot() {
    const char *probe = "/.showduino_wr";
    if (SD.exists(probe)) SD.remove(probe);
    File f = SD.open(probe, FILE_WRITE);
    if (!f) return false;
    size_t n = f.print("ok");
    f.flush();
    f.close();
    if (n < 2) {
      SD.remove(probe);
      return false;
    }
    File r = SD.open(probe, FILE_READ);
    if (!r) {
      SD.remove(probe);
      return false;
    }
    String got = r.readString();
    r.close();
    SD.remove(probe);
    got.trim();
    return got == "ok";
  }

  bool probeWritable() {
    const char *probe = "/showduino/temp/.write_probe";
    if (!ShowduinoFileUtil::ensureDir("/showduino/temp")) {
      return probeWritableRoot();
    }
    bool ok = ShowduinoFileUtil::writeTextDirect(probe, "ok");
    if (ok) SD.remove(probe);
    return ok;
  }
};

#endif
