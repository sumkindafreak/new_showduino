#include "CommandInterfaces.h"
#include "ShowCommand.h"
#include "../DeskUartBridge.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include <ctype.h>

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
    deskUartBridgeSendToP4(SHOWDUINO_LEGACY_SHOW_START);
    return true;
  }
  if (ieq(cmd.action, "pause")) {
    deskUartBridgeSendToP4("SHOW:PAUSE");
    return true;
  }
  if (ieq(cmd.action, "resume")) {
    deskUartBridgeSendToP4("SHOW:RESUME");
    return true;
  }
  if (isAny(cmd.action, "stop", "abort")) {
    deskUartBridgeSendToP4(SHOWDUINO_LEGACY_SHOW_STOP);
    return true;
  }
  if (isAny(cmd.action, "state", "status", "query")) {
    deskUartBridgeSendToP4("SHOW:STATE?");
    return true;
  }
  if (isAny(cmd.action, "load", "loadproduction")) {
    if (cmd.payload[0]) {
      String line = "SHOW:LOAD:";
      line += cmd.payload;
      deskUartBridgeSendToP4(line.c_str());
    } else {
      deskUartBridgeSendToP4("SHOW:LOAD");
    }
    return true;
  }
  return false;
}

bool sendEmergencyCommand(const ShowCommand &cmd) {
  if (isAny(cmd.action, "stop", "panic", "activate", "estop")) {
    deskUartBridgeSendToP4(SHOWDUINO_LEGACY_EMERGENCY_STOP);
    return true;
  }
  if (isAny(cmd.action, "clear", "reset", "release")) {
    deskUartBridgeSendToP4(SHOWDUINO_LEGACY_EMERGENCY_CLEAR);
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

  /* Other categories remain on their dedicated node/routing paths. */
  return false;
}
