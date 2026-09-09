#include "CommsGateway.h"

#include "../EspNowTransport.h"
#include "../ProtocolBridge.h"
#include "../web/CommsWebServer.h"
#include "../../BoardConfig.h"
#include "../../../protocol/showduino_version.h"
#include "../../../protocol/showduino_radio.h"
#include "../../../protocol/showduino_gateway_wire.h"
#include "../../../protocol/showduino_log.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <string.h>

static Preferences sPrefs;
static CommsWifiMode sMode = COMMS_WIFI_AP_ONLY;
static char sSsid[33] = "";
static char sPass[65] = "";
static bool sHasPass = false;
static bool sStaWanted = false;
static bool sStaAssociated = false;
static bool sGotIp = false;
static uint8_t sRadioCh = SHOWDUINO_RADIO_HOME_CHANNEL;
static uint32_t sLastStaTryMs = 0;
static uint32_t sStaBackoffMs = 15000;
static uint32_t sDisconnectSettleMs = 0;
static uint32_t sLastDirectorPushMs = 0;
static uint32_t sLastRadioLogMs = 0;

static volatile uint8_t sNetJob = 0; /* 0 idle, 1 probe, 2 github, 3 scan */
static bool sInternet = false;
static bool sInternetKnown = false;
static bool sAutoCheckArmed = false;

static char sScanJson[1400] = "{\"state\":\"idle\",\"networks\":[]}";
static char sLatestTag[32] = "";
static char sLatestName[48] = "";
static char sLatestNotes[512] = "";
static char sLatestUrl[128] = "";
static CommsUpdateStatus sUpdateStatus = COMMS_UPDATE_NEVER;
static uint32_t sLastCheckMs = 0;
static bool sLastCheckOk = false;
static bool sChecking = false;

static const char *modeWord(CommsWifiMode m) {
  return m == COMMS_WIFI_AP_STA ? "ap_sta" : "ap_only";
}

static void jsonEscape(const char *in, char *out, size_t cap) {
  if (!out || cap == 0) return;
  size_t used = 0;
  if (!in) {
    out[0] = '\0';
    return;
  }
  while (*in && used + 2 < cap) {
    char c = *in++;
    if (c == '"' || c == '\\') {
      if (used + 3 >= cap) break;
      out[used++] = '\\';
      out[used++] = c;
    } else if ((unsigned char)c < 0x20) {
      out[used++] = ' ';
    } else {
      out[used++] = c;
    }
  }
  out[used] = '\0';
}

static bool jsonGetString(const String &body, const char *key, String &out) {
  String k = String("\"") + key + "\"";
  int i = body.indexOf(k);
  if (i < 0) return false;
  int colon = body.indexOf(':', i + k.length());
  int q1 = body.indexOf('"', colon);
  if (q1 < 0) return false;
  int q2 = q1 + 1;
  while (q2 < (int)body.length()) {
    if (body[q2] == '"' && body[q2 - 1] != '\\') break;
    q2++;
  }
  if (q2 >= (int)body.length()) return false;
  out = body.substring(q1 + 1, q2);
  out.replace("\\\"", "\"");
  return true;
}

static uint8_t currentRadioChannel() {
  uint8_t ch = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&ch, &second);
  if (ch >= SHOWDUINO_RADIO_CHANNEL_MIN && ch <= SHOWDUINO_RADIO_CHANNEL_MAX) {
    sRadioCh = ch;
  }
  return sRadioCh;
}

static uint32_t peerCount() {
  esp_now_peer_num_t num = {};
  if (esp_now_get_peer_num(&num) != ESP_OK) return 0;
  return (uint32_t)num.total_num;
}

static void savePrefs() {
  sPrefs.begin("sdnet", false);
  sPrefs.putUChar("mode", (uint8_t)sMode);
  sPrefs.putString("ssid", sSsid);
  sPrefs.putString("pass", sPass);
  sPrefs.putBool("hasPass", sHasPass);
  sPrefs.end();
}

