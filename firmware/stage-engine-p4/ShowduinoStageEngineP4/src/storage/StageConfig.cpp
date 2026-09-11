#include "StageConfig.h"

#include <stdlib.h>
#include <string.h>
#include "../../BoardConfig.h"
#include "../../../protocol/showduino_e131.h"
#include "StageStore.h"
#include "StageLog.h"

static StageSystemConfig sSystem;
static StageE131Persist sE131;
static StagePixelsConfig sPixels;
static StageDirectorConfig sDirector;

static void copyField(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = '\0';
}

static bool jsonStr(const String &json, const char *key, char *out, size_t n) {
  String needle = String("\"") + key + "\"";
  int k = json.indexOf(needle);
  if (k < 0) return false;
  int colon = json.indexOf(':', k + needle.length());
  if (colon < 0) return false;
  int q1 = json.indexOf('"', colon + 1);
  if (q1 < 0) return false;
  int q2 = json.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  copyField(out, n, json.substring(q1 + 1, q2).c_str());
  return true;
}

static bool jsonBool(const String &json, const char *key, bool *out) {
  String needle = String("\"") + key + "\"";
  int k = json.indexOf(needle);
  if (k < 0) return false;
  int colon = json.indexOf(':', k + needle.length());
  if (colon < 0) return false;
  String rest = json.substring(colon + 1);
  rest.trim();
  if (rest.startsWith("true")) {
    *out = true;
    return true;
  }
  if (rest.startsWith("false")) {
    *out = false;
    return true;
  }
  return false;
}

static bool jsonU32(const String &json, const char *key, uint32_t *out) {
  uint32_t v = 0;
  if (!showduino_storage_json_u32(json.c_str(), json.length(), key, &v)) return false;
  *out = v;
  return true;
}

bool stageConfigFileHealthy(const char *path, size_t maxBytes) {
  String json;
  if (!stageStoreReadText(path, maxBytes, json)) return true; /* absent is defaults */
  return showduino_storage_config_check(
             json.c_str(), json.length(), maxBytes, SHOWDUINO_STORAGE_FORMAT_VERSION) ==
         SHOWDUINO_CFG_OK;
}

static String systemToJson(const StageSystemConfig &c) {
  String j;
  j.reserve(280);
  j += "{\n  \"formatVersion\": 1,\n  \"systemName\": \"";
  j += c.systemName;
  j += "\",\n  \"lastLoadedProductionId\": \"";
  j += c.lastLoadedProductionId;
  j += "\",\n  \"bootBehaviour\": \"idle\",\n  \"diagnosticsVerbose\": ";
  j += c.diagnosticsVerbose ? "true" : "false";
  j += ",\n  \"features\": {\n    \"ethernet\": ";
  j += c.featureEthernet ? "true" : "false";
  j += ",\n    \"e131Receive\": ";
  j += c.featureE131Receive ? "true" : "false";
  j += "\n  }\n}\n";
  return j;
}

static bool parseSystem(const String &json, StageSystemConfig &out) {
  if (showduino_storage_config_check(json.c_str(), json.length(),
                                    SHOWDUINO_STORAGE_SYSTEM_MAX, 1) != SHOWDUINO_CFG_OK) {
    return false;
  }
  StageSystemConfig tmp;
  jsonStr(json, "systemName", tmp.systemName, sizeof(tmp.systemName));
  jsonStr(json, "lastLoadedProductionId", tmp.lastLoadedProductionId,
          sizeof(tmp.lastLoadedProductionId));
  char boot[16] = "idle";
  jsonStr(json, "bootBehaviour", boot, sizeof(boot));
  if (!showduino_storage_boot_behaviour_ok(boot)) return false;
  copyField(tmp.bootBehaviour, sizeof(tmp.bootBehaviour), "idle");
  jsonBool(json, "diagnosticsVerbose", &tmp.diagnosticsVerbose);
  jsonBool(json, "ethernet", &tmp.featureEthernet);
  jsonBool(json, "e131Receive", &tmp.featureE131Receive);
  if (!showduino_storage_production_id_ok(tmp.lastLoadedProductionId)) return false;
  if (!tmp.systemName[0]) copyField(tmp.systemName, sizeof(tmp.systemName), "Showduino");
  out = tmp;
  return true;
}

