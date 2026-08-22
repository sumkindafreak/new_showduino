#include "StageStorage.h"

#if SHOWDUINO_SD_ENABLED

#include <string.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "esp32-hal-ldo.h"

static StageStorageStatus sStatus;
static unsigned long sLastRetryMs = 0;
static const unsigned long kRetryMs = 15000UL;
static void (*sLinkPump)() = nullptr;

void stageStorageSetLinkPump(void (*fn)()) {
  sLinkPump = fn;
}

static void pumpLink() {
  if (sLinkPump) sLinkPump();
}

static void waitMs(uint32_t ms) {
  const uint32_t t0 = millis();
  while ((millis() - t0) < ms) {
    pumpLink();
    delay(1);
  }
}

static const char *const kRequiredDirs[] = {
  "/showduino",
  "/showduino/webui",
  "/showduino/webui/css",
  "/showduino/webui/js",
  "/showduino/webui/js/components",
  "/showduino/webui/js/pages",
  "/showduino/webui/assets",
  "/showduino/audio",
  "/showduino/www",
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
  if (SD_MMC.exists(path)) {
    File f = SD_MMC.open(path);
    bool ok = f && f.isDirectory();
    if (f) f.close();
    return ok;
  }
  return SD_MMC.mkdir(path);
}

static bool probeWritable() {
  const char *probe = "/showduino/temp/.stage_write_probe";
  File f = SD_MMC.open(probe, FILE_WRITE);
  if (!f) return false;
  size_t n = f.print("ok");
  f.close();
  SD_MMC.remove(probe);
  return n > 0;
}

static bool ensureFolderStructure() {
  bool ok = true;
  for (uint8_t i = 0; kRequiredDirs[i] != nullptr; i++) {
    pumpLink();
    if (!ensureDir(kRequiredDirs[i])) {
      Serial.printf("[Storage] mkdir failed: %s\n", kRequiredDirs[i]);
      ok = false;
    }
  }
  return ok;
}

enum class SdVolumeKind : uint8_t {
  NoCard = 0,
  ExFat,
  Fat,
  Unknown
};

static void powerCycleSdSlot() {
#if SHOWDUINO_SD_POWER_PIN >= 0
  pinMode(SHOWDUINO_SD_POWER_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_SD_POWER_PIN, !SHOWDUINO_SD_POWER_ON_LEVEL);
  waitMs(200);
  digitalWrite(SHOWDUINO_SD_POWER_PIN, SHOWDUINO_SD_POWER_ON_LEVEL);
  waitMs(80);
#endif
}

static const char *classifySector(const uint8_t *s) {
  if (s[0] == 0xEB || s[0] == 0xE9) {
    if (memcmp(s + 3, "EXFAT   ", 8) == 0) return "exFAT";
    if (memcmp(s + 82, "FAT32   ", 8) == 0) return "FAT32";
    if (memcmp(s + 54, "FAT16   ", 8) == 0) return "FAT16";
    if (memcmp(s + 54, "FAT12   ", 8) == 0) return "FAT12";
    return "VBR-unknown";
  }
  if (s[510] == 0x55 && s[511] == 0xAA) {
    uint8_t type = s[450];
    if (type == 0x07) return "MBR-exFAT/NTFS";
    if (type == 0x0B || type == 0x0C) return "MBR-FAT32";
    if (type == 0x04 || type == 0x06 || type == 0x0E) return "MBR-FAT16";
    return "MBR";
  }
  return "unformatted";
}

static bool nameIsExfat(const char *name) {
  return name && (strcmp(name, "exFAT") == 0 || strcmp(name, "MBR-exFAT/NTFS") == 0);
}

static bool nameIsFat(const char *name) {
  return name && (strncmp(name, "FAT", 3) == 0 || strncmp(name, "MBR-FAT", 7) == 0);
}