static void loadPrefs() {
  sPrefs.begin("sdnet", true);
  sMode = (CommsWifiMode)sPrefs.getUChar("mode", COMMS_WIFI_AP_ONLY);
  String ssid = sPrefs.getString("ssid", "");
  String pass = sPrefs.getString("pass", "");
  sHasPass = sPrefs.getBool("hasPass", false);
  sPrefs.end();
  strncpy(sSsid, ssid.c_str(), sizeof(sSsid) - 1);
  strncpy(sPass, pass.c_str(), sizeof(sPass) - 1);
  if (sMode == COMMS_WIFI_AP_STA && sSsid[0]) sStaWanted = true;
}

static void applyHomeChannel() {
  if (sStaAssociated) return;
  uint8_t ch = currentRadioChannel();
  if (ch == SHOWDUINO_RADIO_HOME_CHANNEL) return;
  Serial.printf("[RADIO] STA idle — restore home ch%u (was %u)\n",
                (unsigned)SHOWDUINO_RADIO_HOME_CHANNEL, (unsigned)ch);
  (void)esp_wifi_set_channel(SHOWDUINO_RADIO_HOME_CHANNEL, WIFI_SECOND_CHAN_NONE);
  currentRadioChannel();
}

static void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  (void)info;
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      sStaAssociated = true;
      sDisconnectSettleMs = 0;
      Serial.println("[RADIO] STA associated");
      commsGatewayLogRadio("sta_connected");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      sGotIp = true;
      sStaAssociated = true;
      sInternetKnown = false;
      sAutoCheckArmed = true;
      currentRadioChannel();
      Serial.printf("[RADIO] STA IP %s ch=%u\n",
                    WiFi.localIP().toString().c_str(),
                    (unsigned)sRadioCh);
      commsGatewayLogRadio("sta_got_ip");
      if (sNetJob == 0) sNetJob = 1;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      sStaAssociated = false;
      sGotIp = false;
      sInternet = false;
      sDisconnectSettleMs = millis();
      Serial.println("[RADIO] STA disconnected");
      commsGatewayLogRadio("sta_disconnected");
      break;
    default:
      break;
  }
}

static void beginSta() {
  if (!sStaWanted || sMode != COMMS_WIFI_AP_STA || !sSsid[0]) return;
  sLastStaTryMs = millis();
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  Serial.printf("[RADIO] STA begin ssid=%s (password not logged)\n", sSsid);
  if (sHasPass && sPass[0]) WiFi.begin(sSsid, sPass);
  else WiFi.begin(sSsid);
}

static void doScanJob() {
  strncpy(sScanJson, "{\"state\":\"scanning\",\"networks\":[]}", sizeof(sScanJson) - 1);
  int16_t n = WiFi.scanNetworks(false, false, false, 120);
  String json = "{\"state\":\"ready\",\"networks\":[";
  int used = 0;
  for (int16_t i = 0; i < n && used < 16; ++i) {
    char ssidEsc[72];
    jsonEscape(WiFi.SSID(i).c_str(), ssidEsc, sizeof(ssidEsc));
    if (used) json += ",";
    json += "{\"ssid\":\"";
    json += ssidEsc;
    json += "\",\"rssi\":";
    json += String((int)WiFi.RSSI(i));
    json += ",\"channel\":";
    json += String((int)WiFi.channel(i));
    json += ",\"secure\":";
    json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true";
    json += "}";
    used++;
  }
  json += "]}";
  strncpy(sScanJson, json.c_str(), sizeof(sScanJson) - 1);
  sScanJson[sizeof(sScanJson) - 1] = '\0';
  WiFi.scanDelete();
  if (!sStaAssociated) applyHomeChannel();
  commsGatewayLogRadio("scan_done");
}

