#ifndef SHOWDUINO_PLUGIN_REGISTRY_H
#define SHOWDUINO_PLUGIN_REGISTRY_H

#include "PluginTypes.h"

void pluginRegistryClear();
void pluginRegistryLoadFromSd();
const PluginDef *pluginRegistryFindDef(const char *id);
const PluginDef *pluginRegistryDefAt(uint8_t index);
uint8_t pluginRegistryDefCount();
const PluginConfigInstance *pluginRegistryConfigAt(uint8_t index);
uint8_t pluginRegistryConfigCount();
bool pluginRegistrySdAvailable();
bool pluginRegistryDefinitionsOk();
const PluginDef *pluginRegistryMatchIdentify(const PluginLocation &loc, uint8_t *matchCount);
const PluginConfigInstance *pluginRegistryConfigFor(const PluginLocation &loc);

#endif
