#include "CommsWebServer.h"
#include "CommsWebTunnel.h"
#include "WebAssets.h"
#include "../EspNowTransport.h"
#include "../ProtocolBridge.h"
#include "../CommsUart.h"
#include "../status/CommsStatusRgb.h"
#include "../../BoardConfig.h"

#if SHOWDUINO_WEBUI_ENABLED

#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_mac.h>

static WebServer sServer(80);
static bool sReady = false;
static bool sFault = false;
static uint8_t sChannel = SHOWDUINO_ESPNOW_CHANNEL;
static char sIp[16] = "0.0.0.0";
static unsigned long sLastApCheckMs = 0;

static const char *wifiModeWord(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_MODE_STA: return "STA";
    case WIFI_MODE_AP: return "AP";
    case WIFI_MODE_APSTA: return "AP+STA";
    default: return "OFF";
  }
}

static void sendJson(int code, const String &body) {
  sServer.sendHeader("Access-Control-Allow-Origin", "*");
  sServer.sendHeader("Cache-Control", "no-store");
  sServer.send(code, "application/json", body);
}

static void sendCors() {
  sServer.sendHeader("Access-Control-Allow-Origin", "*");
  sServer.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  sServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  sServer.send(204);
}

static bool proxyGetToP4(const String &path, uint32_t timeoutMs = 1800) {
  String body;
  String mime;
  int status = 0;
  if (!commsWebTunnelGet(path.c_str(), body, status, mime, timeoutMs)) {
    return false;
  }
  if (mime.length() == 0) mime = "application/json";
  sServer.sendHeader("Access-Control-Allow-Origin", "*");
  sServer.sendHeader("Cache-Control", "no-store");
  sServer.send(status > 0 ? status : 200, mime.c_str(), body);
  return true;
}

static void handleApiComms() {
  uint8_t mac[6] = {0};
  espNowTransportReadStaMac(mac);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  String json = "{\n";
  json += "  \"firmwareVersion\": \"" SHOWDUINO_COMMS_FIRMWARE_VERSION "\",\n";
  json += "  \"role\": \"communications\",\n";
  json += "  \"webui\": \"LOCAL\",\n";
  json += "  \"webuiBuild\": \"" SHOWDUINO_WEBUI_BUILD_HASH "\",\n";
  json += "  \"webuiBuildUtc\": \"" SHOWDUINO_WEBUI_BUILD_UTC "\",\n";
  json += "  \"assetCount\": " + String(SHOWDUINO_WEBUI_ASSET_COUNT) + ",\n";
  json += "  \"assetBytes\": " + String(SHOWDUINO_WEBUI_EMBEDDED_BYTES) + ",\n";
  json += "  \"rawAssetBytes\": " + String(SHOWDUINO_WEBUI_RAW_BYTES) + ",\n";
  json += "  \"mac\": \"" + String(macStr) + "\",\n";
  json += "  \"ssid\": \"" SHOWDUINO_WEBUI_AP_SSID "\",\n";
  json += "  \"ip\": \"" + String(sIp) + "\",\n";
  json += "  \"mdnsHost\": \"" SHOWDUINO_WEBUI_MDNS "\",\n";
  json += "  \"wifiMode\": \"" + String(wifiModeWord(WiFi.getMode())) + "\",\n";
  json += "  \"espnowChannel\": " + String(SHOWDUINO_ESPNOW_CHANNEL) + ",\n";
  json += "  \"radioChannel\": " + String((unsigned)sChannel) + ",\n";
  json += "  \"directorOnline\": " + String(protocolBridgeDirectorOnline() ? "true" : "false") + ",\n";
  json += "  \"directorSeen\": " + String(espNowTransportHaveDirector() ? "true" : "false") + ",\n";
  json += "  \"p4Online\": " + String(protocolBridgeP4Alive() ? "true" : "false") + ",\n";
  json += "  \"p4Seen\": " + String(commsUartEverRx() ? "true" : "false") + ",\n";
  json += "  \"uartRx\": " + String(commsUartRxCount()) + ",\n";
  json += "  \"uartTx\": " + String(commsUartTxCount()) + ",\n";
  json += "  \"espnowRx\": " + String(espNowTransportRxCount()) + ",\n";
  json += "  \"espnowTx\": " + String(espNowTransportTxCount()) + ",\n";
  json += "  \"audioNodeSeen\": " + String(espNowTransportHaveAudioNode() ? "true" : "false") + ",\n";
  json += "  \"statusRgb\": {\n";
  json += "    \"state\": \"" + String(commsStatusRgbStateName()) + "\",\n";
  json += "    \"colour\": \"" + String(commsStatusRgbColourName()) + "\",\n";
  json += "    \"pin\": " + String((unsigned)commsStatusRgbPin()) + ",\n";
  json += "    \"brightness\": " + String((unsigned)commsStatusRgbBrightness()) + "\n";
  json += "  }\n";
  json += "}\n";
  sendJson(200, json);
}

