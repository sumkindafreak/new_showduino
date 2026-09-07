#include "ShowNetwork.h"

#include <ETH.h>
#include <Network.h>
#include <string.h>
#include <stdlib.h>
#include "../../BoardConfig.h"
#include "../../../protocol/showduino_e131.h"
#include "../StageStorage.h"
#include "../storage/StageStore.h"
#include "../storage/StageConfig.h"
#include "ShowHttp.h"
#include "../e131/E131Receiver.h"

#if defined(ETH_RMII_TX_EN)
#if ETH_RMII_TX_EN != SHOWDUINO_ETH_TX_EN_PIN || \
    ETH_RMII_TX0 != SHOWDUINO_ETH_TXD0_PIN || \
    ETH_RMII_TX1 != SHOWDUINO_ETH_TXD1_PIN || \
    ETH_RMII_RX0 != SHOWDUINO_ETH_RXD0_PIN || \
    ETH_RMII_RX1_EN != SHOWDUINO_ETH_RXD1_PIN || \
    ETH_RMII_CRS_DV != SHOWDUINO_ETH_CRS_DV_PIN || \
    ETH_RMII_CLK != SHOWDUINO_ETH_REFCLK_PIN
#error "Arduino ETH RMII pins do not match the Waveshare IP101GRI Showduino map"
#endif
#endif
#if defined(ETH_PHY_MDC)
#if ETH_PHY_MDC != SHOWDUINO_ETH_MDC_PIN || \
    ETH_PHY_MDIO != SHOWDUINO_ETH_MDIO_PIN || \
    ETH_PHY_POWER != SHOWDUINO_ETH_POWER_PIN || \
    ETH_PHY_ADDR != SHOWDUINO_ETH_PHY_ADDR
#error "Arduino ETH PHY pins/address do not match the Waveshare IP101GRI Showduino map"
#endif
#endif

static ShowNetConfig sSaved;
static ShowNetLive sLive;
static bool sApplyPending = false;
static uint32_t sApplyAtMs = 0;
static uint32_t sNextBeginMs = 0;
static bool sLoggedLinkUp = false;
static bool sLoggedIp = false;
static bool sLoggedBeginFail = false;
static volatile uint32_t sEventBits = 0;

enum {
  EV_START = 1u,
  EV_CONNECTED = 2u,
  EV_GOT_IP = 4u,
  EV_LOST_IP = 8u,
  EV_DISCONNECTED = 16u,
  EV_STOP = 32u
};

const char *showNetModeName(ShowNetMode mode) {
  return mode == ShowNetMode::Static ? "STATIC" : "DHCP";
}

ShowNetMode showNetModeFromName(const char *name) {
  if (name && (strcmp(name, "STATIC") == 0 || strcmp(name, "static") == 0)) {
    return ShowNetMode::Static;
  }
  return ShowNetMode::Dhcp;
}

static bool parseIpv4(const char *text, uint8_t out[4]) {
  if (!text) return false;
  unsigned a = 0, b = 0, c = 0, d = 0;
  char extra = 0;
  if (sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) return false;
  if (a > 255 || b > 255 || c > 255 || d > 255) return false;
  if (out) {
    out[0] = (uint8_t)a;
    out[1] = (uint8_t)b;
    out[2] = (uint8_t)c;
    out[3] = (uint8_t)d;
  }
  return true;
}

static bool ipv4ToIp(const char *text, IPAddress &ip) {
  uint8_t o[4];
  if (!parseIpv4(text, o)) return false;
  ip = IPAddress(o[0], o[1], o[2], o[3]);
  return true;
}

