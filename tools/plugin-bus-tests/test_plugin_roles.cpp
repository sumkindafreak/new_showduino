#include <cstdio>
#include <cstring>
#include <string>

#include "plugin/PluginRoles.h"

static int failures = 0;

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

static const PluginDescriptor *findAddr(const PluginDescriptor *table, uint8_t n,
                                        uint8_t address) {
  for (uint8_t i = 0; i < n; ++i) {
    if (table[i].address == address) return &table[i];
  }
  return nullptr;
}

int main() {
  expect(pluginRoleCompatible(PluginChip::SX1509, PluginRole::DigitalInputs),
         "SX1509 accepts DIGITAL_INPUTS");
  expect(pluginRoleCompatible(PluginChip::SX1509, PluginRole::DigitalOutputs),
         "SX1509 accepts DIGITAL_OUTPUTS");
  expect(pluginRoleCompatible(PluginChip::SX1509, PluginRole::DigitalIo),
         "SX1509 accepts DIGITAL_IO");
  expect(!pluginRoleCompatible(PluginChip::SX1509, PluginRole::P4InternalAudio),
         "SX1509 rejects P4_INTERNAL_AUDIO");
  expect(pluginRoleCompatible(PluginChip::MCP23017, PluginRole::DigitalIo),
         "MCP23017 accepts DIGITAL_IO");
  expect(!pluginRoleCompatible(PluginChip::MCP23017, PluginRole::PwmOutputs),
         "MCP23017 rejects PWM_OUTPUTS");
  expect(pluginRoleCompatible(PluginChip::PCA9685, PluginRole::PwmOutputs),
         "PCA9685 accepts PWM_OUTPUTS");
  expect(pluginRoleCompatible(PluginChip::PCA9685, PluginRole::ServoOutputs),
         "PCA9685 accepts SERVO_OUTPUTS");
  expect(pluginRoleCompatible(PluginChip::TCA9548A, PluginRole::I2cMultiplexer),
         "TCA9548A accepts I2C_MULTIPLEXER");
  expect(pluginRoleCompatible(PluginChip::ES8311, PluginRole::P4InternalAudio),
         "ES8311 accepts P4_INTERNAL_AUDIO");
  expect(!pluginRoleCompatible(PluginChip::ES8311, PluginRole::DigitalOutputs),
         "ES8311 rejects DIGITAL_OUTPUTS");

  char name[PLUGIN_DISPLAY_NAME_LEN];
  pluginFormatDisplayName(PluginChip::ES8311, PluginRole::P4InternalAudio, name, sizeof(name));
  expect(std::strcmp(name, "ES8311 - P4 Internal Audio") == 0, "display ES8311");
  pluginFormatDisplayName(PluginChip::SX1509, PluginRole::DigitalInputs, name, sizeof(name));
  expect(std::strcmp(name, "SX1509 - Digital Inputs") == 0, "display SX1509 inputs");
  pluginFormatDisplayName(PluginChip::SX1509, PluginRole::DigitalOutputs, name, sizeof(name));
  expect(std::strcmp(name, "SX1509 - Digital Outputs") == 0, "display SX1509 outputs");
  pluginFormatDisplayName(PluginChip::SX1509, PluginRole::DigitalIo, name, sizeof(name));
  expect(std::strcmp(name, "SX1509 - Digital I/O") == 0, "display SX1509 mixed");
  pluginFormatDisplayName(PluginChip::SX1509, PluginRole::None, name, sizeof(name));
  expect(std::strcmp(name, "SX1509 - Unconfigured") == 0, "display SX1509 unconfigured");
  pluginFormatDisplayName(PluginChip::MCP23017, PluginRole::None, name, sizeof(name));
  expect(std::strcmp(name, "MCP23017 - Unconfigured") == 0, "display MCP23017 unconfigured");
  pluginFormatDisplayName(PluginChip::PCA9685, PluginRole::PwmOutputs, name, sizeof(name));
  expect(std::strcmp(name, "PCA9685 - PWM Outputs") == 0, "display PCA9685");
  pluginFormatDisplayName(PluginChip::TCA9548A, PluginRole::I2cMultiplexer, name, sizeof(name));
  expect(std::strcmp(name, "TCA9548A - I2C Multiplexer") == 0, "display TCA9548A");
  pluginFormatDisplayName(PluginChip::Unknown, PluginRole::None, name, sizeof(name));
  expect(std::strcmp(name, "Unknown I2C Device") == 0, "display unknown");

  const char *valid =
      "{\"formatVersion\":1,\"devices\":["
      "{\"bus\":0,\"address\":\"0x3E\",\"chip\":\"SX1509\",\"role\":\"DIGITAL_INPUTS\"},"
      "{\"bus\":0,\"address\":\"0x3F\",\"chip\":\"SX1509\",\"role\":\"DIGITAL_OUTPUTS\"}"
      "]}";
  PluginRoleFile roles{};
  PluginConfigLoadResult result = PluginConfigLoadResult::Ok;
  expect(pluginParseRoleFile(valid, std::strlen(valid), &roles, &result), "valid role file");
  expect(roles.deviceCount == 2, "role file has two devices");
  expect(roles.devices[0].address == 0x3E && roles.devices[0].role == PluginRole::DigitalInputs,
         "0x3E configured as DIGITAL_INPUTS");
  expect(roles.devices[1].address == 0x3F && roles.devices[1].role == PluginRole::DigitalOutputs,
         "0x3F configured as DIGITAL_OUTPUTS");

  const char *badRole =
      "{\"formatVersion\":1,\"devices\":["
      "{\"bus\":0,\"address\":\"0x3E\",\"chip\":\"SX1509\",\"role\":\"P4_INTERNAL_AUDIO\"}]}";
  expect(!pluginParseRoleFile(badRole, std::strlen(badRole), &roles, &result) &&
             result == PluginConfigLoadResult::IncompatibleRole,
         "Case F rejects SX1509 + P4_INTERNAL_AUDIO");
  const char *builtinConflict =
      "{\"formatVersion\":1,\"devices\":["
      "{\"bus\":0,\"address\":\"0x18\",\"chip\":\"SX1509\",\"role\":\"DIGITAL_INPUTS\"}]}";
  expect(!pluginParseRoleFile(builtinConflict, std::strlen(builtinConflict), &roles, &result) &&
             result == PluginConfigLoadResult::BuiltinConflict,
         "rejects role that conflicts with built-in ES8311");
  const char *badVersion = "{\"formatVersion\":2,\"devices\":[]}";
  expect(!pluginParseRoleFile(badVersion, std::strlen(badVersion), &roles, &result) &&
             result == PluginConfigLoadResult::UnsupportedVersion,
         "rejects unsupported formatVersion");
  expect(!pluginParseRoleFile("{broken", std::strlen("{broken}"), &roles, &result) &&
             result == PluginConfigLoadResult::InvalidJson,
         "rejects malformed JSON");
  const char *badBus =
      "{\"formatVersion\":1,\"devices\":["
      "{\"bus\":1,\"address\":\"0x3E\",\"chip\":\"SX1509\",\"role\":\"DIGITAL_INPUTS\"}]}";
  expect(!pluginParseRoleFile(badBus, std::strlen(badBus), &roles, &result) &&
             result == PluginConfigLoadResult::InvalidBus,
         "rejects invalid bus");
  const char *lowAddr =
      "{\"formatVersion\":1,\"devices\":["
      "{\"bus\":0,\"address\":\"0x07\",\"chip\":\"SX1509\",\"role\":\"DIGITAL_INPUTS\"}]}";
  expect(!pluginParseRoleFile(lowAddr, std::strlen(lowAddr), &roles, &result) &&
             result == PluginConfigLoadResult::InvalidAddress,
         "rejects address below range");
  const char *unsafeAddr =
      "{\"formatVersion\":1,\"devices\":["
      "{\"bus\":0,\"address\":\"../x\",\"chip\":\"SX1509\",\"role\":\"DIGITAL_INPUTS\"}]}";
  expect(!pluginParseRoleFile(unsafeAddr, std::strlen(unsafeAddr), &roles, &result) &&
             result == PluginConfigLoadResult::InvalidAddress,
         "rejects unsafe address path");
  const char *duplicate =
      "{\"formatVersion\":1,\"devices\":["
      "{\"bus\":0,\"address\":\"0x3E\",\"chip\":\"SX1509\",\"role\":\"DIGITAL_INPUTS\"},"
      "{\"bus\":0,\"address\":\"0x3E\",\"chip\":\"SX1509\",\"role\":\"DIGITAL_OUTPUTS\"}]}";
  expect(!pluginParseRoleFile(duplicate, std::strlen(duplicate), &roles, &result) &&
             result == PluginConfigLoadResult::DuplicateAddress,
         "rejects duplicate bus/address");
  const char *badChip =
      "{\"formatVersion\":1,\"devices\":["
      "{\"bus\":0,\"address\":\"0x3E\",\"chip\":\"NOTACHIP\",\"role\":\"DIGITAL_INPUTS\"}]}";
  expect(!pluginParseRoleFile(badChip, std::strlen(badChip), &roles, &result) &&
             result == PluginConfigLoadResult::InvalidChip,
         "rejects unknown chip");
  std::string huge(PLUGIN_ROLE_FILE_MAX_BYTES + 8, 'x');
  expect(!pluginParseRoleFile(huge.c_str(), huge.size(), &roles, &result) &&
             result == PluginConfigLoadResult::FileTooLarge,
         "rejects oversized file");

  pluginParseRoleFile(valid, std::strlen(valid), &roles, &result);

  PluginPresence found[4];
  found[0].busId = 0;
  found[0].address = 0x18;
  found[0].present = true;
  found[1].busId = 0;
  found[1].address = 0x3E;
  found[1].present = true;
  found[2].busId = 0;
  found[2].address = 0x3F;
  found[2].present = true;

  PluginDescriptor table[PLUGIN_ROLE_MAX_DEVICES];
  uint8_t count = 0;
  pluginBuildDeviceTable(found, 3, &roles, table, PLUGIN_ROLE_MAX_DEVICES, &count);

  const PluginDescriptor *es = findAddr(table, count, 0x18);
  expect(es && std::strcmp(es->displayName, "ES8311 - P4 Internal Audio") == 0 &&
             es->classification == PluginClassification::Internal &&
             es->online && es->configured && es->role == PluginRole::P4InternalAudio,
         "Case A ES8311 internal online");

  const PluginDescriptor *in = findAddr(table, count, 0x3E);
  expect(in && std::strcmp(in->displayName, "SX1509 - Digital Inputs") == 0 &&
             in->classification == PluginClassification::Plugin &&
             in->online && in->configured && in->role == PluginRole::DigitalInputs,
         "Case B SX1509 digital inputs");

  const PluginDescriptor *out = findAddr(table, count, 0x3F);
  expect(out && std::strcmp(out->displayName, "SX1509 - Digital Outputs") == 0 &&
             out->classification == PluginClassification::Plugin &&
             out->online && out->configured && out->role == PluginRole::DigitalOutputs,
         "Case C SX1509 digital outputs");

  PluginPresence onlySx;
  onlySx.busId = 0;
  onlySx.address = 0x3E;
  onlySx.present = true;
  pluginBuildDeviceTable(&onlySx, 1, nullptr, table, PLUGIN_ROLE_MAX_DEVICES, &count);
  const PluginDescriptor *uncfg = findAddr(table, count, 0x3E);
  expect(uncfg && std::strcmp(uncfg->displayName, "SX1509 - Unconfigured") == 0 &&
             uncfg->classification == PluginClassification::Plugin &&
             uncfg->online && !uncfg->configured && uncfg->role == PluginRole::None,
         "Case D detected SX1509 with no configuration");

  PluginPresence nonePresent[1];
  pluginBuildDeviceTable(nonePresent, 0, &roles, table, PLUGIN_ROLE_MAX_DEVICES, &count);
  const PluginDescriptor *offline = findAddr(table, count, 0x3E);
  expect(offline && offline->chip == PluginChip::SX1509 &&
             offline->classification == PluginClassification::Plugin &&
             !offline->online && offline->configured,
         "Case E configured SX1509 is offline");
  const PluginDescriptor *esOffline = findAddr(table, count, 0x18);
  expect(esOffline && !esOffline->online && esOffline->configured &&
             esOffline->classification == PluginClassification::Internal,
         "built-in ES8311 stays configured when absent");

  PluginPresence mystery;
  mystery.busId = 0;
  mystery.address = 0x37;
  mystery.present = true;
  pluginBuildDeviceTable(&mystery, 1, nullptr, table, PLUGIN_ROLE_MAX_DEVICES, &count);
  const PluginDescriptor *unknown = findAddr(table, count, 0x37);
  expect(unknown && std::strcmp(unknown->displayName, "Unknown I2C Device") == 0 &&
             unknown->online && unknown->chip == PluginChip::Unknown &&
             unknown->role == PluginRole::None,
         "Case G unknown responding address");

  PluginPresence mcp;
  mcp.busId = 0;
  mcp.address = 0x20;
  mcp.present = true;
  pluginBuildDeviceTable(&mcp, 1, nullptr, table, PLUGIN_ROLE_MAX_DEVICES, &count);
  const PluginDescriptor *mcpDev = findAddr(table, count, 0x20);
  expect(mcpDev && mcpDev->chip == PluginChip::MCP23017 &&
             std::strcmp(mcpDev->displayName, "MCP23017 - Unconfigured") == 0 &&
             !mcpDev->configured,
         "MCP23017 identity does not invent a role");

  expect(pluginChipFromDeviceId("microchip.mcp23017") == PluginChip::MCP23017,
         "device id maps to MCP23017");
  expect(pluginChipFromDeviceId("nxp.pca9685") == PluginChip::PCA9685,
         "device id maps to PCA9685");
  expect(pluginChipFromDeviceId("generic.i2c.unknown") == PluginChip::Unknown,
         "unknown device id stays unknown");

  std::printf("%s\n", failures ? "FAILED" : "ALL TESTS PASSED");
  return failures ? 1 : 0;
}
