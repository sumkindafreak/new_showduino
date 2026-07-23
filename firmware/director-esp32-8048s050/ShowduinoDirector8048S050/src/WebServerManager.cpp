#include "WebServerManager.h"

#if SHOWDUINO_WEBUI_ENABLED
#include "WebApiLogger.h"
#include "WebStudioAssets.h"
#include "StorageConfig.h"
#include "StorageAPI.h"
#include "FileUtil.h"

#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <SD.h>
#include <esp_wifi.h>

static WebServer sWebServer(80);
static unsigned long sBootMs = 0;
static bool sWebReady = false;

static const char *wifiModeString(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_MODE_NULL: return "OFF";
    case WIFI_MODE_STA: return "STA";
    case WIFI_MODE_AP: return "AP";
    case WIFI_MODE_APSTA: return "AP+STA";
    default: return "UNKNOWN";
  }
}

static String clientIpString() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return WiFi.softAPIP().toString();
}

static void sendJsonResponse(int code, const String &body) {
  sWebServer.send(code, "application/json", body);
}

static void handleApiSystem() {
  gWebApiLogger.logHttpRequest("GET", "/api/system");

  const StorageStatus &st = getStorageStatus();
  wifi_mode_t wmode = WiFi.getMode();
  String ssid = WiFi.softAPSSID();
  if (ssid.length() == 0) ssid = SHOWDUINO_WEBUI_AP_SSID;
  bool staConnected = (WiFi.status() == WL_CONNECTED);

  String json = "{\n";
  json += "  \"firmwareVersion\": \"" + String(STORAGE_FW_VERSION) + "\",\n";
  json += "  \"protocolVersion\": \"1.0\",\n";
  json += "  \"boardName\": \"" SHOWDUINO_BOARD_NAME "\",\n";
  json += "  \"uptime\": " + String(millis() - sBootMs) + ",\n";
  json += "  \"heapFree\": " + String(ESP.getFreeHeap()) + ",\n";
  json += "  \"heapTotal\": " + String(ESP.getHeapSize()) + ",\n";
  json += "  \"psramFree\": " + String(ESP.getFreePsram()) + ",\n";
  json += "  \"psramTotal\": " + String(ESP.getPsramSize()) + ",\n";
  json += "  \"cpuMhz\": " + String(getCpuFrequencyMhz()) + ",\n";
  json += "  \"storageReady\": " + String(st.mounted ? "true" : "false") + ",\n";
  json += "  \"showsPath\": \"/showduino/shows\",\n";
  json += "  \"showIndexPath\": \"" PATH_SHOW_INDEX "\",\n";
  json += "  \"showPackagesPath\": \"/showduino/shows/packages\",\n";
  json += "  \"showTrashPath\": \"/showduino/shows/trash\",\n";
  json += "  \"showFavouritesPath\": \"" PATH_SHOW_FAVOURITES "\",\n";
  json += "  \"showRecentPath\": \"" PATH_SHOW_RECENT "\",\n";
  json += "  \"uiSoundsPath\": \"/showduino/ui/sounds\",\n";
  json += "  \"fixtureProfilesPath\": \"/showduino/devices/fixture_profiles\",\n";
  json += "  \"devicePresetsPath\": \"/showduino/devices/presets\",\n";
  json += "  \"pairedDevicesPath\": \"" PATH_PAIRED_DEVICES "\",\n";
  json += "  \"lightingIconsPath\": \"/showduino/ui/icons/lighting\",\n";
  json += "  \"webuiPath\": \"" PATH_WEBUI_WWW "\",\n";
  json += "  \"mdnsHost\": \"" SHOWDUINO_WEBUI_MDNS "\",\n";
  json += "  \"apSsid\": \"" SHOWDUINO_WEBUI_AP_SSID "\",\n";
  json += "  \"espnowChannel\": " + String(SHOWDUINO_ESPNOW_CHANNEL) + ",\n";
  json += "  \"i2sStatus\": \"Deferred — GPIO17/18 reserved for Stage UART\",\n";
  json += "  \"wifi\": {\n";
  json += "    \"mode\": \"" + String(wifiModeString(wmode)) + "\",\n";
  json += "    \"ssid\": \"" + ShowduinoFileUtil::jsonEscape(ssid) + "\",\n";
  json += "    \"ip\": \"" + clientIpString() + "\",\n";
  json += "    \"hostname\": \"" + ShowduinoFileUtil::jsonEscape(String(WiFi.getHostname() ? WiFi.getHostname() : SHOWDUINO_WEBUI_MDNS)) + "\",\n";
  json += "    \"connected\": " + String(staConnected ? "true" : "false") + "\n";
  json += "  }\n";
  json += "}\n";

  sendJsonResponse(200, json);
}

