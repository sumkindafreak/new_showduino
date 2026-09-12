#include "LampConfig.h"
#include "../../shared-node/NodeConfig.h"
#include "../../../protocol/showduino_carbide_lamp.h"

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

uint32_t lampConfigVoltScaleNum() { return nodeConfigGetU16("vnum", 0); }
uint32_t lampConfigVoltScaleDen() {
  const uint16_t d = nodeConfigGetU16("vden", 0);
  return d;
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
