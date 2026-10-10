#ifndef SHOWDUINO_PLUGIN_BUS_H
#define SHOWDUINO_PLUGIN_BUS_H

#include "PluginTypes.h"

bool pluginBusBegin(void (*pump)());
void pluginBusService();
void pluginBusOnEmergency();
bool pluginBusReady();
void pluginBusScan();
void pluginBusPrintList();
void pluginBusPrintStatus();
void pluginBusPrintInfo(const char *key);
bool pluginBusCaptureSelfTest(PluginBusSelfTest *out);

const PluginInstance *pluginBusFindByInstanceId(const char *id);
const PluginInstance *pluginBusFindByLocation(const PluginLocation &loc);
uint8_t pluginBusInstanceCount();
const PluginInstance *pluginBusInstanceAt(uint8_t index);
void pluginBusFormatPath(const PluginLocation &loc, char *out, size_t outLen);
bool pluginBusPing(const PluginLocation &loc);
PluginConfigLoadResult pluginBusRoleFileResult();
const PluginRoleFile *pluginBusRoleFile();

bool pluginBusAssignPreset(uint8_t address,const char *chip,const char *role,char *reply,size_t replyLen);

#endif
