#include "LampConfig.h"
#include "../../shared-node/NodeConfig.h"
#include "../../../protocol/showduino_carbide_lamp.h"
#include "../../../protocol/showduino_lamp_motion.h"

#include <string.h>
#include <stdio.h>

static char sId[SHOWDUINO_LAMP_LOGICAL_MAX] = SHOWDUINO_CARBIDE_LOGICAL_DEFAULT;
static char sName[21] = "CARBIDE LAMP";

static bool nameOk(const char *name) {
  size_t n;
  if (!name || !name[0]) return false;
  n = strlen(name);
  if (n > 20) return false;
  for (size_t i = 0; i < n; i++) {
    const char c = name[i];
    if (c < 32 || c > 126) return false;
  }
  return true;
}

void lampConfigBegin() {
  nodeConfigBegin("sdlamp");
  char stored[SHOWDUINO_LAMP_LOGICAL_MAX] = "";
  nodeConfigGetStr("id", stored, sizeof(stored), SHOWDUINO_CARBIDE_LOGICAL_DEFAULT);
  if (showduino_lamp_id_ok(stored)) {
    strncpy(sId, stored, sizeof(sId) - 1);
    sId[sizeof(sId) - 1] = 0;
  } else {
    strncpy(sId, SHOWDUINO_CARBIDE_LOGICAL_DEFAULT, sizeof(sId) - 1);
    sId[sizeof(sId) - 1] = 0;
    nodeConfigSetStr("id", sId);
  }
  char name[21] = "";
  nodeConfigGetName(name, sizeof(name), "CARBIDE LAMP");
  if (nameOk(name)) {
    strncpy(sName, name, sizeof(sName) - 1);
    sName[sizeof(sName) - 1] = 0;
  }
}

const char *lampConfigId() { return sId; }
const char *lampConfigName() { return sName; }

bool lampConfigSetId(const char *id) {
  if (!showduino_lamp_id_ok(id)) return false;
  strncpy(sId, id, sizeof(sId) - 1);
  sId[sizeof(sId) - 1] = 0;
  nodeConfigSetStr("id", sId);
  return true;
}

bool lampConfigSetName(const char *name) {
  if (!nameOk(name)) return false;
  strncpy(sName, name, sizeof(sName) - 1);
  sName[sizeof(sName) - 1] = 0;
  nodeConfigSetName(sName);
  return true;
}

uint8_t lampConfigBrightness() {
  return showduino_lamp_clamp_bri(nodeConfigGetU8("bri", SHOWDUINO_LAMP_BRI_MAX > 80 ? 80 : SHOWDUINO_LAMP_BRI_MAX));
}

void lampConfigSetBrightness(uint8_t bri) {
  nodeConfigSetU8("bri", showduino_lamp_clamp_bri(bri));
}

uint8_t lampConfigAudioVolume() { return nodeConfigGetU8("vol", 18); }
void lampConfigSetAudioVolume(uint8_t vol) {
  if (vol > 30) vol = 30;
  nodeConfigSetU8("vol", vol);
}

int32_t lampConfigBlowThreshold() {
  const uint16_t v = nodeConfigGetU16("bth", (uint16_t)SHOWDUINO_CARBIDE_DEFAULT_THRESH);
  return (int32_t)v;
}

void lampConfigSetBlowThreshold(int32_t v) {
  if (v < 8) v = 8;
  if (v > 2000) v = 2000;
  nodeConfigSetU16("bth", (uint16_t)v);
}

uint16_t lampConfigPuffMs() { return nodeConfigGetU16("puff", SHOWDUINO_CARBIDE_PUFF_MS); }
uint16_t lampConfigBlowMs() { return nodeConfigGetU16("blow", SHOWDUINO_CARBIDE_BLOW_MS); }

void lampConfigSetBlowWindows(uint16_t puffMs, uint16_t blowMs) {
  if (puffMs < 20) puffMs = 20;
  if (blowMs < puffMs + 40) blowMs = (uint16_t)(puffMs + 40);
  nodeConfigSetU16("puff", puffMs);
  nodeConfigSetU16("blow", blowMs);
}

