#include "StageAudio.h"

#include <string.h>
#include <strings.h>
#include <Wire.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "StageStorage.h"
#include "audio/Es8311Codec.h"

#if SHOWDUINO_STATUS_LED_PIN == 10
#error "GPIO10 is ES8311 LRCK/WS — do not use SHOWDUINO_STATUS_LED_PIN=10"
#endif
#if SHOWDUINO_STATUS_LED_PIN >= 0 && \
    (SHOWDUINO_STATUS_LED_PIN == P4_SYSTEM_AUDIO_I2S_WS || \
     SHOWDUINO_STATUS_LED_PIN == P4_SYSTEM_AUDIO_I2S_BCLK || \
     SHOWDUINO_STATUS_LED_PIN == P4_SYSTEM_AUDIO_I2S_DOUT || \
     SHOWDUINO_STATUS_LED_PIN == P4_SYSTEM_AUDIO_I2S_DIN || \
     SHOWDUINO_STATUS_LED_PIN == P4_SYSTEM_AUDIO_I2S_MCLK || \
     SHOWDUINO_STATUS_LED_PIN == P4_SYSTEM_AUDIO_PA_ENABLE)
#error "Status LED GPIO conflicts with the onboard ES8311 / NS4150B path"
#endif

#if SHOWDUINO_LEGACY_PCM5102A_AUDIO
/*
 * LEGACY — compiled out unless SHOWDUINO_LEGACY_PCM5102A_AUDIO=1.
 * Historical external PCM5102A on GPIO20/21/22. That path wrote I2S into a
 * disconnected DAC while the onboard speaker stayed silent. Do not enable.
 * Attraction audio belongs to the Audio Node.
 */
#warning "SHOWDUINO_LEGACY_PCM5102A_AUDIO is not a supported live path"
#endif

static constexpr uint8_t kVolumePercent = 80;
static constexpr uint32_t kErrorRetriggerMs = 2500;
static constexpr uint32_t kDefaultRate = 48000;

struct SoundDef {
  SystemSound id;
  const char *name;
  const char *path;
  uint8_t priority;
  bool looping;
  bool reserved;
};

static const SoundDef kSounds[] = {
  { SystemSound::Boot,      "BOOT",      PATH_SYSTEM_BOOT_WAV,      1, false, false },
  { SystemSound::Emergency, "EMERGENCY", PATH_SYSTEM_EMERGENCY_WAV, 3, true,  false },
  { SystemSound::Beep,      "BEEP",      PATH_SYSTEM_BEEP_WAV,      1, false, false },
  { SystemSound::Tone,      "TONE",      PATH_SYSTEM_TONE_WAV,      1, false, false },
  { SystemSound::Error,     "ERROR",     PATH_SYSTEM_ERROR_WAV,     2, false, false },
  { SystemSound::Accepted,  "ACCEPTED",  PATH_SYSTEM_ACCEPTED_WAV,  1, false, false },
  { SystemSound::Complete,  "COMPLETE",  PATH_SYSTEM_COMPLETE_WAV,  1, false, false },
  { SystemSound::Shutdown,  "SHUTDOWN",  PATH_SYSTEM_SHUTDOWN_WAV,  0, false, true  },
};

static const char *const kSearchDirs[] = {
  PATH_AUDIO_SYSTEM,
  PATH_AUDIO_SYSTEM_LIBRARY,
};

static constexpr size_t kSoundCount = sizeof(kSounds) / sizeof(kSounds[0]);
static char sResolvedPath[kSoundCount][64];
static bool sResolvedValid[kSoundCount];

static StageAudioStatus sStatus;
static SystemSound sCurrent = SystemSound::None;
static uint8_t sCurrentPri = 0;
static bool sSafetyEmergency = false;
static bool sTestLoop = false;
static bool sBootRequested = false;
static bool sPaEnabled = false;
static bool sHwInited = false;
static bool sLoggedActive = false;
static uint32_t sLastErrorPlayMs = 0;
static uint32_t sErrorStormUntil = 0;

static File sFile;
static bool sFileOpen = false;
static uint32_t sDataStart = 0;
static uint32_t sDataSize = 0;
static uint32_t sDataPos = 0;
static uint32_t sSampleRate = kDefaultRate;
static uint16_t sChannels = 2;
static uint16_t sBits = 16;
static bool sLooping = false;
static bool sMono = false;

static i2s_chan_handle_t sTx = nullptr;
static bool sI2sStarted = false;
static bool sI2sCapable = false;
static uint32_t sI2sRate = 0;

static uint8_t sFileBuf[1024];
static uint8_t sI2sBuf[2048];
static size_t sBufLen = 0;
static size_t sBufOff = 0;

static uint32_t sLastOpenFailMs = 0;
static uint32_t sI2sBytesWritten = 0;
static uint32_t sEmergencyLoopCount = 0;

static const SoundDef *findDef(SystemSound sound) {
  for (size_t i = 0; i < kSoundCount; i++) {
    if (kSounds[i].id == sound) return &kSounds[i];
  }
  return nullptr;
}

static int soundIndex(SystemSound sound) {
  for (size_t i = 0; i < kSoundCount; i++) {
    if (kSounds[i].id == sound) return (int)i;
  }
  return -1;
}

static const char *soundFileName(const SoundDef *def) {
  if (!def || !def->path) return "";
  const char *slash = strrchr(def->path, '/');
  return slash ? slash + 1 : def->path;
}