static SdVolumeKind probeVolume() {
  SdVolumeKind kind = SdVolumeKind::NoCard;
  sd_pwr_ctrl_handle_t pwr = nullptr;
  sdmmc_card_t *card = nullptr;
  uint8_t *sec = nullptr;
  bool hostUp = false;

  powerCycleSdSlot();

#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  sd_pwr_ctrl_ldo_config_t ldo_config = {
    .ldo_chan_id = SHOWDUINO_SD_LDO_CHANNEL,
  };
  ldoSdmmcPrepareAcquire((uint8_t)SHOWDUINO_SD_LDO_CHANNEL);
  if (sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr) != ESP_OK) {
    Serial.println("[Storage] Probe: LDO channel 4 acquire failed");
    ldoSdmmcDriverCreateFailed((uint8_t)SHOWDUINO_SD_LDO_CHANNEL);
    return SdVolumeKind::NoCard;
  }
  ldoSdmmcDriverAttached((uint8_t)SHOWDUINO_SD_LDO_CHANNEL);
#endif

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_0;
  host.max_freq_khz = SDMMC_FREQ_PROBING;
  host.flags = SDMMC_HOST_FLAG_1BIT;
#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  host.pwr_ctrl_handle = pwr;
#endif

  sdmmc_slot_config_t slot = {
    .clk = GPIO_NUM_0,
    .cmd = GPIO_NUM_0,
    .d0 = GPIO_NUM_0,
    .d1 = GPIO_NUM_0,
    .d2 = GPIO_NUM_0,
    .d3 = GPIO_NUM_0,
    .d4 = GPIO_NUM_0,
    .d5 = GPIO_NUM_0,
    .d6 = GPIO_NUM_0,
    .d7 = GPIO_NUM_0,
    .cd = SDMMC_SLOT_NO_CD,
    .wp = SDMMC_SLOT_NO_WP,
    .width = 1,
    .flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP,
  };

  esp_err_t err = sdmmc_host_init();
  if (err != ESP_OK) {
    Serial.printf("[Storage] Probe: host init failed 0x%x\n", (unsigned)err);
    goto probe_done;
  }
  hostUp = true;
  err = sdmmc_host_init_slot(SDMMC_HOST_SLOT_0, &slot);
  if (err != ESP_OK) {
    Serial.printf("[Storage] Probe: slot init failed 0x%x\n", (unsigned)err);
    goto probe_done;
  }

  card = (sdmmc_card_t *)calloc(1, sizeof(sdmmc_card_t));
  if (!card) goto probe_done;
  err = sdmmc_card_init(&host, card);
  if (err != ESP_OK) {
    Serial.printf("[Storage] Probe: card init failed 0x%x (no card / slot power)\n",
                  (unsigned)err);
    goto probe_done;
  }

  Serial.printf("[Storage] Probe: card OK capacity=%llu MB\n",
                (unsigned long long)((uint64_t)card->csd.capacity *
                                     card->csd.sector_size / (1024ULL * 1024ULL)));

  sec = (uint8_t *)heap_caps_malloc(512, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (!sec) sec = (uint8_t *)malloc(512);
  if (!sec) goto probe_done;
  memset(sec, 0, 512);
  err = sdmmc_read_sectors(card, sec, 0, 1);
  if (err != ESP_OK) {
    Serial.printf("[Storage] Probe: sector 0 read failed 0x%x\n", (unsigned)err);
    kind = SdVolumeKind::Unknown;
    goto probe_done;
  }

  {
    const char *fsName = classifySector(sec);
    char oem[9];
    memcpy(oem, sec + 3, 8);
    oem[8] = '\0';
    Serial.printf("[Storage] Probe: LBA0 type=%s OEM='%.8s'\n", fsName, oem);

    if (strcmp(fsName, "MBR") == 0 || strncmp(fsName, "MBR-", 4) == 0) {
      uint32_t lba = (uint32_t)sec[454] | ((uint32_t)sec[455] << 8) |
                     ((uint32_t)sec[456] << 16) | ((uint32_t)sec[457] << 24);
      if (lba > 0 && sdmmc_read_sectors(card, sec, lba, 1) == ESP_OK) {
        fsName = classifySector(sec);
        memcpy(oem, sec + 3, 8);
        oem[8] = '\0';
        Serial.printf("[Storage] Probe: partition LBA %lu type=%s OEM='%.8s'\n",
                      (unsigned long)lba, fsName, oem);
      }
    }

    if (nameIsExfat(fsName)) kind = SdVolumeKind::ExFat;
    else if (nameIsFat(fsName)) kind = SdVolumeKind::Fat;
    else kind = SdVolumeKind::Unknown;
  }

probe_done:
  if (sec) free(sec);
  if (card) free(card);
  if (hostUp) {
    sdmmc_host_deinit_slot(SDMMC_HOST_SLOT_0);
    sdmmc_host_deinit();
  }
#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  if (pwr) {
    sd_pwr_ctrl_del_on_chip_ldo(pwr);
    ldoSdmmcDriverDetached((uint8_t)SHOWDUINO_SD_LDO_CHANNEL);
  }
