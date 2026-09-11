#include "NodeConfig.h"
#include <Preferences.h>
#include <string.h>

static Preferences sPref;
static bool sOk = false;

bool nodeConfigBegin(const char *ns) {
  sOk = sPref.begin((ns && ns[0]) ? ns : "sdnode", false);
  return sOk;
}

void nodeConfigGetName(char *out, size_t n, const char *fallback) {
  if (!out || !n) return;
  if (!sOk) {
    strncpy(out, fallback ? fallback : "", n - 1);
    out[n - 1] = 0;
    return;
  }
  String v = sPref.getString("name", fallback ? fallback : "");
  strncpy(out, v.c_str(), n - 1);
  out[n - 1] = 0;
}

void nodeConfigSetName(const char *name) {
  if (!sOk || !name) return;
  sPref.putString("name", name);
}

uint8_t nodeConfigGetU8(const char *key, uint8_t fallback) {
  if (!sOk || !key) return fallback;
  return (uint8_t)sPref.getUChar(key, fallback);
}

void nodeConfigSetU8(const char *key, uint8_t value) {
  if (!sOk || !key) return;
  sPref.putUChar(key, value);
}

uint16_t nodeConfigGetU16(const char *key, uint16_t fallback) {
  if (!sOk || !key) return fallback;
  return (uint16_t)sPref.getUShort(key, fallback);
}

void nodeConfigSetU16(const char *key, uint16_t value) {
  if (!sOk || !key) return;
  sPref.putUShort(key, value);
}

void nodeConfigGetStr(const char *key, char *out, size_t n, const char *fallback) {
  if (!out || !n) return;
  if (!sOk || !key) {
    strncpy(out, fallback ? fallback : "", n - 1);
    out[n - 1] = 0;
    return;
  }
  String v = sPref.getString(key, fallback ? fallback : "");
  strncpy(out, v.c_str(), n - 1);
  out[n - 1] = 0;
}

void nodeConfigSetStr(const char *key, const char *value) {
  if (!sOk || !key || !value) return;
  sPref.putString(key, value);
}