static void formatIp(const IPAddress &ip, char *out, size_t len) {
  if (!out || len < 8) return;
  snprintf(out, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

static void copyField(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = '\0';
}

bool showNetworkValidateConfig(const ShowNetConfig &in, char *err, size_t errLen) {
  if (err && errLen) err[0] = '\0';
  if (in.formatVersion != 1) {
    if (err && errLen) strncpy(err, "unsupported formatVersion", errLen - 1);
    return false;
  }
  if (!showduino_e131_universe_valid(in.e131Universe)) {
    if (err && errLen) strncpy(err, "invalid E1.31 universe", errLen - 1);
    return false;
  }
  if (in.mode == ShowNetMode::Static) {
    uint8_t ip[4], mask[4], gw[4];
    if (!parseIpv4(in.ip, ip) || !parseIpv4(in.subnet, mask) || !parseIpv4(in.gateway, gw)) {
      if (err && errLen) strncpy(err, "static IP/subnet/gateway invalid", errLen - 1);
      return false;
    }
    if (ip[0] == 0 || (ip[0] == 255 && ip[1] == 255 && ip[2] == 255 && ip[3] == 255)) {
      if (err && errLen) strncpy(err, "static IP reserved", errLen - 1);
      return false;
    }
    if (in.dns[0] && !parseIpv4(in.dns, nullptr)) {
      if (err && errLen) strncpy(err, "static DNS invalid", errLen - 1);
      return false;
    }
  } else if (in.mode != ShowNetMode::Dhcp) {
    if (err && errLen) strncpy(err, "mode must be DHCP or STATIC", errLen - 1);
    return false;
  }
  return true;
}

static bool jsonExtractObject(const String &json, const char *key, String &out) {
  String needle = String("\"") + key + "\"";
  int k = json.indexOf(needle);
  if (k < 0) return false;
  int brace = json.indexOf('{', k);
  if (brace < 0) return false;
  int depth = 0;
  for (int i = brace; i < (int)json.length(); i++) {
    char c = json.charAt(i);
    if (c == '{') depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0) {
        out = json.substring(brace, i + 1);
        return true;
      }
    }
  }
  return false;
}

static bool jsonStr(const String &json, const char *key, char *out, size_t n) {
  String needle = String("\"") + key + "\"";
  int k = json.indexOf(needle);
  if (k < 0) return false;
  int colon = json.indexOf(':', k + needle.length());
  if (colon < 0) return false;
  int q1 = json.indexOf('"', colon + 1);
  if (q1 < 0) return false;
  int q2 = json.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  String v = json.substring(q1 + 1, q2);
  copyField(out, n, v.c_str());
  return true;
}

static bool jsonBool(const String &json, const char *key, bool *out) {
  String needle = String("\"") + key + "\"";
  int k = json.indexOf(needle);
  if (k < 0) return false;
  int colon = json.indexOf(':', k + needle.length());
  if (colon < 0) return false;
  String rest = json.substring(colon + 1);
  rest.trim();
  if (rest.startsWith("true")) {
    *out = true;
    return true;
  }
  if (rest.startsWith("false")) {
    *out = false;
    return true;
  }
  return false;
}

static bool jsonU16(const String &json, const char *key, uint16_t *out) {
  String needle = String("\"") + key + "\"";
  int k = json.indexOf(needle);
  if (k < 0) return false;
  int colon = json.indexOf(':', k + needle.length());
  if (colon < 0) return false;
  long v = strtol(json.c_str() + colon + 1, nullptr, 10);
  if (v < 0 || v > 65535) return false;
  *out = (uint16_t)v;
  return true;
}

static String configToJson(const ShowNetConfig &c) {
  String j;
  j.reserve(360);
  j += "{\n  \"formatVersion\": 1,\n  \"ethernet\": {\n";
  j += "    \"enabled\": ";
  j += c.enabled ? "true" : "false";
  j += ",\n    \"mode\": \"";
  j += showNetModeName(c.mode);
  j += "\",\n    \"ip\": \"";
  j += c.ip;
  j += "\",\n    \"subnet\": \"";
  j += c.subnet;
  j += "\",\n    \"gateway\": \"";
  j += c.gateway;
  j += "\",\n    \"dns\": \"";
  j += c.dns;
  j += "\"\n  }\n}\n";
  return j;
}

