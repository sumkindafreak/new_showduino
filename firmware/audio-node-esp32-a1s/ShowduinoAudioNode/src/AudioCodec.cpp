#include "AudioCodec.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_audio_node.h"
#include <Wire.h>

static bool sReady = false;
static uint8_t sAddr = SHOWDUINO_AUDIO_I2C_ADDR;
static char sErr[40] = "";
static char sOutput[12] = "SPEAKER";
static uint8_t sLastPercent = 80;

static void setErr(const char *m) {
  strncpy(sErr, m ? m : "", sizeof(sErr) - 1);
}

static bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(sAddr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
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

static uint8_t outVol(uint8_t percent) {
  return (uint8_t)((percent * 30) / 100);
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
  wr(0x01, 0x50);
  wr(0x02, 0x00);
  wr(0x08, 0x00);
  wr(0x04, 0xC0);
  wr(0x17, 0x18);
  wr(0x18, 0x02);
  wr(0x19, 0x00);
  wr(0x1A, 0x00);
  wr(0x1B, 0x00);
  wr(0x26, 0x80);
  wr(0x27, 0xB8);
  wr(0x2A, 0xB8);
  wr(0x2E, 0x1E);
  wr(0x2F, 0x1E);
  wr(0x30, 0x1E);
  wr(0x31, 0x1E);
  wr(0x04, 0x3C);

#if SHOWDUINO_AUDIO_HP_DETECT_PIN >= 0
  pinMode(SHOWDUINO_AUDIO_HP_DETECT_PIN, INPUT);
#endif
  audioCodecApplyOutput(SHOWDUINO_AUDIO_DEFAULT_OUTPUT);
  audioCodecSetVolume(SHOWDUINO_AUDIO_DEFAULT_VOLUME);
  audioCodecMute(false);
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

void audioCodecApplyOutput(const char *mode) {
  const char *use = mode && mode[0] ? mode : "SPEAKER";
  if (!strcmp(use, "AUTO")) {
    use = audioCodecHpInserted() ? "HEADPHONE" : "SPEAKER";
  }
  if (!showduino_audio_output_ok(use) || !strcmp(use, "AUTO")) use = "SPEAKER";
  strncpy(sOutput, use, sizeof(sOutput) - 1);
  audioCodecSetVolume(sLastPercent);
}

bool audioCodecSetVolume(uint8_t percent) {
  if (percent > 100) percent = 100;
  sLastPercent = percent;
  const uint8_t d = dacVol(percent);
  const uint8_t o = outVol(percent);
  if (!wr(0x1A, d) || !wr(0x1B, d)) return false;
  const bool hp = !strcmp(sOutput, "HEADPHONE") || !strcmp(sOutput, "LINE");
  const bool spk = !strcmp(sOutput, "SPEAKER") || !strcmp(sOutput, "LINE");
  wr(0x2E, hp ? o : 0);
  wr(0x2F, hp ? o : 0);
  wr(0x30, spk ? o : 0);
  wr(0x31, spk ? o : 0);
  audioCodecSetPa(!strcmp(sOutput, "SPEAKER") && percent > 0);
  return true;
}

void audioCodecMute(bool mute) {
  wr(0x19, mute ? 0x24 : 0x00);
  if (mute) audioCodecSetPa(false);
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
  wr(0x10, 0x00);
  wr(0x11, 0x00);
  Serial.println("[AUDIO] ES8388 ADC/MIC path enabled (LIN1/RIN1, +24 dB PGA)");
  return true;
}
