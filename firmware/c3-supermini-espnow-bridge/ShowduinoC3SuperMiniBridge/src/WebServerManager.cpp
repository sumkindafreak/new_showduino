#include "WebServerManager.h"

#if SHOWDUINO_WEBUI_ENABLED

#include "P4WebTunnel.h"
#include "DeviceManager.h"
#include "WebSocketManager.h"
#include "HeartbeatManager.h"
#include "DeviceEventLog.h"
#include "command/CommandManager.h"
#include "capability/CapabilityManager.h"
#include "routing/DeviceRouter.h"
#include "time/TimeService.h"

#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wifi.h>

static WebServer sWebServer(80);
static unsigned long sBootMs = 0;
static bool sWebReady = false;
static HeartbeatManager sHeartbeat;
static DeviceEventLog sDeviceLog;

static const char kFallbackHtml[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Showduino</title>"
    "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:2rem}</style></head>"
    "<body><h1>Showduino WebUI unavailable</h1>"
    "<p>The Stage Controller could not serve /showduino/webui/ from SD.</p>"
    "<p>Check the P4 SD card, then open this page again.</p>"
    "<p>Emergency safety on the P4 remains operational without the WebUI.</p>"
    "</body></html>";

static const char *guessMime(const String &uri) {
  if (uri.endsWith(".css")) return "text/css";
  if (uri.endsWith(".js") || uri.endsWith(".mjs")) return "application/javascript";
  if (uri.endsWith(".json")) return "application/json";
  if (uri.endsWith(".svg")) return "image/svg+xml";
  if (uri.endsWith(".png")) return "image/png";
  if (uri.endsWith(".jpg") || uri.endsWith(".jpeg")) return "image/jpeg";
  if (uri.endsWith(".ico")) return "image/x-icon";
  if (uri.endsWith(".woff")) return "font/woff";
  if (uri.endsWith(".woff2")) return "font/woff2";
  if (uri.endsWith(".html") || uri == "/" || uri == "/index.html") return "text/html";
  return "text/plain";
}

static void sendJsonResponse(int code, const String &body) {
  sWebServer.sendHeader("Access-Control-Allow-Origin", "*");
  sWebServer.sendHeader("Cache-Control", "no-store");
  sWebServer.send(code, "application/json", body);
}

static void sendCorsOptions() {
  sWebServer.sendHeader("Access-Control-Allow-Origin", "*");
  sWebServer.sendHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
  sWebServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  sWebServer.send(204);
}

static void noteStageSighting() {
  if (!gDeviceManager.ready()) return;
  gDeviceManager.noteUartSighting(
      "ian", "Stage Controller", "ian",
      "SceneRuntime,Scheduler,RelayOutput,Lighting,AudioPlayback,PixelOutput,Temperature,Humidity,Logging");
}

static bool proxyApiToP4(const String &path, uint32_t timeoutMs = 1600) {
  String body;
  String mime;
  int status = 0;
  if (p4WebTunnelGet(path.c_str(), body, status, mime, timeoutMs)) {
    noteStageSighting();
    if (mime.length() == 0) mime = "application/json";
    sWebServer.send(status > 0 ? status : 200, mime.c_str(), body);
    return true;
  }
  return false;
}

static void handleApiSystem() {
  String path = sWebServer.uri();
  if (proxyApiToP4(path, 1600)) return;

  /* Local fallback lets the operator see that SUE is alive when P4 is not. */
  String json = "{\n";
  json += "  \"firmwareVersion\": \"" SHOWDUINO_C3_FW_VERSION "\",\n";
  json += "  \"boardName\": \"ESP32-C3-SUE\",\n";
  json += "  \"role\": \"communications\",\n";
  json += "  \"uptime\": " + String(millis() - sBootMs) + ",\n";
  json += "  \"heapFree\": " + String(ESP.getFreeHeap()) + ",\n";
  json += "  \"apSsid\": \"" SHOWDUINO_WEBUI_AP_SSID "\",\n";
  json += "  \"mdnsHost\": \"" SHOWDUINO_WEBUI_MDNS "\",\n";
  json += "  \"stageLink\": \"offline\",\n";
  json += "  \"showState\": \"OFFLINE\",\n";
  json += "  \"emergencyActive\": false,\n";
  json += "  \"note\": \"P4 API tunnel timeout — serving C3 fallback\"\n";
  json += "}\n";
  sendJsonResponse(200, json);
}

static void handleApiLogs() {
  if (proxyApiToP4(sWebServer.uri(), 1800)) return;
  sendJsonResponse(502, "{\"error\":\"stage_unreachable\",\"path\":\"/api/logs\"}");
}

static void handleApiDevices() {
  String body;
  body.reserve(2048);
  gDeviceManager.appendDevicesJson(body);
  sendJsonResponse(200, body);
}

static void handleApiNetwork() {
  String body;
  body.reserve(1024);
  gDeviceManager.appendNetworkJson(body, millis());
  sendJsonResponse(200, body);
}

static void handleApiDeviceById() {
  const String &uri = sWebServer.uri();
  const char *prefix = "/api/device/";
  String id = uri.substring(strlen(prefix));
  id.trim();
  if (id.length() == 0) {
    sendJsonResponse(400, "{\"error\":\"missing device id\"}\n");
    return;
  }
  String body;
  if (!gDeviceManager.appendDeviceJsonById(id.c_str(), body)) {
    sendJsonResponse(404, "{\"error\":\"device not found\"}\n");
    return;
  }
  sendJsonResponse(200, body);
}

static void handleApiTime() {
  String body;
  body.reserve(512);
  gTimeService.appendTimeJson(body);
  sendJsonResponse(200, body);
}

static void handleApiTimeStatus() {
  String body;
  body.reserve(512);
  gTimeService.appendStatusJson(body);
  sendJsonResponse(200, body);
}

static void handleApiTimeAlarmPost() {
  String json = sWebServer.arg("plain");
  if (json.length() == 0 && sWebServer.args() > 0) json = sWebServer.arg(0);
  if (json.length() == 0) {
    sendJsonResponse(400, "{\"error\":\"empty body\"}\n");
    return;
  }

  bool ok = false;
  const int ep = json.indexOf("\"epoch\"");
  const int daily = json.indexOf("\"daily\"");
  if (ep >= 0) {
    const int colon = json.indexOf(':', ep);
    if (colon >= 0) {
      const uint32_t epoch = (uint32_t)strtoul(json.c_str() + colon + 1, nullptr, 10);
      ok = gTimeService.scheduleAlarmAtEpoch(epoch);
    }
  } else if (daily >= 0) {
    auto readU8 = [&](const char *key) -> int {
      const int k = json.indexOf(key);
      if (k < 0) return -1;
      const int c = json.indexOf(':', k);
      if (c < 0) return -1;
      return atoi(json.c_str() + c + 1);
    };
    const int h = readU8("\"hour\"");
    const int m = readU8("\"minute\"");
    int s = readU8("\"second\"");
    if (s < 0) s = 0;
    if (h < 0 || m < 0) {
      sendJsonResponse(400, "{\"error\":\"daily requires hour and minute\"}\n");
      return;
    }
    ok = gTimeService.scheduleDailyAlarm((uint8_t)h, (uint8_t)m, (uint8_t)s);
  } else {
    sendJsonResponse(400, "{\"error\":\"need epoch or daily\"}\n");
    return;
  }

  if (!ok) {
    sendJsonResponse(400, "{\"error\":\"alarm arm failed\"}\n");
    return;
  }

  String body;
  gTimeService.appendStatusJson(body);
  sendJsonResponse(200, body);
}

static void handleApiTimeAlarmDelete() {
  gTimeService.cancelAlarm();
  String body;
  gTimeService.appendStatusJson(body);
  sendJsonResponse(200, body);
}

static void handleApiCapabilities() {
  String body;
  body.reserve(512);
  gCapabilityManager.appendCapabilitiesCatalogJson(body);
  sendJsonResponse(200, body);
}

static void handleApiDeviceCapabilities() {
  String body;
  body.reserve(2048);
  gCapabilityManager.appendDeviceCapabilitiesJson(body);
  sendJsonResponse(200, body);
}

static void handleApiRoutes() {
  String body;
  body.reserve(1024);
  gDeviceRouter.appendRoutesJson(body);
  sendJsonResponse(200, body);
}

static void handleApiRouteTest() {
  String json = sWebServer.arg("plain");
  if (json.length() == 0 && sWebServer.args() > 0) json = sWebServer.arg(0);
  if (json.length() == 0) {
    sendJsonResponse(400, "{\"error\":\"empty body\"}\n");
    return;
  }

  ShowCommand cmd;
  String err;
  if (!showCommandFromJson(json, cmd, err)) {
    sendJsonResponse(400, String("{\"error\":\"") + err + "\"}");
    return;
  }
  if (!cmd.source[0]) strncpy(cmd.source, "web-studio", sizeof(cmd.source) - 1);

  (void)gDeviceRouter.route(cmd);
  String body;
  body.reserve(1024);
  gDeviceRouter.appendRouteTestJson(cmd, body);
  sendJsonResponse(200, body);
}

static void handleApiCommands() {
  String body;
  body.reserve(4096);
  gCommandManager.appendCommandsApiJson(body);
  sendJsonResponse(200, body);
}

static void handleApiCommandGet() {
  String id = sWebServer.uri().substring(strlen("/api/command/"));
  id.trim();
  String body;
  if (!gCommandManager.getById(id.c_str(), body)) {
    sendJsonResponse(404, "{\"error\":\"command not found\"}\n");
    return;
  }
  sendJsonResponse(200, body);
}

static void handleApiCommandPost() {
  String json = sWebServer.arg("plain");
  if (json.length() == 0 && sWebServer.args() > 0) json = sWebServer.arg(0);
  if (json.length() == 0) {
    sendJsonResponse(400, "{\"error\":\"empty body\"}\n");
    return;
  }

  String response;
  int status = 400;
  gCommandManager.submitJson(json, response, status);
  sendJsonResponse(status, response);
}

static void handleApiCommandDelete() {
  String id = sWebServer.uri().substring(strlen("/api/command/"));
  id.trim();
  String response;
  int status = 404;
  gCommandManager.cancelById(id.c_str(), response, status);
  sendJsonResponse(status, response);
}

static void onTimeEvent(const char *eventName, const char *detailJson) {
  gWebSocketManager.sendJsonEvent(eventName, detailJson);
}

static void onCapabilityEvent(const char *eventName, const char *detailJson) {
  gWebSocketManager.sendJsonEvent(eventName, detailJson);
}

static void onRouteEvent(const char *eventName, const char *detailJson) {
  gWebSocketManager.sendJsonEvent(eventName, detailJson);
}

static void onDeviceChanged(const char *eventName, const DeviceRecord &device) {
  gWebSocketManager.sendDeviceEvent(eventName, device);
  if (strcmp(eventName, "device.online") == 0 ||
      strcmp(eventName, "device.offline") == 0 ||
      strcmp(eventName, "device.discovered") == 0 ||
      strcmp(eventName, "device.heartbeat_timeout") == 0 ||
      strcmp(eventName, "connection.restored") == 0) {
    NetworkStatistics st;
    gDeviceManager.computeNetworkStats(st, millis());
    gWebSocketManager.sendNetworkStats(st);
  }
}

static void onCommandBusEvent(const char *eventName, const ShowCommand *cmd, const char *extraJson) {
  if (!eventName) return;
  if (strcmp(eventName, "queue.updated") == 0) {
    gWebSocketManager.sendQueueUpdated(gCommandManager.queue().size(),
                                       gCommandManager.queue().emergencyDepth());
    return;
  }
  if (cmd) {
    String cj;
    showCommandToJson(*cmd, cj);
    gWebSocketManager.sendCommandEvent(eventName, cj);
    return;
  }
  if (extraJson && extraJson[0]) {
    gWebSocketManager.sendCommandEvent(eventName, String(extraJson));
  }
}

static void handleStaticOrSpa() {
  String uri = sWebServer.uri();
  int q = uri.indexOf('?');
  if (q > 0) uri = uri.substring(0, q);
  if (uri.length() == 0 || uri == "/") uri = "/index.html";

  String body;
  String mime;
  int status = 0;
  if (p4WebTunnelGet(uri.c_str(), body, status, mime, 4000)) {
    noteStageSighting();
    if (mime.length() == 0) mime = guessMime(uri);
    sWebServer.send(status > 0 ? status : 200, mime.c_str(), body);
    return;
  }

  if (uri == "/index.html") {
    Serial.println("[WEB] WebUI unavailable - SD not mounted or P4 unreachable");
    sWebServer.send(503, "text/html", kFallbackHtml);
    return;
  }

  sWebServer.send(404, "text/plain", "Not found");
}

static void logApState(const char *tag) {
  uint8_t ch = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&ch, &second);
  wifi_mode_t mode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&mode);
  Serial.printf("[WebUI] %s mode=%d ch=%u SSID=%s IP=%s STA=%s AP=%s\n",
                tag,
                (int)mode,
                (unsigned)ch,
                WiFi.softAPSSID().c_str(),
                WiFi.softAPIP().toString().c_str(),
                WiFi.macAddress().c_str(),
                WiFi.softAPmacAddress().c_str());
}