static void handleApiSystem() {
  if (proxyGetToP4("/api/system")) return;
  String json = "{\n";
  json += "  \"error\": \"p4_offline\",\n";
  json += "  \"role\": \"communications\",\n";
  json += "  \"p4Online\": false,\n";
  json += "  \"stageLink\": \"offline\",\n";
  json += "  \"note\": \"P4 OFFLINE\"\n";
  json += "}\n";
  sendJson(503, json);
}

static void handleP4ApiOrOffline() {
  if (proxyGetToP4(sServer.uri(), 2500)) return;
  sendJson(503, "{\"error\":\"p4_offline\",\"p4Online\":false,\"note\":\"P4 OFFLINE\"}\n");
}

static void normalizeWebCmd(String &cmd) {
  cmd.trim();
  String upper = cmd;
  upper.toUpperCase();
  if (upper == "PANIC" || upper == "EMERGENCY:PANIC" ||
      upper == "ESTOP" || upper == "E-STOP") {
    cmd = "EMERGENCY:STOP";
  }
}

static bool webCommandAllowed(const String &cmd) {
  if (cmd == "SHOW:START" || cmd == "SHOW:RUN" || cmd == "SHOW:PAUSE" ||
      cmd == "SHOW:RESUME" || cmd == "SHOW:STOP" || cmd == "STOP:ALL") {
    return true;
  }
  if (cmd == "PRODUCTION:UNLOAD" || cmd == "PLUGIN:SCAN" ||
      cmd == "AUDIO:STOP" || cmd == "AUDIO:LOCAL:STOP" ||
      cmd == "AUDIO:NODE:STOP" || cmd == "AUDIO:NODE:PAUSE" ||
      cmd == "AUDIO:NODE:RESUME" || cmd == "AUDIO:NODE:STATUS" ||
      cmd == "AUDIO:NODE:TEST" || cmd == "AUDIO:NODE:DUCK" ||
      cmd == "AUDIO:NODE:UNDUCK" || cmd == "AUDIO:NODE:INVENTORY" ||
      cmd == "EMERGENCY:STOP" || cmd == "EMERGENCY:CLEAR_CONFIRM" ||
      cmd == "EMERGENCY:CLEAR_CANCEL" || cmd == "STATUS:REQUEST" ||
      cmd == "TIME:REQUEST") {
    return true;
  }
  if (cmd.startsWith("TIME:SET:")) {
    String arg = cmd.substring(9);
    arg.trim();
    if (arg.length() < 8 || arg.length() > 32) return false;
    for (unsigned i = 0; i < arg.length(); i++) {
      const char c = arg.charAt(i);
      const bool ok = (c >= '0' && c <= '9') || c == '-' || c == 'T' ||
                      c == ':' || c == 'Z' || c == ' ';
      if (!ok) return false;
    }
    return true;
  }
  if (cmd == "NET:STATUS" || cmd == "NET:APPLY" ||
      cmd == "NET:MODE:DHCP" || cmd == "NET:MODE:STATIC" ||
      cmd == "E131:STATUS" || cmd == "E131:CHANNELS") {
    return true;
  }
  if (cmd.startsWith("NET:ENABLE:")) {
    return cmd.length() == 12 && (cmd.endsWith("0") || cmd.endsWith("1"));
  }
  if (cmd.startsWith("E131:ENABLE:")) {
    return cmd.length() == 13 && (cmd.endsWith("0") || cmd.endsWith("1"));
  }
  if (cmd.startsWith("E131:UNIVERSE:")) {
    String n = cmd.substring(14);
    n.trim();
    if (n.length() == 0 || n.length() > 5) return false;
    for (unsigned i = 0; i < n.length(); i++) {
      if (n.charAt(i) < '0' || n.charAt(i) > '9') return false;
    }
    const long u = n.toInt();
    return u >= 1 && u <= 63999;
  }
  if (cmd.startsWith("E131:CHANNELS:")) {
    return cmd.length() < 32;
  }
  if (cmd.startsWith("NET:STATIC:")) {
    String rest = cmd.substring(11);
    if (rest.length() < 15 || rest.length() > 72) return false;
    for (unsigned i = 0; i < rest.length(); i++) {
      const char c = rest.charAt(i);
      const bool ok = (c >= '0' && c <= '9') || c == '.' || c == ':';
      if (!ok) return false;
    }
    return true;
  }
  if (cmd == "STORAGE:STATUS" || cmd == "STORAGE:LIST" ||
      cmd == "STORAGE:CHECK" || cmd == "STORAGE:BACKUP") {
    return true;
  }
  if (cmd.startsWith("AUDIO:NODE:PLAY:") || cmd.startsWith("AUDIO:NODE:LOOP:") ||
      cmd.startsWith("AUDIO:NODE:VOLUME:") || cmd.startsWith("AUDIO:NODE:STOP:FADE=") ||
      cmd.startsWith("AUDIO:NODE:INVENTORY:") || cmd.startsWith("AUDIO:NODE:SOUND:")) {
    return cmd.length() < 96 && cmd.indexOf("..") < 0;
  }
  if (cmd.startsWith("PRODUCTION:LOAD:")) {
    String id = cmd.substring(16);
    id.trim();
    if (id.length() == 0 || id.length() >= 48) return false;
    const char first = id.charAt(0);
    if (!((first >= 'a' && first <= 'z') || (first >= '0' && first <= '9'))) return false;
    for (unsigned i = 0; i < id.length(); i++) {
      const char c = id.charAt(i);
      const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
      if (!ok) return false;
    }
    return true;
  }
  return false;
}

