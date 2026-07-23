#include "DeviceRouter.h"
#include "../DeviceManager.h"
#include "../DeviceRecord.h"
#include "../DeviceEventLog.h"
#include "../capability/CapabilityManager.h"
#include <string.h>
#include <ctype.h>

DeviceRouter gDeviceRouter;

static bool ieq(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return false;
  }
  return *a == 0 && *b == 0;
}

void DeviceRouter::begin() {
  ready_ = true;
  memset(&last_, 0, sizeof(last_));
  Serial.println("[DeviceRouter] ready (no hardware execution)");
}

void DeviceRouter::emit(const char *eventName, const RouteDecision &d, const ShowCommand *cmd) {
  if (!eventFn_) return;
  String json = "{";
  json += "\"ok\":";
  json += d.ok ? "true" : "false";
  json += ",\"deviceId\":\"";
  json += d.deviceId;
  json += "\",\"deviceName\":\"";
  json += d.deviceName;
  json += "\",\"board\":\"";
  json += d.boardType;
  json += "\",\"decision\":\"";
  json += d.decision;
  json += "\",\"capability\":\"";
  json += d.capability;
  json += "\",\"path\":\"";
  json += d.path;
  json += "\",\"fallbackUsed\":";
  json += d.fallbackUsed ? "true" : "false";
  json += ",\"reason\":\"";
  json += d.reason;
  json += "\"";
  if (cmd) {
    json += ",\"commandId\":\"";
    json += cmd->id;
    json += "\",\"category\":\"";
    json += commandCategoryName(cmd->category);
    json += "\",\"action\":\"";
    json += cmd->action;
    json += "\"";
  }
  json += '}';
  eventFn_(eventName, json.c_str());
}

void DeviceRouter::logEvent(const char *event, const char *detail) {
  if (log_) log_->log(event, detail);
  else Serial.printf("[Route] %s — %s\n", event ? event : "?", detail ? detail : "");
}

bool DeviceRouter::matchDestination(const char *dest, const DeviceRecord &d) const {
  if (!dest || !dest[0]) return false;
  if (ieq(dest, d.id)) return true;
  if (ieq(dest, d.boardType)) return true;
  if (ieq(dest, "relay-nodes") && ieq(d.boardType, "relay")) return true;
  if (ieq(dest, "executor") && (ieq(d.boardType, "ian") || ieq(d.boardType, "sue"))) return true;
  return false;
}

const DeviceRecord *DeviceRouter::findByDestination(const char *dest) const {
  if (!dest || !dest[0]) return nullptr;
  for (size_t i = 0; i < DeviceManager::MAX_DEVICES; i++) {
    const DeviceRecord *d = gDeviceManager.getByIndex(i);
    if (!d) break;
    if (matchDestination(dest, *d)) return d;
  }
  return nullptr;
}

const DeviceRecord *DeviceRouter::findPreferred(const char *capability) const {
  const DeviceRecord *best = nullptr;
  for (size_t i = 0; i < DeviceManager::MAX_DEVICES; i++) {
    const DeviceRecord *d = gDeviceManager.getByIndex(i);
    if (!d) break;
    if (d->presence == DevicePresence::Offline) continue;
    if (d->availability == DeviceAvailability::Unavailable) continue;
    if (!gCapabilityManager.deviceHasCapability(*d, capability)) continue;
    if (!d->preferred) continue;
    if (!best || d->routePriority > best->routePriority) best = d;
  }
  return best;
}

const DeviceRecord *DeviceRouter::findBest(const char *capability, bool onlineOnly) const {
  const DeviceRecord *best = nullptr;
  for (size_t i = 0; i < DeviceManager::MAX_DEVICES; i++) {
    const DeviceRecord *d = gDeviceManager.getByIndex(i);
    if (!d) break;
    if (onlineOnly && d->presence == DevicePresence::Offline) continue;
    if (d->availability == DeviceAvailability::Unavailable) continue;
    if (onlineOnly && d->presence == DevicePresence::Warning) {
      /* allow warning as soft-online */
    }
    if (!gCapabilityManager.deviceHasCapability(*d, capability)) continue;
    if (!best) {
      best = d;
      continue;
    }
    if (d->preferred && !best->preferred) best = d;
    else if (d->preferred == best->preferred && d->routePriority > best->routePriority) best = d;
    else if (d->preferred == best->preferred && d->routePriority == best->routePriority &&
             d->presence == DevicePresence::Online && best->presence != DevicePresence::Online) {
      best = d;
    }
  }
  return best;
}

