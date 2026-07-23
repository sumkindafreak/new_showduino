#include "StageStorage.h"

#if SHOWDUINO_SD_ENABLED

#include <SPI.h>
#include <SD.h>

static StageStorageStatus sStatus;
static unsigned long sLastRetryMs = 0;
static const unsigned long kRetryMs = 15000UL;

static const char *const kRequiredDirs[] = {
  "/showduino",
  "/showduino/www",
  "/showduino/www/css",
  "/showduino/www/js",
  "/showduino/www/js/components",
  "/showduino/www/js/pages",
  "/showduino/system",
  "/showduino/shows",
  "/showduino/shows/packages",
  "/showduino/shows/trash",
  "/showduino/devices",
  "/showduino/devices/fixture_profiles",
  "/showduino/devices/presets",
  "/showduino/logs",
  "/showduino/logs/system",
  "/showduino/logs/communication",
  "/showduino/logs/shows",
  "/showduino/logs/warnings",
  "/showduino/logs/emergency",
  "/showduino/logs/crashes",
  "/showduino/backups",
  "/showduino/backups/automatic",
  "/showduino/backups/manual",
  "/showduino/exports",
  "/showduino/exports/diagnostics",
  "/showduino/exports/logs",
  "/showduino/exports/shows",
  "/showduino/exports/configuration",
  "/showduino/temp",
  "/showduino/updates",
  "/showduino/updates/pending",
  "/showduino/updates/installed",
  "/showduino/updates/failed",
  nullptr
};

static bool ensureDir(const char *path) {
  if (!path || !path[0]) return false;
  if (SD.exists(path)) {
    File f = SD.open(path);
    bool ok = f && f.isDirectory();
    if (f) f.close();
    return ok;
  }
  return SD.mkdir(path);
}

static void enableSdPower() {
#if SHOWDUINO_SD_POWER_PIN >= 0
  pinMode(SHOWDUINO_SD_POWER_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_SD_POWER_PIN, SHOWDUINO_SD_POWER_ON_LEVEL);
  delay(50);
#endif
}

static bool probeWritable() {
  const char *probe = "/showduino/temp/.stage_write_probe";
  File f = SD.open(probe, FILE_WRITE);
  if (!f) return false;
  size_t n = f.print("ok");
  f.close();
  SD.remove(probe);
  return n > 0;
}

static bool ensureFolderStructure() {
  bool ok = true;
  for (uint8_t i = 0; kRequiredDirs[i] != nullptr; i++) {
    if (!ensureDir(kRequiredDirs[i])) {
      Serial.printf("[Storage] mkdir failed: %s\n", kRequiredDirs[i]);
      ok = false;
    }
  }
  return ok;
}

static bool tryMountAtHz(uint32_t hz) {
  SPI.begin(SHOWDUINO_SD_SCK_PIN, SHOWDUINO_SD_MISO_PIN,
            SHOWDUINO_SD_MOSI_PIN, SHOWDUINO_SD_CS_PIN);

  if (!SD.begin(SHOWDUINO_SD_CS_PIN, SPI, hz)) {
    return false;
  }

  uint8_t type = SD.cardType();
  if (type == CARD_NONE) {
    SD.end();
    return false;
  }

  if (type == CARD_MMC) strncpy(sStatus.cardType, "MMC", sizeof(sStatus.cardType) - 1);
  else if (type == CARD_SD) strncpy(sStatus.cardType, "SDSC", sizeof(sStatus.cardType) - 1);
  else if (type == CARD_SDHC) strncpy(sStatus.cardType, "SDHC", sizeof(sStatus.cardType) - 1);
  else strncpy(sStatus.cardType, "UNKNOWN", sizeof(sStatus.cardType) - 1);

  sStatus.totalBytes = SD.cardSize();
  sStatus.spiHz = hz;
  return true;
}

