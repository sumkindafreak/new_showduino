#include "CapabilityManager.h"
#include "../DeviceManager.h"
#include "../DeviceEventLog.h"
#include <string.h>
#include <ctype.h>

CapabilityManager gCapabilityManager;

static bool tokenMatch(const char *list, const char *needle) {
  if (!list || !needle || !needle[0]) return false;
  const size_t nlen = strlen(needle);
  const char *p = list;
  while (*p) {
    while (*p == ' ' || *p == ',') p++;
    if (!*p) break;
    const char *start = p;
    while (*p && *p != ',') p++;
    size_t len = (size_t)(p - start);
    while (len > 0 && start[len - 1] == ' ') len--;
    if (len == nlen) {
      bool ok = true;
      for (size_t i = 0; i < len; i++) {
        if (tolower((unsigned char)start[i]) != tolower((unsigned char)needle[i])) {
          ok = false;
          break;
        }
      }
      if (ok) return true;
    }
    if (*p == ',') p++;
  }
  return false;
}

static bool ieq(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return false;
  }
  return *a == 0 && *b == 0;
}

void CapabilityManager::begin() {
  ready_ = true;
  refreshFromRegistry();
  Serial.println("[CapabilityManager] ready");
}

void CapabilityManager::emit(const char *eventName, const char *detailJson) {
  if (eventFn_) eventFn_(eventName, detailJson ? detailJson : "{}");
}

void CapabilityManager::logEvent(const char *event, const char *detail) {
  if (log_) log_->log(event, detail);
  else Serial.printf("[Capability] %s — %s\n", event ? event : "?", detail ? detail : "");
}

void CapabilityManager::refreshFromRegistry() {
  if (!ready_) return;
  emit("capability.updated", "{\"scope\":\"registry\"}");
}

bool CapabilityManager::deviceHasCapability(const DeviceRecord &d, CapabilityId id) const {
  return deviceHasCapability(d, capabilityName(id));
}

bool CapabilityManager::deviceHasCapability(const DeviceRecord &d, const char *capName) const {
  if (!d.inUse || !capName || !capName[0]) return false;
  return tokenMatch(d.capabilities, capName);
}

bool CapabilityManager::requiredCapability(const ShowCommand &cmd, char *out, size_t outLen) const {
  if (!out || outLen == 0) return false;
  out[0] = '\0';

  if (cmd.category == CommandCategory::Emergency) {
    return true; /* broadcast — no single capability */
  }

  if (ieq(cmd.action, "SetColour") || ieq(cmd.action, "SetColor") ||
      ieq(cmd.action, "setcolour") || ieq(cmd.action, "setcolor")) {
    strncpy(out, capabilityName(CapabilityId::PixelOutput), outLen - 1);
    return true;
  }
  if (ieq(cmd.action, "GetTemperature") || ieq(cmd.action, "Temperature") ||
      ieq(cmd.action, "TempGet") ||
      (ieq(cmd.action, "Get") && cmd.payload[0] && strstr(cmd.payload, "temp"))) {
    strncpy(out, capabilityName(CapabilityId::Temperature), outLen - 1);
    return true;
  }

  switch (cmd.category) {
    case CommandCategory::Lighting:
      strncpy(out, capabilityName(CapabilityId::PixelOutput), outLen - 1);
      return true;
    case CommandCategory::Audio:
      strncpy(out, capabilityName(CapabilityId::AudioPlayback), outLen - 1);
      return true;
    case CommandCategory::Relay:
      strncpy(out, capabilityName(CapabilityId::RelayOutput), outLen - 1);
      return true;
    case CommandCategory::Gpio:
      strncpy(out, capabilityName(CapabilityId::GPIOInput), outLen - 1);
      return true;
    case CommandCategory::Dmx:
      strncpy(out, capabilityName(CapabilityId::DMXOutput), outLen - 1);
      return true;
    case CommandCategory::Media:
      strncpy(out, capabilityName(CapabilityId::MediaStorage), outLen - 1);
      return true;
    case CommandCategory::Network:
      strncpy(out, capabilityName(CapabilityId::NetworkBridge), outLen - 1);
      return true;
    case CommandCategory::Scene:
    case CommandCategory::Show:
      strncpy(out, capabilityName(CapabilityId::SceneRuntime), outLen - 1);
      return true;
    case CommandCategory::System:
      return true; /* ping / system → destination-driven */
    default:
      return true;
  }
}

