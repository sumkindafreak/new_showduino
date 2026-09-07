#ifndef SHOWDUINO_PLUGIN_ROLES_H
#define SHOWDUINO_PLUGIN_ROLES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define PLUGIN_ROLE_FILE_FORMAT     1U
#define PLUGIN_ROLE_FILE_MAX_BYTES  3072U
#define PLUGIN_ROLE_MAX_DEVICES     20U
#define PLUGIN_DISPLAY_NAME_LEN     40U
#define PLUGIN_ROLE_ADDR_MIN        0x08U
#define PLUGIN_ROLE_ADDR_MAX        0x77U
#define PLUGIN_ROLE_BUS_ID          0U
#define PLUGIN_BUILTIN_ES8311_ADDR  0x18U

enum class PluginChip : uint8_t {
  Unknown = 0,
  ES8311,
  SX1509,
  MCP23017,
  PCA9685,
  TCA9548A
};

enum class PluginDeviceClass : uint8_t {
  Unknown = 0,
  AudioCodec,
  GpioExpander,
  PwmController,
  I2cMultiplexer
};

enum class PluginRole : uint8_t {
  None = 0,
  P4InternalAudio,
  DigitalInputs,
  DigitalOutputs,
  DigitalIo,
  PwmOutputs,
  ServoOutputs,
  I2cMultiplexer
};

enum class PluginClassification : uint8_t {
  Unknown = 0,
  Internal,
  Plugin
};

enum class PluginConfigLoadResult : uint8_t {
  Ok = 0,
  Missing,
  InvalidJson,
  UnsupportedVersion,
  InvalidBus,
  InvalidAddress,
  DuplicateAddress,
  InvalidChip,
  InvalidRole,
  IncompatibleRole,
  BuiltinConflict,
  TooManyDevices,
  FileTooLarge,
  MissingField
};

struct PluginRoleEntry {
  uint8_t busId = 0;
  uint8_t address = 0;
  PluginChip chip = PluginChip::Unknown;
  PluginRole role = PluginRole::None;
};

struct PluginRoleFile {
  uint16_t formatVersion = 0;
  uint8_t deviceCount = 0;
  PluginRoleEntry devices[PLUGIN_ROLE_MAX_DEVICES]{};
};

struct PluginPresence {
  uint8_t busId = 0;
  uint8_t address = 0;
  bool present = false;
  PluginChip discoveredChip = PluginChip::Unknown;
  bool discoveredChipValid = false;
};

struct PluginDescriptor {
  uint8_t busId = 0;
  uint8_t address = 0;
  PluginChip chip = PluginChip::Unknown;
  PluginDeviceClass deviceClass = PluginDeviceClass::Unknown;
  PluginRole role = PluginRole::None;
  PluginClassification classification = PluginClassification::Unknown;
  bool online = false;
  bool configured = false;
  char displayName[PLUGIN_DISPLAY_NAME_LEN] = {};
};

const char *pluginChipName(PluginChip chip);
bool pluginChipFromName(const char *name, PluginChip *out);
const char *pluginRoleName(PluginRole role);
const char *pluginRoleDisplayPhrase(PluginRole role);
bool pluginRoleFromName(const char *name, PluginRole *out);
const char *pluginClassName(PluginDeviceClass deviceClass);
const char *pluginClassificationName(PluginClassification classification);
const char *pluginConfigLoadResultName(PluginConfigLoadResult result);

PluginDeviceClass pluginClassForChip(PluginChip chip);
PluginClassification pluginClassificationForChip(PluginChip chip);
bool pluginRoleCompatible(PluginChip chip, PluginRole role);
uint32_t pluginCapabilitiesForRole(PluginRole role);

bool pluginIsBuiltinEs8311(uint8_t busId, uint8_t address);
bool pluginWellKnownChip(uint8_t busId, uint8_t address, PluginChip *out);
PluginChip pluginChipFromDeviceId(const char *id);

void pluginFormatAddress(uint8_t address, char *out, size_t outLen);
void pluginFormatDisplayName(PluginChip chip, PluginRole role, char *out, size_t outLen);
bool pluginParseI2cAddress(const char *text, uint8_t *out);

bool pluginParseRoleFile(const char *json, size_t jsonLen,
                         PluginRoleFile *out,
                         PluginConfigLoadResult *result);

const PluginRoleEntry *pluginRoleFileFind(const PluginRoleFile *roles,
                                          uint8_t busId, uint8_t address);

void pluginResolveOne(uint8_t busId, uint8_t address, bool present,
                      PluginChip discoveredChip, bool discoveredChipValid,
                      const PluginRoleFile *roles,
                      PluginDescriptor *out,
                      bool rootBus = true);

void pluginBuildDeviceTable(const PluginPresence *found, uint8_t foundCount,
                            const PluginRoleFile *roles,
                            PluginDescriptor *out, uint8_t outCap,
                            uint8_t *outCount);

#endif