static bool extractCmd(const String &body, String &cmd) {
  const int key = body.indexOf("\"cmd\"");
  if (key < 0) return false;
  const int colon = body.indexOf(':', key);
  const int q1 = body.indexOf('"', colon);
  const int q2 = body.indexOf('"', q1 + 1);
  if (q1 < 0 || q2 < 0) return false;
  cmd = body.substring(q1 + 1, q2);
  cmd.trim();
  return cmd.length() > 0;
}

static void handleApiCommand() {
  String cmd;
  if (!extractCmd(sServer.arg("plain"), cmd)) {
    sendJson(400, "{\"ok\":false,\"error\":\"missing_cmd\"}\n");
    return;
  }
  normalizeWebCmd(cmd);
  if (!webCommandAllowed(cmd)) {
    sendJson(403, "{\"ok\":false,\"error\":\"command_not_allowed\"}\n");
    return;
  }

  String path = "/api/command/" + cmd;
  String body;
  String mime;
  int status = 0;
  if (!commsWebTunnelPost(path.c_str(), body, status, mime, 4000)) {
    sendJson(503, "{\"ok\":false,\"error\":\"p4_offline\",\"p4Online\":false,\"note\":\"P4 OFFLINE\"}\n");
    return;
  }
  if (mime.length() == 0) mime = "application/json";
  sServer.sendHeader("Access-Control-Allow-Origin", "*");
  sServer.sendHeader("Cache-Control", "no-store");
  sServer.send(status > 0 ? status : 200, mime.c_str(), body);
}

static String assetPathFromUri(String uri) {
  int q = uri.indexOf('?');
  if (q >= 0) uri = uri.substring(0, q);
  if (uri.length() == 0 || uri == "/") return String("/index.html");
  return uri;
}

static void serveAsset(const ShowduinoWebAsset *asset, bool htmlNoCache) {
  (void)htmlNoCache;
  /* JS/CSS paths stay stable across embeds. A year-long immutable cache
   * kept stale app.js after the architecture cleanup and blanked the UI. */
  sServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  sServer.sendHeader("Pragma", "no-cache");
  if (asset->gzip) {
    sServer.sendHeader("Content-Encoding", "gzip");
    sServer.sendHeader("Vary", "Accept-Encoding");
  }
  sServer.send_P(200, asset->mime, (PGM_P)asset->data, asset->length);
}

static bool isMissingAssetPath(const String &path) {
  return path.startsWith("/js/") || path.startsWith("/css/") ||
         path.endsWith(".js") || path.endsWith(".css") ||
         path.endsWith(".map") || path.endsWith(".ico") ||
         path.endsWith(".png") || path.endsWith(".svg") ||
         path.endsWith(".woff") || path.endsWith(".woff2");
}