static bool parseConfigJson(const String &json, ShowNetConfig &out, char *err, size_t errLen) {
  ShowNetConfig tmp = sSaved;
  tmp.formatVersion = 1;
  uint16_t ver = 1;
  if (jsonU16(json, "formatVersion", &ver)) tmp.formatVersion = (uint8_t)ver;
  String eth, e131;
  if (!jsonExtractObject(json, "ethernet", eth)) {
    if (err && errLen) strncpy(err, "missing ethernet object", errLen - 1);
    return false;
  }
  char mode[12] = "";
  if (!jsonBool(eth, "enabled", &tmp.enabled) || !jsonStr(eth, "mode", mode, sizeof(mode))) {
    if (err && errLen) strncpy(err, "ethernet.enabled/mode required", errLen - 1);
    return false;
  }
  tmp.mode = showNetModeFromName(mode);
  if (strcmp(mode, "DHCP") != 0 && strcmp(mode, "STATIC") != 0 &&
      strcmp(mode, "dhcp") != 0 && strcmp(mode, "static") != 0) {
    if (err && errLen) strncpy(err, "mode must be DHCP or STATIC", errLen - 1);
    return false;
  }
  jsonStr(eth, "ip", tmp.ip, sizeof(tmp.ip));
  jsonStr(eth, "subnet", tmp.subnet, sizeof(tmp.subnet));
  jsonStr(eth, "gateway", tmp.gateway, sizeof(tmp.gateway));
  jsonStr(eth, "dns", tmp.dns, sizeof(tmp.dns));
  if (jsonExtractObject(json, "e131", e131)) {
    jsonBool(e131, "enabled", &tmp.e131Enabled);
    jsonU16(e131, "universe", &tmp.e131Universe);
  }
  if (!showNetworkValidateConfig(tmp, err, errLen)) return false;
  out = tmp;
  return true;
}

static bool writeConfigFile(const ShowNetConfig &c, char *err, size_t errLen) {
  if (!stageStoreWritable()) {
    if (err && errLen) strncpy(err, "SD not writable — RAM config only", errLen - 1);
    return false;
  }
  const String json = configToJson(c);
  if (!stageStoreAtomicWrite(PATH_NETWORK_CONFIG, json.c_str(), json.length())) {
    if (err && errLen) strncpy(err, "network.json atomic write failed", errLen - 1);
    return false;
  }
  stageConfigTakeE131From(c.e131Enabled, c.e131Universe);
  return true;
}

bool showNetworkSaveConfig(const ShowNetConfig &in, char *err, size_t errLen) {
  if (!showNetworkValidateConfig(in, err, errLen)) return false;
  sSaved = in;
  char sdErr[48] = "";
  if (!writeConfigFile(sSaved, sdErr, sizeof(sdErr))) {
    Serial.printf("[NET] Config accepted in RAM (%s)\n", sdErr);
    if (err && errLen && !err[0]) copyField(err, errLen, sdErr);
  } else {
    Serial.println("[NET] Saved /showduino/config/network.json");
  }
  showNetworkRequestApply();
  return true;
}

static void loadConfig() {
  sSaved = ShowNetConfig();
  stageConfigApplyE131To(&sSaved.e131Enabled, &sSaved.e131Universe);
  if (!stageStorageIsReady()) {
    Serial.println("[NET] No SD — using default DHCP config");
    return;
  }
  fs::FS &fs = stageStorageFs();
  if (!fs.exists(PATH_NETWORK_CONFIG)) {
    char err[48];
    if (writeConfigFile(sSaved, err, sizeof(err))) {
      Serial.println("[NET] Wrote default /showduino/config/network.json");
    }
    return;
  }
  File f = fs.open(PATH_NETWORK_CONFIG, FILE_READ);
  if (!f) {
    Serial.println("[NET] network.json unreadable — defaults");
    return;
  }
  String json = f.readString();
  f.close();
  ShowNetConfig parsed;
  char err[48] = "";
  if (!parseConfigJson(json, parsed, err, sizeof(err))) {
    Serial.printf("[NET] network.json rejected (%s) — defaults, boot continues\n",
                  err[0] ? err : "invalid");
    return;
  }
  sSaved = parsed;
  stageConfigApplyE131To(&sSaved.e131Enabled, &sSaved.e131Universe);
  Serial.printf("[NET] Loaded config mode=%s enabled=%s universe=%u\n",
                showNetModeName(sSaved.mode),
                sSaved.enabled ? "YES" : "NO",
                (unsigned)sSaved.e131Universe);
}

static void refreshLiveAddress() {
  formatIp(ETH.localIP(), sLive.ip, sizeof(sLive.ip));
  formatIp(ETH.subnetMask(), sLive.subnet, sizeof(sLive.subnet));
  formatIp(ETH.gatewayIP(), sLive.gateway, sizeof(sLive.gateway));
  formatIp(ETH.dnsIP(), sLive.dns, sizeof(sLive.dns));
  sLive.hasIp = ETH.hasIP() && ETH.localIP() != IPAddress((uint32_t)0);
  if (sLive.hasIp) {
    snprintf(sLive.webUrl, sizeof(sLive.webUrl), "http://%s/", sLive.ip);
  } else {
    sLive.webUrl[0] = '\0';
  }
}

