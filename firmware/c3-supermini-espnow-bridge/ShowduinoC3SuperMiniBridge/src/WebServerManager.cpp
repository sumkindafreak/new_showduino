#include "WebServerManager.h"

#if SHOWDUINO_WEBUI_ENABLED

#include "P4WebTunnel.h"

#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wifi.h>

static WebServer sWebServer(80);
static unsigned long sBootMs = 0;
static bool sWebReady = false;

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

static void handleStaticOrSpa() {
  String uri = sWebServer.uri();
  int q = uri.indexOf('?');
  if (q > 0) uri = uri.substring(0, q);
  if (uri.length() == 0 || uri == "/") uri = "/index.html";

  String body;
  String mime;
  int status = 0;
  if (p4WebTunnelGet(uri.c_str(), body, status, mime, 4000)) {
    if (mime.length() == 0) mime = guessMime(uri);
    sWebServer.send(status > 0 ? status : 200, mime.c_str(), body);
    return;
  }

  if (uri == "/index.html") {
    Serial.println("[WEB] WebUI unavailable - SD not mounted");
    sWebServer.send(503, "text/html", kFallbackHtml);
    return;
  }

  sWebServer.send(404, "text/plain", "Not found");
}

static void handleApiProxy() {
  String path = sWebServer.uri();
  if (sWebServer.args() > 0) {
    path += "?";
    for (int i = 0; i < sWebServer.args(); i++) {
      if (i) path += "&";
      path += sWebServer.argName(i) + "=" + sWebServer.arg(i);
    }
  }

  String body;
  String mime;
  int status = 0;
  if (p4WebTunnelGet(path.c_str(), body, status, mime, 1200)) {
    if (mime.length() == 0) mime = "application/json";
    sWebServer.send(status > 0 ? status : 200, mime.c_str(), body);
    return;
  }

  /* Local fallback so Studio still boots when P4 is offline. */
  if (path.startsWith("/api/system")) {
    String json = "{\n";
    json += "  \"firmwareVersion\": \"" SHOWDUINO_C3_FW_VERSION "\",\n";
    json += "  \"boardName\": \"ESP32-C3-SUE\",\n";
    json += "  \"role\": \"communications\",\n";
    json += "  \"uptime\": " + String(millis() - sBootMs) + ",\n";
    json += "  \"heapFree\": " + String(ESP.getFreeHeap()) + ",\n";
    json += "  \"apSsid\": \"" SHOWDUINO_WEBUI_AP_SSID "\",\n";
    json += "  \"mdnsHost\": \"" SHOWDUINO_WEBUI_MDNS "\",\n";
    json += "  \"stageLink\": \"offline\",\n";
    json += "  \"note\": \"P4 API tunnel timeout — serving C3 fallback\"\n";
    json += "}\n";
    sWebServer.send(200, "application/json", json);
    return;
  }

  sWebServer.send(502, "application/json",
                  "{\"error\":\"stage_unreachable\",\"path\":\"" + path + "\"}");
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
  if (apOn && ch == SHOWDUINO_ESPNOW_CHANNEL) {
    return;
  }
  Serial.println("[WebUI] SoftAP missing or off-channel — restarting");
  startSoftAp();
}

void webServerBegin(unsigned long bootMs) {
  sBootMs = bootMs;
  sWebReady = false;

  p4WebTunnelBegin();
  setupWifiAp();

  sWebServer.on("/api/system", HTTP_GET, handleApiProxy);
  sWebServer.on("/api/devices", HTTP_GET, handleApiProxy);
  sWebServer.on("/api/logs", HTTP_GET, handleApiProxy);
  sWebServer.on("/api/time", HTTP_GET, handleApiProxy);
  sWebServer.on("/api/time/status", HTTP_GET, handleApiProxy);
  /* Catch-all for other /api/* and static files */
  sWebServer.onNotFound([]() {
    if (sWebServer.uri().startsWith("/api/")) handleApiProxy();
    else handleStaticOrSpa();
  });
  sWebServer.begin();

  sWebReady = true;
  Serial.println("[WebUI] HTTP server on port 80");
  Serial.println("[WebUI] Join Wi-Fi: " SHOWDUINO_WEBUI_AP_SSID " / " SHOWDUINO_WEBUI_AP_PASSWORD);
  Serial.println("[WebUI] Open http://192.168.4.1/");
}

void webServerLoop() {
  if (!sWebReady) return;
  sWebServer.handleClient();

  static unsigned long lastApCheckMs = 0;
  const unsigned long now = millis();
  if (now - lastApCheckMs >= 8000UL) {
    lastApCheckMs = now;
    webServerReassertAp();
  }
}

#endif /* SHOWDUINO_WEBUI_ENABLED */