static String e131ToJson(const StageE131Persist &c) {
  String j;
  j.reserve(200);
  j += "{\n  \"formatVersion\": 1,\n  \"enabled\": ";
  j += c.enabled ? "true" : "false";
  j += ",\n  \"universe\": ";
  j += String((unsigned)c.universe);
  j += ",\n  \"multicast\": ";
  j += c.multicast ? "true" : "false";
  j += ",\n  \"sourceTimeoutMs\": ";
  j += String((unsigned)c.sourceTimeoutMs);
  j += "\n}\n";
  return j;
}

static bool parseE131(const String &json, StageE131Persist &out) {
  if (showduino_storage_config_check(json.c_str(), json.length(),
                                    SHOWDUINO_STORAGE_SYSTEM_MAX, 1) != SHOWDUINO_CFG_OK) {
    return false;
  }
  StageE131Persist tmp;
  jsonBool(json, "enabled", &tmp.enabled);
  uint32_t u = tmp.universe;
  if (jsonU32(json, "universe", &u)) tmp.universe = (uint16_t)u;
  jsonBool(json, "multicast", &tmp.multicast);
  uint32_t t = tmp.sourceTimeoutMs;
  if (jsonU32(json, "sourceTimeoutMs", &t)) tmp.sourceTimeoutMs = (uint16_t)t;
  if (tmp.universe < SHOWDUINO_E131_UNIVERSE_MIN || tmp.universe > SHOWDUINO_E131_UNIVERSE_MAX) {
    return false;
  }
  if (tmp.sourceTimeoutMs < 200 || tmp.sourceTimeoutMs > 10000) return false;
  out = tmp;
  return true;
}

static String pixelsToJson(const StagePixelsConfig &c) {
  String j;
  j.reserve(240);
  j += "{\n  \"formatVersion\": 1,\n  \"enabled\": ";
  j += c.enabled ? "true" : "false";
  j += ",\n  \"gpio\": ";
  j += String((int)SHOWDUINO_SHOW_PIXEL_PIN);
  j += ",\n  \"pixelCount\": ";
  j += String((unsigned)c.pixelCount);
  j += ",\n  \"colourOrder\": \"";
  j += c.colourOrder;
  j += "\",\n  \"brightnessLimit\": ";
  j += String((unsigned)c.brightnessLimit);
  j += ",\n  \"defaultState\": \"off\"\n}\n";
  return j;
}

static bool parsePixels(const String &json, StagePixelsConfig &out) {
  if (showduino_storage_config_check(json.c_str(), json.length(),
                                    SHOWDUINO_STORAGE_SYSTEM_MAX, 1) != SHOWDUINO_CFG_OK) {
    return false;
  }
  StagePixelsConfig tmp;
  jsonBool(json, "enabled", &tmp.enabled);
  uint32_t gpio = (uint32_t)SHOWDUINO_SHOW_PIXEL_PIN;
  jsonU32(json, "gpio", &gpio);
  if ((int)gpio != SHOWDUINO_SHOW_PIXEL_PIN) return false;
  tmp.gpio = (int16_t)SHOWDUINO_SHOW_PIXEL_PIN;
  uint32_t count = 0;
  if (jsonU32(json, "pixelCount", &count) && count > SHOWDUINO_SHOW_PIXEL_MAX) return false;
  tmp.pixelCount = (uint16_t)count;
  jsonStr(json, "colourOrder", tmp.colourOrder, sizeof(tmp.colourOrder));
  uint32_t bri = tmp.brightnessLimit;
  if (jsonU32(json, "brightnessLimit", &bri) && bri > 255) return false;
  tmp.brightnessLimit = (uint8_t)bri;
  jsonStr(json, "defaultState", tmp.defaultState, sizeof(tmp.defaultState));
  if (strcmp(tmp.defaultState, "off") != 0 && strcmp(tmp.defaultState, "black") != 0) {
    copyField(tmp.defaultState, sizeof(tmp.defaultState), "off");
  }
  out = tmp;
  return true;
}