const char *systemSoundName(SystemSound sound) {
  if (sound == SystemSound::None) return "NONE";
  const SoundDef *d = findDef(sound);
  return d ? d->name : "NONE";
}

const char *systemSoundPath(SystemSound sound) {
  const SoundDef *d = findDef(sound);
  return d ? d->path : "";
}

const char *stageAudioResolvedPath(SystemSound sound) {
  const int idx = soundIndex(sound);
  if (idx < 0 || !sResolvedValid[(size_t)idx]) return "";
  return sResolvedPath[(size_t)idx];
}

SystemSound systemSoundFromName(const char *name) {
  if (!name || !name[0]) return SystemSound::None;
  for (size_t i = 0; i < kSoundCount; i++) {
    if (strcasecmp(name, kSounds[i].name) == 0) return kSounds[i].id;
  }
  return SystemSound::None;
}

bool systemSoundIsReserved(SystemSound sound) {
  const SoundDef *d = findDef(sound);
  return d && d->reserved;
}

bool systemSoundIsActive(SystemSound sound) {
  const SoundDef *d = findDef(sound);
  return d && !d->reserved;
}

static void setErr(char *err, size_t errLen, const char *msg) {
  if (!err || errLen == 0) return;
  strncpy(err, msg, errLen - 1);
  err[errLen - 1] = '\0';
}

static void setStatusErr(const char *msg) {
  setErr(sStatus.lastError, sizeof(sStatus.lastError), msg);
}

static void setPa(bool on) {
  if (P4_SYSTEM_AUDIO_PA_ENABLE < 0) {
    sPaEnabled = false;
    sStatus.amplifierEnabled = false;
    return;
  }
  digitalWrite(P4_SYSTEM_AUDIO_PA_ENABLE, on ? P4_SYSTEM_AUDIO_PA_ON_LEVEL
                                             : !P4_SYSTEM_AUDIO_PA_ON_LEVEL);
  sPaEnabled = on;
  sStatus.amplifierEnabled = on;
}

static void refreshHealth() {
  strncpy(sStatus.currentName, systemSoundName(sCurrent), sizeof(sStatus.currentName) - 1);
  sStatus.currentName[sizeof(sStatus.currentName) - 1] = '\0';
  sStatus.current = sCurrent;
  sStatus.codecDetected = es8311Info().detected;
  sStatus.codecReady = es8311Info().initialized && es8311Info().status == Es8311Status::Ready;
  sStatus.i2sReady = sI2sCapable;
  sStatus.amplifierEnabled = sPaEnabled;
  sStatus.storageReady = stageStorageIsReady();
  sStatus.emergencyPlaying = sSafetyEmergency && sStatus.playbackActive;

  if (sSafetyEmergency && (sFileOpen || sStatus.playbackActive || sStatus.playbackStarted)) {
    sStatus.playState = SystemAudioPlayState::Emergency;
    sStatus.health = SystemAudioHealth::Emergency;
    strncpy(sStatus.healthName, "EMERGENCY", sizeof(sStatus.healthName) - 1);
  } else if (sFileOpen && sStatus.playbackActive) {
    sStatus.playState = SystemAudioPlayState::Playing;
    sStatus.health = SystemAudioHealth::Playing;
    strncpy(sStatus.healthName, "PLAYING", sizeof(sStatus.healthName) - 1);
  } else if (!sStatus.codecReady) {
    sStatus.playState = sStatus.playbackFailed ? SystemAudioPlayState::Failed
                                               : SystemAudioPlayState::Idle;
    sStatus.health = SystemAudioHealth::CodecFault;
    strncpy(sStatus.healthName, "CODEC_FAULT", sizeof(sStatus.healthName) - 1);
  } else if (sHwInited && !sStatus.i2sReady && sStatus.playbackFailed) {
    sStatus.playState = SystemAudioPlayState::Failed;
    sStatus.health = SystemAudioHealth::I2sFault;
    strncpy(sStatus.healthName, "I2S_FAULT", sizeof(sStatus.healthName) - 1);
  } else if (!sStatus.storageReady) {
    sStatus.playState = SystemAudioPlayState::Idle;
    sStatus.health = SystemAudioHealth::NoSd;
    strncpy(sStatus.healthName, "NO_SD", sizeof(sStatus.healthName) - 1);
  } else if (sStatus.playbackFailed) {
    sStatus.playState = SystemAudioPlayState::Failed;
    sStatus.health = SystemAudioHealth::FileError;
    strncpy(sStatus.healthName, "FILE_ERROR", sizeof(sStatus.healthName) - 1);
  } else {
    sStatus.playState = SystemAudioPlayState::Idle;
    sStatus.health = SystemAudioHealth::Ready;
    strncpy(sStatus.healthName, "READY", sizeof(sStatus.healthName) - 1);
  }
}

