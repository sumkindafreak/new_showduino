#include "AudioStorage.h"
#include "input/AudioInputConfig.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_storage.h"

#include <SD.h>
#include <SPI.h>

static bool sReady = false;
static bool sWritable = false;
static bool sConfigFault = false;
static bool sRemovedEvt = false;
static bool sInsertedEvt = false;
static bool sLastPresent = true;
static ShowduinoAudioConfig sCfg;
static uint32_t sVolSaveAt = 0;
static bool sVolDirty = false;
static uint32_t sProbeMs = 0;
static char sErr[40] = "SD not started";

static const char *kFolders[] = {
  PATH_SHOWDUINO,
  "/showduino/config",
  PATH_AUDIO_ROOT,
  PATH_AUDIO_ROOT "/ambience",
  PATH_AUDIO_ROOT "/effects",
  PATH_AUDIO_ROOT "/dialogue",
  PATH_AUDIO_ROOT "/music",
  PATH_AUDIO_ROOT "/stingers",
  PATH_AUDIO_ROOT "/test",
  PATH_AUDIO_DIAG,
  PATH_AUDIO_RECORDINGS
};

static void setErr(const char *m) {
  strncpy(sErr, m ? m : "", sizeof(sErr) - 1);
}

static bool mkdirp(const char *path) {
  if (SD.exists(path)) return true;
  return SD.mkdir(path);
}

static bool detectPinPresent() {
#if SHOWDUINO_AUDIO_SD_DETECT_PIN >= 0
  return digitalRead(SHOWDUINO_AUDIO_SD_DETECT_PIN) == LOW;
#else
  return true;
#endif
}

static bool writeConfig() {
  if (!sWritable) return false;
  char sound[420];
  audioInputConfigFormatJson(sound, sizeof(sound));
  char body[900];
  snprintf(body, sizeof(body),
           "{\n  \"formatVersion\": 1,\n  \"volume\": %u,\n  \"startupVolume\": %u,\n"
           "  \"output\": \"%s\",\n  \"nodeType\": \"AUDIO\",\n"
           "  \"commsTimeoutMs\": %u,\n  \"fadeDefaultMs\": %u,\n"
           "  \"duckVolume\": %u,\n  \"soundInput\": {\n%s\n  }\n}\n",
           (unsigned)sCfg.volume, (unsigned)sCfg.startupVolume, sCfg.output,
           (unsigned)sCfg.commsTimeoutMs, (unsigned)sCfg.fadeDefaultMs,
           (unsigned)sCfg.duckVolume, sound);
  File f = SD.open(PATH_AUDIO_CONFIG, FILE_WRITE);
  if (!f) return false;
  f.print(body);
  f.close();
  return true;
}

bool audioStorageWriteConfig() { return writeConfig(); }

static bool mountCard() {
  sReady = false;
  sWritable = false;
  SPI.begin(SHOWDUINO_AUDIO_SD_SCK, SHOWDUINO_AUDIO_SD_MISO,
            SHOWDUINO_AUDIO_SD_MOSI, SHOWDUINO_AUDIO_SD_CS);
  if (!SD.begin(SHOWDUINO_AUDIO_SD_CS, SPI, 4000000)) {
    setErr("SD CARD NOT AVAILABLE");
    return false;
  }
  sReady = true;
  for (size_t i = 0; i < sizeof(kFolders) / sizeof(kFolders[0]); i++) {
    mkdirp(kFolders[i]);
  }
  File probe = SD.open("/showduino/diagnostics/.w", FILE_WRITE);
  if (probe) {
    probe.print("ok");
    probe.close();
    SD.remove("/showduino/diagnostics/.w");
    sWritable = true;
  }
  sConfigFault = false;
  showduino_audio_config_defaults(&sCfg);
  if (SD.exists(PATH_AUDIO_CONFIG)) {
    File f = SD.open(PATH_AUDIO_CONFIG, FILE_READ);
    if (f) {
      String json = f.readString();
      f.close();
      ShowduinoAudioConfig parsed;
      if (showduino_audio_config_parse(json.c_str(), json.length(), &parsed) !=
          SHOWDUINO_AUDIO_CFG_OK) {
        sConfigFault = true;
        Serial.println("[SD] audio-node.json rejected — defaults (CONFIG_FAULT)");
      } else if (audioInputConfigApplyJson(json.c_str()) != SHOWDUINO_AUDIO_CFG_OK) {
        sConfigFault = true;
        sCfg = parsed;
        Serial.println("[SD] soundInput rejected — defaults (CONFIG_FAULT)");
      } else {
        sCfg = parsed;
      }
    }
  } else if (sWritable) {
    writeConfig();
  }
  setErr(sReady ? "SD ready" : "SD FAULT");
  Serial.printf("[SD] %s size=%llu MB\n", sErr,
                (unsigned long long)(SD.cardSize() / (1024ULL * 1024ULL)));
  return sReady;
}