bool DeviceRouter::resolve(const ShowCommand &cmd, RouteDecision &out) const {
  memset(&out, 0, sizeof(out));
  out.resolvedMs = millis();

  if (cmd.category == CommandCategory::Emergency || ieq(cmd.destination, "broadcast")) {
    strncpy(out.decision, "broadcast", sizeof(out.decision) - 1);
    strncpy(out.deviceId, "broadcast", sizeof(out.deviceId) - 1);
    strncpy(out.deviceName, "All Devices", sizeof(out.deviceName) - 1);
    strncpy(out.path, "Emergency/Broadcast → all online nodes", sizeof(out.path) - 1);
    strncpy(out.reason, "broadcast policy", sizeof(out.reason) - 1);
    out.ok = true;
    return true;
  }

  char need[32] = {0};
  gCapabilityManager.requiredCapability(cmd, need, sizeof(need));
  strncpy(out.capability, need, sizeof(out.capability) - 1);

  /* Specific destination (not any/auto). */
  if (cmd.destination[0] && !ieq(cmd.destination, "any") && !ieq(cmd.destination, "auto")) {
    const DeviceRecord *d = findByDestination(cmd.destination);
    if (d && d->presence != DevicePresence::Offline &&
        d->availability != DeviceAvailability::Unavailable) {
      strncpy(out.decision, "specific", sizeof(out.decision) - 1);
      strncpy(out.deviceId, d->id, sizeof(out.deviceId) - 1);
      strncpy(out.deviceName, d->friendlyName, sizeof(out.deviceName) - 1);
      strncpy(out.boardType, d->boardType, sizeof(out.boardType) - 1);
      snprintf(out.path, sizeof(out.path), "destination:%s → %s", cmd.destination, d->id);
      strncpy(out.reason, "specific destination", sizeof(out.reason) - 1);
      out.ok = true;
      return true;
    }

    /* Fallback by capability if destination offline/missing. */
    if (need[0]) {
      const DeviceRecord *fb = findPreferred(need);
      if (!fb) fb = findBest(need, true);
      if (fb) {
        strncpy(out.decision, "fallback", sizeof(out.decision) - 1);
        strncpy(out.deviceId, fb->id, sizeof(out.deviceId) - 1);
        strncpy(out.deviceName, fb->friendlyName, sizeof(out.deviceName) - 1);
        strncpy(out.boardType, fb->boardType, sizeof(out.boardType) - 1);
        out.fallbackUsed = true;
        snprintf(out.path, sizeof(out.path), "destination:%s unavailable → fallback %s (%s)",
                 cmd.destination, fb->id, need);
        strncpy(out.reason, "destination unavailable; fallback selected", sizeof(out.reason) - 1);
        out.ok = true;
        return true;
      }
    }

    strncpy(out.decision, "unavailable", sizeof(out.decision) - 1);
    snprintf(out.reason, sizeof(out.reason), "destination '%s' unavailable", cmd.destination);
    snprintf(out.path, sizeof(out.path), "destination:%s → unavailable", cmd.destination);
    out.ok = false;
    return false;
  }

  /* Capability-driven routing. */
  if (need[0]) {
    const DeviceRecord *pref = findPreferred(need);
    if (pref) {
      strncpy(out.decision, "preferred", sizeof(out.decision) - 1);
      strncpy(out.deviceId, pref->id, sizeof(out.deviceId) - 1);
      strncpy(out.deviceName, pref->friendlyName, sizeof(out.deviceName) - 1);
      strncpy(out.boardType, pref->boardType, sizeof(out.boardType) - 1);
      snprintf(out.path, sizeof(out.path), "%s:%s → preferred %s",
               commandCategoryName(cmd.category), cmd.action, pref->id);
      strncpy(out.reason, "preferred device for capability", sizeof(out.reason) - 1);
      out.ok = true;
      return true;
    }

    const DeviceRecord *best = findBest(need, true);
    if (best) {
      strncpy(out.decision, "best-match", sizeof(out.decision) - 1);
      strncpy(out.deviceId, best->id, sizeof(out.deviceId) - 1);
      strncpy(out.deviceName, best->friendlyName, sizeof(out.deviceName) - 1);
      strncpy(out.boardType, best->boardType, sizeof(out.boardType) - 1);
      snprintf(out.path, sizeof(out.path), "%s:%s → best %s (%s)",
               commandCategoryName(cmd.category), cmd.action, best->id, need);
      strncpy(out.reason, "best capability match", sizeof(out.reason) - 1);
      out.ok = true;
      return true;
    }

    /* Offline fallback candidate for visibility only — still unavailable for execution path. */
    const DeviceRecord *any = findBest(need, false);
    strncpy(out.decision, "unavailable", sizeof(out.decision) - 1);
    if (any) {
      snprintf(out.reason, sizeof(out.reason), "capability %s devices offline", need);
      snprintf(out.path, sizeof(out.path), "%s → offline:%s", need, any->id);
    } else {
      snprintf(out.reason, sizeof(out.reason), "no device with capability %s", need);
      snprintf(out.path, sizeof(out.path), "%s → none", need);
    }
    out.ok = false;
    return false;
  }

  /* System ping / no capability — require destination or default to sue. */
  const DeviceRecord *def = findByDestination(cmd.destination[0] ? cmd.destination : "sue");
  if (!def) def = gDeviceManager.getByIndex(0);
  if (def) {
    strncpy(out.decision, "specific", sizeof(out.decision) - 1);
    strncpy(out.deviceId, def->id, sizeof(out.deviceId) - 1);
    strncpy(out.deviceName, def->friendlyName, sizeof(out.deviceName) - 1);
    strncpy(out.boardType, def->boardType, sizeof(out.boardType) - 1);
    snprintf(out.path, sizeof(out.path), "system → %s", def->id);
    strncpy(out.reason, "default system target", sizeof(out.reason) - 1);
    out.ok = true;
    return true;
  }

  strncpy(out.decision, "unavailable", sizeof(out.decision) - 1);
  strncpy(out.reason, "no devices registered", sizeof(out.reason) - 1);
  strncpy(out.path, "empty registry", sizeof(out.path) - 1);
  out.ok = false;
  return false;
}

