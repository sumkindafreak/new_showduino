#include "LampSensors.h"
#include "LampConfig.h"
#include "../BoardConfig.h"

#include <Arduino.h>

static ShowduinoBlowDetector sBlow;
static ShowduinoBlowClass sPending = SHOWDUINO_BLOW_NONE;
static uint32_t sLastMic = 0;
static uint32_t sLastLight = 0;
static uint32_t sLastVolt = 0;
static int32_t sLightRaw = 0;
static int32_t sLightFilt = 0;
static int32_t sVoltRaw = 0;
static int32_t sVoltFilt = 0;
static uint8_t sLightArmed = 0;
static uint8_t sVoltArmed = 0;

static int32_t analogOrNeg(int pin) {
  if (pin < 0) return -1;
  return (int32_t)analogRead(pin);
}

static int32_t iir(int32_t prev, int32_t raw, uint8_t *armed) {
  if (!*armed) {
    *armed = 1;
    return raw;
  }
  return (prev * 3 + raw) / 4;
}

void lampSensorsBegin() {
  ShowduinoBlowConfig cfg = showduino_blow_config_defaults();
  cfg.threshold = lampConfigBlowThreshold();
  cfg.puffMs = lampConfigPuffMs();
  cfg.blowMs = lampConfigBlowMs();
  showduino_blow_reset(&sBlow, &cfg);
  sPending = SHOWDUINO_BLOW_NONE;
  sLightArmed = 0;
  sVoltArmed = 0;
  if (SHOWDUINO_LAMP_MIC_PIN >= 0) pinMode(SHOWDUINO_LAMP_MIC_PIN, INPUT);
  if (SHOWDUINO_LAMP_LIGHT_PIN >= 0) pinMode(SHOWDUINO_LAMP_LIGHT_PIN, INPUT);
  if (SHOWDUINO_LAMP_VOLT_PIN >= 0) pinMode(SHOWDUINO_LAMP_VOLT_PIN, INPUT);
  analogReadResolution(12);
}

void lampSensorsService() {
  const uint32_t now = millis();
  if (SHOWDUINO_LAMP_MIC_PIN >= 0 &&
      (now - sLastMic) >= SHOWDUINO_LAMP_SENSOR_MIC_MS) {
    const uint32_t dt = sLastMic ? (now - sLastMic) : SHOWDUINO_LAMP_SENSOR_MIC_MS;
    sLastMic = now;
    const ShowduinoBlowClass ev =
        showduino_blow_feed(&sBlow, analogOrNeg(SHOWDUINO_LAMP_MIC_PIN), dt);
    if (ev != SHOWDUINO_BLOW_NONE) sPending = ev;
  }
  if (SHOWDUINO_LAMP_LIGHT_PIN >= 0 &&
      (now - sLastLight) >= SHOWDUINO_LAMP_SENSOR_LIGHT_MS) {
    sLastLight = now;
    sLightRaw = analogOrNeg(SHOWDUINO_LAMP_LIGHT_PIN);
    sLightFilt = iir(sLightFilt, sLightRaw, &sLightArmed);
  }
  if (SHOWDUINO_LAMP_VOLT_PIN >= 0 &&
      (now - sLastVolt) >= SHOWDUINO_LAMP_SENSOR_VOLT_MS) {
    sLastVolt = now;
    sVoltRaw = analogOrNeg(SHOWDUINO_LAMP_VOLT_PIN);
    sVoltFilt = iir(sVoltFilt, sVoltRaw, &sVoltArmed);
  }
}

const ShowduinoBlowDetector *lampSensorsBlow() { return &sBlow; }

ShowduinoBlowClass lampSensorsTakeBlowEvent() {
  const ShowduinoBlowClass ev = sPending;
  sPending = SHOWDUINO_BLOW_NONE;
  return ev;
}

int32_t lampSensorsMicRaw() { return sBlow.raw; }
int32_t lampSensorsMicFiltered() { return sBlow.filtered; }
int32_t lampSensorsMicBaseline() { return sBlow.baseline; }
int32_t lampSensorsLightRaw() { return sLightRaw; }
int32_t lampSensorsLightFiltered() { return sLightFilt; }
int32_t lampSensorsVoltRaw() { return sVoltRaw; }
int32_t lampSensorsVoltFiltered() { return sVoltFilt; }

int32_t lampSensorsVoltMv() {
  const uint32_t num = lampConfigVoltScaleNum();
  const uint32_t den = lampConfigVoltScaleDen();
  if (num == 0 || den == 0) return -1;
  return (int32_t)((sVoltFilt * (int32_t)num) / (int32_t)den);
}

const char *lampSensorsMicStatus() {
  return SHOWDUINO_LAMP_MIC_PIN < 0 ? "UNCONFIRMED" : "OK";
}

const char *lampSensorsLightStatus() {
  return SHOWDUINO_LAMP_LIGHT_PIN < 0 ? "UNCONFIRMED" : "OK";
}

const char *lampSensorsVoltStatus() {
  if (SHOWDUINO_LAMP_VOLT_PIN < 0) return "UNCONFIRMED";
  if (lampConfigVoltScaleNum() == 0 || lampConfigVoltScaleDen() == 0) {
    return "UNCALIBRATED";
  }
  return "CALIBRATED";
}

const char *lampSensorsVoltWarn() {
  const int32_t mv = lampSensorsVoltMv();
  if (mv < 0) return "-";
  const uint32_t under = lampConfigVoltUnderMv();
  const uint32_t warn = lampConfigVoltWarnMv();
  if (under && (uint32_t)mv <= under) return "UNDERVOLTAGE";
  if (warn && (uint32_t)mv <= warn) return "WARNING";
  return "OK";
}
