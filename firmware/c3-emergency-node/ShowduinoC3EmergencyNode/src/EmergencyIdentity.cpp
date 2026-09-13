#include "EmergencyIdentity.h"
#include "../../shared-node/NodeConfig.h"

#include <string.h>
#include <stdio.h>

static char sId[SHOWDUINO_EMERGENCY_ID_MAX + 1] = SHOWDUINO_EMERGENCY_ID_DEFAULT;
static char sName[SHOWDUINO_EMERGENCY_NAME_MAX + 1] = SHOWDUINO_EMERGENCY_NAME_DEFAULT;

static void defaultIdFromMac(const char *mac, char *out, size_t n) {
  const char *p = mac ? strrchr(mac, ':') : nullptr;
  unsigned v = 1;
  if (p && p[1] && p[2]) {
    sscanf(p + 1, "%02X", &v);
    v = (v % 99) + 1;
  }
  snprintf(out, n, "ESTOP-%02u", v);
}

void emergencyIdentityBegin(const char *macFallback) {
  char stored[SHOWDUINO_EMERGENCY_ID_MAX + 1] = "";
  nodeConfigGetStr("id", stored, sizeof(stored), "");
  if (showduino_emergency_id_ok(stored)) {
    strncpy(sId, stored, sizeof(sId) - 1);
    sId[sizeof(sId) - 1] = 0;
  } else {
    defaultIdFromMac(macFallback, sId, sizeof(sId));
  }
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1] = "";
  nodeConfigGetName(name, sizeof(name), SHOWDUINO_EMERGENCY_NAME_DEFAULT);
  if (showduino_emergency_name_ok(name)) {
    strncpy(sName, name, sizeof(sName) - 1);
    sName[sizeof(sName) - 1] = 0;
  }
}

const char *emergencyIdentityId() { return sId; }
const char *emergencyIdentityName() { return sName; }

bool emergencyIdentitySetId(const char *id) {
  if (!showduino_emergency_id_ok(id)) return false;
  strncpy(sId, id, sizeof(sId) - 1);
  sId[sizeof(sId) - 1] = 0;
  nodeConfigSetStr("id", sId);
  return true;
}

bool emergencyIdentitySetName(const char *name) {
  if (!showduino_emergency_name_ok(name)) return false;
  strncpy(sName, name, sizeof(sName) - 1);
  sName[sizeof(sName) - 1] = 0;
  nodeConfigSetName(sName);
  return true;
}
