#ifndef SHOWDUINO_PLUGIN_DRIVER_H
#define SHOWDUINO_PLUGIN_DRIVER_H

#include "PluginTypes.h"

struct PluginDriver {
  const char *id;
  /* Optional: return true if this native driver claims the live device.
   * Must not write registers unless the definition declared a safe identify. */
  bool (*identify)(const PluginLocation &loc, const PluginDef *def);
  void (*onEmergency)(PluginInstance &inst);
  void (*diagnostic)(const PluginInstance &inst);
};

const PluginDriver *pluginDriverFind(const char *id);
void pluginDriverOnEmergency(PluginInstance &inst);
bool pluginGenericIdentify(const PluginLocation &loc, const PluginDef *def);
bool pluginMuxSelect(uint8_t muxAddr, uint8_t channel); /* channel 0-7, 0xFF = disable */

#endif