uint32_t lampConfigVoltScaleNum() {
  return nodeConfigGetU16("vnum", (uint16_t)SHOWDUINO_LAMP_VOLT_FS_MV);
}
uint32_t lampConfigVoltScaleDen() {
  return nodeConfigGetU16("vden", (uint16_t)SHOWDUINO_LAMP_VOLT_ADC_MAX);
}

void lampConfigSetVoltScale(uint32_t num, uint32_t den) {
  if (num > 65535) num = 65535;
  if (den > 65535) den = 65535;
  nodeConfigSetU16("vnum", (uint16_t)num);
  nodeConfigSetU16("vden", (uint16_t)den);
}

uint32_t lampConfigVoltWarnMv() { return nodeConfigGetU16("vwarn", 0); }
uint32_t lampConfigVoltUnderMv() { return nodeConfigGetU16("vund", 0); }

uint32_t lampConfigLightScale() { return nodeConfigGetU16("lsc", 0); }

void lampConfigSetLightScale(uint32_t scale) {
  if (scale > 65535) scale = 65535;
  nodeConfigSetU16("lsc", (uint16_t)scale);
}

uint8_t lampConfigMotionEnabled() {
  return nodeConfigGetU8("men", SHOWDUINO_MOTION_DEFAULT_ENABLED) ? 1 : 0;
}

void lampConfigSetMotionEnabled(uint8_t en) {
  nodeConfigSetU8("men", en ? 1 : 0);
}

ShowduinoMotionAction lampConfigMotionAction() {
  return showduino_motion_action_from_u8(
      nodeConfigGetU8("mact", (uint8_t)SHOWDUINO_MOTION_ACT_DISABLED));
}

void lampConfigSetMotionAction(ShowduinoMotionAction act) {
  nodeConfigSetU8("mact", (uint8_t)showduino_motion_action_from_u8((uint8_t)act));
}

uint8_t lampConfigMotionActiveLow() {
  return nodeConfigGetU8("mpol", SHOWDUINO_MOTION_DEFAULT_ACTIVE_LOW) ? 1 : 0;
}

void lampConfigSetMotionActiveLow(uint8_t activeLow) {
  nodeConfigSetU8("mpol", activeLow ? 1 : 0);
}

uint16_t lampConfigMotionCooldownMs() {
  uint16_t ms = nodeConfigGetU16("mcd", SHOWDUINO_MOTION_DEFAULT_COOLDOWN_MS);
  if (ms > SHOWDUINO_MOTION_COOLDOWN_MAX_MS) ms = SHOWDUINO_MOTION_COOLDOWN_MAX_MS;
  return ms;
}

void lampConfigSetMotionCooldownMs(uint16_t ms) {
  if (ms > SHOWDUINO_MOTION_COOLDOWN_MAX_MS) ms = SHOWDUINO_MOTION_COOLDOWN_MAX_MS;
  nodeConfigSetU16("mcd", ms);
}

uint8_t lampConfigFlameActivity() {
  uint8_t v = nodeConfigGetU8("fact", 45);
  if (v > 100) v = 100;
  return v;
}

void lampConfigSetFlameActivity(uint8_t v) {
  if (v > 100) v = 100;
  nodeConfigSetU8("fact", v);
}

uint8_t lampConfigFlickerAmount() {
  uint8_t v = nodeConfigGetU8("flic", 35);
  if (v > 100) v = 100;
  return v;
}

void lampConfigSetFlickerAmount(uint8_t v) {
  if (v > 100) v = 100;
  nodeConfigSetU8("flic", v);
}

uint8_t lampConfigIgnitionSpeed() {
  uint8_t v = nodeConfigGetU8("igsp", 100);
  if (v < 50) v = 50;
  if (v > 200) v = 200;
  return v;
}

void lampConfigSetIgnitionSpeed(uint8_t v) {
  if (v < 50) v = 50;
  if (v > 200) v = 200;
  nodeConfigSetU8("igsp", v);
}

uint8_t lampConfigJewelCore() {
  uint8_t v = nodeConfigGetU8("core", 0);
  if (v >= SHOWDUINO_CARBIDE_JEWEL_PIXELS) v = 0;
  return v;
}

void lampConfigSetJewelCore(uint8_t v) {
  if (v >= SHOWDUINO_CARBIDE_JEWEL_PIXELS) v = 0;
  nodeConfigSetU8("core", v);
}