static void doProbeJob() {
  if (!sGotIp) {
    sInternet = false;
    sInternetKnown = true;
    return;
  }
  IPAddress ip;
  const int ok = WiFi.hostByName("api.github.com", ip);
  sInternet = ok == 1;
  sInternetKnown = true;
  Serial.printf("[NET] internet %s\n", sInternet ? "ONLINE" : "OFFLINE");
}

static bool extractRelease(const String &payload) {
  int pos = 0;
  while (true) {
    int tagAt = payload.indexOf("\"tag_name\"", pos);
    if (tagAt < 0) return false;
    String obj = payload.substring(tagAt, tagAt + 1800);
    if (obj.indexOf("\"draft\":true") >= 0 || obj.indexOf("\"draft\": true") >= 0) {
      pos = tagAt + 10;
      continue;
    }
    String tag, name, body, url;
    if (!jsonGetString(obj, "tag_name", tag)) {
      pos = tagAt + 10;
      continue;
    }
    jsonGetString(obj, "name", name);
    jsonGetString(obj, "body", body);
    jsonGetString(obj, "html_url", url);
    strncpy(sLatestTag, showduino_version_skip_v(tag.c_str()), sizeof(sLatestTag) - 1);
    strncpy(sLatestName, name.length() ? name.c_str() : tag.c_str(), sizeof(sLatestName) - 1);
    strncpy(sLatestUrl, url.c_str(), sizeof(sLatestUrl) - 1);
    body.replace("\r", " ");
    body.replace("\n", " ");
    if (body.length() > 480) body = body.substring(0, 480);
    strncpy(sLatestNotes, body.c_str(), sizeof(sLatestNotes) - 1);
    return true;
  }
}

