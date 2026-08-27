#include "PluginBus.h"
#include "PluginRegistry.h"
#include "PluginDriver.h"
#include "../../BoardConfig.h"
#include <Wire.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if SHOWDUINO_PLUGIN_BUS_ENABLED

static PluginInstance sInst[PLUGIN_MAX_INSTANCES];
static uint8_t sCount = 0;
static bool sReady = false;
static bool sSdaIdleHigh = false;
static bool sSclIdleHigh = false;
static void (*sPump)() = nullptr;
static uint8_t sHealthIndex = 0;
static uint32_t sLastHealthMs = 0;
static const uint32_t kHealthPeriodMs = 5000UL;

static void pump() {
  if (sPump) sPump();
}

static void copyStr(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

void pluginBusFormatPath(const PluginLocation &loc, char *out, size_t outLen) {
  if (!out || outLen < 8) return;
  if (loc.muxAddr != PLUGIN_MUX_NONE && loc.muxChannel != PLUGIN_MUX_CH_NONE) {
    snprintf(out, outLen, "bus%u/mux%02X/ch%u/0x%02X",
             (unsigned)loc.busId, loc.muxAddr, loc.muxChannel, loc.address);
  } else {
    snprintf(out, outLen, "bus%u/0x%02X", (unsigned)loc.busId, loc.address);
  }
}

static bool sampleIdle(int pin) {
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(20);
  return digitalRead(pin) == HIGH;
}

static void recoverBusIfNeeded() {
  const int sda = SHOWDUINO_PLUGIN_BUS_SDA_PIN;
  const int scl = SHOWDUINO_PLUGIN_BUS_SCL_PIN;
  sSdaIdleHigh = sampleIdle(sda);
  sSclIdleHigh = sampleIdle(scl);
  if (!sSdaIdleHigh) {
    Serial.println("[PLUGIN] WARN: SDA stuck LOW");
  }
  if (!sSclIdleHigh) {
    Serial.println("[PLUGIN] WARN: SCL stuck LOW");
  }
  if (!sSdaIdleHigh && sSclIdleHigh) {
    /* Bounded I²C recovery: clock SCL up to 9 times, then STOP. */
    pinMode(scl, OUTPUT);
    pinMode(sda, INPUT_PULLUP);
    for (int i = 0; i < 9; i++) {
      digitalWrite(scl, HIGH);
      delayMicroseconds(5);
      digitalWrite(scl, LOW);
      delayMicroseconds(5);
      if (digitalRead(sda) == HIGH) break;
    }
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    pinMode(sda, OUTPUT);
    digitalWrite(sda, HIGH);
    delayMicroseconds(5);
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    sSdaIdleHigh = sampleIdle(sda);
    sSclIdleHigh = sampleIdle(scl);
  }
}

bool pluginBusPing(const PluginLocation &loc) {
  if (loc.muxAddr != PLUGIN_MUX_NONE) {
    if (!pluginMuxSelect(loc.muxAddr, loc.muxChannel)) return false;
  }
  Wire.beginTransmission(loc.address);
  uint8_t err = Wire.endTransmission();
  if (loc.muxAddr != PLUGIN_MUX_NONE) {
    pluginMuxSelect(loc.muxAddr, PLUGIN_MUX_CH_NONE);
  }
  return err == 0;
}

static PluginInstance *findSlot(const PluginLocation &loc) {
  for (uint8_t i = 0; i < sCount; i++) {
    if (pluginLocationEqual(sInst[i].loc, loc)) return &sInst[i];
  }
  return nullptr;
}

static PluginInstance *addSlot(const PluginLocation &loc) {
  PluginInstance *e = findSlot(loc);
  if (e) return e;
  if (sCount >= PLUGIN_MAX_INSTANCES) return nullptr;
  e = &sInst[sCount++];
  *e = PluginInstance{};
  e->loc = loc;
  return e;
}

static void classify(PluginInstance &inst, bool present) {
  const uint32_t now = millis();
  const PluginConfigInstance *cfg = pluginRegistryConfigFor(inst.loc);
  if (cfg) {
    inst.configured = true;
    copyStr(inst.instanceId, sizeof(inst.instanceId), cfg->instanceId);
    copyStr(inst.deviceId, sizeof(inst.deviceId), cfg->deviceId);
    copyStr(inst.friendly, sizeof(inst.friendly), cfg->friendly);
    const PluginDef *def = pluginRegistryFindDef(cfg->deviceId);
    if (def) {
      copyStr(inst.driver, sizeof(inst.driver), def->driver);
      inst.capabilities = def->capabilities;
      inst.safeState = def->safeState;
    } else if (cfg->deviceId[0]) {
      copyStr(inst.driver, sizeof(inst.driver), cfg->deviceId);
    }
  }

  if (!present) {
    if (inst.status == PluginStatus::Online || inst.status == PluginStatus::Unknown ||
        inst.status == PluginStatus::Ambiguous) {
      inst.status = PluginStatus::Offline;
      Serial.printf("[PLUGIN] %s OFFLINE\n",
                    inst.friendly[0] ? inst.friendly : "device");
    } else if (inst.configured) {
      inst.status = PluginStatus::Offline;
    }
    return;
  }

  inst.lastSeenMs = now;
  if (inst.firstSeenMs == 0) inst.firstSeenMs = now;

  if (!cfg) {
    uint8_t matches = 0;
    const PluginDef *idHit = pluginRegistryMatchIdentify(inst.loc, &matches);
    if (matches > 1) {
      copyStr(inst.deviceId, sizeof(inst.deviceId), "generic.i2c.unknown");
      copyStr(inst.driver, sizeof(inst.driver), "generic.i2c.unknown");
      copyStr(inst.friendly, sizeof(inst.friendly), "UNKNOWN/AMBIGUOUS");
      inst.status = PluginStatus::Ambiguous;
      inst.identityFromAddressOnly = false;
    } else if (idHit) {
      copyStr(inst.deviceId, sizeof(inst.deviceId), idHit->id);
      copyStr(inst.driver, sizeof(inst.driver), idHit->driver);
      copyStr(inst.friendly, sizeof(inst.friendly), idHit->name);
      inst.capabilities = idHit->capabilities;
      inst.safeState = idHit->safeState;
      inst.status = PluginStatus::Online;
    } else if (pluginDriverFind("waveshare.es8311") &&
               pluginDriverFind("waveshare.es8311")->identify &&
               pluginDriverFind("waveshare.es8311")->identify(inst.loc, nullptr)) {
      copyStr(inst.deviceId, sizeof(inst.deviceId), "waveshare.es8311");
      copyStr(inst.driver, sizeof(inst.driver), "waveshare.es8311");
      copyStr(inst.friendly, sizeof(inst.friendly), "Onboard ES8311");
      inst.status = PluginStatus::Online;
    } else {
      copyStr(inst.deviceId, sizeof(inst.deviceId), "generic.i2c.unknown");
      copyStr(inst.driver, sizeof(inst.driver), "generic.i2c.unknown");
      if (!inst.friendly[0]) copyStr(inst.friendly, sizeof(inst.friendly), "UNKNOWN I2C DEVICE");
      inst.status = PluginStatus::Unknown;
    }
  } else {
    inst.status = PluginStatus::Online;
  }
}

static void scanRange(uint8_t muxAddr, uint8_t muxCh) {
  for (uint8_t addr = PLUGIN_ADDR_MIN; addr <= PLUGIN_ADDR_MAX; addr++) {
    if (muxAddr != PLUGIN_MUX_NONE && addr == muxAddr) continue;
    pump();
    PluginLocation loc;
    loc.busId = SHOWDUINO_PLUGIN_BUS_ID;
    loc.address = addr;
    loc.muxAddr = muxAddr;
    loc.muxChannel = muxCh;
    if (muxAddr != PLUGIN_MUX_NONE) {
      if (!pluginMuxSelect(muxAddr, muxCh)) continue;
    }
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (muxAddr != PLUGIN_MUX_NONE) {
      pluginMuxSelect(muxAddr, PLUGIN_MUX_CH_NONE);
    }
    if (err != 0) continue;
    PluginInstance *slot = addSlot(loc);
    if (!slot) {
      Serial.println("[PLUGIN] instance table full");
      break;
    }
    classify(*slot, true);
    char path[40];
    pluginBusFormatPath(loc, path, sizeof(path));
    if (slot->status == PluginStatus::Unknown) {
      Serial.printf("[PLUGIN] 0x%02X detected — UNKNOWN I2C DEVICE\n", addr);
    } else if (slot->status == PluginStatus::Ambiguous) {
      Serial.printf("[PLUGIN] 0x%02X detected — UNKNOWN/AMBIGUOUS\n", addr);
    } else {
      Serial.printf("[PLUGIN] 0x%02X detected\n", addr);
    }
    (void)path;
  }
}

static void scanConfiguredOffline() {
  for (uint8_t i = 0; i < pluginRegistryConfigCount(); i++) {
    const PluginConfigInstance *cfg = pluginRegistryConfigAt(i);
    if (!cfg) continue;
    if (findSlot(cfg->loc)) continue;
    PluginInstance *slot = addSlot(cfg->loc);
    if (!slot) break;
    classify(*slot, false);
  }
}

static uint8_t liveCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < sCount; i++) {
    if (sInst[i].status == PluginStatus::Online ||
        sInst[i].status == PluginStatus::Unknown ||
        sInst[i].status == PluginStatus::Ambiguous) {
      n++;
    }
  }
  return n;
}

