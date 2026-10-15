#include "AudioCodec.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_audio_node.h"
#include <Wire.h>

static bool sReady = false;
static uint8_t sAddr = SHOWDUINO_AUDIO_I2C_ADDR;
static char sErr[40] = "";
static char sOutput[12] = "SPEAKER";
static uint8_t sLastPercent = 80;
static bool sMuted = true;
static bool sAutoOutput = false;
static bool sOutputRetryPending = false;
static bool sHpCandidate = false;
static uint32_t sHpChangedAt = 0;
static uint32_t sOutputAttemptAt = 0;
static constexpr uint32_t kJackDebounceMs = 150;
static constexpr uint32_t kOutputRetryMs = 1000;

static void setErr(const char *m) {
  strncpy(sErr, m ? m : "", sizeof(sErr) - 1);
}

static bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(sAddr);
  Wire.write(reg);
  Wire.write(val);
  const bool ok = Wire.endTransmission() == 0;
  if (!ok) {
    char error[40];
    snprintf(error, sizeof(error), "ES8388 write failed reg=0x%02X", reg);
    setErr(error);
    Serial.printf("[AUDIO][ERROR] %s\n", error);
  }
  return ok;
}

static bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static uint8_t dacVol(uint8_t percent) {
  if (percent == 0) return 0xC0;
  const int att = (int)((100 - percent) * 192 / 100);
  return (uint8_t)constrain(att, 0, 192);
}

bool audioCodecBegin() {
  sReady = false;
  setErr("");
  pinMode(SHOWDUINO_AUDIO_PA_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_AUDIO_PA_PIN, !SHOWDUINO_AUDIO_PA_ON_LEVEL);

  Wire.begin(SHOWDUINO_AUDIO_I2C_SDA, SHOWDUINO_AUDIO_I2C_SCL);
  Wire.setClock(100000);
  delay(20);

  if (probe(SHOWDUINO_AUDIO_I2C_ADDR)) {
    sAddr = SHOWDUINO_AUDIO_I2C_ADDR;
    Serial.println("[AUDIO] ES8388 detected");
  } else if (probe(SHOWDUINO_AUDIO_I2C_ADDR_ALT)) {
    sAddr = SHOWDUINO_AUDIO_I2C_ADDR_ALT;
    Serial.println("[AUDIO] ES8388 detected");
  } else {
    setErr("ES8388 not found");
    Serial.println("[AUDIO] ES8388 not found");
    return false;
  }

  /* Conservative ES8388 DAC-out init. Codec is I2S slave. */
  if (!wr(0x00, 0x80) || !wr(0x00, 0x00)) {
    setErr("ES8388 reset failed");
    return false;
  }
  // ES8388 slave, 16-bit I2S, MCLK/LRCK ratio 256, DAC-only mixers.
  // Clock enable and VREF setup follow Espressif's ES8388 driver.
  const uint8_t setup[][2] = {
    {0x01, 0x50}, {0x02, 0x00}, {0x08, 0x00}, {0x04, 0xC0},
    {0x00, 0x12}, {0x17, 0x18}, {0x18, 0x02}, {0x19, 0x04},
    {0x1A, 0x00}, {0x1B, 0x00}, {0x26, 0x00},
    {0x27, 0x90}, {0x2A, 0x90}, {0x2B, 0x80}, {0x2D, 0x00},
    {0x2E, 0x1E}, {0x2F, 0x1E}, {0x30, 0x1E}, {0x31, 0x1E},
    {0x02, 0xF0}, {0x02, 0x00}, {0x04, 0x3C}
  };
  for (const auto &setting : setup) {
    if (!wr(setting[0], setting[1])) return false;
  }

#if SHOWDUINO_AUDIO_HP_DETECT_PIN >= 0
  pinMode(SHOWDUINO_AUDIO_HP_DETECT_PIN, INPUT);
#endif
  if (!audioCodecApplyOutput(SHOWDUINO_AUDIO_DEFAULT_OUTPUT) ||
      !audioCodecSetVolume(SHOWDUINO_AUDIO_DEFAULT_VOLUME) || !wr(0x19, 0x00)) {
    audioCodecSetPa(false);
    return false;
  }
  sMuted = false;
  audioCodecSetPa(!strcmp(sOutput, "SPEAKER") && sLastPercent > 0);
  sReady = true;
  Serial.printf("[AUDIO] Codec initialized addr=0x%02X\n", (unsigned)sAddr);
  return true;
}

bool audioCodecReady() { return sReady; }

bool audioCodecHpInserted() {
#if SHOWDUINO_AUDIO_HP_DETECT_PIN >= 0
  return digitalRead(SHOWDUINO_AUDIO_HP_DETECT_PIN) == LOW;
#else
  return false;
#endif
}

const char *audioCodecOutputName() { return sOutput; }

