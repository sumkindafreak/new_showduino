#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "../BoardConfig.h"

#if SHOWDUINO_WEBUI_ENABLED

namespace {

constexpr uint16_t kProvisionPort = 82;
constexpr const char *kPrefsNamespace = "showduino-net";
constexpr const char *kKeyEnabled = "enabled";
constexpr const char *kKeySsid = "ssid";
constexpr const char *kKeyPassword = "password";
constexpr uint32_t kReconnectIntervalMs = 15000UL;

WebServer gProvisionServer(kProvisionPort);
Preferences gNetworkPrefs;
bool gProvisionServerStarted = false;
bool gLanEnabled = false;
String gLanSsid;
String gLanPassword;
String gLastError;
uint32_t gLastReconnectAttemptMs = 0;

String jsonEscape(const String &value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '\\' || c == '"') out += '\\';
    if (c == '\n') {
      out += "\\n";
    } else if (c != '\r') {
      out += c;
    }
  }
  return out;
}

bool readJsonString(const String &json, const char *key, String &out) {
  const String pattern = String("\"") + key + "\"";
  const int keyPos = json.indexOf(pattern);
  if (keyPos < 0) return false;
  const int colon = json.indexOf(':', keyPos + pattern.length());
  if (colon < 0) return false;
  const int q1 = json.indexOf('"', colon + 1);
  if (q1 < 0) return false;
  int q2 = q1 + 1;
  bool escaped = false;
  while (q2 < (int)json.length()) {
    const char c = json[q2];
    if (c == '"' && !escaped) break;
    escaped = (c == '\\' && !escaped);
    if (c != '\\') escaped = false;
    ++q2;
  }
  if (q2 >= (int)json.length()) return false;
  out = json.substring(q1 + 1, q2);
  out.replace("\\\"", "\"");
  out.replace("\\\\", "\\");
  return true;
}

void sendCorsJson(int code, const String &body) {
  gProvisionServer.sendHeader("Access-Control-Allow-Origin", "*");
  gProvisionServer.sendHeader("Cache-Control", "no-store");
  gProvisionServer.send(code, "application/json", body);
}

void sendCorsOptions() {
  gProvisionServer.sendHeader("Access-Control-Allow-Origin", "*");
  gProvisionServer.sendHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
  gProvisionServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  gProvisionServer.send(204);
}

void loadPreferences() {
  if (!gNetworkPrefs.begin(kPrefsNamespace, true)) {
    gLastError = "preferences unavailable";
    return;
  }
  gLanEnabled = gNetworkPrefs.getBool(kKeyEnabled, false);
  gLanSsid = gNetworkPrefs.getString(kKeySsid, "");
  gLanPassword = gNetworkPrefs.getString(kKeyPassword, "");
  gNetworkPrefs.end();
}

bool savePreferences(bool enabled, const String &ssid, const String &password) {
  if (!gNetworkPrefs.begin(kPrefsNamespace, false)) return false;
  const bool ok = gNetworkPrefs.putBool(kKeyEnabled, enabled) == 1 &&
                  gNetworkPrefs.putString(kKeySsid, ssid) > 0 &&
                  gNetworkPrefs.putString(kKeyPassword, password) >= 0;
  gNetworkPrefs.end();
  if (ok) {
    gLanEnabled = enabled;
    gLanSsid = ssid;
    gLanPassword = password;
  }
  return ok;
}

void clearPreferences() {
  if (gNetworkPrefs.begin(kPrefsNamespace, false)) {
    gNetworkPrefs.clear();
    gNetworkPrefs.end();
  }
  gLanEnabled = false;
  gLanSsid = "";
  gLanPassword = "";
}

int findConfiguredNetworkChannel(const String &ssid, int32_t *rssiOut = nullptr) {
  const int count = WiFi.scanNetworks(false, true);
  if (count <= 0) {
    WiFi.scanDelete();
    return -1;
  }
  int channel = -1;
  int32_t bestRssi = -1000;
  for (int i = 0; i < count; ++i) {
    if (WiFi.SSID(i) == ssid && WiFi.RSSI(i) > bestRssi) {
      channel = WiFi.channel(i);
      bestRssi = WiFi.RSSI(i);
    }
  }
  WiFi.scanDelete();
  if (rssiOut) *rssiOut = bestRssi;
  return channel;
}

bool connectConfiguredLan(bool scanFirst) {
  if (!gLanEnabled || gLanSsid.length() == 0) {
    gLastError = "LAN not configured";
    return false;
  }

  if (scanFirst) {
    const int channel = findConfiguredNetworkChannel(gLanSsid);
    if (channel < 0) {
      gLastError = "configured Wi-Fi not found";
      return false;
    }
    if (channel != SHOWDUINO_ESPNOW_CHANNEL) {
      gLastError = String("Wi-Fi channel ") + channel +
                   " is incompatible with Showduino fabric channel " +
                   SHOWDUINO_ESPNOW_CHANNEL;
      return false;
    }
  }

  Serial.printf("[Network] joining LAN SSID '%s' on Showduino channel %u\n",
                gLanSsid.c_str(), (unsigned)SHOWDUINO_ESPNOW_CHANNEL);
  WiFi.setHostname(SHOWDUINO_WEBUI_MDNS);
  WiFi.begin(gLanSsid.c_str(), gLanPassword.c_str());
  gLastReconnectAttemptMs = millis();
  gLastError = "";
  return true;
}

