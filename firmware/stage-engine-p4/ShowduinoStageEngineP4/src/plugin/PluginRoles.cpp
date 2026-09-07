#include "PluginRoles.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

void copyStr(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

int cmpIgnoreCase(const char *a, const char *b) {
  if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
  while (*a && *b) {
    unsigned char ca = (unsigned char)tolower((unsigned char)*a++);
    unsigned char cb = (unsigned char)tolower((unsigned char)*b++);
    if (ca != cb) return (int)ca - (int)cb;
  }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

bool equalsIgnoreCase(const char *a, const char *b) {
  return cmpIgnoreCase(a, b) == 0;
}

bool endsWithIgnoreCase(const char *text, const char *suffix) {
  if (!text || !suffix) return false;
  size_t tlen = strlen(text);
  size_t slen = strlen(suffix);
  if (slen == 0 || slen > tlen) return false;
  return equalsIgnoreCase(text + (tlen - slen), suffix);
}

class JsonReader {
public:
  JsonReader(const char *json, size_t length) : p_(json), end_(json + length) {}

  void ws() {
    while (p_ < end_ && isspace((unsigned char)*p_)) ++p_;
  }

  bool take(char expected) {
    ws();
    if (p_ >= end_ || *p_ != expected) return false;
    ++p_;
    return true;
  }

  char peek() {
    ws();
    return p_ < end_ ? *p_ : '\0';
  }

  bool finished() {
    ws();
    return p_ == end_;
  }

  bool string(char *out, size_t outLen) {
    if (!out || outLen == 0 || !take('"')) return false;
    size_t used = 0;
    while (p_ < end_) {
      unsigned char c = (unsigned char)*p_++;
      if (c == '"') {
        out[used] = '\0';
        return true;
      }
      if (c < 0x20) return false;
      if (c == '\\') {
        if (p_ >= end_) return false;
        c = (unsigned char)*p_++;
        switch (c) {
          case '"':
          case '\\':
          case '/':
            break;
          case 'b': c = '\b'; break;
          case 'f': c = '\f'; break;
          case 'n': c = '\n'; break;
          case 'r': c = '\r'; break;
          case 't': c = '\t'; break;
          default:
            return false;
        }
      }
      if (used + 1 >= outLen) return false;
      out[used++] = (char)c;
    }
    return false;
  }

  bool u32(uint32_t *out) {
    if (!out) return false;
    ws();
    if (p_ >= end_ || !isdigit((unsigned char)*p_)) return false;
    if (*p_ == '0' && p_ + 1 < end_ && isdigit((unsigned char)p_[1])) return false;
    uint64_t value = 0;
    while (p_ < end_ && isdigit((unsigned char)*p_)) {
      value = value * 10U + (uint64_t)(*p_++ - '0');
      if (value > UINT32_MAX) return false;
    }
    *out = (uint32_t)value;
    return true;
  }

  bool skipString() {
    if (!take('"')) return false;
    while (p_ < end_) {
      unsigned char c = (unsigned char)*p_++;
      if (c == '"') return true;
      if (c == '\\') {
        if (p_ >= end_) return false;
        ++p_;
      }
    }
    return false;
  }

  bool skipValue(unsigned depth = 0) {
    if (depth > 8) return false;
    ws();
    if (p_ >= end_) return false;
    if (*p_ == '"') {
      return skipString();
    }
    if (*p_ == '{') {
      ++p_;
      ws();
      if (p_ < end_ && *p_ == '}') {
        ++p_;
        return true;
      }
      while (p_ < end_) {
        if (!skipString() || !take(':') || !skipValue(depth + 1)) return false;
        ws();
        if (p_ < end_ && *p_ == '}') {
          ++p_;
          return true;
        }
        if (p_ >= end_ || *p_++ != ',') return false;
      }
      return false;
    }
    if (*p_ == '[') {
      ++p_;
      ws();
      if (p_ < end_ && *p_ == ']') {
        ++p_;
        return true;
      }
      while (p_ < end_) {
        if (!skipValue(depth + 1)) return false;
        ws();
        if (p_ < end_ && *p_ == ']') {
          ++p_;
          return true;
        }
        if (p_ >= end_ || *p_++ != ',') return false;
      }
      return false;
    }
    if (*p_ == 't' || *p_ == 'f' || *p_ == 'n') {
      const char *lit = (*p_ == 't') ? "true" : (*p_ == 'f') ? "false" : "null";
      size_t n = strlen(lit);
      if ((size_t)(end_ - p_) < n || strncmp(p_, lit, n) != 0) return false;
      p_ += n;
      return true;
    }
    uint32_t sink = 0;
    return u32(&sink);
  }

private:
  const char *p_;
  const char *end_;
};

bool addressCharsSafe(const char *text) {
  if (!text || !text[0]) return false;
  for (const char *p = text; *p; ++p) {
    char c = *p;
    if (c == '/' || c == '\\' || c == '.' || isspace((unsigned char)c)) {
      return false;
    }
    if (!(isxdigit((unsigned char)c) || c == 'x' || c == 'X')) return false;
  }
  if (strchr(text, '/') || strchr(text, '\\')) return false;
  return true;
}

}  // namespace

const char *pluginChipName(PluginChip chip) {
  switch (chip) {
    case PluginChip::ES8311: return "ES8311";
    case PluginChip::SX1509: return "SX1509";
    case PluginChip::MCP23017: return "MCP23017";
    case PluginChip::PCA9685: return "PCA9685";
    case PluginChip::TCA9548A: return "TCA9548A";
    case PluginChip::Unknown:
    default: return "UNKNOWN";
  }
}

bool pluginChipFromName(const char *name, PluginChip *out) {
  if (!name || !out) return false;
  if (equalsIgnoreCase(name, "ES8311")) {
    *out = PluginChip::ES8311;
    return true;
  }
  if (equalsIgnoreCase(name, "SX1509")) {
    *out = PluginChip::SX1509;
    return true;
  }
  if (equalsIgnoreCase(name, "MCP23017")) {
    *out = PluginChip::MCP23017;
    return true;
  }
  if (equalsIgnoreCase(name, "PCA9685")) {
    *out = PluginChip::PCA9685;
    return true;
  }
  if (equalsIgnoreCase(name, "TCA9548A")) {
    *out = PluginChip::TCA9548A;
    return true;
  }
  return false;
}

const char *pluginRoleName(PluginRole role) {
  switch (role) {
    case PluginRole::P4InternalAudio: return "P4_INTERNAL_AUDIO";
    case PluginRole::DigitalInputs: return "DIGITAL_INPUTS";
    case PluginRole::DigitalOutputs: return "DIGITAL_OUTPUTS";
    case PluginRole::DigitalIo: return "DIGITAL_IO";
    case PluginRole::PwmOutputs: return "PWM_OUTPUTS";
    case PluginRole::ServoOutputs: return "SERVO_OUTPUTS";
    case PluginRole::I2cMultiplexer: return "I2C_MULTIPLEXER";
    case PluginRole::None:
    default: return "NONE";
  }
}

const char *pluginRoleDisplayPhrase(PluginRole role) {
  switch (role) {
    case PluginRole::P4InternalAudio: return "P4 Internal Audio";
    case PluginRole::DigitalInputs: return "Digital Inputs";
    case PluginRole::DigitalOutputs: return "Digital Outputs";
    case PluginRole::DigitalIo: return "Digital I/O";
    case PluginRole::PwmOutputs: return "PWM Outputs";
    case PluginRole::ServoOutputs: return "Servo Outputs";
    case PluginRole::I2cMultiplexer: return "I2C Multiplexer";
    case PluginRole::None:
    default: return "Unconfigured";
  }
}

bool pluginRoleFromName(const char *name, PluginRole *out) {
  if (!name || !out) return false;
  if (equalsIgnoreCase(name, "P4_INTERNAL_AUDIO")) {
    *out = PluginRole::P4InternalAudio;
    return true;
  }
  if (equalsIgnoreCase(name, "DIGITAL_INPUTS")) {
    *out = PluginRole::DigitalInputs;
    return true;
  }
  if (equalsIgnoreCase(name, "DIGITAL_OUTPUTS")) {
    *out = PluginRole::DigitalOutputs;
    return true;
  }
  if (equalsIgnoreCase(name, "DIGITAL_IO")) {
    *out = PluginRole::DigitalIo;
    return true;
  }
  if (equalsIgnoreCase(name, "PWM_OUTPUTS")) {
    *out = PluginRole::PwmOutputs;
    return true;
  }
  if (equalsIgnoreCase(name, "SERVO_OUTPUTS")) {
    *out = PluginRole::ServoOutputs;
    return true;
  }
  if (equalsIgnoreCase(name, "I2C_MULTIPLEXER")) {
    *out = PluginRole::I2cMultiplexer;
    return true;
  }
  return false;
}

const char *pluginClassName(PluginDeviceClass deviceClass) {
  switch (deviceClass) {
    case PluginDeviceClass::AudioCodec: return "AUDIO_CODEC";
    case PluginDeviceClass::GpioExpander: return "GPIO_EXPANDER";
    case PluginDeviceClass::PwmController: return "PWM_CONTROLLER";
    case PluginDeviceClass::I2cMultiplexer: return "I2C_MULTIPLEXER";
    case PluginDeviceClass::Unknown:
    default: return "UNKNOWN";
  }
}

const char *pluginClassificationName(PluginClassification classification) {
  switch (classification) {
    case PluginClassification::Internal: return "INTERNAL";
    case PluginClassification::Plugin: return "PLUGIN";
    case PluginClassification::Unknown:
    default: return "UNKNOWN";
  }
}

const char *pluginConfigLoadResultName(PluginConfigLoadResult result) {
  switch (result) {
    case PluginConfigLoadResult::Ok: return "ok";
    case PluginConfigLoadResult::Missing: return "missing";
    case PluginConfigLoadResult::InvalidJson: return "malformed JSON";
    case PluginConfigLoadResult::UnsupportedVersion: return "unsupported formatVersion";
    case PluginConfigLoadResult::InvalidBus: return "invalid bus";
    case PluginConfigLoadResult::InvalidAddress: return "invalid I2C address";
    case PluginConfigLoadResult::DuplicateAddress: return "duplicate bus/address";
    case PluginConfigLoadResult::InvalidChip: return "invalid chip type";
    case PluginConfigLoadResult::InvalidRole: return "invalid role";
    case PluginConfigLoadResult::IncompatibleRole: return "incompatible chip/role";
    case PluginConfigLoadResult::BuiltinConflict: return "conflicts with built-in ES8311";
    case PluginConfigLoadResult::TooManyDevices: return "too many devices";
    case PluginConfigLoadResult::FileTooLarge: return "file too large";
    case PluginConfigLoadResult::MissingField: return "missing required field";
    default: return "configuration error";
  }
}

PluginDeviceClass pluginClassForChip(PluginChip chip) {
  switch (chip) {
    case PluginChip::ES8311: return PluginDeviceClass::AudioCodec;
    case PluginChip::SX1509:
    case PluginChip::MCP23017: return PluginDeviceClass::GpioExpander;
    case PluginChip::PCA9685: return PluginDeviceClass::PwmController;
    case PluginChip::TCA9548A: return PluginDeviceClass::I2cMultiplexer;
    case PluginChip::Unknown:
    default: return PluginDeviceClass::Unknown;
  }
}

PluginClassification pluginClassificationForChip(PluginChip chip) {
  if (chip == PluginChip::ES8311) return PluginClassification::Internal;
  if (chip == PluginChip::Unknown) return PluginClassification::Unknown;
  return PluginClassification::Plugin;
}

bool pluginRoleCompatible(PluginChip chip, PluginRole role) {
  switch (chip) {
    case PluginChip::ES8311:
      return role == PluginRole::P4InternalAudio;
    case PluginChip::SX1509:
    case PluginChip::MCP23017:
      return role == PluginRole::DigitalInputs ||
             role == PluginRole::DigitalOutputs ||
             role == PluginRole::DigitalIo;
    case PluginChip::PCA9685:
      return role == PluginRole::PwmOutputs ||
             role == PluginRole::ServoOutputs;
    case PluginChip::TCA9548A:
      return role == PluginRole::I2cMultiplexer;
    case PluginChip::Unknown:
    default:
      return false;
  }
}

uint32_t pluginCapabilitiesForRole(PluginRole role) {
  switch (role) {
    case PluginRole::DigitalInputs:
      return 1u << 0;
    case PluginRole::DigitalOutputs:
      return 1u << 1;
    case PluginRole::DigitalIo:
      return (1u << 0) | (1u << 1);
    case PluginRole::PwmOutputs:
      return 1u << 4;
    case PluginRole::ServoOutputs:
      return 1u << 5;
    case PluginRole::I2cMultiplexer:
      return 1u << 23;
    default:
      return 0;
  }
}

bool pluginIsBuiltinEs8311(uint8_t busId, uint8_t address) {
  return busId == PLUGIN_ROLE_BUS_ID && address == PLUGIN_BUILTIN_ES8311_ADDR;
}

bool pluginWellKnownChip(uint8_t busId, uint8_t address, PluginChip *out) {
  if (!out || busId != PLUGIN_ROLE_BUS_ID) return false;
  if (address == 0x3E || address == 0x3F) {
    *out = PluginChip::SX1509;
    return true;
  }
  if (address >= 0x20 && address <= 0x27) {
    *out = PluginChip::MCP23017;
    return true;
  }
  return false;
}

PluginChip pluginChipFromDeviceId(const char *id) {
  if (!id || !id[0]) return PluginChip::Unknown;
  if (equalsIgnoreCase(id, "ES8311") || endsWithIgnoreCase(id, "es8311")) {
    return PluginChip::ES8311;
  }
  if (equalsIgnoreCase(id, "SX1509") || endsWithIgnoreCase(id, "sx1509")) {
    return PluginChip::SX1509;
  }
  if (equalsIgnoreCase(id, "MCP23017") || endsWithIgnoreCase(id, "mcp23017")) {
    return PluginChip::MCP23017;
  }
  if (equalsIgnoreCase(id, "PCA9685") || endsWithIgnoreCase(id, "pca9685")) {
    return PluginChip::PCA9685;
  }
  if (equalsIgnoreCase(id, "TCA9548A") || endsWithIgnoreCase(id, "tca9548a")) {
    return PluginChip::TCA9548A;
  }
  return PluginChip::Unknown;
}

void pluginFormatAddress(uint8_t address, char *out, size_t outLen) {
  if (!out || outLen < 5) {
    if (out && outLen) out[0] = '\0';
    return;
  }
  snprintf(out, outLen, "0x%02X", address);
}

void pluginFormatDisplayName(PluginChip chip, PluginRole role, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  if (chip == PluginChip::Unknown) {
    copyStr(out, outLen, "Unknown I2C Device");
    return;
  }
  if (role == PluginRole::None) {
    snprintf(out, outLen, "%s - Unconfigured", pluginChipName(chip));
    return;
  }
  snprintf(out, outLen, "%s - %s", pluginChipName(chip), pluginRoleDisplayPhrase(role));
}

bool pluginParseI2cAddress(const char *text, uint8_t *out) {
  if (!text || !out || !addressCharsSafe(text)) return false;
  char *term = nullptr;
  unsigned long value = strtoul(text, &term, 0);
  if (!term || term == text || *term != '\0') return false;
  if (value < PLUGIN_ROLE_ADDR_MIN || value > PLUGIN_ROLE_ADDR_MAX) return false;
  *out = (uint8_t)value;
  return true;
}

const PluginRoleEntry *pluginRoleFileFind(const PluginRoleFile *roles,
                                          uint8_t busId, uint8_t address) {
  if (!roles) return nullptr;
  for (uint8_t i = 0; i < roles->deviceCount; ++i) {
    if (roles->devices[i].busId == busId && roles->devices[i].address == address) {
      return &roles->devices[i];
    }
  }
  return nullptr;
}

bool pluginParseRoleFile(const char *json, size_t jsonLen,
                         PluginRoleFile *out,
                         PluginConfigLoadResult *result) {
  PluginConfigLoadResult local = PluginConfigLoadResult::Ok;
  PluginConfigLoadResult *status = result ? result : &local;
  *status = PluginConfigLoadResult::InvalidJson;
  if (out) *out = PluginRoleFile{};
  if (!json || !out) return false;
  if (jsonLen > PLUGIN_ROLE_FILE_MAX_BYTES) {
    *status = PluginConfigLoadResult::FileTooLarge;
    return false;
  }

  JsonReader r(json, jsonLen);
  if (!r.take('{')) return false;

  bool sawVersion = false;
  bool sawDevices = false;
  uint32_t version = 0;
  PluginRoleFile file{};

  if (r.peek() == '}') {
    *status = PluginConfigLoadResult::MissingField;
    return false;
  }

  while (true) {
    char key[24];
    if (!r.string(key, sizeof(key)) || !r.take(':')) return false;

    if (strcmp(key, "formatVersion") == 0) {
      if (!r.u32(&version)) return false;
      sawVersion = true;
    } else if (strcmp(key, "devices") == 0) {
      if (!r.take('[')) return false;
      sawDevices = true;
      if (r.peek() != ']') {
        while (true) {
          if (file.deviceCount >= PLUGIN_ROLE_MAX_DEVICES) {
            *status = PluginConfigLoadResult::TooManyDevices;
            return false;
          }
          if (!r.take('{')) return false;
          PluginRoleEntry entry{};
          bool sawBus = false;
          bool sawAddress = false;
          bool sawChip = false;
          bool sawRole = false;
          if (r.peek() != '}') {
            while (true) {
              char field[20];
              if (!r.string(field, sizeof(field)) || !r.take(':')) return false;
              if (strcmp(field, "bus") == 0) {
                uint32_t bus = 0;
                if (!r.u32(&bus)) return false;
                if (bus != PLUGIN_ROLE_BUS_ID) {
                  *status = PluginConfigLoadResult::InvalidBus;
                  return false;
                }
                entry.busId = (uint8_t)bus;
                sawBus = true;
              } else if (strcmp(field, "address") == 0) {
                if (r.peek() == '"') {
                  char addr[12];
                  if (!r.string(addr, sizeof(addr))) return false;
                  if (!pluginParseI2cAddress(addr, &entry.address)) {
                    *status = PluginConfigLoadResult::InvalidAddress;
                    return false;
                  }
                } else {
                  uint32_t addr = 0;
                  if (!r.u32(&addr)) return false;
                  if (addr < PLUGIN_ROLE_ADDR_MIN || addr > PLUGIN_ROLE_ADDR_MAX) {
                    *status = PluginConfigLoadResult::InvalidAddress;
                    return false;
                  }
                  entry.address = (uint8_t)addr;
                }
                sawAddress = true;
              } else if (strcmp(field, "chip") == 0) {
                char chip[16];
                if (!r.string(chip, sizeof(chip))) return false;
                if (!pluginChipFromName(chip, &entry.chip)) {
                  *status = PluginConfigLoadResult::InvalidChip;
                  return false;
                }
                sawChip = true;
              } else if (strcmp(field, "role") == 0) {
                char role[24];
                if (!r.string(role, sizeof(role))) return false;
                if (!pluginRoleFromName(role, &entry.role)) {
                  *status = PluginConfigLoadResult::InvalidRole;
                  return false;
                }
                sawRole = true;
              } else if (!r.skipValue()) {
                return false;
              }
              if (r.peek() == '}') {
                r.take('}');
                break;
              }
              if (!r.take(',')) return false;
            }
          } else if (!r.take('}')) {
            return false;
          }
          if (!sawBus || !sawAddress || !sawChip || !sawRole) {
            *status = PluginConfigLoadResult::MissingField;
            return false;
          }
          if (!pluginRoleCompatible(entry.chip, entry.role)) {
            *status = PluginConfigLoadResult::IncompatibleRole;
            return false;
          }
          if (pluginIsBuiltinEs8311(entry.busId, entry.address) &&
              (entry.chip != PluginChip::ES8311 ||
               entry.role != PluginRole::P4InternalAudio)) {
            *status = PluginConfigLoadResult::BuiltinConflict;
            return false;
          }
          for (uint8_t i = 0; i < file.deviceCount; ++i) {
            if (file.devices[i].busId == entry.busId &&
                file.devices[i].address == entry.address) {
              *status = PluginConfigLoadResult::DuplicateAddress;
              return false;
            }
          }
          file.devices[file.deviceCount++] = entry;
          if (r.peek() == ']') {
            r.take(']');
            break;
          }
          if (!r.take(',')) return false;
        }
      } else if (!r.take(']')) {
        return false;
      }
    } else if (!r.skipValue()) {
      return false;
    }

    if (r.peek() == '}') {
      r.take('}');
      break;
    }
    if (!r.take(',')) return false;
  }

  if (!r.finished()) return false;
  if (!sawVersion || !sawDevices) {
    *status = PluginConfigLoadResult::MissingField;
    return false;
  }
  if (version != PLUGIN_ROLE_FILE_FORMAT) {
    *status = PluginConfigLoadResult::UnsupportedVersion;
    return false;
  }

  file.formatVersion = (uint16_t)version;
  *out = file;
  *status = PluginConfigLoadResult::Ok;
  return true;
}

void pluginResolveOne(uint8_t busId, uint8_t address, bool present,
                      PluginChip discoveredChip, bool discoveredChipValid,
                      const PluginRoleFile *roles,
                      PluginDescriptor *out,
                      bool rootBus) {
  if (!out) return;
  *out = PluginDescriptor{};
  out->busId = busId;
  out->address = address;
  out->online = present;

  const bool builtin = rootBus && pluginIsBuiltinEs8311(busId, address);
  const PluginRoleEntry *cfg = rootBus ? pluginRoleFileFind(roles, busId, address)
                                       : nullptr;

  if (builtin) {
    out->chip = PluginChip::ES8311;
    out->role = PluginRole::P4InternalAudio;
    out->configured = true;
    out->classification = PluginClassification::Internal;
  }

  if (cfg) {
    out->configured = true;
    if (!builtin) {
      out->chip = cfg->chip;
      out->role = cfg->role;
      out->classification = PluginClassification::Plugin;
    }
  }

  if (out->chip == PluginChip::Unknown && discoveredChipValid &&
      discoveredChip != PluginChip::Unknown) {
    out->chip = discoveredChip;
  }

  if (out->chip == PluginChip::Unknown && rootBus) {
    pluginWellKnownChip(busId, address, &out->chip);
  }

  out->deviceClass = pluginClassForChip(out->chip);
  if (out->classification == PluginClassification::Unknown) {
    out->classification = pluginClassificationForChip(out->chip);
  }

  if (out->role == PluginRole::None && out->chip == PluginChip::ES8311) {
    out->role = PluginRole::P4InternalAudio;
    out->configured = true;
    out->classification = PluginClassification::Internal;
  }
  if (out->role == PluginRole::None && out->chip == PluginChip::TCA9548A) {
    out->role = PluginRole::I2cMultiplexer;
  }

  pluginFormatDisplayName(out->chip, out->role, out->displayName, sizeof(out->displayName));
}

void pluginBuildDeviceTable(const PluginPresence *found, uint8_t foundCount,
                            const PluginRoleFile *roles,
                            PluginDescriptor *out, uint8_t outCap,
                            uint8_t *outCount) {
  if (outCount) *outCount = 0;
  if (!out || outCap == 0) return;

  uint8_t count = 0;
  bool esPresent = false;
  PluginChip esDiscovered = PluginChip::Unknown;
  bool esDiscoveredValid = false;
  if (found) {
    for (uint8_t i = 0; i < foundCount; ++i) {
      if (pluginIsBuiltinEs8311(found[i].busId, found[i].address) && found[i].present) {
        esPresent = true;
        esDiscovered = found[i].discoveredChip;
        esDiscoveredValid = found[i].discoveredChipValid;
      }
    }
  }

  pluginResolveOne(PLUGIN_ROLE_BUS_ID, PLUGIN_BUILTIN_ES8311_ADDR, esPresent,
                   esDiscovered, esDiscoveredValid, roles, &out[count]);
  count++;

  if (found) {
    for (uint8_t i = 0; i < foundCount && count < outCap; ++i) {
      if (pluginIsBuiltinEs8311(found[i].busId, found[i].address)) continue;
      pluginResolveOne(found[i].busId, found[i].address, found[i].present,
                       found[i].discoveredChip, found[i].discoveredChipValid,
                       roles, &out[count]);
      count++;
    }
  }

  if (roles) {
    for (uint8_t i = 0; i < roles->deviceCount && count < outCap; ++i) {
      const PluginRoleEntry &entry = roles->devices[i];
      bool already = false;
      for (uint8_t j = 0; j < count; ++j) {
        if (out[j].busId == entry.busId && out[j].address == entry.address) {
          already = true;
          break;
        }
      }
      if (already) continue;
      pluginResolveOne(entry.busId, entry.address, false,
                       PluginChip::Unknown, false, roles, &out[count]);
      count++;
    }
  }

  if (outCount) *outCount = count;
}
