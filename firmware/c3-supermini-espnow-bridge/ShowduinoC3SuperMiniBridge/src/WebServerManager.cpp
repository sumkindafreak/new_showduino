#include "WebServerManager.h"
#include "../BoardConfig.h"

#if SHOWDUINO_WEBUI_ENABLED
#include "WebStudioAssets.h"
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
static bool sWebReady = false;
static HeartbeatManager sHeartbeat;
static DeviceEventLog sDeviceLog;

static const char *wifiModeString(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_MODE_NULL: return "OFF";
    case WIFI_MODE_STA: return "STA";
    case WIFI_MODE_AP: return "AP";
    case WIFI_MODE_APSTA: return "AP+STA";
    default: return "UNKNOWN";
  }
}

static void sendJsonResponse(int code, const String &body) {
  sWebServer.sendHeader("Access-Control-Allow-Origin", "*");
  sWebServer.send(code, "application/json", body);
}

static void sendCorsOptions() {
  sWebServer.sendHeader("Access-Control-Allow-Origin", "*");
  sWebServer.sendHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
  sWebServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  sWebServer.send(204);
}

static bool proxyApiToP4(const char *path) {
  String body;
  int status = 502;
  Serial.printf("[WebUI] proxy %s ...\n", path);

  bool ok = p4WebTunnelGet(path, body, status, 3000);
  if (!ok) {
    Serial.println("[WebUI] proxy retry...");
    ok = p4WebTunnelGet(path, body, status, 5000);
  }

  if (!ok) {
    Serial.println("[WebUI] proxy FAILED — Show Engine unreachable");
    sendJsonResponse(502,
      "{\"error\":\"show engine unreachable\","
      "\"hint\":\"reflash P4+C3; wire C3 TX21→P4 RX5, C3 RX20←P4 TX6; USB WEBTEST\"}\n");
    return true;
  }

  Serial.printf("[WebUI] proxy OK status=%d bytes=%u\n", status, (unsigned)body.length());
  sendJsonResponse(status, body);
  return true;
}

static void handleApiSystem() { proxyApiToP4("/api/system"); }
static void handleApiLogs() { proxyApiToP4("/api/logs"); }

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
  if (!uri.startsWith(prefix)) {
    sendJsonResponse(404, "{\"error\":\"not found\"}\n");
    return;
  }
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
  /* {"epoch":1730000000} or {"daily":true,"hour":19,"minute":30,"second":0} */
  int ep = json.indexOf("\"epoch\"");
  int daily = json.indexOf("\"daily\"");
  bool ok = false;
  if (ep >= 0) {
    int colon = json.indexOf(':', ep);
    uint32_t epoch = (uint32_t)strtoul(json.c_str() + colon + 1, nullptr, 10);
    ok = gTimeService.scheduleAlarmAtEpoch(epoch);
  } else if (daily >= 0) {
    auto readU8 = [&](const char *key) -> int {
      int k = json.indexOf(key);
      if (k < 0) return -1;
      int c = json.indexOf(':', k);
      if (c < 0) return -1;
      return atoi(json.c_str() + c + 1);
    };
    int h = readU8("\"hour\"");
    int m = readU8("\"minute\"");
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

static void onTimeEvent(const char *eventName, const char *detailJson) {
  gWebSocketManager.sendJsonEvent(eventName, detailJson);
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
  /* Test resolve only — no queue, no Stage Runtime, no hardware. */
  bool ok = gDeviceRouter.route(cmd);
  String body;
  body.reserve(1024);
  gDeviceRouter.appendRouteTestJson(cmd, body);
  (void)ok;
  sendJsonResponse(200, body);
}

static void onCapabilityEvent(const char *eventName, const char *detailJson) {
  gWebSocketManager.sendJsonEvent(eventName, detailJson);
}

static void onRouteEvent(const char *eventName, const char *detailJson) {
  gWebSocketManager.sendJsonEvent(eventName, detailJson);
}
static void handleApiCommands() {
  String body;
  body.reserve(4096);
  gCommandManager.appendCommandsApiJson(body);
  sendJsonResponse(200, body);
}

static void handleApiCommandGet() {
  const String &uri = sWebServer.uri();
  String id = uri.substring(strlen("/api/command/"));
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
  if (json.length() == 0) {
    /* Some cores place body in arg(0). */
    if (sWebServer.args() > 0) json = sWebServer.arg(0);
  }
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
  const String &uri = sWebServer.uri();
  String id = uri.substring(strlen("/api/command/"));
  id.trim();
  String response;
  int status = 404;
  gCommandManager.cancelById(id.c_str(), response, status);
  sendJsonResponse(status, response);
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
  } else {
    gWebSocketManager.sendCommandEvent(eventName, String());
  }
}