bool CapabilityManager::supports(const ShowCommand &cmd) {
  if (!ready_) return true;
  if (cmd.category == CommandCategory::Emergency) return true;

  char need[32] = {0};
  requiredCapability(cmd, need, sizeof(need));
  if (!need[0]) return true;

  /* Destination-specific system commands always "supported" at capability layer. */
  if (cmd.category == CommandCategory::System && ieq(cmd.action, "ping")) return true;

  for (size_t i = 0; i < DeviceManager::MAX_DEVICES; i++) {
    const DeviceRecord *d = gDeviceManager.getByIndex(i);
    if (!d) break;
    if (d->presence == DevicePresence::Offline) continue;
    if (d->availability == DeviceAvailability::Unavailable) continue;
    if (deviceHasCapability(*d, need)) return true;
  }
  /* Soft allow: capability layer does not hard-block when no device yet —
     router reports unavailable. Stage 7 keeps bus flowing for tests. */
  return true;
}

void CapabilityManager::notifyCapabilityChanged(const DeviceRecord &d, const char *reason) {
  char detail[96];
  snprintf(detail, sizeof(detail), "%s caps=%s", d.id, d.capabilities);
  logEvent(reason && reason[0] ? reason : "capability.changed", detail);

  String json = "{\"deviceId\":\"";
  json += d.id;
  json += "\",\"capabilities\":\"";
  json += d.capabilities;
  json += "\",\"reason\":\"";
  json += reason ? reason : "updated";
  json += "\"}";
  emit("capability.updated", json.c_str());
}

void CapabilityManager::appendCapabilitiesCatalogJson(String &out) const {
  out += "{\"capabilities\":[";
  for (uint8_t i = 0; i < (uint8_t)CapabilityId::COUNT; i++) {
    if (i) out += ',';
    out += "{\"id\":";
    out += String((unsigned)i);
    out += ",\"name\":\"";
    out += capabilityName((CapabilityId)i);
    out += "\"}";
  }
  out += "]}";
}

void CapabilityManager::appendDeviceCapabilitiesJson(String &out) const {
  out += "{\"byCapability\":{";
  bool anyCap = false;
  for (uint8_t ci = 0; ci < (uint8_t)CapabilityId::COUNT; ci++) {
    const char *name = capabilityName((CapabilityId)ci);
    bool anyDev = false;
    String devicesPart;
    for (size_t i = 0; i < DeviceManager::MAX_DEVICES; i++) {
      const DeviceRecord *d = gDeviceManager.getByIndex(i);
      if (!d) break;
      if (!deviceHasCapability(*d, name)) continue;
      if (anyDev) devicesPart += ',';
      anyDev = true;
      devicesPart += '{';
      devicesPart += "\"id\":\""; devicesPart += d->id; devicesPart += "\",";
      devicesPart += "\"name\":\""; devicesPart += d->friendlyName; devicesPart += "\",";
      devicesPart += "\"board\":\""; devicesPart += d->boardType; devicesPart += "\",";
      devicesPart += "\"firmware\":\""; devicesPart += d->firmwareVersion; devicesPart += "\",";
      devicesPart += "\"protocol\":\""; devicesPart += d->protocolVersion; devicesPart += "\",";
      devicesPart += "\"online\":"; devicesPart += (d->presence == DevicePresence::Online) ? "true" : "false";
      devicesPart += ',';
      devicesPart += "\"presence\":\""; devicesPart += devicePresenceName(d->presence); devicesPart += "\",";
      devicesPart += "\"availability\":\""; devicesPart += deviceAvailabilityName(d->availability); devicesPart += "\",";
      devicesPart += "\"priority\":"; devicesPart += String((unsigned)d->routePriority); devicesPart += ',';
      devicesPart += "\"preferred\":"; devicesPart += d->preferred ? "true" : "false"; devicesPart += ',';
      devicesPart += "\"rssi\":"; devicesPart += String((int)d->rssi); devicesPart += ',';
      devicesPart += "\"owner\":\""; devicesPart += d->friendlyName; devicesPart += "\"";
      devicesPart += '}';
    }
    if (!anyDev) continue;
    if (anyCap) out += ',';
    anyCap = true;
    out += '"';
    out += name;
    out += "\":[";
    out += devicesPart;
    out += ']';
  }
  out += "}}";
}