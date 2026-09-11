#include "PixelIdentity.h"
#include "../../shared-node/NodeConfig.h"

#include <string.h>
#include <stdio.h>

static char sId[SHOWDUINO_PIXEL_ID_MAX + 1] = "LED-00";
static char sName[SHOWDUINO_PIXEL_NAME_MAX + 1] = "PIXEL";

static void defaultIdFromMac(const char *mac, char *out, size_t n) {
  const char *p = mac ? strrchr(mac, ':') : nullptr;
  unsigned v = 0;
  if (p && p[1] && p[2]) {
    sscanf(p + 1, "%02X", &v);
  }
  snprintf(out, n, "LED-%02X", v & 0xFF);
}

void pixelIdentityBegin(const char *macFallback) {
  char stored[SHOWDUINO_PIXEL_ID_MAX + 1] = "";
  nodeConfigGetStr("id", stored, sizeof(stored), "");
  if (showduino_pixel_id_ok(stored)) {
    strncpy(sId, stored, sizeof(sId) - 1);
    sId[sizeof(sId) - 1] = 0;
  } else {
    defaultIdFromMac(macFallback, sId, sizeof(sId));
  }
  char name[SHOWDUINO_PIXEL_NAME_MAX + 1] = "";
  nodeConfigGetName(name, sizeof(name), "PIXEL");
  if (showduino_pixel_name_ok(name)) {
    strncpy(sName, name, sizeof(sName) - 1);
    sName[sizeof(sName) - 1] = 0;
  }
}

const char *pixelIdentityId() { return sId; }
const char *pixelIdentityName() { return sName; }

bool pixelIdentitySetId(const char *id) {
  if (!showduino_pixel_id_ok(id)) return false;
  strncpy(sId, id, sizeof(sId) - 1);
  sId[sizeof(sId) - 1] = 0;
  nodeConfigSetStr("id", sId);
  return true;
}

bool pixelIdentitySetName(const char *name) {
  if (!showduino_pixel_name_ok(name)) return false;
  strncpy(sName, name, sizeof(sName) - 1);
  sName[sizeof(sName) - 1] = 0;
  nodeConfigSetName(sName);
  return true;
}