static void unmountCard(const char *why) {
  if (sReady) SD.end();
  sReady = false;
  sWritable = false;
  setErr(why ? why : "SD REMOVED");
}

bool audioStorageBegin() {
#if SHOWDUINO_AUDIO_SD_DETECT_PIN >= 0
  pinMode(SHOWDUINO_AUDIO_SD_DETECT_PIN, INPUT);
#endif
  audioInputConfigBegin();
  showduino_audio_config_defaults(&sCfg);
  const bool ok = mountCard();
  sLastPresent = ok;
  return ok;
}

bool audioStorageReady() { return sReady; }
bool audioStorageWritable() { return sWritable; }
bool audioStorageConfigFault() { return sConfigFault; }
fs::FS &audioStorageFs() { return SD; }
const ShowduinoAudioConfig &audioStorageConfig() { return sCfg; }

void audioStorageSetVolume(uint8_t percent) {
  sCfg.volume = (uint8_t)showduino_audio_clamp_volume(percent);
  sVolDirty = true;
  sVolSaveAt = millis() + SHOWDUINO_AUDIO_VOLUME_DEBOUNCE_MS;
}

void audioStorageLoop() {
  if (sVolDirty && sWritable && (int32_t)(millis() - sVolSaveAt) >= 0) {
    sVolDirty = false;
    writeConfig();
  }

  if ((millis() - sProbeMs) < 1500UL) return;
  sProbeMs = millis();

  const bool pinPresent = detectPinPresent();
  bool present = pinPresent;
  if (sReady) {
    if (SD.cardSize() == 0) present = false;
  } else {
    present = pinPresent;
  }

  if (sLastPresent && !present) {
    unmountCard("SD REMOVED");
    sRemovedEvt = true;
    sLastPresent = false;
    Serial.println("[SD] STORAGE:REMOVED");
  } else if (!sLastPresent && pinPresent) {
    if (mountCard()) {
      sInsertedEvt = true;
      sLastPresent = true;
      Serial.println("[SD] STORAGE:ONLINE");
    }
  }
}

bool audioStorageResolve(const char *rel, char *out, size_t outLen) {
  return showduino_audio_resolve_path(rel, out, outLen) == SHOWDUINO_AUDIO_PATH_OK;
}

bool audioStorageExists(const char *absPath) {
  return sReady && absPath && SD.exists(absPath);
}

static void collectDir(const char *relDir, char names[][40], uint16_t maxNames, uint16_t *n) {
  char abs[80];
  if (relDir && relDir[0]) {
    snprintf(abs, sizeof(abs), "%s/%s", PATH_AUDIO_ROOT, relDir);
  } else {
    strncpy(abs, PATH_AUDIO_ROOT, sizeof(abs) - 1);
  }
  File dir = SD.open(abs);
  if (!dir) return;
  File e = dir.openNextFile();
  while (e && *n < maxNames) {
    if (!e.isDirectory()) {
      const char *nm = e.name();
      const char *base = strrchr(nm, '/');
      base = base ? base + 1 : nm;
      if (showduino_audio_has_wav_ext(base)) {
        char entry[40];
        if (relDir && relDir[0]) snprintf(entry, sizeof(entry), "%s/%s", relDir, base);
        else strncpy(entry, base, sizeof(entry) - 1);
        entry[39] = '\0';
        strncpy(names[*n], entry, 39);
        names[*n][39] = '\0';
        (*n)++;
      }
    }
    e.close();
    e = dir.openNextFile();
  }
  dir.close();
}

uint16_t audioStorageListWav(char names[][40], uint16_t maxNames) {
  return audioStorageInventory(names, maxNames);
}

uint16_t audioStorageInventory(char names[][40], uint16_t maxNames) {
  uint16_t n = 0;
  if (!sReady || !names || maxNames == 0) return 0;
  if (maxNames > SHOWDUINO_AUDIO_INV_MAX) maxNames = SHOWDUINO_AUDIO_INV_MAX;
  collectDir("", names, maxNames, &n);
  collectDir("ambience", names, maxNames, &n);
  collectDir("effects", names, maxNames, &n);
  collectDir("dialogue", names, maxNames, &n);
  collectDir("music", names, maxNames, &n);
  collectDir("stingers", names, maxNames, &n);
  collectDir("test", names, maxNames, &n);
  return n;
}

const char *audioStorageLastError() { return sErr; }

const char *audioStorageStateName() {
  if (!sReady) return "OFFLINE";
  if (sConfigFault) return "DEGRADED";
  return sWritable ? "ONLINE" : "READ_ONLY";
}

bool audioStorageCardPresent() { return sReady; }

bool audioStorageJustRemoved() {
  if (!sRemovedEvt) return false;
  sRemovedEvt = false;
  return true;
}

bool audioStorageJustInserted() {
  if (!sInsertedEvt) return false;
  sInsertedEvt = false;
  return true;
}
