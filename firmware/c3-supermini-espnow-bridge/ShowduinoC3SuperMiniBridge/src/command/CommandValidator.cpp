#include "CommandValidator.h"
#include <string.h>

static bool actionAllowed(CommandCategory cat, const char *action) {
  if (!action || !action[0]) return false;
  /* Stage 6: accept well-formed actions; hardware policy comes later. */
  (void)cat;
  if (strlen(action) >= 36) return false;
  for (const char *p = action; *p; p++) {
    if (*p < 32 || *p > 126) return false;
  }
  return true;
}

static bool payloadRequired(CommandCategory cat, const char *action) {
  if (!action) return false;
  if (cat == CommandCategory::Relay) return true;
  if (cat == CommandCategory::Lighting) return true;
  if (cat == CommandCategory::Audio) return true;
  if (cat == CommandCategory::Dmx) return true;
  if (strcmp(action, "set") == 0 || strcmp(action, "write") == 0) return true;
  return false;
}

CommandValidationResult CommandValidator::validate(const ShowCommand &cmd) const {
  CommandValidationResult r;
  if (!showCommandKnownSource(cmd.source)) {
    strncpy(r.error, "unknown source", sizeof(r.error) - 1);
    return r;
  }
  if (!showCommandKnownDestination(cmd.destination)) {
    strncpy(r.error, "unknown destination", sizeof(r.error) - 1);
    return r;
  }
  if (!commandCategoryName(cmd.category) ||
      (cmd.category != CommandCategory::Custom &&
       cmd.category > CommandCategory::Emergency)) {
    /* enum always valid if cast carefully; still check action */
  }
  if (!actionAllowed(cmd.category, cmd.action)) {
    strncpy(r.error, "invalid action", sizeof(r.error) - 1);
    return r;
  }
  if ((uint8_t)cmd.priority > (uint8_t)CommandPriority::Emergency) {
    strncpy(r.error, "priority out of range", sizeof(r.error) - 1);
    return r;
  }
  if (payloadRequired(cmd.category, cmd.action) && !cmd.payload[0]) {
    strncpy(r.error, "payload required", sizeof(r.error) - 1);
    return r;
  }
  r.ok = true;
  return r;
}