static void scanAssets(bool verbose = false) {
  sStatus.storageReady = stageStorageIsReady();
  sStatus.activeAssets = 0;
  sStatus.shutdownAssetPresent = false;
  sStatus.wavPresent = false;
  sStatus.mp3Present = false;
  sStatus.mp3Path[0] = '\0';
  sStatus.wavPath[0] = '\0';
  memset(sResolvedPath, 0, sizeof(sResolvedPath));
  memset(sResolvedValid, 0, sizeof(sResolvedValid));

  if (!sStatus.storageReady) return;

  if (verbose) {
    Serial.println("[AUDIO] Matching system WAV to valid PCM files");
    Serial.printf("[AUDIO] Search: %s  then  %s\n",
                  PATH_AUDIO_SYSTEM, PATH_AUDIO_SYSTEM_LIBRARY);
  }

  for (size_t i = 0; i < kSoundCount; i++) {
    const char *fileName = soundFileName(&kSounds[i]);
    bool matched = false;
    char lastErr[48] = "WAV missing";

    for (size_t d = 0; d < sizeof(kSearchDirs) / sizeof(kSearchDirs[0]); d++) {
      char cand[64];
      snprintf(cand, sizeof(cand), "%s/%s", kSearchDirs[d], fileName);
      if (!stageStorageFs().exists(cand)) continue;

      if (kSounds[i].reserved) {
        strncpy(sResolvedPath[i], cand, sizeof(sResolvedPath[i]) - 1);
        sResolvedValid[i] = true;
        sStatus.shutdownAssetPresent = true;
        matched = true;
        if (verbose) {
          Serial.printf("[AUDIO] %-9s reserved unused  %s\n", kSounds[i].name, cand);
        }
        break;
      }

      StageWavInfo info;
      if (!stageAudioInspectWav(cand, &info) || !info.engineSupported) {
        strncpy(lastErr, info.error[0] ? info.error : "invalid WAV", sizeof(lastErr) - 1);
        if (verbose) {
          Serial.printf("[AUDIO] %-9s skip   %s  (%s)\n",
                        kSounds[i].name, cand, lastErr);
        }
        continue;
      }

      strncpy(sResolvedPath[i], cand, sizeof(sResolvedPath[i]) - 1);
      sResolvedValid[i] = true;
      matched = true;
      sStatus.activeAssets++;
      if (kSounds[i].id == SystemSound::Emergency) {
        sStatus.wavPresent = true;
        strncpy(sStatus.wavPath, cand, sizeof(sStatus.wavPath) - 1);
        sStatus.wavPath[sizeof(sStatus.wavPath) - 1] = '\0';
      }
      if (verbose) {
        Serial.printf("[AUDIO] %-9s valid  %s  PCM %u-bit %s %lu Hz\n",
                      kSounds[i].name, cand,
                      (unsigned)info.bits,
                      info.channels == 1 ? "mono" : "stereo",
                      (unsigned long)info.sampleRate);
      }
      break;
    }

    if (!matched) {
      if (kSounds[i].reserved) {
        if (verbose) Serial.printf("[AUDIO] %-9s absent (reserved unused)\n", kSounds[i].name);
      } else if (verbose) {
        Serial.printf("[AUDIO] %-9s missing (%s)\n", kSounds[i].name, lastErr);
      }
    }
  }
}

static bool parseWav(File &f, uint32_t *dataStart, uint32_t *dataSize,
                     uint32_t *rate, uint16_t *channels, uint16_t *bits,
                     char *err, size_t errLen) {
  if (!f) {
    setErr(err, errLen, "WAV open failed");
    return false;
  }
  f.seek(0);
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12) {
    setErr(err, errLen, "WAV header truncated");
    return false;
  }
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
    setErr(err, errLen, "not RIFF/WAVE");
    return false;
  }

  bool gotFmt = false;
  uint32_t dataOff = 0;
  uint32_t dataLen = 0;
  uint32_t sr = 0;
  uint16_t ch = 0;
  uint16_t bps = 0;
  uint16_t format = 0;

  while (f.available()) {
    uint8_t chunk[8];
    if (f.read(chunk, 8) != 8) break;
    uint32_t sz = (uint32_t)chunk[4] | ((uint32_t)chunk[5] << 8) |
                  ((uint32_t)chunk[6] << 16) | ((uint32_t)chunk[7] << 24);
    uint32_t chunkPos = f.position();

    if (memcmp(chunk, "fmt ", 4) == 0) {
      uint8_t fmt[40];
      size_t n = (sz > sizeof(fmt)) ? sizeof(fmt) : (size_t)sz;
      if (f.read(fmt, n) != (int)n) {
        setErr(err, errLen, "WAV fmt truncated");
        return false;
      }
      format = (uint16_t)fmt[0] | ((uint16_t)fmt[1] << 8);
      ch = (uint16_t)fmt[2] | ((uint16_t)fmt[3] << 8);
      sr = (uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) |
           ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
      bps = (uint16_t)fmt[14] | ((uint16_t)fmt[15] << 8);
      /* WAVE_FORMAT_EXTENSIBLE wrapping PCM is a valid engine file. */
      if (format == 0xFFFE && n >= 40) {
        const uint32_t sub = (uint32_t)fmt[24] | ((uint32_t)fmt[25] << 8) |
                             ((uint32_t)fmt[26] << 16) | ((uint32_t)fmt[27] << 24);
        if (sub == 1) format = 1;
      }
      gotFmt = true;
      if (sz > n) f.seek(chunkPos + sz + (sz & 1));
    } else if (memcmp(chunk, "data", 4) == 0) {
      dataOff = chunkPos;
      dataLen = sz;
      break;
    } else {
      f.seek(chunkPos + sz + (sz & 1));
    }
  }

  if (!gotFmt) {
    setErr(err, errLen, "WAV fmt chunk missing");
    return false;
  }
  if (format != 1) {
    setErr(err, errLen, "WAV not PCM");
    return false;
  }
  if (dataLen == 0) {
    setErr(err, errLen, "WAV data chunk missing");
    return false;
  }
  if ((ch != 1 && ch != 2) || bps != 16) {
    setErr(err, errLen, "need 16-bit PCM mono/stereo");
    return false;
  }
  if (sr != 32000 && sr != 44100 && sr != 48000) {
    setErr(err, errLen, "need 32 / 44.1 / 48 kHz");
    return false;
  }

  *dataStart = dataOff;
  *dataSize = dataLen;
  *rate = sr;
  *channels = ch;
  *bits = bps;
  return true;
}