bool audioCodecApplyOutput(const char *mode) {
  const char *use = mode && mode[0] ? mode : "SPEAKER";
  sAutoOutput = !strcmp(use, "AUTO");
  sOutputRetryPending = false;
  sHpCandidate = audioCodecHpInserted();
  sHpChangedAt = millis();
  sOutputAttemptAt = millis() - kOutputRetryMs;
  if (sAutoOutput) use = sHpCandidate ? "HEADPHONE" : "SPEAKER";
  if (!showduino_audio_output_ok(use) || !strcmp(use, "AUTO")) use = "SPEAKER";
  char previous[sizeof(sOutput)];
  memcpy(previous, sOutput, sizeof(previous));
  audioCodecSetPa(false);
  strncpy(sOutput, use, sizeof(sOutput) - 1);
  if (audioCodecSetVolume(sLastPercent)) return true;
  memcpy(sOutput, previous, sizeof(previous));
  sOutputRetryPending = sAutoOutput;
  sOutputAttemptAt = millis();
  audioCodecSetPa(false);
  return false;
}

void audioCodecService() {
  if (!sReady || !sAutoOutput) return;
  const uint32_t now = millis();
  const bool inserted = audioCodecHpInserted();
  if (inserted != sHpCandidate) {
    sHpCandidate = inserted;
    sHpChangedAt = now;
    return;
  }
  if ((uint32_t)(now - sHpChangedAt) < kJackDebounceMs) return;
  const char *target = inserted ? "HEADPHONE" : "SPEAKER";
  if (!sOutputRetryPending && !strcmp(target, sOutput)) return;
  if (sOutputRetryPending &&
      (uint32_t)(now - sOutputAttemptAt) < kOutputRetryMs) return;
  sOutputAttemptAt = now;
  char previous[sizeof(sOutput)];
  memcpy(previous, sOutput, sizeof(previous));
  audioCodecSetPa(false);
  snprintf(sOutput, sizeof(sOutput), "%s", target);
  // Reapply the current fade/duck volume without changing playback or mute.
  if (!audioCodecSetVolume(sLastPercent)) {
    sOutputRetryPending = true;
    memcpy(sOutput, previous, sizeof(previous));
    audioCodecSetPa(false);
    return;
  }
  sOutputRetryPending = false;
  Serial.printf("[AUDIO] Headphones %s -> %s (AUTO)\n",
                inserted ? "inserted" : "removed", sOutput);
}

bool audioCodecSetVolume(uint8_t percent) {
  if (percent > 100) percent = 100;
  sLastPercent = percent;
  const uint8_t d = dacVol(percent);
  // Keep enabled analogue outputs at unity; digital DAC attenuation controls volume.
  const uint8_t o = 0x1E;
  if (!wr(0x1A, d) || !wr(0x1B, d)) return false;
  const bool hp = !strcmp(sOutput, "HEADPHONE") || !strcmp(sOutput, "LINE");
  const bool spk = !strcmp(sOutput, "SPEAKER") || !strcmp(sOutput, "LINE");
  if (!wr(0x2E, hp ? o : 0) || !wr(0x2F, hp ? o : 0) ||
      !wr(0x30, spk ? o : 0) || !wr(0x31, spk ? o : 0)) {
    audioCodecSetPa(false);
    return false;
  }
  audioCodecSetPa(!sMuted && !strcmp(sOutput, "SPEAKER") && percent > 0);
  return true;
}

void audioCodecMute(bool mute) {
  if (wr(0x19, mute ? 0x24 : 0x00)) sMuted = mute;
  audioCodecSetPa(!sMuted && !strcmp(sOutput, "SPEAKER") && sLastPercent > 0);
}

void audioCodecSetPa(bool on) {
  digitalWrite(SHOWDUINO_AUDIO_PA_PIN,
               on ? SHOWDUINO_AUDIO_PA_ON_LEVEL : !SHOWDUINO_AUDIO_PA_ON_LEVEL);
}

uint8_t audioCodecI2cAddress() { return sAddr; }
const char *audioCodecLastError() { return sErr; }

bool audioCodecEnableInput() {
  if (!sReady) {
    setErr("codec not ready");
    return false;
  }
  /* ES8388 ADC + MICBIAS. LIN1/RIN1 = A161 onboard MIC1/MIC2. PGA +24 dB. */
  if (!wr(0x03, 0x00)) {
    setErr("ADC power failed");
    return false;
  }
  wr(0x09, 0x88);
  wr(0x0A, 0x00);
  wr(0x0B, 0x02);
  wr(0x0C, 0x0C);
  wr(0x0D, 0x02);  // ADC LRCK uses the same MCLK/256 ratio as the DAC.
  wr(0x10, 0x00);
  wr(0x11, 0x00);
  Serial.println("[AUDIO] ES8388 ADC/MIC path enabled (LIN1/RIN1, +24 dB PGA)");
  return true;
}