static void refreshLiveLink() {
  if (!sLive.hardwareInit) {
    sLive.linkUp = false;
    sLive.speedMbps = 0;
    return;
  }
  sLive.linkUp = ETH.linkUp();
  sLive.speedMbps = sLive.linkUp ? ETH.linkSpeed() : 0;
  String mac = ETH.macAddress();
  copyField(sLive.mac, sizeof(sLive.mac), mac.c_str());
}

static void stopEthernet() {
  e131ReceiverStop();
  showHttpOnLinkLost();
  if (sLive.hardwareInit) {
    ETH.end();
  }
  sLive.hardwareInit = false;
  sLive.linkUp = false;
  sLive.hasIp = false;
  sLive.httpListening = false;
  sLive.ip[0] = '\0';
  sLive.webUrl[0] = '\0';
  sLive.speedMbps = 0;
  sLoggedLinkUp = false;
  sLoggedIp = false;
}

static bool startEthernet() {
  sLive.enabled = sSaved.enabled;
  sLive.liveMode = sSaved.mode;
  if (!sSaved.enabled) {
    copyField(sLive.lastError, sizeof(sLive.lastError), "Ethernet disabled");
    stopEthernet();
    Serial.println("[NET] Ethernet disabled");
    return true;
  }

  sLive.beginHeap = ESP.getFreeHeap();
  ETH.setHostname("showduino-p4");

  if (sSaved.mode == ShowNetMode::Static) {
    IPAddress ip, gw, mask, dns;
    if (!ipv4ToIp(sSaved.ip, ip) || !ipv4ToIp(sSaved.subnet, mask) ||
        !ipv4ToIp(sSaved.gateway, gw)) {
      copyField(sLive.lastError, sizeof(sLive.lastError), "static config invalid");
      Serial.println("[NET] Static config invalid — Ethernet not started");
      return false;
    }
    if (sSaved.dns[0]) ipv4ToIp(sSaved.dns, dns);
    else dns = gw;
    ETH.config(ip, gw, mask, dns);
  } else {
    ETH.config(IPAddress((uint32_t)0), IPAddress((uint32_t)0), IPAddress((uint32_t)0));
  }

  const bool ok = ETH.begin(ETH_PHY_IP101, SHOWDUINO_ETH_PHY_ADDR,
                            SHOWDUINO_ETH_MDC_PIN, SHOWDUINO_ETH_MDIO_PIN,
                            SHOWDUINO_ETH_POWER_PIN, EMAC_CLK_EXT_IN);
  sLive.hardwareInit = ok;
  if (!ok) {
    copyField(sLive.lastError, sizeof(sLive.lastError), "ETH.begin failed");
    sNextBeginMs = millis() + 15000UL;
    if (!sLoggedBeginFail) {
      Serial.println("[NET] ETH.begin failed — show continues offline");
      sLoggedBeginFail = true;
    }
    return false;
  }
  sLoggedBeginFail = false;
  copyField(sLive.lastError, sizeof(sLive.lastError), "");
  refreshLiveLink();
  Serial.println("[NET] Ethernet hardware init PHY=IP101GRI RMII EXT_CLK");
  Serial.printf("[NET] PHY addr=%d MDC=%d MDIO=%d RST=%d CLK=%d\n",
                SHOWDUINO_ETH_PHY_ADDR, SHOWDUINO_ETH_MDC_PIN,
                SHOWDUINO_ETH_MDIO_PIN, SHOWDUINO_ETH_POWER_PIN,
                SHOWDUINO_ETH_REFCLK_PIN);
  Serial.printf("[NET] RMII TXEN=%d TXD=%d/%d RXD=%d/%d CRS=%d\n",
                SHOWDUINO_ETH_TX_EN_PIN, SHOWDUINO_ETH_TXD0_PIN,
                SHOWDUINO_ETH_TXD1_PIN, SHOWDUINO_ETH_RXD0_PIN,
                SHOWDUINO_ETH_RXD1_PIN, SHOWDUINO_ETH_CRS_DV_PIN);
  return true;
}

static void onNetEvent(arduino_event_id_t event, arduino_event_info_t info) {
  (void)info;
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      sEventBits |= EV_START;
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      sEventBits |= EV_CONNECTED;
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      sEventBits |= EV_GOT_IP;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      sEventBits |= EV_LOST_IP;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      sEventBits |= EV_DISCONNECTED;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      sEventBits |= EV_STOP;
      break;
    default:
      break;
  }
}