static void handleApiDevices() {
  gWebApiLogger.logHttpRequest("GET", "/api/devices");

  String mac = WiFi.macAddress();
  String json = "{\n  \"devices\": [\n    {\n";
  json += "      \"name\": \"Director\",\n";
  json += "      \"board\": \"" SHOWDUINO_BOARD_NAME "\",\n";
  json += "      \"mac\": \"" + mac + "\",\n";
  json += "      \"role\": \"director\",\n";
  json += "      \"online\": true,\n";
  json += "      \"rssi\": 0,\n";
  json += "      \"protocolVersion\": \"1.0\",\n";
  json += "      \"connectionStatus\": \"local\"\n";
  json += "    }\n  ]\n}\n";

  sendJsonResponse(200, json);
}

static void handleApiLogs() {
  gWebApiLogger.logHttpRequest("GET", "/api/logs");

  String json = "{\n  \"logs\": ";
  gWebApiLogger.appendJsonArray(json);
  json += "\n}\n";

  sendJsonResponse(200, json);
}

static bool serveFromSd(const String &webPath) {
  if (!getStorageStatus().mounted) return false;
  if (!ShowduinoFileUtil::pathLooksSafe(webPath.c_str())) return false;
  if (!SD.exists(webPath.c_str())) return false;

  File f = SD.open(webPath.c_str(), FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return false;
  }

  String mime = "application/octet-stream";
  if (webPath.endsWith(".html")) mime = "text/html";
  else if (webPath.endsWith(".css")) mime = "text/css";
  else if (webPath.endsWith(".js")) mime = "application/javascript";
  else if (webPath.endsWith(".json")) mime = "application/json";
  else if (webPath.endsWith(".svg")) mime = "image/svg+xml";
  else if (webPath.endsWith(".png")) mime = "image/png";

  sWebServer.streamFile(f, mime);
  f.close();
  return true;
}

static String urlToWebPath(const String &uri) {
  String path = uri;
  if (path.length() == 0 || path == "/") return String(PATH_WEBUI_WWW) + "/index.html";
  if (path.endsWith("/")) path += "index.html";
  return String(PATH_WEBUI_WWW) + path;
}

static void handleStaticOrSpa() {
  const String &uri = sWebServer.uri();
  const char *method = (sWebServer.method() == HTTP_GET) ? "GET" :
                       (sWebServer.method() == HTTP_POST) ? "POST" : "OTHER";
  gWebApiLogger.logHttpRequest(method, uri.c_str());

  if (uri.startsWith("/api/")) {
    sWebServer.send(404, "application/json", "{\"error\":\"not found\"}\n");
    return;
  }

  String sdPath = urlToWebPath(uri);
  if (serveFromSd(sdPath)) return;

  const char *reqPath = uri.c_str();
  if (reqPath[0] == '\0' || strcmp(reqPath, "/") == 0) reqPath = "/index.html";

  WebStudioAsset asset = getEmbeddedAsset(reqPath);
  if (asset.data && asset.length > 0) {
    sWebServer.send_P(200, asset.mime, (PGM_P)asset.data, asset.length);
    return;
  }

  /* SPA fallback */
  asset = getEmbeddedAsset("/index.html");
  if (asset.data && asset.length > 0) {
    sWebServer.send_P(200, "text/html", (PGM_P)asset.data, asset.length);
    return;
  }

  sWebServer.send(404, "text/plain", "Not found\n");
}

static void setupWifiAp() {
  Serial.println("[WebUI] Starting AP+STA (ESP-NOW safe)...");

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);

  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(50);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP(SHOWDUINO_WEBUI_AP_SSID, SHOWDUINO_WEBUI_AP_PASSWORD,
              SHOWDUINO_ESPNOW_CHANNEL, 0, 4);

  WiFi.setHostname(SHOWDUINO_WEBUI_MDNS);

  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&primary, &second);
  Serial.printf("[WebUI] WiFi channel=%u mode=%s AP IP=%s\n",
                primary, wifiModeString(WiFi.getMode()), WiFi.softAPIP().toString().c_str());

  if (MDNS.begin(SHOWDUINO_WEBUI_MDNS)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WebUI] mDNS: http://%s.local/\n", SHOWDUINO_WEBUI_MDNS);
  } else {
    Serial.println("[WebUI] mDNS failed");
    gWebApiLogger.log(WEB_LOG_WARN, "WebUI", "mDNS init failed");
  }

  gWebApiLogger.log(WEB_LOG_INFO, "WebUI", "AP started on channel 1");
}

void webServerBegin(unsigned long bootMs) {
  sBootMs = bootMs;
  sWebReady = false;

  if (getStorageStatus().mounted) {
    ensureWwwOnSd();
  }

  setupWifiAp();

  sWebServer.on("/api/system", HTTP_GET, handleApiSystem);
  sWebServer.on("/api/devices", HTTP_GET, handleApiDevices);
  sWebServer.on("/api/logs", HTTP_GET, handleApiLogs);
  sWebServer.onNotFound(handleStaticOrSpa);
  sWebServer.begin();

  sWebReady = true;
  gWebApiLogger.log(WEB_LOG_INFO, "WebUI", "HTTP server on port 80");
  Serial.println("[WebUI] HTTP server started on port 80");
}

void webServerLoop() {
  if (!sWebReady) return;
  sWebServer.handleClient();
}

#endif /* SHOWDUINO_WEBUI_ENABLED */