String buildStatusJson() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const int staChannel = connected ? WiFi.channel() : 0;
  String json;
  json.reserve(768);
  json += '{';
  json += "\"mode\":\"";
  json += gLanEnabled ? "hybrid" : "ap";
  json += "\",";
  json += "\"fabricChannel\":" + String(SHOWDUINO_ESPNOW_CHANNEL) + ',';
  json += "\"provisionPort\":" + String(kProvisionPort) + ',';
  json += "\"hostname\":\"" SHOWDUINO_WEBUI_MDNS "\",";
  json += "\"ap\":{";
  json += "\"enabled\":true,";
  json += "\"ssid\":\"" SHOWDUINO_WEBUI_AP_SSID "\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"clients\":" + String(WiFi.softAPgetStationNum());
  json += "},";
  json += "\"sta\":{";
  json += "\"enabled\":" + String(gLanEnabled ? "true" : "false") + ',';
  json += "\"configured\":" + String(gLanSsid.length() ? "true" : "false") + ',';
  json += "\"connected\":" + String(connected ? "true" : "false") + ',';
  json += "\"ssid\":\"" + jsonEscape(gLanSsid) + "\",";
  json += "\"ip\":\"" + String(connected ? WiFi.localIP().toString() : "0.0.0.0") + "\",";
  json += "\"rssi\":" + String(connected ? WiFi.RSSI() : 0) + ',';
  json += "\"channel\":" + String(staChannel) + ',';
  json += "\"compatible\":" + String(!connected || staChannel == SHOWDUINO_ESPNOW_CHANNEL ? "true" : "false");
  json += "},";
  json += "\"lastError\":\"" + jsonEscape(gLastError) + "\"";
  json += '}';
  return json;
}

void handleStatus() {
  sendCorsJson(200, buildStatusJson());
}

void handleScan() {
  const int count = WiFi.scanNetworks(false, true);
  String json = "{\"fabricChannel\":" + String(SHOWDUINO_ESPNOW_CHANNEL) + ",\"networks\":[";
  bool first = true;
  for (int i = 0; i < count; ++i) {
    if (WiFi.SSID(i).length() == 0) continue;
    if (!first) json += ',';
    first = false;
    const int ch = WiFi.channel(i);
    json += '{';
    json += "\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ',';
    json += "\"channel\":" + String(ch) + ',';
    json += "\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + ',';
    json += "\"compatible\":" + String(ch == SHOWDUINO_ESPNOW_CHANNEL ? "true" : "false");
    json += '}';
  }
  json += "]}";
  WiFi.scanDelete();
  sendCorsJson(200, json);
}

void handleSave() {
  String body = gProvisionServer.arg("plain");
  if (body.length() == 0 && gProvisionServer.args() > 0) body = gProvisionServer.arg(0);

  String ssid;
  String password;
  if (!readJsonString(body, "ssid", ssid) || ssid.length() == 0 || ssid.length() > 32) {
    sendCorsJson(400, "{\"error\":\"valid ssid required\"}");
    return;
  }
  readJsonString(body, "password", password);
  if (password.length() > 0 && password.length() < 8) {
    sendCorsJson(400, "{\"error\":\"Wi-Fi password must be at least 8 characters\"}");
    return;
  }

  int32_t rssi = 0;
  const int channel = findConfiguredNetworkChannel(ssid, &rssi);
  if (channel < 0) {
    sendCorsJson(409, "{\"error\":\"network not found; use Scan and try again\"}");
    return;
  }
  if (channel != SHOWDUINO_ESPNOW_CHANNEL) {
    String error = "{\"error\":\"network uses channel ";
    error += channel;
    error += "; Showduino currently requires channel ";
    error += SHOWDUINO_ESPNOW_CHANNEL;
    error += " to preserve ESP-NOW control\"}";
    sendCorsJson(409, error);
    return;
  }

  if (!savePreferences(true, ssid, password)) {
    sendCorsJson(500, "{\"error\":\"failed to save network settings\"}");
    return;
  }

  WiFi.disconnect(false, false);
  delay(50);
  connectConfiguredLan(false);
  sendCorsJson(202, buildStatusJson());
}

void handleReconnect() {
  if (!connectConfiguredLan(true)) {
    sendCorsJson(409, buildStatusJson());
    return;
  }
  sendCorsJson(202, buildStatusJson());
}

void handleClear() {
  clearPreferences();
  WiFi.disconnect(false, true);
  gLastError = "";
  sendCorsJson(200, buildStatusJson());
}

