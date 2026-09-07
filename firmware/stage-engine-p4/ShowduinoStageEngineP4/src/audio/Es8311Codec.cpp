#include "Es8311Codec.h"

#include <Wire.h>
#include <string.h>
#include "../../BoardConfig.h"

/*
 * Register sequence follows the public Espressif ES8311 driver
 * (Apache-2.0, espressif/esp-bsp components/es8311). Compacted for
 * Arduino-ESP32 3.3.11 + the shared Showduino Plug-in Bus Wire instance.
 *
 * Clock table entries used here: MCLK = sample_rate * 256
 *   44100 → 11.2896 MHz
 *   48000 → 12.288 MHz
 */

#define ES8311_RESET_REG00          0x00
#define ES8311_CLK_MANAGER_REG01    0x01
#define ES8311_CLK_MANAGER_REG02    0x02
#define ES8311_CLK_MANAGER_REG03    0x03
#define ES8311_CLK_MANAGER_REG04    0x04
#define ES8311_CLK_MANAGER_REG05    0x05
#define ES8311_CLK_MANAGER_REG06    0x06
#define ES8311_CLK_MANAGER_REG07    0x07
#define ES8311_CLK_MANAGER_REG08    0x08
#define ES8311_SDPIN_REG09          0x09
#define ES8311_SDPOUT_REG0A         0x0A
#define ES8311_SYSTEM_REG0D         0x0D
#define ES8311_SYSTEM_REG0E         0x0E
#define ES8311_SYSTEM_REG12         0x12
#define ES8311_SYSTEM_REG13         0x13
#define ES8311_ADC_REG1C            0x1C
#define ES8311_DAC_REG31            0x31
#define ES8311_DAC_REG32            0x32
#define ES8311_DAC_REG37            0x37
#define ES8311_CHIP_ID1             0xFD
#define ES8311_CHIP_ID2             0xFE

struct Es8311Coeff {
  uint32_t mclk;
  uint32_t rate;
  uint8_t pre_div;
  uint8_t pre_multi;
  uint8_t adc_div;
  uint8_t dac_div;
  uint8_t fs_mode;
  uint8_t lrck_h;
  uint8_t lrck_l;
  uint8_t bclk_div;
  uint8_t adc_osr;
  uint8_t dac_osr;
};

/* Known-good MCLK=256*Fs rows from the Espressif coefficient table. */
static const Es8311Coeff kCoeff[] = {
  { 8192000, 32000, 1, 0, 1, 1, 0, 0x00, 0xff, 4, 0x10, 0x10},
  {11289600, 44100, 1, 0, 1, 1, 0, 0x00, 0xff, 4, 0x10, 0x10},
  {12288000, 48000, 1, 0, 1, 1, 0, 0x00, 0xff, 4, 0x10, 0x10},
};

static Es8311Info sInfo;
static bool sWireOk = false;

static void setErr(const char *msg) {
  strncpy(sInfo.lastError, msg, sizeof(sInfo.lastError) - 1);
  sInfo.lastError[sizeof(sInfo.lastError) - 1] = '\0';
}

static bool ensureWire() {
  if (sWireOk) return true;
  if (!Wire.begin(P4_SYSTEM_AUDIO_I2C_SDA, P4_SYSTEM_AUDIO_I2C_SCL,
                  SHOWDUINO_PLUGIN_BUS_HZ)) {
    setErr("I2C begin failed");
    return false;
  }
  Wire.setTimeOut((uint16_t)SHOWDUINO_PLUGIN_BUS_TIMEOUT_MS);
  sWireOk = true;
  return true;
}

static bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(P4_ES8311_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool readReg(uint8_t reg, uint8_t *val) {
  if (!val) return false;
  Wire.beginTransmission(P4_ES8311_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)P4_ES8311_I2C_ADDR, 1) != 1) return false;
  *val = (uint8_t)Wire.read();
  return true;
}