static void handleStaticOrSpa() {
  const String path = assetPathFromUri(sServer.uri());
  if (path.startsWith("/api/")) {
    sendJson(404, "{\"error\":\"not_found\"}\n");
    return;
  }

  const ShowduinoWebAsset *asset = commsWebFindAsset(path.c_str());
  if (asset) {
    serveAsset(asset, path == "/index.html");
    return;
  }

  /* Missing JS/CSS must not become index.html — browsers then report
   * "Failed to load module script" / MIME text/html and the boot screen
   * stays on "WebUI script failed to load". */
  if (isMissingAssetPath(path)) {
    sServer.sendHeader("Cache-Control", "no-store");
    sServer.send(404, "text/plain", "Not found\n");
    return;
  }

  asset = commsWebFindAsset("/index.html");
  if (asset) {
    serveAsset(asset, true);
    return;
  }
  sServer.send(404, "text/plain", "Not found\n");
}

static void logRadio(const char *tag) {
  uint8_t ch = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&ch, &second);
  sChannel = ch;
  Serial.printf("[WEBUI] %s mode=%s ch=%u SSID=%s IP=%s\n",
                tag,
                wifiModeWord(WiFi.getMode()),
                (unsigned)ch,
                SHOWDUINO_WEBUI_AP_SSID,
                sIp);
}

static bool startSoftAp() {
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  esp_wifi_set_ps(WIFI_PS_NONE);

  (void)esp_wifi_set_protocol(WIFI_IF_AP,
                              WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  (void)esp_wifi_set_protocol(WIFI_IF_STA,
                              WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  wifi_country_t country = {};
  memcpy(country.cc, "01", 2);
  country.schan = 1;
  country.nchan = 13;
  country.max_tx_power = 20;
  country.policy = WIFI_COUNTRY_POLICY_MANUAL;
  (void)esp_wifi_set_country(&country);

  delay(20);
  (void)esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

  const bool ok = WiFi.softAP(SHOWDUINO_WEBUI_AP_SSID, SHOWDUINO_WEBUI_AP_PASSWORD,
                              SHOWDUINO_ESPNOW_CHANNEL, false, 4);
  WiFi.setHostname(SHOWDUINO_WEBUI_MDNS);
  strncpy(sIp, WiFi.softAPIP().toString().c_str(), sizeof(sIp) - 1);
  sIp[sizeof(sIp) - 1] = '\0';

  wifi_config_t conf = {};
  if (esp_wifi_get_config(WIFI_IF_AP, &conf) == ESP_OK) {
    conf.ap.channel = SHOWDUINO_ESPNOW_CHANNEL;
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    conf.ap.max_connection = 4;
    conf.ap.beacon_interval = 100;
    (void)esp_wifi_set_config(WIFI_IF_AP, &conf);
  }

  uint8_t ch = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&ch, &second);
  sChannel = ch;
  sFault = !ok;
  Serial.printf("[WEBUI] SoftAP %s SSID=%s password=%s IP=%s\n",
                ok ? "OK" : "FAILED",
                SHOWDUINO_WEBUI_AP_SSID,
                SHOWDUINO_WEBUI_AP_PASSWORD,
                sIp);
  Serial.println("[WEBUI] CANONICAL SHOWDUINO WEBUI HOST");
  Serial.printf("[WEBUI] SSID: %s\n", SHOWDUINO_WEBUI_AP_SSID);
  Serial.printf("[WEBUI] AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
  Serial.printf("[WEBUI] CHANNEL: %u\n", (unsigned)SHOWDUINO_ESPNOW_CHANNEL);
  logRadio("after start");
  if (ch != SHOWDUINO_ESPNOW_CHANNEL) {
    Serial.printf("[WEBUI] WARNING: radio channel %u != ESP-NOW %u\n",
                  (unsigned)ch, (unsigned)SHOWDUINO_ESPNOW_CHANNEL);
  } else {
    Serial.printf("[WEBUI] ESP-NOW channel preserved: %u\n", (unsigned)ch);
  }
  return ok;
}

static void reassertAp() {
  wifi_mode_t mode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&mode);
  uint8_t ch = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&ch, &second);
  sChannel = ch;
  const bool apOn = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
  if (apOn && ch == SHOWDUINO_ESPNOW_CHANNEL) return;
  if (apOn && ch != SHOWDUINO_ESPNOW_CHANNEL) {
    Serial.printf("[WEBUI] radio left ch%u — locking ch%u without AP restart\n",
                  (unsigned)ch, (unsigned)SHOWDUINO_ESPNOW_CHANNEL);
    (void)esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    return;
  }
  Serial.println("[WEBUI] SoftAP missing — restarting");
  startSoftAp();
}