static void doGithubJob() {
  sChecking = true;
  if (!sGotIp) {
    sUpdateStatus = COMMS_UPDATE_OFFLINE;
    sChecking = false;
    sInternet = false;
    sInternetKnown = true;
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(4);
  HTTPClient http;
  http.setTimeout(4000);
  if (!http.begin(client, SHOWDUINO_GITHUB_RELEASES_API)) {
    sUpdateStatus = COMMS_UPDATE_FAILED;
    sChecking = false;
    return;
  }
  http.addHeader("User-Agent", "Showduino/" SHOWDUINO_PLATFORM_VERSION);
  http.addHeader("Accept", "application/vnd.github+json");
  const int code = http.GET();
  String payload;
  if (code > 0) {
    WiFiClient *stream = http.getStreamPtr();
    payload.reserve(12288);
    const uint32_t deadline = millis() + 4000;
    while (http.connected() && payload.length() < 12000 &&
           (int32_t)(millis() - deadline) < 0) {
      while (stream && stream->available() && payload.length() < 12000) {
        payload += (char)stream->read();
      }
      delay(1);
    }
  }
  http.end();
  sLastCheckMs = millis();
  if (code != 200) {
    sLastCheckOk = false;
    if (code < 0) {
      sInternet = false;
      sInternetKnown = true;
      sUpdateStatus = COMMS_UPDATE_OFFLINE;
    } else {
      sUpdateStatus = COMMS_UPDATE_FAILED;
    }
    sChecking = false;
    Serial.printf("[UPDATE] GitHub HTTP %d\n", code);
    return;
  }
  sInternet = true;
  sInternetKnown = true;
  sLastCheckOk = true;
  if (!extractRelease(payload)) {
    sUpdateStatus = COMMS_UPDATE_NONE;
    sChecking = false;
    Serial.println("[UPDATE] no GitHub Releases published");
    return;
  }
  const int cmp = showduino_version_compare(sLatestTag, SHOWDUINO_PLATFORM_VERSION);
  sUpdateStatus = (cmp > 0) ? COMMS_UPDATE_AVAILABLE : COMMS_UPDATE_CURRENT;
  sChecking = false;
  Serial.printf("[UPDATE] installed=%s latest=%s status=%d\n",
                SHOWDUINO_PLATFORM_VERSION, sLatestTag, (int)sUpdateStatus);
}

static void netTask(void *) {
  for (;;) {
    const uint8_t job = sNetJob;
    if (job == 1) {
      doProbeJob();
      if (sNetJob == 1) {
        if (sAutoCheckArmed && sInternet) {
          sAutoCheckArmed = false;
          sNetJob = 2;
        } else {
          sNetJob = 0;
        }
      }
    } else if (job == 2) {
      doGithubJob();
      if (sNetJob == 2) sNetJob = 0;
    } else if (job == 3) {
      doScanJob();
      if (sNetJob == 3) sNetJob = 0;
    } else {
      vTaskDelay(pdMS_TO_TICKS(40));
    }
  }
}

void commsGatewayLogRadio(const char *reason) {
  uint8_t staMac[6] = {0};
  uint8_t apMac[6] = {0};
  espNowTransportReadStaMac(staMac);
  (void)esp_read_mac(apMac, ESP_MAC_WIFI_SOFTAP);
  currentRadioChannel();
  Serial.printf(
      "[RADIO] %s mode=%s sta=%s ssid=%s staCh=%u ap=%s apCh=%u espnow=%u "
      "apMac=%02X:%02X:%02X:%02X:%02X:%02X staMac=%02X:%02X:%02X:%02X:%02X:%02X "
      "peers=%lu director=%s audio=%s lamp=%s internet=%s\n",
      reason ? reason : "-",
      modeWord(sMode),
      sStaAssociated ? "CONNECTED" : "IDLE",
      sSsid[0] ? sSsid : "-",
      (unsigned)sRadioCh,
      WiFi.softAPgetStationNum() || WiFi.softAPIP().toString() != "0.0.0.0" ? "ONLINE" : "OFFLINE",
      (unsigned)sRadioCh,
      (unsigned)sRadioCh,
      apMac[0], apMac[1], apMac[2], apMac[3], apMac[4], apMac[5],
      staMac[0], staMac[1], staMac[2], staMac[3], staMac[4], staMac[5],
      (unsigned long)peerCount(),
      protocolBridgeDirectorOnline() ? "YES" : "NO",
      espNowTransportHaveAudioNode() ? "YES" : "NO",
      espNowTransportHaveLampNode() ? "YES" : "NO",
      sInternet ? "ONLINE" : (sInternetKnown ? "OFFLINE" : "UNKNOWN"));
}

void commsGatewayPushDirectorWires() {
  if (!espNowTransportHaveDirector()) return;
  char line[SHOWDUINO_DESK_COMMAND_MAX];
  if (showduino_gateway_format(line, sizeof(line),
                               commsGatewayApOnline() ? 1 : 0,
                               sStaAssociated ? 1 : 0,
                               sInternet ? 1 : 0,
                               currentRadioChannel())) {
    espNowTransportSendToDirector(line);
  }
  const char *st = SHOWDUINO_UPDATE_NONE;
  const char *latest = nullptr;
  switch (sUpdateStatus) {
    case COMMS_UPDATE_CURRENT: st = SHOWDUINO_UPDATE_CURRENT; break;
    case COMMS_UPDATE_AVAILABLE:
      st = SHOWDUINO_UPDATE_AVAILABLE;
      latest = sLatestTag;
      break;
    case COMMS_UPDATE_OFFLINE: st = SHOWDUINO_UPDATE_OFFLINE; break;
    case COMMS_UPDATE_FAILED: st = SHOWDUINO_UPDATE_FAILED; break;
    case COMMS_UPDATE_NEVER: st = SHOWDUINO_UPDATE_NONE; break;
    default: break;
  }
  if (showduino_update_format(line, sizeof(line), st, latest)) {
    espNowTransportSendToDirector(line);
  }
}

void commsGatewayBegin() {
  loadPrefs();
  WiFi.onEvent(onWiFiEvent);
  xTaskCreatePinnedToCore(netTask, "sdnet", 12288, nullptr, 1, nullptr, 1);
  if (sStaWanted) beginSta();
  commsGatewayLogRadio("boot");
}

void commsGatewayLoop() {
  currentRadioChannel();
  const uint32_t now = millis();

  if (!sStaAssociated && sDisconnectSettleMs &&
      (now - sDisconnectSettleMs) > 3000UL) {
    sDisconnectSettleMs = 0;
    applyHomeChannel();
    commsGatewayLogRadio("home_channel");
  }

  if (sStaWanted && sMode == COMMS_WIFI_AP_STA && !sStaAssociated &&
      sNetJob != 3 &&
      (now - sLastStaTryMs) >= sStaBackoffMs) {
    beginSta();
    if (sStaBackoffMs < 60000UL) sStaBackoffMs += 15000UL;
  }
  if (sStaAssociated) sStaBackoffMs = 15000UL;

  if (now - sLastDirectorPushMs >= 4000UL) {
    sLastDirectorPushMs = now;
    commsGatewayPushDirectorWires();
  }
  if (now - sLastRadioLogMs >= 30000UL) {
    sLastRadioLogMs = now;
    commsGatewayLogRadio("periodic");
  }
}

bool commsGatewayStaAssociated() { return sStaAssociated; }
uint8_t commsGatewayRadioChannel() { return currentRadioChannel(); }
uint8_t commsGatewayTargetChannel() {
  if (sStaAssociated) return currentRadioChannel();
  return SHOWDUINO_RADIO_HOME_CHANNEL;
}
bool commsGatewayApOnline() {
  wifi_mode_t mode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&mode);
  return mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
}
bool commsGatewayInternetOnline() { return sInternet; }
CommsWifiMode commsGatewayMode() { return sMode; }
const char *commsGatewayStaSsid() { return sSsid; }
const char *commsGatewayStaIp() {
  static char ip[16] = "0.0.0.0";
  if (sGotIp) strncpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip) - 1);
  else strncpy(ip, "0.0.0.0", sizeof(ip) - 1);
  return ip;
}
int commsGatewayRssi() { return sStaAssociated ? WiFi.RSSI() : 0; }
bool commsGatewayPasswordConfigured() { return sHasPass; }