static const Es8311Coeff *findCoeff(uint32_t rate) {
  const uint32_t mclk = rate * 256UL;
  for (size_t i = 0; i < sizeof(kCoeff) / sizeof(kCoeff[0]); i++) {
    if (kCoeff[i].rate == rate && kCoeff[i].mclk == mclk) return &kCoeff[i];
  }
  return nullptr;
}

static bool applyClock(uint32_t rate) {
  const Es8311Coeff *c = findCoeff(rate);
  if (!c) {
    setErr("unsupported sample rate");
    return false;
  }

  uint8_t reg = 0;
  if (!readReg(ES8311_CLK_MANAGER_REG02, &reg)) {
    setErr("clk read 0x02 failed");
    return false;
  }
  reg &= 0x07;
  reg |= (uint8_t)((c->pre_div - 1) << 5);
  reg |= (uint8_t)(c->pre_multi << 3);
  if (!writeReg(ES8311_CLK_MANAGER_REG02, reg)) {
    setErr("clk write 0x02 failed");
    return false;
  }
  if (!writeReg(ES8311_CLK_MANAGER_REG03, (uint8_t)((c->fs_mode << 6) | c->adc_osr)) ||
      !writeReg(ES8311_CLK_MANAGER_REG04, c->dac_osr) ||
      !writeReg(ES8311_CLK_MANAGER_REG05,
                (uint8_t)(((c->adc_div - 1) << 4) | (c->dac_div - 1)))) {
    setErr("clk write 0x03-0x05 failed");
    return false;
  }

  if (!readReg(ES8311_CLK_MANAGER_REG06, &reg)) {
    setErr("clk read 0x06 failed");
    return false;
  }
  reg &= 0xE0;
  if (c->bclk_div < 19) {
    reg |= (uint8_t)(c->bclk_div - 1);
  } else {
    reg |= c->bclk_div;
  }
  if (!writeReg(ES8311_CLK_MANAGER_REG06, reg)) {
    setErr("clk write 0x06 failed");
    return false;
  }

  if (!readReg(ES8311_CLK_MANAGER_REG07, &reg)) {
    setErr("clk read 0x07 failed");
    return false;
  }
  reg &= 0xC0;
  reg |= c->lrck_h;
  if (!writeReg(ES8311_CLK_MANAGER_REG07, reg) ||
      !writeReg(ES8311_CLK_MANAGER_REG08, c->lrck_l)) {
    setErr("clk write 0x07/0x08 failed");
    return false;
  }

  sInfo.sampleRate = rate;
  return true;
}

const char *es8311StatusName(Es8311Status s) {
  switch (s) {
    case Es8311Status::Missing: return "MISSING";
    case Es8311Status::Detected: return "DETECTED";
    case Es8311Status::Ready: return "READY";
    case Es8311Status::Fault: return "FAULT";
    default: return "UNKNOWN";
  }
}

bool es8311Detect() {
  if (!ensureWire()) {
    sInfo.status = Es8311Status::Fault;
    sInfo.detected = false;
    return false;
  }
  Wire.beginTransmission(P4_ES8311_I2C_ADDR);
  const uint8_t err = Wire.endTransmission();
  sInfo.detected = (err == 0);
  if (!sInfo.detected) {
    sInfo.status = Es8311Status::Missing;
    setErr("ES8311 not on I2C 0x18");
    return false;
  }

  uint8_t id1 = 0;
  uint8_t id2 = 0;
  if (readReg(ES8311_CHIP_ID1, &id1)) sInfo.chipId1 = id1;
  if (readReg(ES8311_CHIP_ID2, &id2)) sInfo.chipId2 = id2;
  if (!sInfo.initialized) sInfo.status = Es8311Status::Detected;
  setErr("ES8311 detected");
  return true;
}