static void refreshSpace() {
  if (!sStatus.mounted) return;
  uint64_t used = SD.usedBytes();
  if (sStatus.totalBytes == 0) sStatus.totalBytes = SD.cardSize();
  if (used > sStatus.totalBytes) used = sStatus.totalBytes;
  sStatus.freeBytes = sStatus.totalBytes - used;
  sStatus.hasWww = SD.exists(PATH_WEBUI_WWW "/index.html");
}

bool stageStorageBegin() {
  Serial.println("[Storage] Stage Controller SD bring-up...");
  Serial.printf("[Storage] SPI SCK=%d MISO=%d MOSI=%d CS=%d PWR=%d\n",
                SHOWDUINO_SD_SCK_PIN, SHOWDUINO_SD_MISO_PIN,
                SHOWDUINO_SD_MOSI_PIN, SHOWDUINO_SD_CS_PIN,
                SHOWDUINO_SD_POWER_PIN);

  enableSdPower();

  static const uint32_t kSpeeds[] = {
    SHOWDUINO_SD_SPI_HZ,
    4000000UL,
    1000000UL
  };

  bool mounted = false;
  for (uint8_t i = 0; i < 3; i++) {
    if (tryMountAtHz(kSpeeds[i])) {
      mounted = true;
      break;
    }
    Serial.printf("[Storage] Mount failed at %lu Hz — retrying slower...\n",
                  (unsigned long)kSpeeds[i]);
    SD.end();
    delay(80);
  }

  if (!mounted) {
    sStatus.mounted = false;
    sStatus.writable = false;
    sStatus.folderOk = false;
    sStatus.hasWww = false;
    strncpy(sStatus.message, "SD CARD NOT AVAILABLE", sizeof(sStatus.message) - 1);
    Serial.println("[Storage] SD CARD NOT AVAILABLE");
    Serial.println("[Storage] HINT: FAT32 card; check BoardConfig pins; reseat.");
    return false;
  }

  sStatus.mounted = true;
  Serial.printf("[Storage] SD ready @ %lu Hz type=%s size=%llu MB\n",
                (unsigned long)sStatus.spiHz,
                sStatus.cardType,
                (unsigned long long)(sStatus.totalBytes / (1024ULL * 1024ULL)));

  sStatus.folderOk = ensureFolderStructure();
  sStatus.writable = probeWritable();
  refreshSpace();

  if (!sStatus.writable) {
    strncpy(sStatus.message, "SD mounted (read-only)", sizeof(sStatus.message) - 1);
    Serial.println("[Storage] Card mounted but NOT writable (WP switch / format).");
  } else if (!sStatus.folderOk) {
    strncpy(sStatus.message, "SD mounted (folders incomplete)", sizeof(sStatus.message) - 1);
  } else if (!sStatus.hasWww) {
    strncpy(sStatus.message, "SD ready (copy WebUI to /showduino/www)", sizeof(sStatus.message) - 1);
    Serial.println("[Storage] /showduino/www/index.html missing — copy Studio WebUI to card.");
  } else {
    strncpy(sStatus.message, "SD ready", sizeof(sStatus.message) - 1);
  }

  Serial.printf("[Storage] free=%llu MB writable=%s www=%s\n",
                (unsigned long long)(sStatus.freeBytes / (1024ULL * 1024ULL)),
                sStatus.writable ? "yes" : "no",
                sStatus.hasWww ? "yes" : "no");
  return sStatus.mounted;
}

void stageStorageLoop() {
  if (sStatus.mounted) return;
  unsigned long now = millis();
  if (now - sLastRetryMs < kRetryMs) return;
  sLastRetryMs = now;
  /* Cap retries so a missing card cannot starve the C3 UART tunnel forever. */
  static uint8_t sRetries = 0;
  if (sRetries >= 3) return;
  sRetries++;
  Serial.printf("[Storage] Retrying SD mount (%u/3)...\n", (unsigned)sRetries);
  stageStorageBegin();
}

const StageStorageStatus &stageStorageStatus() {
  return sStatus;
}

bool stageStorageIsReady() {
  return sStatus.mounted;
}

#endif /* SHOWDUINO_SD_ENABLED */
