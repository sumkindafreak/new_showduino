#include "AudioPlayback.h"
#include "AudioStorage.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_audio_node.h"

#include <ESP_I2S.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static I2SClass sI2s;
static SemaphoreHandle_t sLock = nullptr;
static File sFile;
static bool sOpen = false;
static bool sPaused = false;
static bool sLoop = false;
static bool sI2sOn = false;
static volatile bool sCompleted = false;
static volatile bool sFailed = false;
static uint32_t sDataStart = 0;
static uint32_t sDataBytes = 0;
static uint32_t sDataPos = 0;
static uint16_t sChannels = 2;
static uint32_t sRate = 44100;
static uint32_t sUnderrun = 0;
static uint32_t sBytes = 0;
static uint32_t sPlayed = 0;
static uint32_t sDone = 0;
static uint32_t sFail = 0;
static char sPath[SHOWDUINO_AUDIO_PATH_MAX + 1] = "";
static char sRel[SHOWDUINO_AUDIO_REL_MAX + 1] = "";
static char sErr[40] = "";
static uint8_t sBuf[512];

static void setErr(const char *m) {
  strncpy(sErr, m ? m : "", sizeof(sErr) - 1);
}

static uint16_t rd16(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t rd32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool parseWav(File &f) {
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12) return false;
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return false;
  bool fmt = false;
  uint16_t format = 0, bits = 0, ch = 0;
  uint32_t rate = 0;
  while (f.available()) {
    uint8_t ck[8];
    if (f.read(ck, 8) != 8) return false;
    const uint32_t sz = rd32(ck + 4);
    if (memcmp(ck, "fmt ", 4) == 0) {
      uint8_t fmtb[16];
      if (sz < 16 || f.read(fmtb, 16) != 16) return false;
      if (sz > 16) f.seek(f.position() + (sz - 16));
      format = rd16(fmtb);
      ch = rd16(fmtb + 2);
      rate = rd32(fmtb + 4);
      bits = rd16(fmtb + 14);
      fmt = true;
    } else if (memcmp(ck, "data", 4) == 0) {
      if (!fmt || !showduino_audio_wav_pcm_ok(format, ch, rate, bits)) return false;
      sChannels = ch;
      sRate = rate;
      sDataStart = f.position();
      sDataBytes = sz;
      sDataPos = 0;
      return true;
    } else {
      f.seek(f.position() + sz + (sz & 1));
    }
  }
  return false;
}