static bool startSoftAp() {
  Serial.println("[WebUI] Starting SoftAP (Showduino Studio)...");

  WiFi.persistent(false);
  WiFi.setSleep(false);
  /* AP_STA keeps ESP-NOW on the STA MAC the Director already peers with. */
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);

  esp_wifi_set_ps(WIFI_PS_NONE);

  /* C3 Arduino 3 / IDF 5 defaults to 802.11ax. Many phones never list AX APs. */
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
  (void)esp_wifi_set_max_tx_power(78);

  /* Channel before AP create. Do not set_channel again after softAP(). */
  delay(20);
  (void)esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

  bool ok = WiFi.softAP(SHOWDUINO_WEBUI_AP_SSID, SHOWDUINO_WEBUI_AP_PASSWORD,
                        SHOWDUINO_ESPNOW_CHANNEL, false, 4);
  WiFi.setHostname(SHOWDUINO_WEBUI_MDNS);

  wifi_config_t conf = {};
  if (esp_wifi_get_config(WIFI_IF_AP, &conf) == ESP_OK) {
    conf.ap.ssid_hidden = 0;
    conf.ap.channel = SHOWDUINO_ESPNOW_CHANNEL;
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    conf.ap.max_connection = 4;
    conf.ap.beacon_interval = 100;
    (void)esp_wifi_set_config(WIFI_IF_AP, &conf);
  }

  Serial.printf("[WebUI] SoftAP %s  SSID=%s  password=%s  IP=%s\n",
                ok ? "OK" : "FAILED",
                SHOWDUINO_WEBUI_AP_SSID,
                SHOWDUINO_WEBUI_AP_PASSWORD,
                WiFi.softAPIP().toString().c_str());
  logApState("after start");
  return ok;
}

