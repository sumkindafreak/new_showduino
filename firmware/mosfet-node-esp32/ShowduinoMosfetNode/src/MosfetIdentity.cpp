#include "MosfetIdentity.h"
#include "../BoardConfig.h"
#include "../../shared-node/NodeConfig.h"
#include <stdio.h>
#include <string.h>

static char sId[SHOWDUINO_MOSFET_ID_MAX + 1] = SHOWDUINO_MOSFET_DEFAULT_ID;
static char sName[SHOWDUINO_MOSFET_NAME_MAX + 1] = SHOWDUINO_MOSFET_DEFAULT_NAME;
static char sOut[4][SHOWDUINO_MOSFET_OUT_NAME_MAX + 1] = {
  "OUT1", "OUT2", "OUT3", "OUT4"
};

void mosfetIdentityBegin(const char *macFallback) {
  (void)macFallback;
  char stored[SHOWDUINO_MOSFET_ID_MAX + 1] = "";
  nodeConfigGetStr("id", stored, sizeof(stored), SHOWDUINO_MOSFET_DEFAULT_ID);
  if (showduino_mosfet_id_ok(stored)) {
    strncpy(sId, stored, sizeof(sId) - 1);
  }
  char name[SHOWDUINO_MOSFET_NAME_MAX + 1] = "";
  nodeConfigGetName(name, sizeof(name), SHOWDUINO_MOSFET_DEFAULT_NAME);
  if (showduino_mosfet_name_ok(name)) {
    strncpy(sName, name, sizeof(sName) - 1);
  }
  for (int i = 0; i < 4; ++i) {
    char key[8];
    snprintf(key, sizeof(key), "out%u", (unsigned)(i + 1));
    char fallback[8];
    snprintf(fallback, sizeof(fallback), "OUT%u", (unsigned)(i + 1));
    nodeConfigGetStr(key, sOut[i], sizeof(sOut[i]), fallback);
  }
}

const char *mosfetIdentityId() { return sId; }
const char *mosfetIdentityName() { return sName; }
const char *mosfetIdentityOutName(uint8_t ch) {
  if (!showduino_mosfet_channel_ok(ch)) return "OUT?";
  return sOut[ch - 1];
}

bool mosfetIdentitySetId(const char *id) {
  if (!showduino_mosfet_id_ok(id)) return false;
  strncpy(sId, id, sizeof(sId) - 1);
  sId[sizeof(sId) - 1] = 0;
  nodeConfigSetStr("id", sId);
  return true;
}

bool mosfetIdentitySetName(const char *name) {
  if (!showduino_mosfet_name_ok(name)) return false;
  strncpy(sName, name, sizeof(sName) - 1);
  sName[sizeof(sName) - 1] = 0;
  nodeConfigSetName(sName);
  return true;
}

bool mosfetIdentitySetOutName(uint8_t ch, const char *name) {
  if (!showduino_mosfet_channel_ok(ch) || !showduino_mosfet_name_ok(name)) return false;
  strncpy(sOut[ch - 1], name, sizeof(sOut[ch - 1]) - 1);
  sOut[ch - 1][sizeof(sOut[ch - 1]) - 1] = 0;
  char key[8];
  snprintf(key, sizeof(key), "out%u", (unsigned)ch);
  nodeConfigSetStr(key, sOut[ch - 1]);
  return true;
}