static void closeFile() {
  if (sFileOpen) {
    sFile.close();
    sFileOpen = false;
  }
  sBufLen = 0;
  sBufOff = 0;
  sDataPos = 0;
}

static void stopI2s() {
  if (sTx) {
    i2s_channel_disable(sTx);
    i2s_del_channel(sTx);
    sTx = nullptr;
  }
  sI2sStarted = false;
  sI2sRate = 0;
}

static bool startI2s(uint32_t rate) {
  if (sI2sStarted && sTx && sI2sRate == rate) {
    sStatus.i2sReady = true;
    return true;
  }

  stopI2s();

  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num = 6;
  chanCfg.dma_frame_num = 240;
  chanCfg.auto_clear = true;

  if (i2s_new_channel(&chanCfg, &sTx, nullptr) != ESP_OK) {
    setStatusErr("I2S channel alloc failed");
    sTx = nullptr;
    sI2sCapable = false;
    return false;
  }

  i2s_std_config_t stdCfg = {};
  stdCfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
  stdCfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  stdCfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  stdCfg.gpio_cfg.mclk = (gpio_num_t)P4_SYSTEM_AUDIO_I2S_MCLK;
  stdCfg.gpio_cfg.bclk = (gpio_num_t)P4_SYSTEM_AUDIO_I2S_BCLK;
  stdCfg.gpio_cfg.ws = (gpio_num_t)P4_SYSTEM_AUDIO_I2S_WS;
  stdCfg.gpio_cfg.dout = (gpio_num_t)P4_SYSTEM_AUDIO_I2S_DOUT;
  stdCfg.gpio_cfg.din = I2S_GPIO_UNUSED;
  stdCfg.gpio_cfg.invert_flags.mclk_inv = false;
  stdCfg.gpio_cfg.invert_flags.bclk_inv = false;
  stdCfg.gpio_cfg.invert_flags.ws_inv = false;

  if (i2s_channel_init_std_mode(sTx, &stdCfg) != ESP_OK) {
    setStatusErr("I2S std init failed");
    sI2sCapable = false;
    stopI2s();
    return false;
  }
  if (i2s_channel_enable(sTx) != ESP_OK) {
    setStatusErr("I2S enable failed");
    sI2sCapable = false;
    stopI2s();
    return false;
  }

  static const uint8_t kZeros[128] = {};
  size_t written = 0;
  (void)i2s_channel_write(sTx, kZeros, sizeof(kZeros), &written, 20);

  sI2sStarted = true;
  sI2sCapable = true;
  sI2sRate = rate;
  sStatus.i2sReady = true;
  return true;
}

static void releaseOutputPath() {
  (void)es8311SetMute(true);
  setPa(false);
  stopI2s();
}

static bool armOutputPath(uint32_t rate) {
  if (!sHwInited || !es8311Info().initialized) {
    setStatusErr("codec not ready");
    return false;
  }
  if (!es8311SetSampleRate(rate)) {
    setStatusErr(es8311Info().lastError);
    return false;
  }
  if (!startI2s(rate)) return false;
  if (!es8311SetMute(false)) {
    setStatusErr("codec unmute failed");
    return false;
  }
  setPa(true);
  Serial.printf("[AUDIO] Output path armed  codec=READY i2s=READY pa=ENABLED  MCLK=%d BCLK=%d WS=%d DOUT=%d PA=%d\n",
                P4_SYSTEM_AUDIO_I2S_MCLK, P4_SYSTEM_AUDIO_I2S_BCLK,
                P4_SYSTEM_AUDIO_I2S_WS, P4_SYSTEM_AUDIO_I2S_DOUT,
                P4_SYSTEM_AUDIO_PA_ENABLE);
  return true;
}

static void finishPlayback(const char *why) {
  closeFile();
  sLooping = false;
  sTestLoop = false;
  sCurrent = SystemSound::None;
  sCurrentPri = 0;
  sStatus.playbackStarted = false;
  sStatus.playbackActive = false;
  sStatus.emergencyPlaying = false;
  if (why) setStatusErr(why);
  releaseOutputPath();
  refreshHealth();
}

static void rewindLoop() {
  if (!sFileOpen) return;
  sFile.seek(sDataStart);
  sDataPos = 0;
  sBufLen = 0;
  sBufOff = 0;
  if (sCurrent == SystemSound::Emergency) sEmergencyLoopCount++;
}

static bool expandToStereo(const uint8_t *src, size_t srcLen, size_t *outLen) {
  if (!sMono) {
    if (srcLen > sizeof(sI2sBuf)) srcLen = sizeof(sI2sBuf);
    memcpy(sI2sBuf, src, srcLen);
    *outLen = srcLen;
    return srcLen > 0;
  }
  size_t frames = srcLen / 2;
  if (frames * 4 > sizeof(sI2sBuf)) frames = sizeof(sI2sBuf) / 4;
  const int16_t *in = (const int16_t *)src;
  int16_t *out = (int16_t *)sI2sBuf;
  for (size_t i = 0; i < frames; i++) {
    out[i * 2] = in[i];
    out[i * 2 + 1] = in[i];
  }
  *outLen = frames * 4;
  return *outLen > 0;
}

