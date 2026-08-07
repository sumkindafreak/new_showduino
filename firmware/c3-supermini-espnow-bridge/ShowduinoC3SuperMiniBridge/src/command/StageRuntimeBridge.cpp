#include "CommandInterfaces.h"
#include "ShowCommand.h"
#include "../../../../../protocol/showduino_legacy_strings.h"
#include <ctype.h>
#include <string.h>

// Implemented by ShowduinoC3SuperMiniBridge.ino. This is the existing,
// authoritative UART path from SUE (C3) to the P4 Stage Engine.
extern void forwardToP4(const char *command);

namespace {

bool ieq(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return false;
  }
  return *a == 0 && *b == 0;
}

bool isAny(const char *value, const char *a, const char *b = nullptr,
           const char *c = nullptr, const char *d = nullptr) {
  return ieq(value, a) || (b && ieq(value, b)) || (c && ieq(value, c)) ||
         (d && ieq(value, d));
}

bool sendShowCommand(const ShowCommand &cmd) {
  if (isAny(cmd.action, "start", "play", "run")) {
    forwardToP4(SHOWDUINO_LEGACY_SHOW_START);
    return true;
  }
  if (ieq(cmd.action, "pause")) {
    forwardToP4("SHOW:PAUSE");
    return true;
  }
  if (ieq(cmd.action, "resume")) {
    forwardToP4("SHOW:RESUME");
    return true;
  }
  if (isAny(cmd.action, "stop", "abort")) {
    forwardToP4(SHOWDUINO_LEGACY_SHOW_STOP);
    return true;
  }
  if (isAny(cmd.action, "state", "status", "query")) {
    forwardToP4("SHOW:STATE?");
    return true;
  }

  // Load uses payload as the show name. Empty payload falls back to the P4's
  // current show name via the canonical SHOW:LOAD command.
  if (isAny(cmd.action, "load", "loadproduction")) {
    if (cmd.payload[0]) {
      String line = "SHOW:LOAD:";
      line += cmd.payload;
      forwardToP4(line.c_str());
    } else {
      forwardToP4("SHOW:LOAD");
    }
    return true;
  }

  return false;
}

bool sendEmergencyCommand(const ShowCommand &cmd) {
  if (isAny(cmd.action, "stop", "panic", "activate", "estop")) {
    forwardToP4(SHOWDUINO_LEGACY_EMERGENCY_STOP);
    return true;
  }
  if (isAny(cmd.action, "clear", "reset", "release")) {
    forwardToP4(SHOWDUINO_LEGACY_EMERGENCY_CLEAR);
    return true;
  }
  return false;
}

}  // namespace

bool StageRuntimeBridge::submit(const ShowCommand &cmd) {
  if (cmd.category == CommandCategory::Show || cmd.category == CommandCategory::Scene) {
    return sendShowCommand(cmd);
  }
  if (cmd.category == CommandCategory::Emergency) {
    return sendEmergencyCommand(cmd);
  }

  // Other categories continue through their existing router/node paths. Stage
  // Runtime bridging is deliberately limited to commands the P4 implements.
  return false;
}