static void setupWifiAp() {
  startSoftAp();

  if (MDNS.begin(SHOWDUINO_WEBUI_MDNS)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WebUI] mDNS: http://%s.local/\n", SHOWDUINO_WEBUI_MDNS);
  } else {
    Serial.println("[WebUI] mDNS failed (IP still works)");
  }
}

void webServerReassertAp() {
  wifi_mode_t mode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&mode);
  const bool apOn = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
  uint8_t ch = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&ch, &second);
  if (apOn && ch == SHOWDUINO_ESPNOW_CHANNEL) return;

  Serial.println("[WebUI] SoftAP missing or off-channel — restarting");
  startSoftAp();
}

void webServerBegin(unsigned long bootMs) {
  sBootMs = bootMs;
  sWebReady = false;

  p4WebTunnelBegin();
  setupWifiAp();

  sHeartbeat.configure(SHOWDUINO_DEVICE_HB_ONLINE_MS,
                       SHOWDUINO_DEVICE_HB_WARNING_MS,
                       SHOWDUINO_DEVICE_HB_OFFLINE_MS);

  gTimeService.begin();
  gTimeService.setEventCallback(onTimeEvent);

  gDeviceManager.begin(&sHeartbeat, &sDeviceLog);
  gDeviceManager.setChangeCallback(onDeviceChanged);

  gCapabilityManager.begin();
  gCapabilityManager.setEventLog(&sDeviceLog);
  gCapabilityManager.setEventCallback(onCapabilityEvent);

  gDeviceRouter.begin();
  gDeviceRouter.setEventLog(&sDeviceLog);
  gDeviceRouter.setEventCallback(onRouteEvent);

  gCommandManager.begin();
  gCommandManager.setEventCallback(onCommandBusEvent);

  sWebServer.on("/api/system", HTTP_GET, handleApiSystem);
  sWebServer.on("/api/logs", HTTP_GET, handleApiLogs);
  sWebServer.on("/api/devices", HTTP_GET, handleApiDevices);
  sWebServer.on("/api/network", HTTP_GET, handleApiNetwork);
  sWebServer.on("/api/commands", HTTP_GET, handleApiCommands);
  sWebServer.on("/api/command", HTTP_POST, handleApiCommandPost);
  sWebServer.on("/api/time", HTTP_GET, handleApiTime);
  sWebServer.on("/api/time/status", HTTP_GET, handleApiTimeStatus);
  sWebServer.on("/api/time/alarm", HTTP_POST, handleApiTimeAlarmPost);
  sWebServer.on("/api/time/alarm", HTTP_DELETE, handleApiTimeAlarmDelete);
  sWebServer.on("/api/capabilities", HTTP_GET, handleApiCapabilities);
  sWebServer.on("/api/device-capabilities", HTTP_GET, handleApiDeviceCapabilities);
  sWebServer.on("/api/routes", HTTP_GET, handleApiRoutes);
  sWebServer.on("/api/route-test", HTTP_POST, handleApiRouteTest);

  sWebServer.on("/api/command", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/commands", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/time", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/time/status", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/time/alarm", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/capabilities", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/device-capabilities", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/routes", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/route-test", HTTP_OPTIONS, sendCorsOptions);

  sWebServer.onNotFound([]() {
    const String uri = sWebServer.uri();
    if (uri.startsWith("/api/device/") && sWebServer.method() == HTTP_GET) {
      handleApiDeviceById();
      return;
    }
    if (uri.startsWith("/api/command/") && sWebServer.method() == HTTP_GET) {
      handleApiCommandGet();
      return;
    }
    if (uri.startsWith("/api/command/") && sWebServer.method() == HTTP_DELETE) {
      handleApiCommandDelete();
      return;
    }
    if (uri.startsWith("/api/")) {
      sendJsonResponse(404, "{\"error\":\"not_found\"}\n");
      return;
    }
    handleStaticOrSpa();
  });

  sWebServer.begin();
  sWebReady = true;

  Serial.println("[WebUI] HTTP server on port 80");
  Serial.println("[WebUI] REST command bus + RTC + routing restored");
  Serial.println("[WebUI] Join Wi-Fi: " SHOWDUINO_WEBUI_AP_SSID " / " SHOWDUINO_WEBUI_AP_PASSWORD);
  Serial.println("[WebUI] Open http://192.168.4.1/");
}

void webServerLoop() {
  if (!sWebReady) return;

  sWebServer.handleClient();

  const uint32_t now = millis();
  gTimeService.loop(now);
  gDeviceManager.loop(now);
  gCommandManager.loop(now);

  static unsigned long lastApCheckMs = 0;
  if (now - lastApCheckMs >= 8000UL) {
    lastApCheckMs = now;
    webServerReassertAp();
  }
}

#endif /* SHOWDUINO_WEBUI_ENABLED */
