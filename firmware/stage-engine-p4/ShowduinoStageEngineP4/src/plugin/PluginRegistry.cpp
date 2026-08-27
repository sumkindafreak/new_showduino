#include "PluginRegistry.h"
#include "PluginJson.h"
#include "PluginDriver.h"
#include "../StageStorage.h"
#include "../../BoardConfig.h"
#include <SD_MMC.h>
#include <string.h>
#include <stdlib.h>

static PluginDef sDefs[PLUGIN_MAX_DEFS];
static uint8_t sDefCount = 0;
static PluginConfigInstance sConfigs[PLUGIN_MAX_INSTANCES];
static uint8_t sConfigCount = 0;
static bool sSdAvailable = false;
static bool sDefsOk = true;

static bool parseSafeState(const char *json, PluginSafeState *out) {
  char s[12];
  if (!pluginJsonExtractString(json, "safe_state", s, sizeof(s))) return false;
  if (!strcmp(s, "hold")) *out = PluginSafeState::Hold;
  else if (!strcmp(s, "custom")) *out = PluginSafeState::Custom;
  else *out = PluginSafeState::Off;
  return true;
}

static bool parseAddresses(const char *json, uint8_t *minAddr, uint8_t *maxAddr) {
  const char *p = strstr(json, "\"addresses\"");
  if (!p) return false;
  p = strchr(p, '[');
  if (!p) return false;
  p = strchr(p, '"');
  if (!p) return false;
  p++;
  char range[24];
  size_t n = 0;
  while (*p && *p != '"' && n + 1 < sizeof(range)) range[n++] = *p++;
  range[n] = '\0';
  return pluginJsonParseAddressRange(range, minAddr, maxAddr);
}

static bool parseIdentify(const char *json, PluginIdentify *id) {
  const char *b = nullptr;
  const char *e = nullptr;
  if (!pluginJsonObjectSlice(json, "identify", &b, &e)) return false;
  char obj[192];
  size_t n = (size_t)(e - b);
  if (n >= sizeof(obj)) n = sizeof(obj) - 1;
  memcpy(obj, b, n);
  obj[n] = '\0';
  if (!pluginJsonExtractHexByte(obj, "register", &id->reg)) return false;
  id->mask = 0xFF;
  pluginJsonExtractHexByte(obj, "mask", &id->mask);
  if (!pluginJsonExtractHexByte(obj, "equals", &id->equals)) return false;
  id->enabled = true;
  return true;
}

static bool loadDefFile(const char *path) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    Serial.printf("[PLUGIN] Invalid definition: %s\n", path);
    sDefsOk = false;
    return false;
  }
  if (f.size() > PLUGIN_MAX_JSON_BYTES) {
    Serial.printf("[PLUGIN] Invalid definition: %s\n", path);
    f.close();
    sDefsOk = false;
    return false;
  }
  char buf[PLUGIN_MAX_JSON_BYTES + 1];
  int n = f.read((uint8_t *)buf, PLUGIN_MAX_JSON_BYTES);
  f.close();
  if (n < 0) {
    Serial.printf("[PLUGIN] Invalid definition: %s\n", path);
    sDefsOk = false;
    return false;
  }
  buf[n] = '\0';

  uint32_t schema = 0;
  pluginJsonExtractU32(buf, "showduino_plugin_schema", &schema);
  if (schema != PLUGIN_SCHEMA_VERSION) {
    Serial.printf("[PLUGIN] Invalid definition: %s\n", path);
    sDefsOk = false;
    return false;
  }
  if (sDefCount >= PLUGIN_MAX_DEFS) {
    Serial.println("[PLUGIN] Definition table full — extra files ignored");
    return false;
  }

  PluginDef def = {};
  if (!pluginJsonExtractString(buf, "id", def.id, sizeof(def.id))) {
    Serial.printf("[PLUGIN] Invalid definition: %s\n", path);
    sDefsOk = false;
    return false;
  }
  for (uint8_t i = 0; i < sDefCount; i++) {
    if (!strcmp(sDefs[i].id, def.id)) {
      Serial.printf("[PLUGIN] Duplicate definition id: %s\n", def.id);
      sDefsOk = false;
      return false;
    }
  }
  pluginJsonExtractString(buf, "name", def.name, sizeof(def.name));
  pluginJsonExtractString(buf, "driver", def.driver, sizeof(def.driver));
  if (!parseAddresses(buf, &def.addrMin, &def.addrMax)) {
    def.addrMin = PLUGIN_ADDR_MIN;
    def.addrMax = PLUGIN_ADDR_MAX;
  }
  def.capabilities = pluginJsonParseCapabilities(buf);
  parseIdentify(buf, &def.identify);
  parseSafeState(buf, &def.safeState);
  def.fromSd = true;
  sDefs[sDefCount++] = def;
  return true;
}

static void loadDeviceDir() {
  File dir = SD_MMC.open(PATH_PLUGIN_DEVICES);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      const char *name = f.name();
      const char *slash = strrchr(name, '/');
      const char *base = slash ? slash + 1 : name;
      if (strstr(base, ".json")) {
        char path[96];
        snprintf(path, sizeof(path), "%s/%s", PATH_PLUGIN_DEVICES, base);
        loadDefFile(path);
      }
    }
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
}