void pluginBusScan() {
  Serial.println("[PLUGIN] Scanning...");
  for (uint8_t i = 0; i < sCount; i++) {
    if (sInst[i].status == PluginStatus::Online ||
        sInst[i].status == PluginStatus::Unknown ||
        sInst[i].status == PluginStatus::Ambiguous) {
      sInst[i].status = PluginStatus::Offline;
    }
  }
  scanRange(PLUGIN_MUX_NONE, PLUGIN_MUX_CH_NONE);

  for (uint8_t i = 0; i < sCount; i++) {
    if (!(sInst[i].capabilities & PLUGIN_CAP_MUX) &&
        strcmp(sInst[i].driver, "tca9548a") != 0) {
      continue;
    }
    if (sInst[i].status != PluginStatus::Online) continue;
    uint8_t mux = sInst[i].loc.address;
    for (uint8_t ch = 0; ch < 8; ch++) {
      scanRange(mux, ch);
    }
    pluginMuxSelect(mux, PLUGIN_MUX_CH_NONE);
  }

  scanConfiguredOffline();
  const uint8_t found = liveCount();
  if (found == 0) {
    Serial.println("[PLUGIN] Plug-in Bus ready — no devices detected");
  } else {
    Serial.printf("[PLUGIN] %u devices found\n", (unsigned)found);
  }
}