static void pumpPlayback() {
  if (!sFileOpen || !sTx) return;

  if (sBufOff >= sBufLen) {
    size_t remain = (sDataSize > sDataPos) ? (sDataSize - sDataPos) : 0;
    if (remain == 0) {
      if (sLooping && sCurrent == SystemSound::Emergency) {
        Serial.println("[AUDIO] Emergency WAV EOF — looping");
        rewindLoop();
        remain = sDataSize;
      } else {
        Serial.printf("[AUDIO] %s completed\n", systemSoundName(sCurrent));
        finishPlayback("playback completed");
        return;
      }
    }

    size_t want = sizeof(sFileBuf);
    if (want > remain) want = remain;
    int n = sFile.read(sFileBuf, want);
    if (n <= 0) {
      if (sLooping && sCurrent == SystemSound::Emergency) {
        rewindLoop();
      } else {
        sStatus.playbackFailed = true;
        finishPlayback("WAV read failed");
      }
      return;
    }
    sDataPos += (uint32_t)n;
    if (!expandToStereo(sFileBuf, (size_t)n, &sBufLen)) {
      sStatus.playbackFailed = true;
      finishPlayback("WAV expand failed");
      return;
    }
    sBufOff = 0;
  }

  size_t todo = sBufLen - sBufOff;
  size_t written = 0;
  esp_err_t err = i2s_channel_write(sTx, sI2sBuf + sBufOff, todo, &written, 0);
  if (err == ESP_OK || err == ESP_ERR_TIMEOUT) {
    sBufOff += written;
    sI2sBytesWritten += (uint32_t)written;
    if (written > 0) {
      sStatus.playbackActive = true;
      if (!sLoggedActive) {
        sLoggedActive = true;
        Serial.printf("[AUDIO] Playback active  sound=%s  I2S bytes flowing\n",
                      systemSoundName(sCurrent));
      }
    }
  }
  refreshHealth();
}

static bool openSystemWav(SystemSound sound, bool looping) {
  const SoundDef *def = findDef(sound);
  if (!def) {
    setStatusErr("unknown system sound");
    return false;
  }
  if (def->reserved) {
    setStatusErr("SHUTDOWN reserved");
    return false;
  }
  if (!stageStorageIsReady()) {
    setStatusErr("SD not mounted");
    return false;
  }
  const int idx = soundIndex(sound);
  const char *path = (idx >= 0 && sResolvedValid[(size_t)idx])
                         ? sResolvedPath[(size_t)idx]
                         : "";
  if (!path[0]) {
    setStatusErr("WAV missing or invalid");
    return false;
  }

  closeFile();
  sFile = stageStorageFs().open(path, FILE_READ);
  if (!sFile) {
    setStatusErr("WAV open failed");
    return false;
  }
  if (!parseWav(sFile, &sDataStart, &sDataSize, &sSampleRate, &sChannels, &sBits,
                sStatus.lastError, sizeof(sStatus.lastError))) {
    sFile.close();
    return false;
  }

  Serial.printf("[AUDIO] WAV valid  %s  PCM %u-bit %s %lu Hz  %lu bytes\n",
                path,
                (unsigned)sBits,
                sChannels == 1 ? "mono" : "stereo",
                (unsigned long)sSampleRate,
                (unsigned long)sDataSize);

  if (!armOutputPath(sSampleRate)) {
    sFile.close();
    return false;
  }

  sFile.seek(sDataStart);
  sFileOpen = true;
  sDataPos = 0;
  sBufLen = 0;
  sBufOff = 0;
  sLooping = looping;
  sMono = (sChannels == 1);
  sCurrent = sound;
  sCurrentPri = def->priority;
  sStatus.playbackStarted = true;
  sStatus.playbackActive = false;
  sStatus.playbackFailed = false;
  sLoggedActive = false;
  strncpy(sStatus.selectedPath, path, sizeof(sStatus.selectedPath) - 1);
  sStatus.selectedPath[sizeof(sStatus.selectedPath) - 1] = '\0';
  setStatusErr("output path ready");
  refreshHealth();
  return true;
}

static uint8_t livePriority() {
  if (sSafetyEmergency) return 3;
  return sFileOpen ? sCurrentPri : 0;
}

static bool playInternal(SystemSound sound, bool safetyEmergency, bool testLoop) {
  if (systemSoundIsReserved(sound)) {
    Serial.println("[AUDIO] SHUTDOWN is reserved — not implemented");
    setStatusErr("SHUTDOWN reserved");
    return false;
  }

  const SoundDef *def = findDef(sound);
  if (!def) return false;

  if (sound == SystemSound::Error && !safetyEmergency) {
    const uint32_t now = millis();
    if (sCurrent == SystemSound::Error && sFileOpen) {
      Serial.println("[AUDIO] ERROR already playing — ignored");
      return false;
    }
    if (now < sErrorStormUntil) {
      Serial.println("[AUDIO] ERROR retrigger suppressed");
      return false;
    }
  }

  const uint8_t incoming = safetyEmergency ? (uint8_t)3 : def->priority;
  if (sSafetyEmergency && !safetyEmergency) {
    Serial.printf("[AUDIO] %s rejected — EMERGENCY has priority\n", def->name);
    setStatusErr("emergency has priority");
    return false;
  }
  if (incoming < livePriority()) {
    Serial.printf("[AUDIO] %s rejected — lower priority than %s\n",
                  def->name, systemSoundName(sCurrent));
    setStatusErr("lower priority");
    return false;
  }

  if (sFileOpen) {
    Serial.printf("[AUDIO] Interrupting %s for %s\n",
                  systemSoundName(sCurrent), def->name);
    closeFile();
  }

  sSafetyEmergency = safetyEmergency;
  sTestLoop = testLoop && (sound == SystemSound::Emergency) && !safetyEmergency;

  if (!sHwInited && !stageAudioInitHardware()) {
    sStatus.playbackFailed = true;
    refreshHealth();
    return false;
  }

  scanAssets();
  const bool loop = safetyEmergency || sTestLoop || def->looping;
  if (!openSystemWav(sound, loop)) {
    sStatus.playbackFailed = true;
    if (safetyEmergency) {
      sLastOpenFailMs = millis();
      Serial.printf("[AUDIO] Emergency WAV failed: %s — latch is independent\n",
                    sStatus.lastError);
    } else {
      Serial.printf("[AUDIO] %s failed: %s\n", def->name, sStatus.lastError);
    }
    refreshHealth();
    return false;
  }

  if (sound == SystemSound::Error) {
    sLastErrorPlayMs = millis();
    sErrorStormUntil = sLastErrorPlayMs + kErrorRetriggerMs;
  }

  Serial.printf("[AUDIO] Started %s  loop=%s  safety=%s\n",
                def->name, loop ? "yes" : "no", safetyEmergency ? "yes" : "no");
  return true;
}

