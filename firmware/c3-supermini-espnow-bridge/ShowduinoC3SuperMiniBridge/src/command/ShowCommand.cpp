#include "ShowCommand.h"
#include <string.h>
#include <stdlib.h>

static bool ieq(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
    char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
    if (ca != cb) return false;
    a++; b++;
  }
  return *a == 0 && *b == 0;
}

const char *commandCategoryName(CommandCategory c) {
  switch (c) {
    case CommandCategory::System: return "system";
    case CommandCategory::Scene: return "scene";
    case CommandCategory::Show: return "show";
    case CommandCategory::Lighting: return "lighting";
    case CommandCategory::Audio: return "audio";
    case CommandCategory::Relay: return "relay";
    case CommandCategory::Gpio: return "gpio";
    case CommandCategory::Dmx: return "dmx";
    case CommandCategory::Media: return "media";
    case CommandCategory::Network: return "network";
    case CommandCategory::Emergency: return "emergency";
    default: return "custom";
  }
}

bool commandCategoryFromName(const char *name, CommandCategory &out) {
  if (!name) return false;
  if (ieq(name, "system")) { out = CommandCategory::System; return true; }
  if (ieq(name, "scene")) { out = CommandCategory::Scene; return true; }
  if (ieq(name, "show")) { out = CommandCategory::Show; return true; }
  if (ieq(name, "lighting")) { out = CommandCategory::Lighting; return true; }
  if (ieq(name, "audio")) { out = CommandCategory::Audio; return true; }
  if (ieq(name, "relay")) { out = CommandCategory::Relay; return true; }
  if (ieq(name, "gpio")) { out = CommandCategory::Gpio; return true; }
  if (ieq(name, "dmx")) { out = CommandCategory::Dmx; return true; }
  if (ieq(name, "media")) { out = CommandCategory::Media; return true; }
  if (ieq(name, "network")) { out = CommandCategory::Network; return true; }
  if (ieq(name, "emergency")) { out = CommandCategory::Emergency; return true; }
  if (ieq(name, "custom")) { out = CommandCategory::Custom; return true; }
  return false;
}

const char *commandPriorityName(CommandPriority p) {
  switch (p) {
    case CommandPriority::High: return "high";
    case CommandPriority::Emergency: return "emergency";
    default: return "normal";
  }
}

bool commandPriorityFromName(const char *name, CommandPriority &out) {
  if (!name) return false;
  if (ieq(name, "normal") || ieq(name, "0")) { out = CommandPriority::Normal; return true; }
  if (ieq(name, "high") || ieq(name, "1")) { out = CommandPriority::High; return true; }
  if (ieq(name, "emergency") || ieq(name, "2")) { out = CommandPriority::Emergency; return true; }
  return false;
}

const char *commandStatusName(CommandStatus s) {
  switch (s) {
    case CommandStatus::Received: return "received";
    case CommandStatus::Validated: return "validated";
    case CommandStatus::Rejected: return "rejected";
    case CommandStatus::Queued: return "queued";
    case CommandStatus::Started: return "started";
    case CommandStatus::Completed: return "completed";
    case CommandStatus::Cancelled: return "cancelled";
    case CommandStatus::Failed: return "failed";
    default: return "unknown";
  }
}

bool commandStatusFromName(const char *name, CommandStatus &out) {
  if (!name) return false;
  if (ieq(name, "received")) { out = CommandStatus::Received; return true; }
  if (ieq(name, "validated")) { out = CommandStatus::Validated; return true; }
  if (ieq(name, "rejected")) { out = CommandStatus::Rejected; return true; }
  if (ieq(name, "queued")) { out = CommandStatus::Queued; return true; }
  if (ieq(name, "started")) { out = CommandStatus::Started; return true; }
  if (ieq(name, "completed")) { out = CommandStatus::Completed; return true; }
  if (ieq(name, "cancelled")) { out = CommandStatus::Cancelled; return true; }
  if (ieq(name, "failed")) { out = CommandStatus::Failed; return true; }
  return false;
}

bool showCommandKnownSource(const char *source) {
  if (!source || !source[0]) return false;
  return ieq(source, "web-studio") || ieq(source, "web") ||
         ieq(source, "director") || ieq(source, "touchscreen") ||
         ieq(source, "scheduler") || ieq(source, "scene-runtime") ||
         ieq(source, "scene") || ieq(source, "rest-api") || ieq(source, "rest") ||
         ieq(source, "gpio") || ieq(source, "external-api") || ieq(source, "future");
}

bool showCommandKnownDestination(const char *destination) {
  if (!destination || !destination[0]) return false;
  return ieq(destination, "director") || ieq(destination, "sue") ||
         ieq(destination, "ian") || ieq(destination, "relay") ||
         ieq(destination, "relay-nodes") || ieq(destination, "executor") ||
         ieq(destination, "broadcast") || ieq(destination, "future") ||
         ieq(destination, "any") || ieq(destination, "auto");
}

void showCommandAssignId(ShowCommand &cmd, uint32_t seq) {
  snprintf(cmd.id, sizeof(cmd.id), "cmd-%08lx", (unsigned long)seq);
}

