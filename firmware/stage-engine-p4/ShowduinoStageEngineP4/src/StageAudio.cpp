#include "StageAudio.h"

#include <string.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "StageStorage.h"

static StageAudioStatus sStatus;
static StageAudioMode sMode = StageAudioMode::Idle;

static File sFile;
static bool sFileOpen = false;
static uint32_t sDataStart = 0;
static uint32_t sDataSize = 0;
static uint32_t sDataPos = 0;
static uint32_t sSampleRate = 32000;
static uint16_t sChannels = 2;
static uint16_t sBits = 16;
static bool sLooping = false;

static i2s_chan_handle_t sTx = nullptr;
static bool sI2sStarted = false;
static uint32_t sI2sRate = 0;
static uint16_t sI2sChannels = 0;

static uint8_t sBuf[2048];
static size_t sBufLen = 0;
static size_t sBufOff = 0;

static uint32_t sLastOpenFailMs = 0;

static bool i2sPinsAssigned() {
  return P4_AUDIO_I2S_BCLK >= 0 && P4_AUDIO_I2S_WS >= 0 && P4_AUDIO_I2S_DOUT >= 0;
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

static bool parseWav(File &f, uint32_t *dataStart, uint32_t *dataSize,
                     uint32_t *rate, uint16_t *channels, uint16_t *bits) {
  if (!f) return false;
  f.seek(0);
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12) return false;
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return false;

  bool gotFmt = false;
  uint32_t dataOff = 0;
  uint32_t dataLen = 0;
  uint32_t sr = 0;
  uint16_t ch = 0;
  uint16_t bps = 0;

  while (f.available()) {
    uint8_t chunk[8];
    if (f.read(chunk, 8) != 8) break;
    uint32_t sz = (uint32_t)chunk[4] | ((uint32_t)chunk[5] << 8) |
                  ((uint32_t)chunk[6] << 16) | ((uint32_t)chunk[7] << 24);
    uint32_t chunkPos = f.position();

    if (memcmp(chunk, "fmt ", 4) == 0) {
      uint8_t fmt[16];
      size_t n = (sz > 16) ? 16 : (size_t)sz;
      if (f.read(fmt, n) != (int)n) return false;
      uint16_t format = (uint16_t)fmt[0] | ((uint16_t)fmt[1] << 8);
      ch = (uint16_t)fmt[2] | ((uint16_t)fmt[3] << 8);
      sr = (uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) |
           ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
      bps = (uint16_t)fmt[14] | ((uint16_t)fmt[15] << 8);
      if (format != 1) {
        strncpy(sStatus.lastError, "WAV not PCM", sizeof(sStatus.lastError) - 1);
        return false;
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

  if (!gotFmt || dataLen == 0 || (ch != 1 && ch != 2) || (bps != 16 && bps != 8)) {
    strncpy(sStatus.lastError, "WAV header invalid", sizeof(sStatus.lastError) - 1);
    return false;
  }

  *dataStart = dataOff;
  *dataSize = dataLen;
  *rate = sr;
  *channels = ch;
  *bits = bps;
  return true;
}

static void stopI2s() {
  if (sTx) {
    i2s_channel_disable(sTx);
    i2s_del_channel(sTx);
    sTx = nullptr;
  }
  sI2sStarted = false;
  sI2sRate = 0;
  sI2sChannels = 0;
}

static bool startI2s(uint32_t rate, uint16_t channels, uint16_t bits) {
  if (!i2sPinsAssigned()) {
    strncpy(sStatus.lastError, "I2S pins not assigned", sizeof(sStatus.lastError) - 1);
    sStatus.i2sReady = false;
    return false;
  }
  if (bits != 16) {
    strncpy(sStatus.lastError, "I2S supports 16-bit PCM only", sizeof(sStatus.lastError) - 1);
    return false;
  }
  if (sI2sStarted && sTx && sI2sRate == rate && sI2sChannels == channels) {
    return true;
  }

  stopI2s();

  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num = 6;
  chanCfg.dma_frame_num = 240;
  chanCfg.auto_clear = true;

  if (i2s_new_channel(&chanCfg, &sTx, nullptr) != ESP_OK) {
    strncpy(sStatus.lastError, "I2S channel alloc failed", sizeof(sStatus.lastError) - 1);
    sTx = nullptr;
    return false;
  }

  i2s_std_config_t stdCfg = {};
  stdCfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
  stdCfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT,
      channels == 1 ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO);
  stdCfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  stdCfg.gpio_cfg.bclk = (gpio_num_t)P4_AUDIO_I2S_BCLK;
  stdCfg.gpio_cfg.ws = (gpio_num_t)P4_AUDIO_I2S_WS;
  stdCfg.gpio_cfg.dout = (gpio_num_t)P4_AUDIO_I2S_DOUT;
  stdCfg.gpio_cfg.din = I2S_GPIO_UNUSED;
  stdCfg.gpio_cfg.invert_flags.mclk_inv = false;
  stdCfg.gpio_cfg.invert_flags.bclk_inv = false;
  stdCfg.gpio_cfg.invert_flags.ws_inv = false;

  if (i2s_channel_init_std_mode(sTx, &stdCfg) != ESP_OK) {
    strncpy(sStatus.lastError, "I2S std init failed", sizeof(sStatus.lastError) - 1);
    stopI2s();
    return false;
  }
  if (i2s_channel_enable(sTx) != ESP_OK) {
    strncpy(sStatus.lastError, "I2S enable failed", sizeof(sStatus.lastError) - 1);
    stopI2s();
    return false;
  }

  sI2sStarted = true;
  sI2sRate = rate;
  sI2sChannels = channels;
  sStatus.i2sReady = true;
  strncpy(sStatus.lastError, "I2S ready", sizeof(sStatus.lastError) - 1);
  return true;
}

static bool openWavPath(const char *path, bool looping) {
  closeFile();
  if (!path || !path[0] || !stageStorageFs().exists(path)) {
    strncpy(sStatus.lastError, "WAV missing", sizeof(sStatus.lastError) - 1);
    return false;
  }

  sFile = stageStorageFs().open(path, FILE_READ);
  if (!sFile) {
    strncpy(sStatus.lastError, "WAV open failed", sizeof(sStatus.lastError) - 1);
    return false;
  }

  if (!parseWav(sFile, &sDataStart, &sDataSize, &sSampleRate, &sChannels, &sBits)) {
    sFile.close();
    return false;
  }
  Serial.printf("[AUDIO] WAV PCM %u-bit %s %lu Hz\n",
                (unsigned)sBits,
                sChannels == 1 ? "mono" : "stereo",
                (unsigned long)sSampleRate);
  if (!startI2s(sSampleRate, sChannels, sBits)) {
    sFile.close();
    return false;
  }

  sFile.seek(sDataStart);
  sFileOpen = true;
  sDataPos = 0;
  sBufLen = 0;
  sBufOff = 0;
  sLooping = looping;
  strncpy(sStatus.selectedPath, path, sizeof(sStatus.selectedPath) - 1);
  sStatus.selectedPath[sizeof(sStatus.selectedPath) - 1] = '\0';
  strncpy(sStatus.lastError, "playing", sizeof(sStatus.lastError) - 1);
  return true;
}

static void rewindEmergency() {
  if (!sFileOpen) return;
  sFile.seek(sDataStart);
  sDataPos = 0;
  sBufLen = 0;
  sBufOff = 0;
}

static void pumpPlayback() {
  if (!sFileOpen || !sTx) return;

  if (sBufOff >= sBufLen) {
    size_t remain = (sDataSize > sDataPos) ? (sDataSize - sDataPos) : 0;
    if (remain == 0) {
      if (sLooping && sMode == StageAudioMode::Emergency) {
        Serial.println("[ESTOP] Emergency audio EOF");
        Serial.println("[ESTOP] Restarting emergency audio");
        rewindEmergency();
        Serial.println("[ESTOP] Emergency audio loop restarted");
        remain = sDataSize;
      } else {
        closeFile();
        sMode = StageAudioMode::Idle;
        sStatus.emergencyPlaying = false;
        strncpy(sStatus.lastError, "finished", sizeof(sStatus.lastError) - 1);
        return;
      }
    }

    size_t want = sizeof(sBuf);
    if (want > remain) want = remain;
    int n = sFile.read(sBuf, want);
    if (n <= 0) {
      if (sLooping && sMode == StageAudioMode::Emergency) {
        Serial.println("[ESTOP] Emergency audio EOF");
        Serial.println("[ESTOP] Restarting emergency audio");
        rewindEmergency();
        Serial.println("[ESTOP] Emergency audio loop restarted");
      } else {
        closeFile();
        sMode = StageAudioMode::Idle;
        sStatus.emergencyPlaying = false;
      }
      return;
    }
    sBufLen = (size_t)n;
    sBufOff = 0;
    sDataPos += (uint32_t)n;
  }

  size_t todo = sBufLen - sBufOff;
  size_t written = 0;
  esp_err_t err = i2s_channel_write(sTx, sBuf + sBufOff, todo, &written, 0);
  if (err == ESP_OK || err == ESP_ERR_TIMEOUT) {
    sBufOff += written;
  }
}

static bool pickExisting(char *dest, size_t destLen, const char *primary, const char *fallback) {
  if (primary && primary[0] && stageStorageFs().exists(primary)) {
    strncpy(dest, primary, destLen - 1);
    dest[destLen - 1] = '\0';
    return true;
  }
  if (fallback && fallback[0] && stageStorageFs().exists(fallback)) {
    strncpy(dest, fallback, destLen - 1);
    dest[destLen - 1] = '\0';
    return true;
  }
  dest[0] = '\0';
  return false;
}

bool stageAudioBegin() {
  memset(&sStatus, 0, sizeof(sStatus));
  strncpy(sStatus.lastError, "audio idle", sizeof(sStatus.lastError) - 1);

  sStatus.wavPresent = pickExisting(sStatus.wavPath, sizeof(sStatus.wavPath),
                                    PATH_EMERGENCY_WAV, PATH_EMERGENCY_WAV_ROOT);
  if (sStatus.wavPresent) {
    Serial.printf("[SD] Emergency audio found: %s\n", sStatus.wavPath);
    Serial.printf("[SD] Emergency WAV: %s\n", sStatus.wavPath);
  } else {
    Serial.printf("[SD] Emergency WAV: MISSING (%s or %s)\n",
                  PATH_EMERGENCY_WAV, PATH_EMERGENCY_WAV_ROOT);
  }

  sStatus.mp3Present = pickExisting(sStatus.mp3Path, sizeof(sStatus.mp3Path),
                                    PATH_EMERGENCY_MP3, PATH_EMERGENCY_MP3_ROOT);
  if (sStatus.mp3Present) {
    Serial.printf("[SD] Emergency MP3: %s (present; PCM WAV is the selected engine format)\n",
                  sStatus.mp3Path);
  } else {
    Serial.printf("[SD] Emergency MP3: MISSING (%s or %s)\n",
                  PATH_EMERGENCY_MP3, PATH_EMERGENCY_MP3_ROOT);
  }

  if (i2sPinsAssigned()) {
    Serial.println("[AUDIO] PCM5102A I2S configuration");
    Serial.printf("[AUDIO] BCLK=%d\n", P4_AUDIO_I2S_BCLK);
    Serial.printf("[AUDIO] WS=%d\n", P4_AUDIO_I2S_WS);
    Serial.printf("[AUDIO] DOUT=%d\n", P4_AUDIO_I2S_DOUT);
    strncpy(sStatus.lastError, "I2S pins assigned", sizeof(sStatus.lastError) - 1);
  } else {
    sStatus.i2sReady = false;
    strncpy(sStatus.lastError, "I2S pins not assigned", sizeof(sStatus.lastError) - 1);
    Serial.println("[AUDIO] I2S pins not assigned in BoardConfig.h (BCLK/WS/DOUT are -1)");
    Serial.println("[AUDIO] Emergency audio will not reach a DAC until I2S pins are wired");
  }

  if (sStatus.wavPresent) {
    strncpy(sStatus.selectedPath, sStatus.wavPath, sizeof(sStatus.selectedPath) - 1);
    Serial.printf("[SD] Selected emergency audio: %s (PCM WAV)\n", sStatus.wavPath);
  } else if (sStatus.mp3Present) {
    Serial.println("[SD] Emergency MP3 present but no MP3 decoder in this firmware - WAV required");
    strncpy(sStatus.lastError, "WAV missing (MP3 unsupported)", sizeof(sStatus.lastError) - 1);
  } else {
    Serial.println("[SD] No emergency audio file found");
    strncpy(sStatus.lastError, "emergency audio missing", sizeof(sStatus.lastError) - 1);
  }

  return sStatus.wavPresent;
}

void stageAudioStopShow() {
  if (sMode == StageAudioMode::Emergency) return;
  if (sMode == StageAudioMode::Show || sFileOpen) {
    Serial.println("[AUDIO] Normal show audio stopped");
  }
  closeFile();
  sMode = StageAudioMode::Idle;
  sStatus.emergencyPlaying = false;
}

bool stageAudioStartEmergency() {
  stageAudioStopShow();

  if (!sStatus.wavPresent) {
    sStatus.wavPresent = pickExisting(sStatus.wavPath, sizeof(sStatus.wavPath),
                                      PATH_EMERGENCY_WAV, PATH_EMERGENCY_WAV_ROOT);
  }

  const char *path = sStatus.wavPath[0] ? sStatus.wavPath : PATH_EMERGENCY_WAV;
  Serial.printf("[ESTOP] Opening emergency audio: %s\n", path);
  if (!openWavPath(path, true)) {
    Serial.printf("[ESTOP] Emergency audio open failed: %s\n", sStatus.lastError);
    sMode = StageAudioMode::Emergency;
    sStatus.emergencyPlaying = false;
    sLastOpenFailMs = millis();
    return false;
  }

  sMode = StageAudioMode::Emergency;
  sStatus.emergencyPlaying = true;
  Serial.println("[ESTOP] Emergency audio started");
  return true;
}

void stageAudioStopEmergency() {
  if (sMode == StageAudioMode::Emergency || sStatus.emergencyPlaying) {
    closeFile();
    sStatus.emergencyPlaying = false;
  }
  sMode = StageAudioMode::Idle;
}

bool stageAudioStartShow(const char *path) {
  if (sMode == StageAudioMode::Emergency) return false;
  stageAudioStopShow();
  if (!openWavPath(path, false)) return false;
  sMode = StageAudioMode::Show;
  return true;
}

void stageAudioLoop() {
  if (sMode == StageAudioMode::Emergency) {
    if (!sFileOpen) {
      uint32_t now = millis();
      if (now - sLastOpenFailMs >= 5000UL) {
        sLastOpenFailMs = now;
        Serial.println("[ESTOP] Retrying emergency audio");
        (void)stageAudioStartEmergency();
      }
      return;
    }
    pumpPlayback();
    return;
  }

  if (sMode == StageAudioMode::Show && sFileOpen) {
    pumpPlayback();
  }
}

const StageAudioStatus &stageAudioStatus() {
  return sStatus;
}

bool stageAudioIsEmergencyPlaying() {
  return sMode == StageAudioMode::Emergency && sFileOpen;
}

bool stageAudioIsShowPlaying() {
  return sMode == StageAudioMode::Show && sFileOpen;
}