bool stageAudioBegin() {
  memset(&sStatus, 0, sizeof(sStatus));
  sCurrent = SystemSound::None;
  sCurrentPri = 0;
  sSafetyEmergency = false;
  sTestLoop = false;
  sBootRequested = false;
  sHwInited = false;
  setStatusErr("audio idle");
  strncpy(sStatus.currentName, "NONE", sizeof(sStatus.currentName) - 1);
  strncpy(sStatus.healthName, "CODEC_FAULT", sizeof(sStatus.healthName) - 1);

  Serial.println("[AUDIO] Showduino System Audio");
  Serial.println("[AUDIO] Role: P4 onboard ES8311 speaker = system/safety only");
  Serial.println("[AUDIO] Attraction/programme audio remains Audio-Node-only");
  Serial.println("[AUDIO] LEGACY PCM5102A GPIO20/21/22 is compiled out");
  Serial.printf("[AUDIO] ES8311 I2C=0x%02X SDA=%d SCL=%d\n",
                P4_ES8311_I2C_ADDR, P4_SYSTEM_AUDIO_I2C_SDA, P4_SYSTEM_AUDIO_I2C_SCL);
  Serial.printf("[AUDIO] I2S MCLK=%d BCLK=%d WS=%d DOUT=%d DIN=%d (unused)\n",
                P4_SYSTEM_AUDIO_I2S_MCLK, P4_SYSTEM_AUDIO_I2S_BCLK,
                P4_SYSTEM_AUDIO_I2S_WS, P4_SYSTEM_AUDIO_I2S_DOUT,
                P4_SYSTEM_AUDIO_I2S_DIN);
  Serial.printf("[AUDIO] NS4150B PA GPIO%d active-%s\n",
                P4_SYSTEM_AUDIO_PA_ENABLE,
                P4_SYSTEM_AUDIO_PA_ON_LEVEL == HIGH ? "HIGH" : "LOW");
  Serial.println("[AUDIO] GPIO10 is ES8311 LRCK — not a status LED");

  if (P4_SYSTEM_AUDIO_PA_ENABLE >= 0) {
    pinMode(P4_SYSTEM_AUDIO_PA_ENABLE, OUTPUT);
    setPa(false);
  }

  scanAssets(true);
  if (sStatus.storageReady) {
    Serial.printf("[AUDIO] System assets %u/7  shutdown=%s\n",
                  (unsigned)sStatus.activeAssets,
                  sStatus.shutdownAssetPresent ? "present (reserved unused)" : "absent (ok)");
    if (sStatus.wavPresent) {
      Serial.printf("[AUDIO] Emergency WAV: %s\n", sStatus.wavPath);
    } else {
      Serial.printf("[AUDIO] Emergency WAV missing (%s or %s/emergency.wav) — latch still works\n",
                    PATH_SYSTEM_EMERGENCY_WAV, PATH_AUDIO_SYSTEM_LIBRARY);
    }
  } else {
    Serial.println("[AUDIO] SD not mounted — system WAV unavailable");
  }

  refreshHealth();
  return true;
}

bool stageAudioInitHardware() {
  if (sHwInited && es8311Info().initialized) {
    refreshHealth();
    return true;
  }

  Serial.println("[AUDIO] Initialising onboard ES8311");
  if (!es8311Detect()) {
    Serial.printf("[AUDIO] ES8311 not detected: %s\n", es8311Info().lastError);
    Serial.println("[AUDIO] Local system audio fault — Showduino continues");
    sHwInited = false;
    refreshHealth();
    return false;
  }
  Serial.printf("[AUDIO] ES8311 ACK 0x%02X  id=%02X%02X\n",
                P4_ES8311_I2C_ADDR, es8311Info().chipId1, es8311Info().chipId2);

  if (!es8311Begin(kDefaultRate, kVolumePercent)) {
    Serial.printf("[AUDIO] ES8311 init failed: %s\n", es8311Info().lastError);
    Serial.println("[AUDIO] Local system audio fault — Showduino continues");
    refreshHealth();
    return false;
  }

  if (!startI2s(kDefaultRate)) {
    Serial.printf("[AUDIO] I2S init failed: %s\n", sStatus.lastError);
    Serial.println("[AUDIO] Local system audio fault — Showduino continues");
    refreshHealth();
    return false;
  }

  (void)es8311SetMute(true);
  setPa(false);
  sHwInited = true;
  sStatus.playbackFailed = false;
  setStatusErr("hardware ready");
  Serial.println("[AUDIO] Codec: ES8311 READY");
  Serial.println("[AUDIO] I2S: READY");
  Serial.println("[AUDIO] Amplifier: DISABLED (idle)");
  refreshHealth();
  return true;
}