static void processEvents() {
  const uint32_t bits = sEventBits;
  if (!bits) return;
  sEventBits = 0;
  sLive.lastEventMs = millis();
  refreshLiveLink();
  refreshLiveAddress();

  if (bits & EV_START) {
    refreshLiveLink();
  }
  if (bits & EV_CONNECTED) {
    sLive.linkUp = true;
    sLive.linkUpMs = millis();
    if (!sLoggedLinkUp) {
      Serial.println("[NET] Ethernet link UP");
      sLoggedLinkUp = true;
    }
  }
  if (bits & EV_GOT_IP) {
    refreshLiveAddress();
    sLive.ipUpMs = millis();
    if (!sLoggedIp && sLive.hasIp) {
      if (sLive.liveMode == ShowNetMode::Dhcp) {
        Serial.println("[NET] DHCP address acquired");
      } else {
        Serial.println("[NET] Static address applied");
      }
      Serial.printf("[NET] IP: %s\n", sLive.ip);
      Serial.printf("[NET] Mask: %s\n", sLive.subnet);
      Serial.printf("[NET] Gateway: %s\n", sLive.gateway);
      if (sLive.webUrl[0]) Serial.printf("[WEB] %s\n", sLive.webUrl);
      sLoggedIp = true;
    }
    showHttpOnAddress(sLive.ip);
    e131ReceiverOnNetworkChange();
  }
  if (bits & (EV_LOST_IP | EV_DISCONNECTED | EV_STOP)) {
    const bool hadIp = sLive.hasIp || sLoggedIp;
    sLive.hasIp = false;
    sLive.httpListening = false;
    sLive.webUrl[0] = '\0';
    if (bits & (EV_DISCONNECTED | EV_STOP)) {
      sLive.linkUp = false;
      sLive.speedMbps = 0;
      if (sLoggedLinkUp) Serial.println("[NET] Ethernet link DOWN — show continues");
      sLoggedLinkUp = false;
    }
    if (hadIp) Serial.println("[NET] Address lost — show continues");
    sLoggedIp = false;
    showHttpOnLinkLost();
    e131ReceiverOnNetworkChange();
  }
}

void showNetworkRequestApply() {
  sApplyPending = true;
  sApplyAtMs = millis() + 250;
}

void showNetworkBegin() {
  memset(&sLive, 0, sizeof(sLive));
  loadConfig();
  sLive.enabled = sSaved.enabled;
  sLive.liveMode = sSaved.mode;
  Network.onEvent(onNetEvent);
  showHttpBegin();
  e131ReceiverBegin();
  if (sSaved.enabled) {
    startEthernet();
  } else {
    Serial.println("[NET] Ethernet disabled by config — show engine running");
  }
}

void showNetworkLoop() {
  processEvents();
  refreshLiveLink();
  if (sLive.hardwareInit) refreshLiveAddress();
  sLive.httpListening = showHttpListening();

  if (sApplyPending && (int32_t)(millis() - sApplyAtMs) >= 0) {
    sApplyPending = false;
    Serial.println("[NET] Applying network configuration");
    stopEthernet();
    if (sSaved.enabled) startEthernet();
    e131ReceiverOnNetworkChange();
  }

  if (sSaved.enabled && !sLive.hardwareInit &&
      (int32_t)(millis() - sNextBeginMs) >= 0) {
    sNextBeginMs = millis() + 15000UL;
    startEthernet();
  }

  showHttpLoop();
  e131ReceiverLoop();
}

const ShowNetConfig &showNetworkSavedConfig() { return sSaved; }
const ShowNetLive &showNetworkLive() { return sLive; }

bool showNetworkHasAddress() {
  return sLive.hardwareInit && sLive.hasIp;
}

bool showNetworkFullDuplex() {
  return sLive.hardwareInit && sLive.linkUp && ETH.fullDuplex();
}