bool DeviceRouter::route(const ShowCommand &cmd) {
  RouteDecision d;
  bool ok = resolve(cmd, d);
  last_ = d;

  char detail[96];
  if (ok) {
    snprintf(detail, sizeof(detail), "%s → %s (%s)", cmd.id, d.deviceId, d.decision);
    if (d.fallbackUsed) logEvent("route.fallback", detail);
    else logEvent("route.success", detail);
    emit("route.resolved", d, &cmd);
  } else {
    snprintf(detail, sizeof(detail), "%s — %s", cmd.id, d.reason);
    logEvent("route.failed", detail);
    if (strstr(d.reason, "unavailable") || strstr(d.decision, "unavailable")) {
      logEvent("device.unavailable", detail);
    }
    emit("route.failed", d, &cmd);
  }

  emit("routing.table.updated", d, &cmd);
  return ok;
}

void DeviceRouter::appendRoutesJson(String &out) const {
  out += "{\"rules\":[";
  out += "{\"match\":\"Emergency:*\",\"policy\":\"broadcast\"},";
  out += "{\"match\":\"Lighting:SetColour\",\"policy\":\"best-match\",\"capability\":\"PixelOutput\"},";
  out += "{\"match\":\"Lighting:*\",\"policy\":\"best-match\",\"capability\":\"PixelOutput\"},";
  out += "{\"match\":\"Temperature:Get\",\"policy\":\"best-match\",\"capability\":\"Temperature\"},";
  out += "{\"match\":\"Relay:Set\",\"policy\":\"best-match\",\"capability\":\"RelayOutput\"},";
  out += "{\"match\":\"Relay:*\",\"policy\":\"best-match\",\"capability\":\"RelayOutput\"},";
  out += "{\"match\":\"Audio:*\",\"policy\":\"best-match\",\"capability\":\"AudioPlayback\"},";
  out += "{\"match\":\"Show:*|Scene:*\",\"policy\":\"preferred\",\"capability\":\"SceneRuntime\"},";
  out += "{\"match\":\"Dmx:*\",\"policy\":\"best-match\",\"capability\":\"DMXOutput\"},";
  out += "{\"match\":\"Gpio:*\",\"policy\":\"best-match\",\"capability\":\"GPIOInput\"},";
  out += "{\"match\":\"Media:*\",\"policy\":\"best-match\",\"capability\":\"MediaStorage\"},";
  out += "{\"match\":\"Network:*\",\"policy\":\"best-match\",\"capability\":\"NetworkBridge\"},";
  out += "{\"match\":\"System:Ping\",\"policy\":\"specific\",\"capability\":\"\"}";
  out += "],\"last\":{";
  out += "\"ok\":";
  out += last_.ok ? "true" : "false";
  out += ",\"deviceId\":\"";
  out += last_.deviceId;
  out += "\",\"decision\":\"";
  out += last_.decision;
  out += "\",\"capability\":\"";
  out += last_.capability;
  out += "\",\"path\":\"";
  out += last_.path;
  out += "\",\"fallbackUsed\":";
  out += last_.fallbackUsed ? "true" : "false";
  out += ",\"reason\":\"";
  out += last_.reason;
  out += "\"}}";
}

void DeviceRouter::appendRouteTestJson(const ShowCommand &cmd, String &out) const {
  RouteDecision d;
  resolve(cmd, d);
  out += '{';
  out += "\"ok\":";
  out += d.ok ? "true" : "false";
  out += ",\"resolvedDevice\":{";
  out += "\"id\":\""; out += d.deviceId; out += "\",";
  out += "\"name\":\""; out += d.deviceName; out += "\",";
  out += "\"board\":\""; out += d.boardType; out += "\"";
  out += "},";
  out += "\"routingDecision\":\""; out += d.decision; out += "\",";
  out += "\"capability\":\""; out += d.capability; out += "\",";
  out += "\"path\":\""; out += d.path; out += "\",";
  out += "\"fallbackUsed\":"; out += d.fallbackUsed ? "true" : "false"; out += ',';
  out += "\"reason\":\""; out += d.reason; out += "\",";
  out += "\"command\":";
  showCommandToJson(cmd, out);
  out += '}';
}