void stageAudioRequestBoot() {
  sBootRequested = true;
}

void stageAudioOnDirectorHello() {
  Serial.println("[AUDIO] Director startup detected — queue BOOT");
  sBootRequested = true;
}

void stageAudioRefreshAssets() {
  scanAssets(false);
}

void stageAudioStopNotifications() {
  if (sSafetyEmergency) return;
  if (sFileOpen || sCurrent != SystemSound::None) {
    Serial.println("[AUDIO] System notification stopped");
  }
  finishPlayback("idle");
}

void stageAudioStop() {
  if (sSafetyEmergency) {
    Serial.println("[AUDIO] STOP rejected — safety emergency is latched");
    setStatusErr("emergency has priority");
    return;
  }
  stageAudioStopNotifications();
}

bool stageAudioStartEmergency() {
  stageAudioStopNotifications();
  scanAssets();
  if (!playInternal(SystemSound::Emergency, true, false)) {
    sSafetyEmergency = true;
    sCurrent = SystemSound::Emergency;
    sCurrentPri = 3;
    sStatus.emergencyPlaying = false;
    sLastOpenFailMs = millis();
    refreshHealth();
    return false;
  }
  return true;
}

void stageAudioStopEmergency() {
  if (sSafetyEmergency || sCurrent == SystemSound::Emergency) {
    Serial.println("[AUDIO] Emergency WAV stopped — idle (no resume)");
  }
  sSafetyEmergency = false;
  sTestLoop = false;
  finishPlayback("idle");
}

bool stageAudioPlay(SystemSound sound) {
  return playInternal(sound, false, false);
}

bool stageAudioPlayTest(SystemSound sound) {
  if (sound == SystemSound::Shutdown) {
    setStatusErr("SHUTDOWN reserved");
    return false;
  }
  if (sound == SystemSound::Emergency) {
    Serial.println("[AUDIO] TEST:EMERGENCY plays WAV only — latch unchanged");
    return playInternal(sound, false, true);
  }
  return playInternal(sound, false, false);
}

void stageAudioLoop() {
  if (sBootRequested) {
    sBootRequested = false;
    if (sSafetyEmergency) {
      Serial.println("[AUDIO] BOOT skipped — emergency active");
    } else if (!sHwInited || !es8311Info().initialized) {
      Serial.println("[AUDIO] BOOT skipped — output path not ready");
    } else if (!stageStorageIsReady()) {
      Serial.println("[AUDIO] BOOT skipped — SD not mounted");
    } else if (sCurrent == SystemSound::Boot && sFileOpen) {
      Serial.println("[AUDIO] BOOT already playing");
    } else {
      if (!stageAudioPlay(SystemSound::Boot)) {
        Serial.printf("[AUDIO] BOOT not played: %s\n", sStatus.lastError);
      }
    }
  }

  if (sSafetyEmergency && !sFileOpen) {
    uint32_t now = millis();
    if (now - sLastOpenFailMs >= 5000UL) {
      sLastOpenFailMs = now;
      Serial.println("[AUDIO] Retrying emergency WAV");
      (void)stageAudioStartEmergency();
    }
    refreshHealth();
    return;
  }

  if (sFileOpen) {
    pumpPlayback();
  } else {
    refreshHealth();
  }
}

const StageAudioStatus &stageAudioStatus() {
  return sStatus;
}

bool stageAudioIsEmergencyPlaying() {
  return sSafetyEmergency && sFileOpen && sStatus.playbackActive;
}

bool stageAudioIsPlaying() {
  return sFileOpen && sStatus.playbackActive && !sSafetyEmergency;
}

bool stageAudioInspectWav(const char *path, StageWavInfo *out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  if (!path || !path[0] || !stageStorageIsReady() || !stageStorageFs().exists(path)) {
    setErr(out->error, sizeof(out->error), "WAV missing");
    return false;
  }

  File f = stageStorageFs().open(path, FILE_READ);
  if (!f) {
    setErr(out->error, sizeof(out->error), "WAV open failed");
    return false;
  }

  uint32_t dataStart = 0;
  uint32_t dataSize = 0;
  uint32_t rate = 0;
  uint16_t channels = 0;
  uint16_t bits = 0;
  if (!parseWav(f, &dataStart, &dataSize, &rate, &channels, &bits,
                out->error, sizeof(out->error))) {
    f.close();
    return false;
  }

  out->valid = true;
  out->pcm = true;
  out->bits = bits;
  out->channels = channels;
  out->sampleRate = rate;
  out->dataBytes = dataSize;
  out->engineSupported = (bits == 16 && (channels == 1 || channels == 2) &&
                          (rate == 32000 || rate == 44100 || rate == 48000));

  f.seek(dataStart);
  uint8_t sample[64];
  size_t n = (dataSize < sizeof(sample)) ? (size_t)dataSize : sizeof(sample);
  int got = f.read(sample, n);
  f.close();
  if (got > 0) {
    for (int i = 0; i < got; i++) {
      if (sample[i] != 0) {
        out->dataNonZero = true;
        break;
      }
    }
  }
  if (!out->engineSupported) {
    setErr(out->error, sizeof(out->error), "format not supported");
  } else if (!out->dataNonZero) {
    setErr(out->error, sizeof(out->error), "data chunk empty or silent");
  }
  return out->valid;
}

