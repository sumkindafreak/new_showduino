#include "MosfetRecoveryStore.h"
#include <Preferences.h>
#include <string.h>
static Preferences sRecovery;
static bool sStoreReady = false;
static uint8_t sCommitted[4] = {};
static bool sHaveCommitted = false;
static uint8_t checksum(const uint8_t* bytes) {
  uint8_t value = 0xA5;
  for (uint8_t i = 0; i < 5; ++i) value ^= bytes[i];
  return value;
}
void mosfetRecoveryLoad(uint8_t levels[4]) {
  memset(levels, 0, 4);
  sHaveCommitted = false;
  if (sStoreReady) sRecovery.end();
  sStoreReady = sRecovery.begin("sdmosresume", false);
  if (!sStoreReady) {
    Serial.println("[MOSFET][ERROR] Resume storage unavailable; boot restore disabled");
    return;
  }
  uint8_t record[6] = {};
  if (sRecovery.getBytesLength("steady") != sizeof(record) ||
      sRecovery.getBytes("steady", record, sizeof(record)) != sizeof(record)) return;
  if (record[0] != 1 || record[5] != checksum(record)) return;
  for (uint8_t i = 0; i < 4; ++i) if (record[i + 1] > 100) return;
  memcpy(levels, record + 1, 4);
  memcpy(sCommitted, record + 1, 4);
  sHaveCommitted = true;
}
bool mosfetRecoverySave(const uint8_t levels[4]) {
  if (!sStoreReady) return false;
  if (sHaveCommitted && memcmp(levels, sCommitted, 4) == 0) return true;
  uint8_t record[6] = {1, levels[0], levels[1], levels[2], levels[3], 0};
  record[5] = checksum(record);
  if (sRecovery.putBytes("steady", record, sizeof(record)) != sizeof(record)) {
    Serial.println("[MOSFET][ERROR] Could not save steady output levels");
    return false;
  }
  memcpy(sCommitted, record + 1, 4);
  sHaveCommitted = true;
  return true;
}