bool pluginBusBegin(void (*pumpFn)()) {
  sPump = pumpFn;
  sCount = 0;
  sReady = false;
  sHealthIndex = 0;
  sLastHealthMs = millis();

  Serial.println("[PLUGIN] Initialising Showduino Plug-in Bus");
  Serial.printf("[PLUGIN] SDA=%d SCL=%d frequency=%lu\n",
                SHOWDUINO_PLUGIN_BUS_SDA_PIN,
                SHOWDUINO_PLUGIN_BUS_SCL_PIN,
                (unsigned long)SHOWDUINO_PLUGIN_BUS_HZ);

  recoverBusIfNeeded();
  pluginRegistryLoadFromSd();

  if (!Wire.begin(SHOWDUINO_PLUGIN_BUS_SDA_PIN, SHOWDUINO_PLUGIN_BUS_SCL_PIN,
                  SHOWDUINO_PLUGIN_BUS_HZ)) {
    Serial.println("[PLUGIN] WARN: I2C begin failed — continuing without Plug-in Bus");
    return false;
  }
  Wire.setTimeOut((uint16_t)SHOWDUINO_PLUGIN_BUS_TIMEOUT_MS);
  sReady = true;
  pluginBusScan();
  return true;
}

void pluginBusService() {
  if (!sReady || sCount == 0) return;
  const uint32_t now = millis();
  if ((now - sLastHealthMs) < kHealthPeriodMs) return;
  if (sHealthIndex >= sCount) sHealthIndex = 0;
  PluginInstance &inst = sInst[sHealthIndex++];
  sLastHealthMs = now;
  if (inst.status == PluginStatus::Absent) return;
  const bool present = pluginBusPing(inst.loc);
  const PluginStatus before = inst.status;
  classify(inst, present);
  if (present && before == PluginStatus::Offline) {
    Serial.printf("[PLUGIN] %s ONLINE\n",
                  inst.friendly[0] ? inst.friendly : "device");
  }
}