static String directorToJson(const StageDirectorConfig &c) {
  String j;
  j.reserve(120);
  j += "{\n  \"formatVersion\": 1,\n  \"statusPublishMs\": ";
  j += String((unsigned)c.statusPublishMs);
  j += "\n}\n";
  return j;
}

static bool parseDirector(const String &json, StageDirectorConfig &out) {
  if (showduino_storage_config_check(json.c_str(), json.length(),
                                    SHOWDUINO_STORAGE_SYSTEM_MAX, 1) != SHOWDUINO_CFG_OK) {
    return false;
  }
  StageDirectorConfig tmp;
  uint32_t ms = tmp.statusPublishMs;
  if (jsonU32(json, "statusPublishMs", &ms)) {
    if (ms < 200 || ms > 10000) return false;
    tmp.statusPublishMs = (uint16_t)ms;
  }
  out = tmp;
  return true;
}

static void loadOrDefault(const char *path,
                          bool (*parse)(const String &, void *),
                          void *target,
                          const String &defaults,
                          const char *label) {
  String json;
  if (stageStoreReadText(path, SHOWDUINO_STORAGE_SYSTEM_MAX, json)) {
    if (parse(json, target)) {
      Serial.printf("[STORAGE] Loaded %s\n", path);
      return;
    }
    Serial.printf("[STORAGE] %s rejected — firmware defaults\n", label);
    stageLogWrite(StageLogChannel::System, "WARN", label);
  }
  if (stageStoreWritable()) {
    if (stageStoreAtomicWrite(path, defaults.c_str(), defaults.length())) {
      Serial.printf("[STORAGE] Wrote default %s\n", path);
    }
  }
}

static bool parseSystemWrap(const String &json, void *target) {
  return parseSystem(json, *static_cast<StageSystemConfig *>(target));
}
static bool parseE131Wrap(const String &json, void *target) {
  return parseE131(json, *static_cast<StageE131Persist *>(target));
}
static bool parsePixelsWrap(const String &json, void *target) {
  return parsePixels(json, *static_cast<StagePixelsConfig *>(target));
}
static bool parseDirectorWrap(const String &json, void *target) {
  return parseDirector(json, *static_cast<StageDirectorConfig *>(target));
}

static bool migrateE131FromNetwork() {
  String json;
  if (!stageStoreReadText(PATH_NETWORK_CONFIG, SHOWDUINO_STORAGE_CONFIG_MAX, json)) return false;
  const int key = json.indexOf("\"e131\"");
  if (key < 0) return false;
  const int brace = json.indexOf('{', key);
  if (brace < 0) return false;
  String slice;
  int depth = 0;
  for (int i = brace; i < (int)json.length(); i++) {
    if (json.charAt(i) == '{') depth++;
    else if (json.charAt(i) == '}') {
      depth--;
      if (depth == 0) {
        slice = json.substring(brace, i + 1);
        break;
      }
    }
  }
  if (slice.length() == 0) return false;
  bool en = sE131.enabled;
  const bool haveEn = jsonBool(slice, "enabled", &en);
  uint32_t u = 0;
  const bool haveU = jsonU32(slice, "universe", &u);
  if (!haveU && !haveEn) return false;
  if (haveU) {
    if (u < SHOWDUINO_E131_UNIVERSE_MIN || u > SHOWDUINO_E131_UNIVERSE_MAX) return false;
    sE131.universe = (uint16_t)u;
  }
  if (haveEn) sE131.enabled = en;
  Serial.printf("[STORAGE] Migrated E1.31 from network.json universe=%u\n",
                (unsigned)sE131.universe);
  return true;
}