bool stageAudioI2sStarted() {
  return sI2sStarted && sTx != nullptr;
}

uint32_t stageAudioI2sBytesWritten() {
  return sI2sBytesWritten;
}

uint32_t stageAudioEmergencyLoopCount() {
  return sEmergencyLoopCount;
}

void stageAudioResetDiagCounters() {
  sI2sBytesWritten = 0;
  sEmergencyLoopCount = 0;
}

void stageAudioPrintStatus() {
  refreshHealth();
  scanAssets(true);
  Serial.println("[AUDIO] Showduino System Audio");
  Serial.printf("[AUDIO] Codec: ES8311 %s\n",
                sStatus.codecReady ? "READY" :
                (sStatus.codecDetected ? "DETECTED" : "FAULT"));
  Serial.printf("[AUDIO] I2S: %s\n", sStatus.i2sReady ? "READY" : "FAULT");
  Serial.printf("[AUDIO] Amplifier: %s\n", sStatus.amplifierEnabled ? "ENABLED" : "DISABLED");
  Serial.printf("[AUDIO] Storage: %s\n", sStatus.storageReady ? "READY" : "NO_SD");
  Serial.printf("[AUDIO] Assets: %u/7  shutdown=%s\n",
                (unsigned)sStatus.activeAssets,
                sStatus.shutdownAssetPresent ? "present" : "absent");
  Serial.printf("[AUDIO] Current: %s\n", sStatus.currentName);
  Serial.printf("[AUDIO] State: %s\n",
                sStatus.playState == SystemAudioPlayState::Emergency ? "EMERGENCY" :
                (sStatus.playState == SystemAudioPlayState::Playing ? "PLAYING" :
                 (sStatus.playState == SystemAudioPlayState::Failed ? "FAILED" : "IDLE")));
  Serial.printf("[AUDIO] Health: %s\n", sStatus.healthName);
  Serial.printf("[AUDIO] Path: %s\n",
                sStatus.selectedPath[0] ? sStatus.selectedPath : "-");
  Serial.printf("[AUDIO] Last: %s\n", sStatus.lastError);
  Serial.printf("[AUDIO] Truth: started=%d active=%d failed=%d bytes=%lu loops=%lu\n",
                (int)sStatus.playbackStarted, (int)sStatus.playbackActive,
                (int)sStatus.playbackFailed,
                (unsigned long)sI2sBytesWritten,
                (unsigned long)sEmergencyLoopCount);
}

bool stageAudioHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (!command || !reply || replyLen == 0) return false;
  reply[0] = '\0';

  if (strcmp(command, "AUDIO:STATUS") == 0) {
    stageAudioPrintStatus();
    snprintf(reply, replyLen, "AUDIO:STATUS:%s", sStatus.healthName);
    return true;
  }
  if (strcmp(command, "AUDIO:STOP") == 0 || strcmp(command, "AUDIO:LOCAL:STOP") == 0) {
    if (sSafetyEmergency) {
      snprintf(reply, replyLen, "REJECTED:AUDIO:EMERGENCY_ACTIVE");
      return true;
    }
    stageAudioStop();
    snprintf(reply, replyLen, "ACK:AUDIO:STOP");
    return true;
  }
  if (strcmp(command, "AUDIO:TEST:SHUTDOWN") == 0) {
    snprintf(reply, replyLen, "REJECTED:AUDIO:SHUTDOWN_RESERVED");
    return true;
  }

  static const char *kTests[][2] = {
    { "AUDIO:TEST:BOOT", "BOOT" },
    { "AUDIO:TEST:EMERGENCY", "EMERGENCY" },
    { "AUDIO:TEST:BEEP", "BEEP" },
    { "AUDIO:TEST:TONE", "TONE" },
    { "AUDIO:TEST:ERROR", "ERROR" },
    { "AUDIO:TEST:ACCEPTED", "ACCEPTED" },
    { "AUDIO:TEST:COMPLETE", "COMPLETE" },
  };
  for (size_t i = 0; i < sizeof(kTests) / sizeof(kTests[0]); i++) {
    if (strcmp(command, kTests[i][0]) != 0) continue;
    const SystemSound sound = systemSoundFromName(kTests[i][1]);
    if (sSafetyEmergency && sound != SystemSound::Emergency) {
      snprintf(reply, replyLen, "REJECTED:AUDIO:EMERGENCY_ACTIVE");
      return true;
    }
    if (stageAudioPlayTest(sound)) {
      snprintf(reply, replyLen, "ACK:%s", kTests[i][0]);
    } else {
      snprintf(reply, replyLen, "ERR:%s:%s", kTests[i][0], sStatus.lastError);
    }
    return true;
  }

  if (strncmp(command, "AUDIO:PLAY", 10) == 0 ||
      strncmp(command, "AUDIO:LOCAL:PLAY", 16) == 0 ||
      strncmp(command, "AUDIO:SHOW:", 11) == 0) {
    Serial.println("[AUDIO] Attraction/programme play rejected — use AUDIO:NODE:*");
    snprintf(reply, replyLen, "REJECTED:AUDIO:LOCAL:ATTRACTION_FORBIDDEN");
    return true;
  }

  return false;
}