bool commsGatewaySetMode(CommsWifiMode mode) {
  sMode = mode;
  sStaWanted = (mode == COMMS_WIFI_AP_STA) && sSsid[0];
  savePrefs();
  if (!sStaWanted) {
    WiFi.disconnect(false, false);
    sStaAssociated = false;
    sGotIp = false;
    applyHomeChannel();
  } else {
    beginSta();
  }
  commsGatewayLogRadio("mode");
  return true;
}

bool commsGatewayConnect(const char *ssid, const char *password) {
  if (!ssid || !ssid[0] || strlen(ssid) > 32) return false;
  strncpy(sSsid, ssid, sizeof(sSsid) - 1);
  if (password && password[0]) {
    if (strlen(password) > 63) return false;
    strncpy(sPass, password, sizeof(sPass) - 1);
    sHasPass = true;
  } else if (!sHasPass) {
    sPass[0] = '\0';
    sHasPass = false;
  }
  sMode = COMMS_WIFI_AP_STA;
  sStaWanted = true;
  sStaBackoffMs = 15000;
  savePrefs();
  beginSta();
  commsGatewayLogRadio("connect");
  return true;
}

void commsGatewayDisconnect() {
  sStaWanted = false;
  sMode = COMMS_WIFI_AP_ONLY;
  savePrefs();
  WiFi.disconnect(false, false);
  sStaAssociated = false;
  sGotIp = false;
  applyHomeChannel();
  commsGatewayLogRadio("disconnect");
}

void commsGatewayForget() {
  sSsid[0] = '\0';
  sPass[0] = '\0';
  sHasPass = false;
  commsGatewayDisconnect();
}

bool commsGatewayScanStart() {
  if (sNetJob != 0) return false;
  strncpy(sScanJson, "{\"state\":\"scanning\",\"networks\":[]}", sizeof(sScanJson) - 1);
  sNetJob = 3;
  return true;
}