void pluginBusOnEmergency() {
  if (!sReady) return;
  for (uint8_t i = 0; i < sCount; i++) {
    pump();
    if (sInst[i].status != PluginStatus::Online) continue;
    if (!(sInst[i].capabilities & (PLUGIN_CAP_DIGITAL_OUT | PLUGIN_CAP_PWM_OUT |
                                   PLUGIN_CAP_SERVO_OUT | PLUGIN_CAP_ANALOG_OUT))) {
      continue;
    }
    pluginDriverOnEmergency(sInst[i]);
  }
}

bool pluginBusReady() { return sReady; }

static void printOne(const PluginInstance &inst) {
  char path[40];
  pluginBusFormatPath(inst.loc, path, sizeof(path));
  const char *name = inst.friendly[0] ? inst.friendly : inst.deviceId;
  Serial.printf("  %s  %-22s  %s\n", path, name, pluginStatusName(inst.status));
}

void pluginBusPrintList() {
  for (uint8_t i = 0; i < sCount; i++) printOne(sInst[i]);
  Serial.printf("[PLUGIN] %u devices found\n", (unsigned)liveCount());
}

void pluginBusPrintStatus() {
  PluginBusSelfTest st;
  pluginBusCaptureSelfTest(&st);
  Serial.println("[PLUGIN] status");
  Serial.printf("  bus init=%s SDA=%s SCL=%s scan=%s\n",
                st.busInit ? "PASS" : "FAIL",
                st.sdaIdleHigh ? "PASS" : "WARN",
                st.sclIdleHigh ? "PASS" : "WARN",
                st.scanOk ? "PASS" : "FAIL");
  Serial.printf("  detected=%u known=%u unknown=%u offline_configured=%u defs=%s\n",
                (unsigned)st.devicesFound, (unsigned)st.known, (unsigned)st.unknown,
                (unsigned)st.offlineConfigured,
                st.definitionsOk ? "PASS" : "FAIL");
  for (uint8_t i = 0; i < sCount; i++) printOne(sInst[i]);
}

static bool keyEquals(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++;
    b++;
  }
  return *a == *b;
}

static bool keyMatches(const PluginInstance &inst, const char *key) {
  if (!key || !key[0]) return false;
  if (inst.instanceId[0] && keyEquals(inst.instanceId, key)) return true;
  if (inst.friendly[0] && keyEquals(inst.friendly, key)) return true;
  if (key[0] == '0' && (key[1] == 'x' || key[1] == 'X')) {
    unsigned long a = strtoul(key, nullptr, 16);
    return inst.loc.muxAddr == PLUGIN_MUX_NONE && inst.loc.address == (uint8_t)a;
  }
  if (isdigit((unsigned char)key[0])) {
    unsigned long a = strtoul(key, nullptr, 0);
    return inst.loc.muxAddr == PLUGIN_MUX_NONE && inst.loc.address == (uint8_t)a;
  }
  return false;
}