#endif
  delay(50);
  pumpLink();
  return kind;
}

static bool tryMount(int freqKhz, bool oneBit) {
  SD_MMC.end();
#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  if (!SD_MMC.setPowerChannel(SHOWDUINO_SD_LDO_CHANNEL)) {
    Serial.println("[Storage] SDMMC LDO channel 4 not accepted");
    return false;
  }
#endif
  if (!SD_MMC.setPins(SHOWDUINO_SD_CLK_PIN, SHOWDUINO_SD_CMD_PIN,
                      SHOWDUINO_SD_D0_PIN, SHOWDUINO_SD_D1_PIN,
                      SHOWDUINO_SD_D2_PIN, SHOWDUINO_SD_D3_PIN)) {
    Serial.println("[Storage] SDMMC pin map rejected");
    return false;
  }

  if (!SD_MMC.begin("/sd", oneBit, false, freqKhz, 5)) {
    return false;
  }

  uint8_t type = SD_MMC.cardType();
  if (type == CARD_NONE) {
    SD_MMC.end();
    return false;
  }

  if (type == CARD_MMC) strncpy(sStatus.cardType, "MMC", sizeof(sStatus.cardType) - 1);
  else if (type == CARD_SD) strncpy(sStatus.cardType, "SDSC", sizeof(sStatus.cardType) - 1);
  else if (type == CARD_SDHC) strncpy(sStatus.cardType, "SDHC", sizeof(sStatus.cardType) - 1);
  else strncpy(sStatus.cardType, "UNKNOWN", sizeof(sStatus.cardType) - 1);

  sStatus.totalBytes = SD_MMC.cardSize();
  sStatus.spiHz = (uint32_t)freqKhz * 1000UL;
  return true;
}

static void refreshSpace() {
  if (!sStatus.mounted) return;
  uint64_t used = SD_MMC.usedBytes();
  if (sStatus.totalBytes == 0) sStatus.totalBytes = SD_MMC.cardSize();
  if (used > sStatus.totalBytes) used = sStatus.totalBytes;
  sStatus.freeBytes = sStatus.totalBytes - used;
  sStatus.hasWww = SD_MMC.exists(PATH_WEBUI "/index.html");
}