static void handleStaticOrSpa() {
  const String &uri = sWebServer.uri();

  if (uri.startsWith("/api/device/")) {
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
    sWebServer.send(404, "application/json", "{\"error\":\"not found\"}\n");
    return;
  }

  const char *reqPath = uri.c_str();
  if (reqPath[0] == '\0' || strcmp(reqPath, "/") == 0) reqPath = "/index.html";

  WebStudioAsset asset = getEmbeddedAsset(reqPath);
  if (asset.data && asset.length > 0) {
    sWebServer.send_P(200, asset.mime, (PGM_P)asset.data, asset.length);
    return;
  }

  asset = getEmbeddedAsset("/index.html");
  if (asset.data && asset.length > 0) {
    sWebServer.send_P(200, "text/html", (PGM_P)asset.data, asset.length);
    return;
  }

  sWebServer.send(404, "text/plain", "Not found\n");
}

static void setupWifiAp() {
  Serial.println("[WebUI] C3 front door: AP+STA (ESP-NOW safe)...");

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);

  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(50);

  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(SHOWDUINO_WEBUI_AP_SSID, SHOWDUINO_WEBUI_AP_PASSWORD,
              SHOWDUINO_ESPNOW_CHANNEL, 0, 4);

  /* SoftAP can leave home_channel stale vs peer.channel — force fabric channel. */
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(30);

  WiFi.setHostname(SHOWDUINO_WEBUI_MDNS);

  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&primary, &second);
  Serial.printf("[WebUI] channel=%u mode=%s AP IP=%s\n",
                primary, wifiModeString(WiFi.getMode()), WiFi.softAPIP().toString().c_str());

  if (MDNS.begin(SHOWDUINO_WEBUI_MDNS)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WebUI] mDNS: http://%s.local/\n", SHOWDUINO_WEBUI_MDNS);
  } else {
    Serial.println("[WebUI] mDNS failed");
  }
}

void webServerBegin(unsigned long bootMs) {
  (void)bootMs;
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
  sWebServer.on("/api/devices", HTTP_GET, handleApiDevices);
  sWebServer.on("/api/network", HTTP_GET, handleApiNetwork);
  sWebServer.on("/api/logs", HTTP_GET, handleApiLogs);
  sWebServer.on("/api/commands", HTTP_GET, handleApiCommands);
  sWebServer.on("/api/command", HTTP_POST, handleApiCommandPost);
  sWebServer.on("/api/command", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/commands", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/time", HTTP_GET, handleApiTime);
  sWebServer.on("/api/time/status", HTTP_GET, handleApiTimeStatus);
  sWebServer.on("/api/time/alarm", HTTP_POST, handleApiTimeAlarmPost);
  sWebServer.on("/api/time/alarm", HTTP_DELETE, handleApiTimeAlarmDelete);
  sWebServer.on("/api/time", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/time/status", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/time/alarm", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/capabilities", HTTP_GET, handleApiCapabilities);
  sWebServer.on("/api/device-capabilities", HTTP_GET, handleApiDeviceCapabilities);
  sWebServer.on("/api/routes", HTTP_GET, handleApiRoutes);
  sWebServer.on("/api/route-test", HTTP_POST, handleApiRouteTest);
  sWebServer.on("/api/route-test", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/capabilities", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/device-capabilities", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.on("/api/routes", HTTP_OPTIONS, sendCorsOptions);
  sWebServer.onNotFound(handleStaticOrSpa);
  sWebServer.begin();

  gWebSocketManager.begin(SHOWDUINO_WEBSOCKET_PORT);

  sWebReady = true;
  Serial.println("[WebUI] HTTP :80 + WebSocket (devices + command bus + routing + time)");
}

void webServerLoop() {
  if (!sWebReady) return;
  sWebServer.handleClient();
  gTimeService.loop(millis());
  gDeviceManager.loop(millis());
  gCommandManager.loop(millis());
  gWebSocketManager.loop();
}

#endif /* SHOWDUINO_WEBUI_ENABLED */