bool es8311Begin(uint32_t sampleRate, uint8_t volumePercent) {
  sInfo.initialized = false;
  sInfo.muted = true;
  if (!es8311Detect()) return false;

  if (!writeReg(ES8311_RESET_REG00, 0x1F)) {
    sInfo.status = Es8311Status::Fault;
    setErr("reset write failed");
    return false;
  }
  delay(20);
  if (!writeReg(ES8311_RESET_REG00, 0x00) ||
      !writeReg(ES8311_RESET_REG00, 0x80)) {
    sInfo.status = Es8311Status::Fault;
    setErr("power-on failed");
    return false;
  }

  /* Enable clocks; MCLK comes from the I2S MCLK pin, not BCLK. */
  if (!writeReg(ES8311_CLK_MANAGER_REG01, 0x3F)) {
    sInfo.status = Es8311Status::Fault;
    setErr("clock enable failed");
    return false;
  }

  uint8_t reg06 = 0;
  if (readReg(ES8311_CLK_MANAGER_REG06, &reg06)) {
    reg06 &= ~(uint8_t)(1u << 5); /* SCLK not inverted */
    (void)writeReg(ES8311_CLK_MANAGER_REG06, reg06);
  }

  if (!applyClock(sampleRate)) {
    sInfo.status = Es8311Status::Fault;
    return false;
  }

  /* Slave I2S, 16-bit SDP in/out (Espressif ES8311_RESOLUTION_16 = 3 << 2). */
  uint8_t reg00 = 0;
  if (readReg(ES8311_RESET_REG00, &reg00)) {
    reg00 &= 0xBF;
    (void)writeReg(ES8311_RESET_REG00, reg00);
  }
  if (!writeReg(ES8311_SDPIN_REG09, 0x0C) ||
      !writeReg(ES8311_SDPOUT_REG0A, 0x0C)) {
    sInfo.status = Es8311Status::Fault;
    setErr("I2S format failed");
    return false;
  }

  if (!writeReg(ES8311_SYSTEM_REG0D, 0x01) ||
      !writeReg(ES8311_SYSTEM_REG0E, 0x02) ||
      !writeReg(ES8311_SYSTEM_REG12, 0x00) ||
      !writeReg(ES8311_SYSTEM_REG13, 0x10) ||
      !writeReg(ES8311_ADC_REG1C, 0x6A) ||
      !writeReg(ES8311_DAC_REG37, 0x08)) {
    sInfo.status = Es8311Status::Fault;
    setErr("analog/DAC power-up failed");
    return false;
  }

  if (!es8311SetMute(true) || !es8311SetVolume(volumePercent)) {
    sInfo.status = Es8311Status::Fault;
    return false;
  }

  sInfo.initialized = true;
  sInfo.status = Es8311Status::Ready;
  setErr("ES8311 ready");
  return true;
}

bool es8311SetSampleRate(uint32_t sampleRate) {
  if (!sInfo.initialized) return es8311Begin(sampleRate, sInfo.volume ? sInfo.volume : 80);
  if (sInfo.sampleRate == sampleRate) return true;
  if (!applyClock(sampleRate)) {
    sInfo.status = Es8311Status::Fault;
    return false;
  }
  return true;
}

bool es8311SetVolume(uint8_t volumePercent) {
  uint8_t v = volumePercent;
  if (v > 100) v = 100;
  const uint8_t reg32 = (v == 0) ? 0 : (uint8_t)(((uint16_t)v * 256 / 100) - 1);
  if (!writeReg(ES8311_DAC_REG32, reg32)) {
    setErr("volume write failed");
    return false;
  }
  sInfo.volume = v;
  return true;
}

bool es8311SetMute(bool mute) {
  uint8_t reg31 = 0;
  if (!readReg(ES8311_DAC_REG31, &reg31)) {
    setErr("mute read failed");
    return false;
  }
  if (mute) {
    reg31 |= (uint8_t)((1u << 6) | (1u << 5));
  } else {
    reg31 &= (uint8_t)~((1u << 6) | (1u << 5));
  }
  if (!writeReg(ES8311_DAC_REG31, reg31)) {
    setErr("mute write failed");
    return false;
  }
  sInfo.muted = mute;
  return true;
}

void es8311Standby() {
  if (!sInfo.initialized) return;
  (void)es8311SetMute(true);
}

const Es8311Info &es8311Info() {
  return sInfo;
}