static void loadRegistry() {
  File f = SD_MMC.open(PATH_PLUGIN_REGISTRY, FILE_READ);
  if (!f) return;
  if (f.size() > PLUGIN_MAX_JSON_BYTES) {
    Serial.println("[PLUGIN] Invalid definition: registry.json");
    sDefsOk = false;
    f.close();
    return;
  }
  char buf[PLUGIN_MAX_JSON_BYTES + 1];
  int n = f.read((uint8_t *)buf, PLUGIN_MAX_JSON_BYTES);
  f.close();
  if (n < 0) return;
  buf[n] = '\0';

  uint32_t schema = 0;
  if (!pluginJsonExtractU32(buf, "showduino_plugin_schema", &schema) ||
      schema != PLUGIN_SCHEMA_VERSION) {
    Serial.println("[PLUGIN] Invalid definition: registry.json");
    sDefsOk = false;
    return;
  }

  int count = pluginJsonArrayObjectCount(buf, "instances");
  for (int i = 0; i < count && sConfigCount < PLUGIN_MAX_INSTANCES; i++) {
    const char *b = nullptr;
    const char *e = nullptr;
    if (!pluginJsonArrayObjectAt(buf, "instances", i, &b, &e)) continue;
    char obj[384];
    size_t len = (size_t)(e - b);
    if (len >= sizeof(obj)) len = sizeof(obj) - 1;
    memcpy(obj, b, len);
    obj[len] = '\0';

    PluginConfigInstance cfg = {};
    pluginJsonExtractString(obj, "instance", cfg.instanceId, sizeof(cfg.instanceId));
    pluginJsonExtractString(obj, "device", cfg.deviceId, sizeof(cfg.deviceId));
    pluginJsonExtractString(obj, "friendly_name", cfg.friendly, sizeof(cfg.friendly));
    const char *lb = nullptr;
    const char *le = nullptr;
    if (pluginJsonObjectSlice(obj, "location", &lb, &le)) {
      char loc[160];
      size_t ln = (size_t)(le - lb);
      if (ln >= sizeof(loc)) ln = sizeof(loc) - 1;
      memcpy(loc, lb, ln);
      loc[ln] = '\0';
      uint32_t bus = SHOWDUINO_PLUGIN_BUS_ID;
      pluginJsonExtractU32(loc, "bus", &bus);
      cfg.loc.busId = (uint8_t)bus;
      char addr[12];
      if (pluginJsonExtractString(loc, "address", addr, sizeof(addr))) {
        cfg.loc.address = (uint8_t)strtoul(addr, nullptr, 0);
      }
      uint32_t mux = 0;
      if (pluginJsonExtractU32(loc, "mux", &mux) || pluginJsonExtractU32(loc, "mux_address", &mux)) {
        cfg.loc.muxAddr = (uint8_t)mux;
      }
      uint32_t ch = PLUGIN_MUX_CH_NONE;
      if (pluginJsonExtractU32(loc, "channel", &ch) || pluginJsonExtractU32(loc, "mux_channel", &ch)) {
        cfg.loc.muxChannel = (uint8_t)ch;
      }
    }
    if (cfg.instanceId[0] == '\0' && cfg.loc.address == 0) continue;
    for (uint8_t j = 0; j < sConfigCount; j++) {
      if (cfg.instanceId[0] && !strcmp(sConfigs[j].instanceId, cfg.instanceId)) {
        Serial.printf("[PLUGIN] Duplicate instance id: %s\n", cfg.instanceId);
        sDefsOk = false;
        cfg.instanceId[0] = '\0';
        break;
      }
    }
    cfg.used = true;
    sConfigs[sConfigCount++] = cfg;
  }
}

void pluginRegistryClear() {
  for (uint8_t i = 0; i < PLUGIN_MAX_DEFS; i++) sDefs[i] = PluginDef{};
  for (uint8_t i = 0; i < PLUGIN_MAX_INSTANCES; i++) sConfigs[i] = PluginConfigInstance{};
  sDefCount = 0;
  sConfigCount = 0;
  sSdAvailable = false;
  sDefsOk = true;
}

void pluginRegistryLoadFromSd() {
  pluginRegistryClear();
  if (!stageStorageIsReady()) {
    Serial.println("[PLUGIN] SD plugin registry unavailable — native discovery only");
    return;
  }
  sSdAvailable = true;
  loadRegistry();
  loadDeviceDir();
}

const PluginDef *pluginRegistryFindDef(const char *id) {
  if (!id) return nullptr;
  for (uint8_t i = 0; i < sDefCount; i++) {
    if (!strcmp(sDefs[i].id, id)) return &sDefs[i];
  }
  return nullptr;
}

const PluginDef *pluginRegistryDefAt(uint8_t index) {
  return (index < sDefCount) ? &sDefs[index] : nullptr;
}

uint8_t pluginRegistryDefCount() { return sDefCount; }

const PluginConfigInstance *pluginRegistryConfigAt(uint8_t index) {
  return (index < sConfigCount) ? &sConfigs[index] : nullptr;
}

uint8_t pluginRegistryConfigCount() { return sConfigCount; }

bool pluginRegistrySdAvailable() { return sSdAvailable; }

bool pluginRegistryDefinitionsOk() { return sDefsOk; }

const PluginDef *pluginRegistryMatchIdentify(const PluginLocation &loc, uint8_t *matchCount) {
  uint8_t n = 0;
  const PluginDef *hit = nullptr;
  for (uint8_t i = 0; i < sDefCount; i++) {
    const PluginDef &d = sDefs[i];
    if (loc.address < d.addrMin || loc.address > d.addrMax) continue;
    if (!d.identify.enabled) continue;
    if (pluginGenericIdentify(loc, &d)) {
      n++;
      hit = &d;
    }
  }
  if (matchCount) *matchCount = n;
  return (n == 1) ? hit : nullptr;
}

const PluginConfigInstance *pluginRegistryConfigFor(const PluginLocation &loc) {
  for (uint8_t i = 0; i < sConfigCount; i++) {
    if (pluginLocationEqual(sConfigs[i].loc, loc)) return &sConfigs[i];
  }
  return nullptr;
}
