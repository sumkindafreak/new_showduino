#include "PluginDriver.h"
#include "PluginBus.h"
#include <Wire.h>
#include <string.h>

static bool identifyEs8311(const PluginLocation &loc, const PluginDef *) {
  /* Onboard Waveshare codec at 0x18 on the factory I²C net. Do not write. */
  return loc.muxAddr == PLUGIN_MUX_NONE && loc.address == 0x18;
}

static void emergencyNoOp(PluginInstance &) {}

static void diagEs8311(const PluginInstance &inst) {
  Serial.printf("[PLUGIN] diagnostic %s — onboard ES8311 (not Showduino show audio)\n",
                inst.friendly[0] ? inst.friendly : "es8311");
}

static bool identifyGeneric(const PluginLocation &loc, const PluginDef *def) {
  return pluginGenericIdentify(loc, def);
}

static const PluginDriver kDrivers[] = {
  { "generic.i2c.unknown", nullptr, emergencyNoOp, nullptr },
  { "generic.i2c.register", identifyGeneric, emergencyNoOp, nullptr },
  { "waveshare.es8311", identifyEs8311, emergencyNoOp, diagEs8311 },
  { "tca9548a", nullptr, emergencyNoOp, nullptr },
};

const PluginDriver *pluginDriverFind(const char *id) {
  if (!id) return nullptr;
  for (size_t i = 0; i < sizeof(kDrivers) / sizeof(kDrivers[0]); i++) {
    if (!strcmp(kDrivers[i].id, id)) return &kDrivers[i];
  }
  return nullptr;
}

void pluginDriverOnEmergency(PluginInstance &inst) {
  const PluginDriver *d = pluginDriverFind(inst.driver);
  if (d && d->onEmergency) d->onEmergency(inst);
}

bool pluginGenericIdentify(const PluginLocation &loc, const PluginDef *def) {
  if (!def || !def->identify.enabled) return false;
  if (loc.address < def->addrMin || loc.address > def->addrMax) return false;
  if (loc.muxAddr != PLUGIN_MUX_NONE) {
    if (!pluginMuxSelect(loc.muxAddr, loc.muxChannel)) return false;
  }
  Wire.beginTransmission(loc.address);
  Wire.write(def->identify.reg);
  if (Wire.endTransmission(false) != 0) {
    if (loc.muxAddr != PLUGIN_MUX_NONE) pluginMuxSelect(loc.muxAddr, PLUGIN_MUX_CH_NONE);
    return false;
  }
  if (Wire.requestFrom((int)loc.address, 1) < 1) {
    if (loc.muxAddr != PLUGIN_MUX_NONE) pluginMuxSelect(loc.muxAddr, PLUGIN_MUX_CH_NONE);
    return false;
  }
  uint8_t v = (uint8_t)Wire.read();
  if (loc.muxAddr != PLUGIN_MUX_NONE) pluginMuxSelect(loc.muxAddr, PLUGIN_MUX_CH_NONE);
  return ((v & def->identify.mask) == def->identify.equals);
}

bool pluginMuxSelect(uint8_t muxAddr, uint8_t channel) {
  if (muxAddr < PLUGIN_ADDR_MIN || muxAddr > PLUGIN_ADDR_MAX) return false;
  uint8_t mask = 0;
  if (channel != PLUGIN_MUX_CH_NONE) {
    if (channel > 7) return false;
    mask = (uint8_t)(1u << channel);
  }
  Wire.beginTransmission(muxAddr);
  Wire.write(mask);
  return Wire.endTransmission() == 0;
}