void stageConfigBegin() {
  sSystem = StageSystemConfig();
  sE131 = StageE131Persist();
  sPixels = StagePixelsConfig();
  sPixels.gpio = (int16_t)SHOWDUINO_SHOW_PIXEL_PIN;
  sDirector = StageDirectorConfig();

  loadOrDefault(PATH_SYSTEM_CONFIG, parseSystemWrap, &sSystem, systemToJson(sSystem), "system.json");

  String e131Json;
  if (stageStoreReadText(PATH_E131_CONFIG, SHOWDUINO_STORAGE_SYSTEM_MAX, e131Json) &&
      parseE131(e131Json, sE131)) {
    Serial.println("[STORAGE] Loaded /showduino/config/e131.json");
  } else {
    if (e131Json.length() > 0) {
      Serial.println("[STORAGE] e131.json rejected — firmware defaults");
    }
    (void)migrateE131FromNetwork();
    if (stageStoreWritable()) {
      const String def = e131ToJson(sE131);
      if (stageStoreAtomicWrite(PATH_E131_CONFIG, def.c_str(), def.length())) {
        Serial.println("[STORAGE] Wrote /showduino/config/e131.json");
      }
    }
  }

  loadOrDefault(PATH_PIXELS_CONFIG, parsePixelsWrap, &sPixels, pixelsToJson(sPixels), "pixels.json");
  loadOrDefault(PATH_DIRECTOR_CONFIG, parseDirectorWrap, &sDirector, directorToJson(sDirector),
                "director.json");
}

const StageSystemConfig &stageConfigSystem() { return sSystem; }
const StageE131Persist &stageConfigE131() { return sE131; }
const StagePixelsConfig &stageConfigPixels() { return sPixels; }
const StageDirectorConfig &stageConfigDirector() { return sDirector; }

bool stageConfigSaveSystem() {
  const String json = systemToJson(sSystem);
  if (!stageStoreAtomicWrite(PATH_SYSTEM_CONFIG, json.c_str(), json.length())) return false;
  stageLogWrite(StageLogChannel::System, "INFO", "system.json saved");
  return true;
}

bool stageConfigSaveE131() {
  const String json = e131ToJson(sE131);
  if (!stageStoreAtomicWrite(PATH_E131_CONFIG, json.c_str(), json.length())) return false;
  stageLogWrite(StageLogChannel::Network, "INFO", "e131.json saved");
  return true;
}

bool stageConfigSavePixels() {
  const String json = pixelsToJson(sPixels);
  if (!stageStoreAtomicWrite(PATH_PIXELS_CONFIG, json.c_str(), json.length())) return false;
  stageLogWrite(StageLogChannel::System, "INFO", "pixels.json saved");
  return true;
}

void stageConfigSetPixelCount(uint16_t count) {
  sPixels.pixelCount = count;
  (void)stageConfigSavePixels();
}

void stageConfigSetPixelsEnabled(bool enabled) {
  sPixels.enabled = enabled;
  (void)stageConfigSavePixels();
}

void stageConfigSetLastProduction(const char *id) {
  if (!showduino_storage_production_id_ok(id)) return;
  copyField(sSystem.lastLoadedProductionId, sizeof(sSystem.lastLoadedProductionId), id ? id : "");
  copyField(sSystem.bootBehaviour, sizeof(sSystem.bootBehaviour), "idle");
  (void)stageConfigSaveSystem();
}

void stageConfigApplyE131To(bool *enabled, uint16_t *universe) {
  if (enabled) *enabled = sE131.enabled;
  if (universe) *universe = sE131.universe;
}

void stageConfigTakeE131From(bool enabled, uint16_t universe) {
  sE131.enabled = enabled;
  sE131.universe = universe;
  (void)stageConfigSaveE131();
}