const char kSetupPage[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Showduino Network Setup</title><style>
body{margin:0;background:#0a0a0a;color:#e8e8e8;font:15px system-ui,sans-serif}.wrap{max-width:720px;margin:40px auto;padding:20px}.card{background:#141414;border:1px solid #2a2a2a;border-radius:8px;padding:20px;margin-bottom:16px}h1{color:#c41e1e}input,select,button{width:100%;box-sizing:border-box;margin:6px 0;padding:11px;border-radius:6px;border:1px solid #333;background:#0a0a0a;color:#eee}button{cursor:pointer;background:#c41e1e;border:0;font-weight:700}.secondary{background:#242424}.warn{color:#f39c12}.ok{color:#2ecc71}small{color:#888}</style></head>
<body><div class="wrap"><h1>Showduino Network Setup</h1><div class="card"><p>The Showduino-Studio fallback access point always remains available.</p><p class="warn">For this firmware, the local router must use Wi-Fi channel 1 so ESP-NOW remains reliable.</p><button onclick="scan()" class="secondary">Scan compatible networks</button><select id="ssid"><option>Scan first</option></select><input id="pass" type="password" placeholder="Wi-Fi password"><button onclick="save()">Save & Connect</button><button onclick="clearCfg()" class="secondary">Use Showduino AP only</button></div><div class="card"><pre id="status">Loading…</pre></div></div>
<script>
const api=location.origin;async function status(){const r=await fetch(api+'/api/network/config');const j=await r.json();document.getElementById('status').textContent=JSON.stringify(j,null,2)}
async function scan(){const r=await fetch(api+'/api/network/scan');const j=await r.json();const s=document.getElementById('ssid');s.innerHTML='';j.networks.filter(n=>n.compatible).forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm, ch '+n.channel+')';s.append(o)});if(!s.options.length)s.innerHTML='<option>No compatible channel-1 networks found</option>'}
async function save(){const r=await fetch(api+'/api/network/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:document.getElementById('ssid').value,password:document.getElementById('pass').value})});const j=await r.json();if(!r.ok)alert(j.error||'Connect failed');status()}
async function clearCfg(){await fetch(api+'/api/network/config',{method:'DELETE'});status()}status();setInterval(status,3000);
</script></body></html>)HTML";

void handleRoot() {
  gProvisionServer.sendHeader("Cache-Control", "no-store");
  gProvisionServer.send_P(200, "text/html", kSetupPage);
}

void beginProvisionServer() {
  if (gProvisionServerStarted) return;
  gProvisionServer.on("/", HTTP_GET, handleRoot);
  gProvisionServer.on("/api/network/config", HTTP_GET, handleStatus);
  gProvisionServer.on("/api/network/config", HTTP_POST, handleSave);
  gProvisionServer.on("/api/network/config", HTTP_DELETE, handleClear);
  gProvisionServer.on("/api/network/config", HTTP_OPTIONS, sendCorsOptions);
  gProvisionServer.on("/api/network/scan", HTTP_GET, handleScan);
  gProvisionServer.on("/api/network/scan", HTTP_OPTIONS, sendCorsOptions);
  gProvisionServer.on("/api/network/reconnect", HTTP_POST, handleReconnect);
  gProvisionServer.on("/api/network/reconnect", HTTP_OPTIONS, sendCorsOptions);
  gProvisionServer.onNotFound([]() { sendCorsJson(404, "{\"error\":\"not found\"}"); });
  gProvisionServer.begin();
  gProvisionServerStarted = true;
  Serial.printf("[Network] provisioning service ready on port %u\n", (unsigned)kProvisionPort);
}

void provisioningTask(void *) {
  delay(1000);
  loadPreferences();
  beginProvisionServer();
  if (gLanEnabled && gLanSsid.length()) connectConfiguredLan(true);

  for (;;) {
    gProvisionServer.handleClient();

    if (gLanEnabled && WiFi.status() == WL_CONNECTED &&
        WiFi.channel() != SHOWDUINO_ESPNOW_CHANNEL) {
      gLastError = String("connected router moved to incompatible channel ") + WiFi.channel();
      WiFi.disconnect(false, false);
    }

    if (gLanEnabled && WiFi.status() != WL_CONNECTED &&
        (millis() - gLastReconnectAttemptMs) >= kReconnectIntervalMs) {
      connectConfiguredLan(true);
    }

    delay(5);
  }
}

}  // namespace

/*
 * Arduino-ESP32 provides initVariant() as a weak hook. Defining it here lets the
 * network provisioning service initialise without changing the existing C3
 * sketch or WebServerManager. The main Showduino web server remains on port 80;
 * this isolated setup service uses port 82 and the same C3 network interfaces.
 */
void initVariant() {
  xTaskCreate(provisioningTask, "showduino-net", 6144, nullptr, 1, nullptr);
}

#endif  // SHOWDUINO_WEBUI_ENABLED