void showNetworkPrintStatus() {
  const uint32_t now = millis();
  Serial.println("[NET]");
  Serial.printf("Ethernet: %s\n",
                !sSaved.enabled ? "DISABLED" :
                (sLive.hasIp ? "ONLINE" : (sLive.linkUp ? "LINK" : "OFFLINE")));
  Serial.printf("Hardware: %s\n", sLive.hardwareInit ? "INIT" : "OFF");
  Serial.printf("MAC: %s\n", sLive.mac[0] ? sLive.mac : "--");
  Serial.printf("Mode: %s\n", showNetModeName(sLive.liveMode));
  Serial.printf("IP: %s\n", sLive.ip[0] ? sLive.ip : "--");
  Serial.printf("Mask: %s\n", sLive.subnet[0] ? sLive.subnet : "--");
  Serial.printf("Gateway: %s\n", sLive.gateway[0] ? sLive.gateway : "--");
  Serial.printf("Speed: %u Mbps%s\n",
                (unsigned)sLive.speedMbps,
                (sLive.hardwareInit && sLive.linkUp) ? (ETH.fullDuplex() ? " full-duplex" : " half-duplex") : "");
  Serial.printf("Heap: free=%lu min=%lu begin=%lu\n",
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMinFreeHeap(),
                (unsigned long)sLive.beginHeap);
  Serial.printf("Saved: enabled=%s mode=%s\n",
                sSaved.enabled ? "YES" : "NO", showNetModeName(sSaved.mode));
  if (sLive.webUrl[0]) Serial.printf("[WEB] %s\n", sLive.webUrl);
  if (sLive.linkUp && sLive.linkUpMs) {
    Serial.printf("Link age: %lu ms\n", (unsigned long)(now - sLive.linkUpMs));
  }
  if (sLive.lastError[0]) Serial.printf("Note: %s\n", sLive.lastError);
}

static bool parseEnable(const String &rest, bool *out) {
  if (rest == "1" || rest == "ON" || rest == "YES" || rest == "TRUE") {
    *out = true;
    return true;
  }
  if (rest == "0" || rest == "OFF" || rest == "NO" || rest == "FALSE") {
    *out = false;
    return true;
  }
  return false;
}

bool showNetworkHandleCommand(const String &command) {
  if (command == "NET:STATUS") {
    showNetworkPrintStatus();
    return true;
  }
  if (command.startsWith("NET:ENABLE:")) {
    bool en = false;
    if (!parseEnable(command.substring(11), &en)) return false;
    ShowNetConfig next = sSaved;
    next.enabled = en;
    char err[48] = "";
    return showNetworkSaveConfig(next, err, sizeof(err));
  }
  if (command == "NET:MODE:DHCP") {
    ShowNetConfig next = sSaved;
    next.mode = ShowNetMode::Dhcp;
    char err[48] = "";
    return showNetworkSaveConfig(next, err, sizeof(err));
  }
  if (command == "NET:MODE:STATIC") {
    ShowNetConfig next = sSaved;
    next.mode = ShowNetMode::Static;
    char err[48] = "";
    if (!showNetworkValidateConfig(next, err, sizeof(err))) {
      Serial.printf("[NET] STATIC rejected: %s\n", err);
      return false;
    }
    return showNetworkSaveConfig(next, err, sizeof(err));
  }
  if (command.startsWith("NET:STATIC:")) {
    String rest = command.substring(11);
    int c1 = rest.indexOf(':');
    int c2 = rest.indexOf(':', c1 + 1);
    if (c1 < 0 || c2 < 0) return false;
    int c3 = rest.indexOf(':', c2 + 1);
    ShowNetConfig next = sSaved;
    next.enabled = true;
    next.mode = ShowNetMode::Static;
    copyField(next.ip, sizeof(next.ip), rest.substring(0, c1).c_str());
    copyField(next.subnet, sizeof(next.subnet), rest.substring(c1 + 1, c2).c_str());
    if (c3 < 0) {
      copyField(next.gateway, sizeof(next.gateway), rest.substring(c2 + 1).c_str());
      next.dns[0] = '\0';
    } else {
      copyField(next.gateway, sizeof(next.gateway), rest.substring(c2 + 1, c3).c_str());
      copyField(next.dns, sizeof(next.dns), rest.substring(c3 + 1).c_str());
    }
    char err[48] = "";
    if (!showNetworkSaveConfig(next, err, sizeof(err))) {
      Serial.printf("[NET] STATIC rejected: %s\n", err[0] ? err : "invalid");
      return false;
    }
    return true;
  }
  if (command == "NET:APPLY") {
    showNetworkRequestApply();
    return true;
  }
  return false;
}