const char *commsGatewayScanJson() { return sScanJson; }

void commsGatewayRequestUpdateCheck() {
  if (sNetJob == 0) sNetJob = 2;
}

void commsGatewayAppendStatusJson(String &json) {
  currentRadioChannel();
  char ssidEsc[72];
  jsonEscape(sSsid, ssidEsc, sizeof(ssidEsc));
  json += "  \"gateway\": {\n";
  json += "    \"mode\": \"";
  json += modeWord(sMode);
  json += "\",\n";
  json += "    \"apOnline\": ";
  json += commsGatewayApOnline() ? "true" : "false";
  json += ",\n    \"apSsid\": \"" SHOWDUINO_RADIO_AP_SSID "\",\n";
  json += "    \"apIp\": \"";
  json += commsWebIp();
  json += "\",\n    \"staState\": \"";
  json += sStaAssociated ? "connected" : (sStaWanted ? "connecting" : "idle");
  json += "\",\n    \"staSsid\": \"";
  json += ssidEsc;
  json += "\",\n    \"staIp\": \"";
  json += commsGatewayStaIp();
  json += "\",\n    \"rssi\": ";
  json += String(commsGatewayRssi());
  json += ",\n    \"staChannel\": ";
  json += String((unsigned)sRadioCh);
  json += ",\n    \"radioChannel\": ";
  json += String((unsigned)sRadioCh);
  json += ",\n    \"espnowChannel\": ";
  json += String((unsigned)sRadioCh);
  json += ",\n    \"internet\": \"";
  json += sInternet ? "online" : (sInternetKnown ? "offline" : "unknown");
  json += "\",\n    \"passwordConfigured\": ";
  json += sHasPass ? "true" : "false";
  json += ",\n    \"peerCount\": ";
  json += String((unsigned long)peerCount());
  json += "\n  }";
}

void commsGatewayUpdatesJson(String &json) {
  const char *status = "never_checked";
  switch (sUpdateStatus) {
    case COMMS_UPDATE_CURRENT: status = "up_to_date"; break;
    case COMMS_UPDATE_AVAILABLE: status = "update_available"; break;
    case COMMS_UPDATE_OFFLINE: status = "offline"; break;
    case COMMS_UPDATE_FAILED: status = "check_failed"; break;
    case COMMS_UPDATE_NONE: status = "no_releases"; break;
    default: break;
  }
  char notesEsc[560];
  jsonEscape(sLatestNotes, notesEsc, sizeof(notesEsc));
  json = "{\n";
  json += "  \"productName\": \"" SHOWDUINO_PRODUCT_NAME "\",\n";
  json += "  \"installed\": \"" SHOWDUINO_PLATFORM_VERSION "\",\n";
  json += "  \"latest\": ";
  if (sLatestTag[0]) {
    json += "\"";
    json += sLatestTag;
    json += "\"";
  } else {
    json += "null";
  }
  json += ",\n  \"status\": \"";
  json += status;
  json += "\",\n  \"internet\": \"";
  json += sInternet ? "online" : (sInternetKnown ? "offline" : "unknown");
  json += "\",\n  \"checking\": ";
  json += sChecking ? "true" : "false";
  json += ",\n  \"lastCheckAgeMs\": ";
  if (sLastCheckMs) json += String((unsigned long)(millis() - sLastCheckMs));
  else json += "null";
  json += ",\n  \"lastCheckOk\": ";
  json += sLastCheckOk ? "true" : "false";
  json += ",\n  \"releaseName\": \"";
  json += sLatestName;
  json += "\",\n  \"releaseNotes\": \"";
  json += notesEsc;
  json += "\",\n  \"htmlUrl\": \"";
  json += sLatestUrl;
  json += "\",\n  \"otaInstall\": false,\n";
  json += "  \"note\": \"Update discovery only. Installation is not performed automatically. Internet is optional.\"\n";
  json += "}\n";
}