void commsWebBegin() {
  commsWebTunnelBegin();
  commsWebTunnelSetPump(protocolBridgeLoop);

  Serial.println("[WEBUI] Embedded assets ready");
  Serial.printf("[WEBUI] Build: %s (%s)\n",
                SHOWDUINO_WEBUI_BUILD_HASH, SHOWDUINO_WEBUI_BUILD_UTC);
  Serial.printf("[WEBUI] Asset bytes: %u compressed / %u raw (%u files)\n",
                (unsigned)SHOWDUINO_WEBUI_EMBEDDED_BYTES,
                (unsigned)SHOWDUINO_WEBUI_RAW_BYTES,
                (unsigned)SHOWDUINO_WEBUI_ASSET_COUNT);

  startSoftAp();

  if (MDNS.begin(SHOWDUINO_WEBUI_MDNS)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WEBUI] mDNS: http://%s.local/\n", SHOWDUINO_WEBUI_MDNS);
  } else {
    Serial.println("[WEBUI] mDNS failed (IP still works)");
  }

  sServer.on("/api/comms", HTTP_GET, handleApiComms);
  sServer.on("/api/system", HTTP_GET, handleApiSystem);
  sServer.on("/api/logs", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/devices", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/productions", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/show", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/plugins", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/audio", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/time", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/capabilities", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/network", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/e131", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/e131/channels", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/storage", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/lighting", HTTP_GET, handleP4ApiOrOffline);
  sServer.on("/api/command", HTTP_POST, handleApiCommand);
  sServer.on("/api/comms", HTTP_OPTIONS, sendCors);
  sServer.on("/api/system", HTTP_OPTIONS, sendCors);
  sServer.on("/api/logs", HTTP_OPTIONS, sendCors);
  sServer.on("/api/devices", HTTP_OPTIONS, sendCors);
  sServer.on("/api/productions", HTTP_OPTIONS, sendCors);
  sServer.on("/api/show", HTTP_OPTIONS, sendCors);
  sServer.on("/api/plugins", HTTP_OPTIONS, sendCors);
  sServer.on("/api/audio", HTTP_OPTIONS, sendCors);
  sServer.on("/api/time", HTTP_OPTIONS, sendCors);
  sServer.on("/api/capabilities", HTTP_OPTIONS, sendCors);
  sServer.on("/api/network", HTTP_OPTIONS, sendCors);
  sServer.on("/api/e131", HTTP_OPTIONS, sendCors);
  sServer.on("/api/e131/channels", HTTP_OPTIONS, sendCors);
  sServer.on("/api/storage", HTTP_OPTIONS, sendCors);
  sServer.on("/api/lighting", HTTP_OPTIONS, sendCors);
  sServer.on("/api/command", HTTP_OPTIONS, sendCors);
  sServer.onNotFound(handleStaticOrSpa);
  sServer.begin();
  sReady = true;

  Serial.println("[WEBUI] HTTP server on port 80");
  Serial.println("[WEBUI] Join Wi-Fi: " SHOWDUINO_WEBUI_AP_SSID " / " SHOWDUINO_WEBUI_AP_PASSWORD);
  Serial.println("[WEBUI] Open http://192.168.4.1/");
  Serial.println("[WEBUI] Default SoftAP password is a documented bench secret — change before a public venue.");
}

void commsWebLoop() {
  if (!sReady) return;
  sServer.handleClient();
  const uint32_t now = millis();
  if (now - sLastApCheckMs >= 8000UL) {
    sLastApCheckMs = now;
    reassertAp();
  }
}

bool commsWebReady() { return sReady && !sFault; }
bool commsWebFault() { return sFault; }
uint8_t commsWebRadioChannel() { return sChannel; }
const char *commsWebSsid() { return SHOWDUINO_WEBUI_AP_SSID; }
const char *commsWebIp() { return sIp; }

#else

void commsWebBegin() {}
void commsWebLoop() {}
bool commsWebReady() { return false; }
bool commsWebFault() { return true; }
uint8_t commsWebRadioChannel() { return SHOWDUINO_ESPNOW_CHANNEL; }
const char *commsWebSsid() { return ""; }
const char *commsWebIp() { return "0.0.0.0"; }

#endif
