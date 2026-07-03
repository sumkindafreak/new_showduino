/*
  =========================================================
  Showduino v1 - CYD Director WebUI-from-SD Firmware
  =========================================================

  Target board:
    ESP32-2432S028R / CYD 2.8 inch touchscreen

  Purpose:
    - Starts a Showduino WiFi access point
    - Mounts the CYD SD card
    - Serves the WebUI from /www/ on the CYD SD card
    - Provides API endpoints for browser -> CYD -> Mega commands
    - Sends commands to the Arduino Mega Stage Engine over UART serial

  CYD SD card layout:
    /www/index.html
    /www/app.js
    /www/style.css
    /www/assets/...
    /scenes/
    /shows/
    /projects/
    /logs/
    /settings.json

  Mega serial wiring:
    CYD TX pin 1  -> Mega RX1 pin 19
    CYD RX pin 3  -> Mega TX1 pin 18
    CYD GND       -> Mega GND

  Required libraries:
    - WiFi
    - WebServer
    - ESPmDNS
    - SPI
    - SD
    - TFT_eSPI
    - XPT2046_Bitbang
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include "XPT2046_Bitbang.h"

// =========================================================
// WiFi Access Point configuration
// =========================================================

#define AP_SSID "Showduino"
#define AP_PASSWORD "showduino"
#define AP_CHANNEL 6
#define AP_HIDDEN false
#define AP_MAX_CLIENTS 4
#define MDNS_NAME "showduino"

IPAddress apIP(192, 168, 4, 1);
IPAddress gatewayIP(192, 168, 4, 1);
IPAddress subnetIP(255, 255, 255, 0);

// =========================================================
// Serial configuration to Mega
// =========================================================

#define USB_BAUD_RATE 115200
#define MEGA_BAUD_RATE 115200
#define MEGA_RX_PIN 3
#define MEGA_TX_PIN 1

// =========================================================
// CYD SD card pins
// =========================================================

#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5

SPIClass sdSPI(VSPI);
bool sdReady = false;

// =========================================================
// CYD touch/display pins
// =========================================================

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
#define BACKLIGHT_PIN 21
#define CYD_LED_BLUE 17
#define CYD_LED_RED 4
#define CYD_LED_GREEN 16

TFT_eSPI tft = TFT_eSPI();
XPT2046_Bitbang touch(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS);

// =========================================================
// Web server
// =========================================================

WebServer server(80);

// =========================================================
// Runtime state
// =========================================================

String megaBuffer = "";
String lastMegaMessage = "No Mega response yet";
String lastCommandSent = "None";
bool megaAlive = false;
unsigned long lastHeartbeatMs = 0;
unsigned long lastScreenRefreshMs = 0;

// =========================================================
// LED helper. CYD RGB LEDs are usually active LOW.
// =========================================================

void setCydLed(bool redOn, bool greenOn, bool blueOn) {
  digitalWrite(CYD_LED_RED, redOn ? LOW : HIGH);
  digitalWrite(CYD_LED_GREEN, greenOn ? LOW : HIGH);
  digitalWrite(CYD_LED_BLUE, blueOn ? LOW : HIGH);
}

// =========================================================
// Display helpers
// =========================================================

void drawStatusScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("SHOWDUINO v1", 160, 10, 4);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CYD Director Web Server", 160, 44, 2);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.drawString("WiFi AP:", 10, 78, 2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(AP_SSID, 110, 78, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Password:", 10, 102, 2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(AP_PASSWORD, 110, 102, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("URL:", 10, 126, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("http://192.168.4.1", 110, 126, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("mDNS:", 10, 150, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("showduino.local", 110, 150, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CYD SD:", 10, 174, 2);
  tft.setTextColor(sdReady ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.drawString(sdReady ? "OK" : "FAIL", 110, 174, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Mega:", 10, 198, 2);
  tft.setTextColor(megaAlive ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
  tft.drawString(megaAlive ? "ONLINE" : "WAITING", 110, 198, 2);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  String rx = lastMegaMessage;
  if (rx.length() > 38) rx = rx.substring(0, 38);
  tft.drawString("RX: " + rx, 10, 224, 1);
}

// =========================================================
// Content type helper
// =========================================================

String getContentType(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".jpg")) return "image/jpeg";
  if (path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".gif")) return "image/gif";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".txt")) return "text/plain";
  if (path.endsWith(".shdo")) return "application/json";
  return "application/octet-stream";
}

// =========================================================
// SD file serving
// =========================================================

bool serveFileFromSD(String requestPath) {
  if (!sdReady) return false;

  if (requestPath == "/") {
    requestPath = "/www/index.html";
  } else {
    requestPath = "/www" + requestPath;
  }

  if (requestPath.endsWith("/")) {
    requestPath += "index.html";
  }

  if (!SD.exists(requestPath)) {
    Serial.print(F("[WEB] Missing file: "));
    Serial.println(requestPath);
    return false;
  }

  File file = SD.open(requestPath, FILE_READ);
  if (!file) return false;

  String contentType = getContentType(requestPath);
  server.streamFile(file, contentType);
  file.close();
  return true;
}

void sendMissingWebUIPage() {
  String html;
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Showduino WebUI Missing</title>");
  html += F("<style>body{font-family:Arial;background:#05070a;color:#fff;padding:24px}code{background:#111827;padding:3px 6px;border-radius:6px}.box{max-width:720px;margin:auto;border:1px solid #334155;border-radius:18px;padding:24px;background:#0f172a}</style>");
  html += F("</head><body><div class='box'><h1>Showduino WebUI Missing</h1>");
  html += F("<p>The CYD is running, but it could not find the WebUI files on the SD card.</p>");
  html += F("<p>Create this folder on the CYD SD card:</p><p><code>/www/</code></p>");
  html += F("<p>Then place your WebUI build inside it, including:</p><ul><li><code>/www/index.html</code></li><li><code>/www/app.js</code></li><li><code>/www/style.css</code></li></ul>");
  html += F("<p>API still works at <code>/api/status</code> and <code>/api/command?cmd=HEARTBEAT</code>.</p>");
  html += F("</div></body></html>");
  server.send(200, "text/html", html);
}

// =========================================================
// Mega serial helpers
// =========================================================

void sendToMega(const String& command) {
  Serial2.println(command);
  lastCommandSent = command;

  Serial.print(F("[CYD -> MEGA] "));
  Serial.println(command);

  setCydLed(false, false, true);
  delay(10);
  setCydLed(false, false, false);
}

void handleMegaLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print(F("[MEGA -> CYD] "));
  Serial.println(line);

  lastMegaMessage = line;

  if (line == "STATUS:ALIVE" || line == "STATUS:READY") {
    megaAlive = true;
    setCydLed(false, true, false);
  }

  if (line == "STATUS:EMERGENCY_ACTIVE") {
    megaAlive = true;
    setCydLed(true, false, false);
  }
}

void readMegaSerial() {
  while (Serial2.available() > 0) {
    char c = Serial2.read();

    if (c == '\r') continue;

    if (c == '\n') {
      handleMegaLine(megaBuffer);
      megaBuffer = "";
    } else {
      if (megaBuffer.length() < 180) {
        megaBuffer += c;
      } else {
        megaBuffer = "";
      }
    }
  }
}

// =========================================================
// API helpers
// =========================================================

void sendJson(const String& json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

String jsonEscape(const String& input) {
  String out = "";
  for (uint16_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "";
    else out += c;
  }
  return out;
}

void handleApiStatus() {
  String json = "{";
  json += "\"device\":\"Showduino CYD Director\",";
  json += "\"ap_ssid\":\"" + String(AP_SSID) + "\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"sd_ready\":" + String(sdReady ? "true" : "false") + ",";
  json += "\"mega_alive\":" + String(megaAlive ? "true" : "false") + ",";
  json += "\"last_command\":\"" + jsonEscape(lastCommandSent) + "\",";
  json += "\"last_mega_message\":\"" + jsonEscape(lastMegaMessage) + "\"";
  json += "}";
  sendJson(json);
}

void handleApiCommand() {
  if (!server.hasArg("cmd")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing cmd parameter\"}");
    return;
  }

  String command = server.arg("cmd");
  command.trim();

  if (command.length() == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty command\"}");
    return;
  }

  if (command.length() > 160) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"command too long\"}");
    return;
  }

  sendToMega(command);

  String json = "{";
  json += "\"ok\":true,";
  json += "\"sent\":\"" + jsonEscape(command) + "\",";
  json += "\"last_mega_message\":\"" + jsonEscape(lastMegaMessage) + "\"";
  json += "}";
  sendJson(json);
}

void handleApiDeploy() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing deploy body\"}");
    return;
  }

  String body = server.arg("plain");
  int sentCount = 0;
  int start = 0;

  while (start < body.length()) {
    int end = body.indexOf('\n', start);
    if (end < 0) end = body.length();

    String line = body.substring(start, end);
    line.trim();

    if (line.length() > 0) {
      sendToMega(line);
      sentCount++;
      delay(5);
    }

    start = end + 1;
  }

  sendJson("{\"ok\":true,\"lines_sent\":" + String(sentCount) + "}");
}

void handleApiListScenes() {
  if (!sdReady) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"SD not ready\"}");
    return;
  }

  if (!SD.exists("/scenes")) {
    sendJson("{\"ok\":true,\"files\":[]}");
    return;
  }

  File dir = SD.open("/scenes");
  if (!dir || !dir.isDirectory()) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"cannot open scenes folder\"}");
    return;
  }

  String json = "{\"ok\":true,\"files\":[";
  bool first = true;

  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.endsWith(".shdo") || name.endsWith(".json")) {
        if (!first) json += ",";
        json += "\"" + jsonEscape(name) + "\"";
        first = false;
      }
    }
    file.close();
    file = dir.openNextFile();
  }

  json += "]}";
  dir.close();
  sendJson(json);
}

void handleNotFound() {
  if (serveFileFromSD(server.uri())) {
    return;
  }

  if (server.uri() == "/" || server.uri() == "/index.html") {
    sendMissingWebUIPage();
    return;
  }

  server.send(404, "text/plain", "Showduino: file not found");
}

// =========================================================
// SD setup helpers
// =========================================================

void ensureFolder(const char* path) {
  if (!SD.exists(path)) {
    SD.mkdir(path);
  }
}

void setupSdCard() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, sdSPI);

  if (sdReady) {
    Serial.println(F("[SD] CYD SD card mounted"));
    ensureFolder("/www");
    ensureFolder("/scenes");
    ensureFolder("/shows");
    ensureFolder("/projects");
    ensureFolder("/assets");
    ensureFolder("/logs");
  } else {
    Serial.println(F("[SD] CYD SD card failed"));
  }
}

// =========================================================
// WiFi/web setup
// =========================================================

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, gatewayIP, subnetIP);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_HIDDEN, AP_MAX_CLIENTS);

  Serial.print(F("[WiFi] AP started: "));
  Serial.println(AP_SSID);
  Serial.print(F("[WiFi] IP: "));
  Serial.println(WiFi.softAPIP());

  if (MDNS.begin(MDNS_NAME)) {
    Serial.println(F("[mDNS] showduino.local started"));
  } else {
    Serial.println(F("[mDNS] failed"));
  }
}

void setupWebServer() {
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/command", HTTP_GET, handleApiCommand);
  server.on("/api/deploy", HTTP_POST, handleApiDeploy);
  server.on("/api/scenes", HTTP_GET, handleApiListScenes);

  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println(F("[WEB] Server started on port 80"));
}

// =========================================================
// Heartbeat
// =========================================================

void updateHeartbeat() {
  if (millis() - lastHeartbeatMs >= 5000) {
    lastHeartbeatMs = millis();
    sendToMega("HEARTBEAT");
  }
}

void updateScreen() {
  if (millis() - lastScreenRefreshMs >= 2500) {
    lastScreenRefreshMs = millis();
    drawStatusScreen();
  }
}

// =========================================================
// Arduino setup/loop
// =========================================================

void setup() {
  Serial.begin(USB_BAUD_RATE);
  Serial2.begin(MEGA_BAUD_RATE, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  pinMode(CYD_LED_RED, OUTPUT);
  pinMode(CYD_LED_GREEN, OUTPUT);
  pinMode(CYD_LED_BLUE, OUTPUT);
  setCydLed(false, false, false);

  tft.init();
  tft.setRotation(1);
  touch.begin();

  Serial.println();
  Serial.println(F("==============================================="));
  Serial.println(F("Showduino v1 - CYD Director WebUI SD Server"));
  Serial.println(F("==============================================="));

  setupSdCard();
  setupWiFi();
  setupWebServer();
  drawStatusScreen();

  sendToMega("HEARTBEAT");
}

void loop() {
  server.handleClient();
  readMegaSerial();
  updateHeartbeat();
  updateScreen();
}