static bool startI2s() {
  if (sI2sOn) sI2s.end();
  sI2s.setPins(SHOWDUINO_AUDIO_I2S_BCLK, SHOWDUINO_AUDIO_I2S_WS,
               SHOWDUINO_AUDIO_I2S_DOUT, SHOWDUINO_AUDIO_I2S_DIN,
               SHOWDUINO_AUDIO_I2S_MCLK);
  if (!sI2s.begin(I2S_MODE_STD, sRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    setErr("I2S begin failed");
    sI2sOn = false;
    return false;
  }
  sI2sOn = true;
  return true;
}

static void playbackTask(void *) {
  for (;;) {
    audioPlaybackService();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

bool audioPlaybackBegin() {
  sOpen = false;
  sPaused = false;
  if (!sLock) sLock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(playbackTask, "audioWav", 4096, nullptr, 4, nullptr, 1);
  return true;
}

static void lockPlayback() {
  if (sLock) xSemaphoreTake(sLock, portMAX_DELAY);
}
static void unlockPlayback() {
  if (sLock) xSemaphoreGive(sLock);
}

bool audioPlaybackStart(const char *absPath, AudioPlayMode mode) {
  lockPlayback();
  if (sOpen) {
    sFile.close();
    sOpen = false;
  }
  if (sI2sOn) {
    sI2s.end();
    sI2sOn = false;
  }
  sCompleted = false;
  sFailed = false;
  if (!absPath || !audioStorageReady() || !SD.exists(absPath)) {
    setErr("FILE_NOT_FOUND");
    sFailed = true;
    sFail++;
    unlockPlayback();
    return false;
  }
  sFile = SD.open(absPath, FILE_READ);
  if (!sFile) {
    setErr("open failed");
    sFailed = true;
    sFail++;
    unlockPlayback();
    return false;
  }
  if (!parseWav(sFile)) {
    sFile.close();
    setErr("unsupported WAV");
    sFailed = true;
    sFail++;
    unlockPlayback();
    return false;
  }
  if (!startI2s()) {
    sFile.close();
    sFailed = true;
    sFail++;
    unlockPlayback();
    return false;
  }
  strncpy(sPath, absPath, sizeof(sPath) - 1);
  const char *rel = absPath;
  if (strncmp(absPath, SHOWDUINO_AUDIO_ROOT "/", strlen(SHOWDUINO_AUDIO_ROOT) + 1) == 0) {
    rel = absPath + strlen(SHOWDUINO_AUDIO_ROOT) + 1;
  }
  strncpy(sRel, rel, sizeof(sRel) - 1);
  sLoop = (mode == AudioPlayMode::Loop);
  sOpen = true;
  sPaused = false;
  sPlayed++;
  setErr("");
  unlockPlayback();
  return true;
}

void audioPlaybackStop() {
  lockPlayback();
  if (sOpen) sFile.close();
  sOpen = false;
  sPaused = false;
  sLoop = false;
  if (sI2sOn) {
    sI2s.end();
    sI2sOn = false;
  }
  unlockPlayback();
}

bool audioPlaybackPause() {
  lockPlayback();
  const bool ok = sOpen && !sPaused;
  if (ok) sPaused = true;
  unlockPlayback();
  return ok;
}

bool audioPlaybackResume() {
  lockPlayback();
  const bool ok = sOpen && sPaused;
  if (ok) sPaused = false;
  unlockPlayback();
  return ok;
}

void audioPlaybackService() {
  if (!sLock || xSemaphoreTake(sLock, 0) != pdTRUE) return;
  if (!sOpen || sPaused) {
    xSemaphoreGive(sLock);
    return;
  }
  const uint32_t remain = (sDataPos < sDataBytes) ? (sDataBytes - sDataPos) : 0;
  if (remain == 0) {
    if (sLoop) {
      sFile.seek(sDataStart);
      sDataPos = 0;
      /* Immediate refill — not sample-accurate, but avoids an extra task slice. */
    } else {
    sFile.close();
    sOpen = false;
    sPaused = false;
    if (sI2sOn) {
      sI2s.end();
      sI2sOn = false;
    }
      sCompleted = true;
      sDone++;
      xSemaphoreGive(sLock);
      return;
    }
  }
  const uint32_t remainNow = (sDataPos < sDataBytes) ? (sDataBytes - sDataPos) : 0;
  if (remainNow == 0) {
    xSemaphoreGive(sLock);
    return;
  }
  size_t want = remainNow > sizeof(sBuf) ? sizeof(sBuf) : (size_t)remainNow;
  if (sChannels == 1 && want > 256) want = 256;
  const int n = sFile.read(sBuf, want);
  if (n <= 0) {
    setErr("read failed");
    sFile.close();
    sOpen = false;
    if (sI2sOn) {
      sI2s.end();
      sI2sOn = false;
    }
    sFailed = true;
    sFail++;
    xSemaphoreGive(sLock);
    return;
  }
  sDataPos += (uint32_t)n;
  sBytes += (uint32_t)n;
  if (sChannels == 1) {
    int16_t stereo[256];
    const int samples = n / 2;
    const int16_t *mono = (const int16_t *)sBuf;
    for (int i = 0; i < samples && i < 128; i++) {
      stereo[i * 2] = mono[i];
      stereo[i * 2 + 1] = mono[i];
    }
    const size_t out = (size_t)samples * 4;
    if (sI2s.write((uint8_t *)stereo, out) != out) sUnderrun++;
  } else if (sI2s.write(sBuf, (size_t)n) != (size_t)n) {
    sUnderrun++;
  }
  xSemaphoreGive(sLock);
}

bool audioPlaybackActive() { return sOpen && !sPaused; }
bool audioPlaybackPaused() { return sOpen && sPaused; }
bool audioPlaybackLooping() { return sOpen && sLoop; }
bool audioPlaybackJustCompleted() {
  if (!sCompleted) return false;
  sCompleted = false;
  return true;
}
bool audioPlaybackJustFailed() {
  if (!sFailed) return false;
  sFailed = false;
  return true;
}
const char *audioPlaybackPath() { return sPath; }
const char *audioPlaybackRel() { return sRel; }
uint32_t audioPlaybackPosition() { return sDataPos; }
uint32_t audioPlaybackDuration() { return sDataBytes; }
uint32_t audioPlaybackUnderruns() { return sUnderrun; }
uint32_t audioPlaybackBytesRead() { return sBytes; }
uint32_t audioPlaybackFilesPlayed() { return sPlayed; }
uint32_t audioPlaybackCompleted() { return sDone; }
uint32_t audioPlaybackFailed() { return sFail; }
const char *audioPlaybackLastError() { return sErr; }