bool stageStorageBegin() {
  Serial.println("[Storage] Stage Controller SD bring-up...");
  Serial.printf("[Storage] SDMMC CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d PWR=%d LDO=%d\n",
                SHOWDUINO_SD_CLK_PIN, SHOWDUINO_SD_CMD_PIN,
                SHOWDUINO_SD_D0_PIN, SHOWDUINO_SD_D1_PIN,
                SHOWDUINO_SD_D2_PIN, SHOWDUINO_SD_D3_PIN,
                SHOWDUINO_SD_POWER_PIN, SHOWDUINO_SD_LDO_CHANNEL);

  SdVolumeKind kind = probeVolume();
  if (kind == SdVolumeKind::ExFat) {
    sStatus.mounted = false;
    sStatus.writable = false;
    sStatus.folderOk = false;
    sStatus.hasWww = false;
    strncpy(sStatus.message, "SD is exFAT — format FAT32", sizeof(sStatus.message) - 1);
    Serial.println("[Storage] SD CARD NOT AVAILABLE");
    Serial.println("[Storage] Card is exFAT. Arduino FatFs on this P4 cannot mount exFAT.");
    Serial.println("[Storage] Copy files off, format FAT32, copy files back, reseat.");
    return false;
  }
  if (kind == SdVolumeKind::NoCard) {
    sStatus.mounted = false;
    sStatus.writable = false;
    sStatus.folderOk = false;
    sStatus.hasWww = false;
    strncpy(sStatus.message, "SD CARD NOT AVAILABLE", sizeof(sStatus.message) - 1);
    Serial.println("[Storage] SD CARD NOT AVAILABLE");
    Serial.println("[Storage] HINT: no card response — reseat; check slot power.");
    return false;
  }

  struct MountTry {
    int freqKhz;
    bool oneBit;
  };
  static const MountTry kTries[] = {
    {SHOWDUINO_SD_FREQ_KHZ, false},
    {SDMMC_FREQ_PROBING, false},
    {SDMMC_FREQ_PROBING, true},
  };

  bool mounted = false;
  for (uint8_t i = 0; i < 3; i++) {
    if (tryMount(kTries[i].freqKhz, kTries[i].oneBit)) {
      mounted = true;
      break;
    }
    Serial.printf("[Storage] Mount failed %s %d kHz (vfs/FatFs)\n",
                  kTries[i].oneBit ? "1-bit" : "4-bit", kTries[i].freqKhz);
    SD_MMC.end();
    waitMs(80);
  }

  if (!mounted) {
    sStatus.mounted = false;
    sStatus.writable = false;
    sStatus.folderOk = false;
    sStatus.hasWww = false;
    strncpy(sStatus.message, "SD CARD NOT AVAILABLE", sizeof(sStatus.message) - 1);
    Serial.println("[Storage] SD CARD NOT AVAILABLE");
    Serial.println("[Storage] SDMMC reached the card; FatFs could not mount the volume.");
    Serial.println("[Storage] This core has exFAT disabled. Format the card FAT32, not exFAT.");
    return false;
  }

  sStatus.mounted = true;
  Serial.println("[SD] SD card mounted");
  Serial.printf("[Storage] SD ready @ %lu Hz type=%s size=%llu MB\n",
                (unsigned long)sStatus.spiHz,
                sStatus.cardType,
                (unsigned long long)(sStatus.totalBytes / (1024ULL * 1024ULL)));
  Serial.printf("[SD] Card capacity: %llu MB (%s)\n",
                (unsigned long long)(sStatus.totalBytes / (1024ULL * 1024ULL)),
                sStatus.cardType);

  sStatus.folderOk = ensureFolderStructure();
  sStatus.writable = probeWritable();
  refreshSpace();

  if (!sStatus.writable) {
    strncpy(sStatus.message, "SD mounted (read-only)", sizeof(sStatus.message) - 1);
    Serial.println("[Storage] Card mounted but NOT writable (WP switch / format).");
  } else if (!sStatus.folderOk) {
    strncpy(sStatus.message, "SD mounted (folders incomplete)", sizeof(sStatus.message) - 1);
  } else if (!sStatus.hasWww) {
    strncpy(sStatus.message, "SD ready (copy WebUI to /showduino/webui)", sizeof(sStatus.message) - 1);
    Serial.println("[WEB] Checking /showduino/webui/index.html");
    Serial.println("[WEB] WebUI missing - run deploy-webui-to-sd.ps1");
  } else {
    strncpy(sStatus.message, "SD ready", sizeof(sStatus.message) - 1);
    Serial.println("[WEB] Checking /showduino/webui/index.html");
    Serial.println("[WEB] WebUI found");
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

fs::FS &stageStorageFs() {
  return SD_MMC;
}

#endif /* SHOWDUINO_SD_ENABLED */