static void jsonEscapeAppend(String &out, const char *s) {
  if (!s) return;
  for (const char *p = s; *p; p++) {
    if (*p == '"' || *p == '\\') out += '\\';
    if (*p == '\n') { out += "\\n"; continue; }
    if (*p == '\r') continue;
    out += *p;
  }
}

void showCommandToJson(const ShowCommand &cmd, String &out) {
  out += '{';
  out += "\"id\":\""; out += cmd.id; out += "\",";
  out += "\"timestampMs\":"; out += String(cmd.timestampMs); out += ',';
  out += "\"createdEpoch\":"; out += String(cmd.createdEpoch); out += ',';
  out += "\"queuedEpoch\":"; out += String(cmd.queuedEpoch); out += ',';
  out += "\"startedEpoch\":"; out += String(cmd.startedEpoch); out += ',';
  out += "\"completedEpoch\":"; out += String(cmd.completedEpoch); out += ',';
  out += "\"source\":\""; jsonEscapeAppend(out, cmd.source); out += "\",";
  out += "\"destination\":\""; jsonEscapeAppend(out, cmd.destination); out += "\",";
  out += "\"category\":\""; out += commandCategoryName(cmd.category); out += "\",";
  out += "\"action\":\""; jsonEscapeAppend(out, cmd.action); out += "\",";
  out += "\"payload\":\""; jsonEscapeAppend(out, cmd.payload); out += "\",";
  out += "\"priority\":\""; out += commandPriorityName(cmd.priority); out += "\",";
  out += "\"status\":\""; out += commandStatusName(cmd.status); out += "\",";
  out += "\"result\":\""; jsonEscapeAppend(out, cmd.result); out += "\",";
  out += "\"executionTimeMs\":"; out += String(cmd.executionTimeMs); out += ',';
  out += "\"startedMs\":"; out += String(cmd.startedMs); out += ',';
  out += "\"completedMs\":"; out += String(cmd.completedMs);
  out += '}';
}

static bool extractJsonString(const String &json, const char *key, char *dst, size_t dstLen) {
  if (!dst || dstLen == 0) return false;
  dst[0] = 0;
  String pattern = String("\"") + key + "\"";
  int k = json.indexOf(pattern);
  if (k < 0) return false;
  int colon = json.indexOf(':', k + pattern.length());
  if (colon < 0) return false;
  int q1 = json.indexOf('"', colon + 1);
  if (q1 < 0) return false;
  int q2 = q1 + 1;
  while (q2 < (int)json.length()) {
    if (json[q2] == '"' && json[q2 - 1] != '\\') break;
    q2++;
  }
  if (q2 >= (int)json.length()) return false;
  String val = json.substring(q1 + 1, q2);
  val.replace("\\\"", "\"");
  val.replace("\\n", "\n");
  strncpy(dst, val.c_str(), dstLen - 1);
  dst[dstLen - 1] = 0;
  return true;
}

static bool extractJsonNumber(const String &json, const char *key, uint32_t &out) {
  String pattern = String("\"") + key + "\"";
  int k = json.indexOf(pattern);
  if (k < 0) return false;
  int colon = json.indexOf(':', k + pattern.length());
  if (colon < 0) return false;
  out = (uint32_t)strtoul(json.c_str() + colon + 1, nullptr, 10);
  return true;
}

bool showCommandFromJson(const String &json, ShowCommand &outCmd, String &error) {
  memset(&outCmd, 0, sizeof(outCmd));
  outCmd.inUse = true;
  outCmd.timestampMs = millis();
  outCmd.status = CommandStatus::Received;
  outCmd.priority = CommandPriority::Normal;
  outCmd.category = CommandCategory::System;

  char tmp[40];
  if (!extractJsonString(json, "source", outCmd.source, sizeof(outCmd.source))) {
    error = "missing source";
    return false;
  }
  if (!extractJsonString(json, "destination", outCmd.destination, sizeof(outCmd.destination))) {
    error = "missing destination";
    return false;
  }
  if (!extractJsonString(json, "action", outCmd.action, sizeof(outCmd.action))) {
    error = "missing action";
    return false;
  }
  extractJsonString(json, "payload", outCmd.payload, sizeof(outCmd.payload));
  extractJsonString(json, "id", outCmd.id, sizeof(outCmd.id));

  if (extractJsonString(json, "category", tmp, sizeof(tmp))) {
    if (!commandCategoryFromName(tmp, outCmd.category)) {
      error = "unknown category";
      return false;
    }
  } else {
    error = "missing category";
    return false;
  }

  if (extractJsonString(json, "priority", tmp, sizeof(tmp))) {
    if (!commandPriorityFromName(tmp, outCmd.priority)) {
      error = "invalid priority";
      return false;
    }
  } else {
    uint32_t pnum = 0;
    if (extractJsonNumber(json, "priority", pnum)) {
      if (pnum > 2) { error = "priority out of range"; return false; }
      outCmd.priority = (CommandPriority)pnum;
    }
  }

  uint32_t ts = 0;
  if (extractJsonNumber(json, "timestampMs", ts)) outCmd.timestampMs = ts;
  return true;
}