void pluginBusPrintInfo(const char *key) {
  if (!key || !key[0]) {
    Serial.println("[PLUGIN] usage: PLUGIN:INFO:<instance|address>");
    return;
  }
  bool any = false;
  for (uint8_t i = 0; i < sCount; i++) {
    if (!keyMatches(sInst[i], key)) continue;
    any = true;
    char path[40];
    pluginBusFormatPath(sInst[i].loc, path, sizeof(path));
    Serial.printf("[PLUGIN] %s\n", path);
    Serial.printf("  instance=%s device=%s driver=%s status=%s\n",
                  sInst[i].instanceId[0] ? sInst[i].instanceId : "-",
                  sInst[i].deviceId[0] ? sInst[i].deviceId : "-",
                  sInst[i].driver[0] ? sInst[i].driver : "-",
                  pluginStatusName(sInst[i].status));
    Serial.printf("  friendly=%s caps=0x%lx first=%lu last=%lu\n",
                  sInst[i].friendly[0] ? sInst[i].friendly : "-",
                  (unsigned long)sInst[i].capabilities,
                  (unsigned long)sInst[i].firstSeenMs,
                  (unsigned long)sInst[i].lastSeenMs);
    const PluginDriver *d = pluginDriverFind(sInst[i].driver);
    if (d && d->diagnostic) d->diagnostic(sInst[i]);
  }
  if (!any) Serial.println("[PLUGIN] no matching instance");
}

bool pluginBusCaptureSelfTest(PluginBusSelfTest *out) {
  if (!out) return false;
  *out = PluginBusSelfTest{};
  out->busInit = sReady;
  out->sdaIdleHigh = sSdaIdleHigh;
  out->sclIdleHigh = sSclIdleHigh;
  out->scanOk = sReady;
  out->definitionsOk = pluginRegistryDefinitionsOk();
  for (uint8_t i = 0; i < sCount; i++) {
    switch (sInst[i].status) {
      case PluginStatus::Online:
        out->devicesFound++;
        out->known++;
        break;
      case PluginStatus::Unknown:
      case PluginStatus::Ambiguous:
        out->devicesFound++;
        out->unknown++;
        break;
      case PluginStatus::Offline:
        if (sInst[i].configured) out->offlineConfigured++;
        break;
      default:
        break;
    }
  }
  if (!sReady) copyStr(out->detail, sizeof(out->detail), "bus not ready");
  else if (out->devicesFound == 0) copyStr(out->detail, sizeof(out->detail), "no devices");
  else copyStr(out->detail, sizeof(out->detail), "ok");
  return true;
}

const PluginInstance *pluginBusFindByInstanceId(const char *id) {
  if (!id) return nullptr;
  for (uint8_t i = 0; i < sCount; i++) {
    if (sInst[i].instanceId[0] && !strcmp(sInst[i].instanceId, id)) return &sInst[i];
  }
  return nullptr;
}

const PluginInstance *pluginBusFindByLocation(const PluginLocation &loc) {
  return findSlot(loc);
}

uint8_t pluginBusInstanceCount() { return sCount; }

const PluginInstance *pluginBusInstanceAt(uint8_t index) {
  return (index < sCount) ? &sInst[index] : nullptr;
}

#else

bool pluginBusBegin(void (*)()) { return false; }
void pluginBusService() {}
void pluginBusOnEmergency() {}
bool pluginBusReady() { return false; }
void pluginBusScan() {}
void pluginBusPrintList() { Serial.println("[PLUGIN] disabled"); }
void pluginBusPrintStatus() { Serial.println("[PLUGIN] disabled"); }
void pluginBusPrintInfo(const char *) { Serial.println("[PLUGIN] disabled"); }
bool pluginBusCaptureSelfTest(PluginBusSelfTest *out) {
  if (out) *out = PluginBusSelfTest{};
  return false;
}
const PluginInstance *pluginBusFindByInstanceId(const char *) { return nullptr; }
const PluginInstance *pluginBusFindByLocation(const PluginLocation &) { return nullptr; }
uint8_t pluginBusInstanceCount() { return 0; }
const PluginInstance *pluginBusInstanceAt(uint8_t) { return nullptr; }
void pluginBusFormatPath(const PluginLocation &, char *out, size_t) {
  if (out) out[0] = '\0';
}
bool pluginBusPing(const PluginLocation &) { return false; }

#endif
