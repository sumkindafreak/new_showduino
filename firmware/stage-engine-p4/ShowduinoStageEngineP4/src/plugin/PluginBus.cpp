#include "PluginBus.h"
#include "PluginRegistry.h"
#include "PluginDriver.h"
#include "../StageStorage.h"
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
static PluginRoleFile sRoles;
static PluginConfigLoadResult sRoleLoad = PluginConfigLoadResult::Missing;

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
    Serial.println("[I2C] WARN: SDA stuck LOW");
  }
  if (!sSclIdleHigh) {
    Serial.println("[I2C] WARN: SCL stuck LOW");
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

static PluginChip discoverChip(const PluginLocation &loc, bool *valid, char *deviceId, size_t deviceIdLen) {
  if (valid) *valid = false;
  if (loc.muxAddr != PLUGIN_MUX_NONE) {
    uint8_t matches = 0;
    const PluginDef *idHit = pluginRegistryMatchIdentify(loc, &matches);
    if (matches == 1 && idHit) {
      PluginChip chip = pluginChipFromDeviceId(idHit->id);
      if (deviceId && deviceIdLen) copyStr(deviceId, deviceIdLen, idHit->id);
      if (valid) *valid = (chip != PluginChip::Unknown);
      return chip;
    }
    return PluginChip::Unknown;
  }

  const PluginConfigInstance *reg = pluginRegistryConfigFor(loc);
  if (reg && reg->deviceId[0]) {
    PluginChip chip = pluginChipFromDeviceId(reg->deviceId);
    if (deviceId && deviceIdLen) copyStr(deviceId, deviceIdLen, reg->deviceId);
    if (chip != PluginChip::Unknown) {
      if (valid) *valid = true;
      return chip;
    }
  }

  uint8_t matches = 0;
  const PluginDef *idHit = pluginRegistryMatchIdentify(loc, &matches);
  if (matches > 1) {
    return PluginChip::Unknown;
  }
  if (idHit) {
    PluginChip chip = pluginChipFromDeviceId(idHit->id);
    if (deviceId && deviceIdLen) copyStr(deviceId, deviceIdLen, idHit->id);
    if (valid) *valid = (chip != PluginChip::Unknown);
    return chip;
  }
  return PluginChip::Unknown;
}

/* Apply identity + role. Do not start SX1509/MCP23017/PCA9685 engines here. */
static void applyDescriptor(PluginInstance &inst, const PluginDescriptor &d) {
  inst.chip = d.chip;
  inst.deviceClass = d.deviceClass;
  inst.role = d.role;
  inst.classification = d.classification;
  inst.configured = d.configured;
  copyStr(inst.friendly, sizeof(inst.friendly), d.displayName);
  copyStr(inst.deviceId, sizeof(inst.deviceId),
          d.chip == PluginChip::Unknown ? "generic.i2c.unknown" : pluginChipName(d.chip));

  inst.driver[0] = '\0';
  inst.capabilities = 0;
  inst.identityFromAddressOnly = false;
  if (d.chip == PluginChip::ES8311) {
    copyStr(inst.driver, sizeof(inst.driver), "waveshare.es8311");
  } else if (d.chip == PluginChip::TCA9548A && d.role == PluginRole::I2cMultiplexer) {
    copyStr(inst.driver, sizeof(inst.driver), "tca9548a");
    inst.capabilities = PLUGIN_CAP_MUX;
  } else if (d.configured && d.role != PluginRole::None) {
    switch (d.role) {
      case PluginRole::DigitalInputs:
        inst.capabilities = PLUGIN_CAP_DIGITAL_IN;
        break;
      case PluginRole::DigitalOutputs:
        inst.capabilities = PLUGIN_CAP_DIGITAL_OUT;
        break;
      case PluginRole::DigitalIo:
        inst.capabilities = PLUGIN_CAP_DIGITAL_IN | PLUGIN_CAP_DIGITAL_OUT;
        break;
      case PluginRole::PwmOutputs:
        inst.capabilities = PLUGIN_CAP_PWM_OUT;
        break;
      case PluginRole::ServoOutputs:
        inst.capabilities = PLUGIN_CAP_SERVO_OUT;
        break;
      default:
        inst.capabilities = 0;
        break;
    }
  }

  if (!d.online) {
    inst.status = PluginStatus::Offline;
    return;
  }
  inst.status = (d.chip == PluginChip::Unknown) ? PluginStatus::Unknown
                                                : PluginStatus::Online;
}

static void classify(PluginInstance &inst, bool present) {
  const uint32_t now = millis();
  const PluginStatus before = inst.status;
  char discoveredId[PLUGIN_ID_LEN] = {};
  bool discoveredValid = false;
  PluginChip discovered = discoverChip(inst.loc, &discoveredValid, discoveredId, sizeof(discoveredId));
  const bool rootBus = (inst.loc.muxAddr == PLUGIN_MUX_NONE);

  PluginDescriptor desc;
  pluginResolveOne(inst.loc.busId, inst.loc.address, present,
                   discovered, discoveredValid, &sRoles, &desc, rootBus);
  applyDescriptor(inst, desc);
  if (discoveredId[0] && inst.instanceId[0] == '\0') {
    copyStr(inst.instanceId, sizeof(inst.instanceId), discoveredId);
  }
  const PluginConfigInstance *reg = pluginRegistryConfigFor(inst.loc);
  if (reg && reg->instanceId[0]) {
    copyStr(inst.instanceId, sizeof(inst.instanceId), reg->instanceId);
  }

  if (present) {
    inst.lastSeenMs = now;
    if (inst.firstSeenMs == 0) inst.firstSeenMs = now;
  }

  if (!present && (before == PluginStatus::Online || before == PluginStatus::Unknown ||
                   before == PluginStatus::Ambiguous)) {
    Serial.printf("[I2C] %s OFFLINE\n",
                  inst.friendly[0] ? inst.friendly : "device");
  }
}

static bool probeAddress(uint8_t muxAddr, uint8_t muxCh, uint8_t addr) {
  if (muxAddr != PLUGIN_MUX_NONE) {
    if (addr == muxAddr) return false;
    if (!pluginMuxSelect(muxAddr, muxCh)) return false;
  }
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  if (muxAddr != PLUGIN_MUX_NONE) {
    pluginMuxSelect(muxAddr, PLUGIN_MUX_CH_NONE);
  }
  return err == 0;
}

static void scanMuxChannel(uint8_t muxAddr, uint8_t muxCh) {
  for (uint8_t addr = PLUGIN_ADDR_MIN; addr <= PLUGIN_ADDR_MAX; addr++) {
    pump();
    if (!probeAddress(muxAddr, muxCh, addr)) continue;
    PluginLocation loc;
    loc.busId = SHOWDUINO_PLUGIN_BUS_ID;
    loc.address = addr;
    loc.muxAddr = muxAddr;
    loc.muxChannel = muxCh;
    PluginInstance *slot = addSlot(loc);
    if (!slot) {
      Serial.println("[I2C] instance table full");
      break;
    }
    classify(*slot, true);
    Serial.printf("[I2C] mux%02X/ch%u/0x%02X detected\n", muxAddr, muxCh, addr);
  }
}

static void loadRoleFile() {
  sRoles = PluginRoleFile{};
  sRoleLoad = PluginConfigLoadResult::Missing;
#if !SHOWDUINO_SD_ENABLED
  Serial.println("[I2C] plugin-bus.json unavailable — firmware identity only");
  return;
#else
  if (!stageStorageIsReady()) {
    Serial.println("[I2C] plugin-bus.json unavailable — firmware identity only");
    return;
  }
  if (!stageStorageFs().exists(PATH_PLUGIN_BUS_CONFIG)) {
    Serial.println("[I2C] plugin-bus.json not found — plug-in roles stay unassigned");
    return;
  }

  File f = stageStorageFs().open(PATH_PLUGIN_BUS_CONFIG, FILE_READ);
  if (!f) {
    Serial.println("[I2C] plugin-bus.json rejected: cannot open");
    sRoleLoad = PluginConfigLoadResult::InvalidJson;
    return;
  }
  if (f.size() > PLUGIN_ROLE_FILE_MAX_BYTES) {
    Serial.printf("[I2C] plugin-bus.json rejected: %s\n",
                  pluginConfigLoadResultName(PluginConfigLoadResult::FileTooLarge));
    sRoleLoad = PluginConfigLoadResult::FileTooLarge;
    f.close();
    return;
  }
  char buf[PLUGIN_ROLE_FILE_MAX_BYTES + 1];
  int n = f.read((uint8_t *)buf, PLUGIN_ROLE_FILE_MAX_BYTES);
  f.close();
  if (n < 0) {
    Serial.println("[I2C] plugin-bus.json rejected: read failed");
    sRoleLoad = PluginConfigLoadResult::InvalidJson;
    return;
  }
  buf[n] = '\0';

  PluginConfigLoadResult result = PluginConfigLoadResult::InvalidJson;
  if (!pluginParseRoleFile(buf, (size_t)n, &sRoles, &result)) {
    sRoles = PluginRoleFile{};
    sRoleLoad = result;
    Serial.printf("[I2C] plugin-bus.json rejected: %s\n",
                  pluginConfigLoadResultName(result));
    return;
  }
  sRoleLoad = PluginConfigLoadResult::Ok;
  Serial.printf("[I2C] plugin-bus.json loaded — %u role(s)\n",
                (unsigned)sRoles.deviceCount);
#endif
}

static void installDescriptors(const PluginDescriptor *desc, uint8_t n) {
  PluginInstance previous[PLUGIN_MAX_INSTANCES];
  uint8_t prevCount = sCount;
  for (uint8_t i = 0; i < prevCount; i++) previous[i] = sInst[i];

  sCount = 0;
  for (uint8_t i = 0; i < n && sCount < PLUGIN_MAX_INSTANCES; i++) {
    PluginLocation loc;
    loc.busId = desc[i].busId;
    loc.address = desc[i].address;
    loc.muxAddr = PLUGIN_MUX_NONE;
    loc.muxChannel = PLUGIN_MUX_CH_NONE;
    PluginInstance *slot = addSlot(loc);
    if (!slot) break;
    applyDescriptor(*slot, desc[i]);
    const PluginConfigInstance *reg = pluginRegistryConfigFor(loc);
    if (reg && reg->instanceId[0]) {
      copyStr(slot->instanceId, sizeof(slot->instanceId), reg->instanceId);
    }
    for (uint8_t p = 0; p < prevCount; p++) {
      if (pluginLocationEqual(previous[p].loc, loc)) {
        slot->firstSeenMs = previous[p].firstSeenMs;
        slot->lastSeenMs = previous[p].lastSeenMs;
        if (slot->instanceId[0] == '\0') {
          copyStr(slot->instanceId, sizeof(slot->instanceId), previous[p].instanceId);
        }
        break;
      }
    }
    if (desc[i].online) {
      const uint32_t now = millis();
      slot->lastSeenMs = now;
      if (slot->firstSeenMs == 0) slot->firstSeenMs = now;
    }
  }
}

void pluginBusScan() {
  Serial.println("[I2C] Scanning Showduino hardware bus...");
  PluginPresence found[PLUGIN_MAX_INSTANCES];
  uint8_t foundCount = 0;

  for (uint8_t addr = PLUGIN_ADDR_MIN; addr <= PLUGIN_ADDR_MAX; addr++) {
    pump();
    if (!probeAddress(PLUGIN_MUX_NONE, PLUGIN_MUX_CH_NONE, addr)) continue;
    Serial.printf("[I2C] 0x%02X detected\n", addr);
    if (foundCount >= PLUGIN_MAX_INSTANCES) {
      Serial.println("[I2C] instance table full");
      break;
    }
    PluginLocation loc;
    loc.busId = SHOWDUINO_PLUGIN_BUS_ID;
    loc.address = addr;
    loc.muxAddr = PLUGIN_MUX_NONE;
    loc.muxChannel = PLUGIN_MUX_CH_NONE;
    found[foundCount].busId = loc.busId;
    found[foundCount].address = addr;
    found[foundCount].present = true;
    found[foundCount].discoveredChip = discoverChip(loc, &found[foundCount].discoveredChipValid,
                                                    nullptr, 0);
    foundCount++;
  }

  PluginDescriptor desc[PLUGIN_MAX_INSTANCES];
  uint8_t n = 0;
  pluginBuildDeviceTable(found, foundCount, &sRoles, desc, PLUGIN_MAX_INSTANCES, &n);
  installDescriptors(desc, n);

  for (uint8_t i = 0; i < sCount; i++) {
    if (sInst[i].chip != PluginChip::TCA9548A ||
        sInst[i].role != PluginRole::I2cMultiplexer ||
        sInst[i].status != PluginStatus::Online) {
      continue;
    }
    uint8_t mux = sInst[i].loc.address;
    for (uint8_t ch = 0; ch < 8; ch++) {
      scanMuxChannel(mux, ch);
    }
    pluginMuxSelect(mux, PLUGIN_MUX_CH_NONE);
  }

  pluginBusPrintList();
}

bool pluginBusBegin(void (*pumpFn)()) {
  sPump = pumpFn;
  sCount = 0;
  sReady = false;
  sHealthIndex = 0;
  sLastHealthMs = millis();
  sRoles = PluginRoleFile{};
  sRoleLoad = PluginConfigLoadResult::Missing;

  Serial.println("[I2C] Initialising Showduino Plug-in Bus");
  Serial.printf("[I2C] SDA=%d SCL=%d frequency=%lu\n",
                SHOWDUINO_PLUGIN_BUS_SDA_PIN,
                SHOWDUINO_PLUGIN_BUS_SCL_PIN,
                (unsigned long)SHOWDUINO_PLUGIN_BUS_HZ);

  recoverBusIfNeeded();
  pluginRegistryLoadFromSd();
  loadRoleFile();

  if (!Wire.begin(SHOWDUINO_PLUGIN_BUS_SDA_PIN, SHOWDUINO_PLUGIN_BUS_SCL_PIN,
                  SHOWDUINO_PLUGIN_BUS_HZ)) {
    Serial.println("[I2C] WARN: I2C begin failed — continuing without Plug-in Bus");
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
    Serial.printf("[I2C] %s ONLINE\n",
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
  const char *name = inst.friendly[0] ? inst.friendly : "Unknown I2C Device";
  Serial.printf("  %-16s  %-28s  %-8s  %s\n",
                path, name,
                pluginClassificationName(inst.classification),
                pluginStatusName(inst.status));
}

static void printCounts(const PluginBusSelfTest &st) {
  Serial.printf("[I2C] Internal devices: %u\n", (unsigned)st.internal);
  Serial.printf("[I2C] Configured plug-ins: %u\n", (unsigned)st.configuredPlugins);
  Serial.printf("[I2C] Unconfigured plug-ins: %u\n", (unsigned)st.unconfiguredPlugins);
  Serial.printf("[I2C] Unknown devices: %u\n", (unsigned)st.unknown);
  Serial.printf("[I2C] Total devices: %u\n", (unsigned)sCount);
}

void pluginBusPrintList() {
  for (uint8_t i = 0; i < sCount; i++) printOne(sInst[i]);
  PluginBusSelfTest st;
  pluginBusCaptureSelfTest(&st);
  printCounts(st);
}

void pluginBusPrintStatus() {
  PluginBusSelfTest st;
  pluginBusCaptureSelfTest(&st);
  Serial.println("[I2C] status");
  Serial.printf("  bus init=%s SDA=%s SCL=%s scan=%s roles=%s\n",
                st.busInit ? "PASS" : "FAIL",
                st.sdaIdleHigh ? "PASS" : "WARN",
                st.sclIdleHigh ? "PASS" : "WARN",
                st.scanOk ? "PASS" : "FAIL",
                st.roleFileOk ? "PASS" : "FAIL");
  Serial.printf("  detected=%u known=%u unknown=%u offline_configured=%u defs=%s\n",
                (unsigned)st.devicesFound, (unsigned)st.known, (unsigned)st.unknown,
                (unsigned)st.offlineConfigured,
                st.definitionsOk ? "PASS" : "FAIL");
  for (uint8_t i = 0; i < sCount; i++) printOne(sInst[i]);
  printCounts(st);
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
    Serial.println("[I2C] usage: PLUGIN:INFO:<instance|address>");
    return;
  }
  bool any = false;
  for (uint8_t i = 0; i < sCount; i++) {
    if (!keyMatches(sInst[i], key)) continue;
    any = true;
    char path[40];
    pluginBusFormatPath(sInst[i].loc, path, sizeof(path));
    Serial.printf("[I2C] %s\n", path);
    Serial.printf("  name=%s chip=%s class=%s role=%s\n",
                  sInst[i].friendly[0] ? sInst[i].friendly : "Unknown I2C Device",
                  pluginChipName(sInst[i].chip),
                  pluginClassName(sInst[i].deviceClass),
                  pluginRoleName(sInst[i].role));
    Serial.printf("  classification=%s configured=%s online=%s status=%s\n",
                  pluginClassificationName(sInst[i].classification),
                  sInst[i].configured ? "yes" : "no",
                  (sInst[i].status == PluginStatus::Online ||
                   sInst[i].status == PluginStatus::Unknown ||
                   sInst[i].status == PluginStatus::Ambiguous) ? "yes" : "no",
                  pluginStatusName(sInst[i].status));
    Serial.printf("  instance=%s driver=%s caps=0x%lx first=%lu last=%lu\n",
                  sInst[i].instanceId[0] ? sInst[i].instanceId : "-",
                  sInst[i].driver[0] ? sInst[i].driver : "-",
                  (unsigned long)sInst[i].capabilities,
                  (unsigned long)sInst[i].firstSeenMs,
                  (unsigned long)sInst[i].lastSeenMs);
    const PluginDriver *d = pluginDriverFind(sInst[i].driver);
    if (d && d->diagnostic) d->diagnostic(sInst[i]);
  }
  if (!any) Serial.println("[I2C] no matching instance");
}

bool pluginBusCaptureSelfTest(PluginBusSelfTest *out) {
  if (!out) return false;
  *out = PluginBusSelfTest{};
  out->busInit = sReady;
  out->sdaIdleHigh = sSdaIdleHigh;
  out->sclIdleHigh = sSclIdleHigh;
  out->scanOk = sReady;
  out->definitionsOk = pluginRegistryDefinitionsOk();
  out->roleFileOk = (sRoleLoad == PluginConfigLoadResult::Ok ||
                     sRoleLoad == PluginConfigLoadResult::Missing);
  for (uint8_t i = 0; i < sCount; i++) {
    const PluginInstance &inst = sInst[i];
    const bool live = inst.status == PluginStatus::Online ||
                      inst.status == PluginStatus::Unknown ||
                      inst.status == PluginStatus::Ambiguous;
    if (live) out->devicesFound++;
    if (inst.classification == PluginClassification::Internal) out->internal++;
    if (inst.chip == PluginChip::Unknown) {
      if (live) out->unknown++;
    } else if (inst.status == PluginStatus::Online) {
      out->known++;
    }
    if (inst.classification == PluginClassification::Plugin) {
      if (inst.configured) out->configuredPlugins++;
      else out->unconfiguredPlugins++;
    }
    if (inst.status == PluginStatus::Offline && inst.configured) {
      out->offlineConfigured++;
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

PluginConfigLoadResult pluginBusRoleFileResult() { return sRoleLoad; }

const PluginRoleFile *pluginBusRoleFile() { return &sRoles; }

#else

bool pluginBusBegin(void (*)()) { return false; }
void pluginBusService() {}
void pluginBusOnEmergency() {}
bool pluginBusReady() { return false; }
void pluginBusScan() {}
void pluginBusPrintList() { Serial.println("[I2C] disabled"); }
void pluginBusPrintStatus() { Serial.println("[I2C] disabled"); }
void pluginBusPrintInfo(const char *) { Serial.println("[I2C] disabled"); }
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
PluginConfigLoadResult pluginBusRoleFileResult() { return PluginConfigLoadResult::Missing; }
const PluginRoleFile *pluginBusRoleFile() { return nullptr; }

#endif
