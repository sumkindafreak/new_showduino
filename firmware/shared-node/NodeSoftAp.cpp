#include "NodeSoftAp.h"

#include <string.h>
#include <WiFi.h>
#include <esp_wifi.h>

static bool sStarted = false;
static bool sReady = false;
static uint8_t sChannel = 1;
static char sSsid[33] = "";
static char sPass[32] = "showduino";
static char sIp[16] = "0.0.0.0";
static uint32_t sLastCheck = 0;
static NodeSoftApRadioHook sHook = nullptr;

static void logRadio(const char *when) {
  uint8_t ch = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&ch, &second);
  Serial.printf("[NODE-AP] %s mode=%d ch=%u ssid=%s ip=%s\n",
                when, (int)WiFi.getMode(), (unsigned)ch, sSsid, sIp);
}

static void stopStaScan() {
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  (void)esp_wifi_scan_stop();
}

void nodeSoftApLockChannel() {
  stopStaScan();
  (void)esp_wifi_set_channel(sChannel, WIFI_SECOND_CHAN_NONE);
}

static bool startAp() {
  stopStaScan();
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  stopStaScan();

  (void)esp_wifi_set_protocol(WIFI_IF_AP,
                              WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  (void)esp_wifi_set_protocol(WIFI_IF_STA,
                              WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  wifi_country_t country = {};
  memcpy(country.cc, "01", 2);
  country.schan = 1;
  country.nchan = 13;
  country.max_tx_power = 8;
  country.policy = WIFI_COUNTRY_POLICY_MANUAL;
  (void)esp_wifi_set_country(&country);

  delay(20);
  (void)esp_wifi_set_channel(sChannel, WIFI_SECOND_CHAN_NONE);

  /* Node AP must not collide with Comms Studio at 192.168.4.1. */
  IPAddress apIP(192, 168, 5, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  const bool ok = WiFi.softAP(sSsid, sPass, sChannel, false, 2);

  wifi_config_t conf = {};
  if (esp_wifi_get_config(WIFI_IF_AP, &conf) == ESP_OK) {
    conf.ap.channel = sChannel;
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    conf.ap.max_connection = 2;
    conf.ap.beacon_interval = 300;
    (void)esp_wifi_set_config(WIFI_IF_AP, &conf);
  }
  (void)esp_wifi_set_channel(sChannel, WIFI_SECOND_CHAN_NONE);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  stopStaScan();

  strncpy(sIp, WiFi.softAPIP().toString().c_str(), sizeof(sIp) - 1);
  sIp[sizeof(sIp) - 1] = 0;
  sReady = ok;
  sStarted = true;
  logRadio("start");
  if (!ok) Serial.println("[NODE-AP] SoftAP start FAILED");
  if (sHook) sHook();
  return ok;
}

void nodeSoftApSetRadioHook(NodeSoftApRadioHook fn) { sHook = fn; }

bool nodeSoftApBegin(const char *typeToken, const uint8_t mac[6],
                     uint8_t channel, const char *password) {
  sChannel = channel ? channel : 1;
  showduino_owner_ssid(typeToken, mac, sSsid, sizeof(sSsid));
  strncpy(sPass, (password && password[0]) ? password : "showduino", sizeof(sPass) - 1);
  sPass[sizeof(sPass) - 1] = 0;
  return startAp();
}

bool nodeSoftApBeginNamed(const char *ssid, uint8_t channel, const char *password) {
  sChannel = channel ? channel : 1;
  strncpy(sSsid, (ssid && ssid[0]) ? ssid : "Showduino-Lamp", sizeof(sSsid) - 1);
  sSsid[sizeof(sSsid) - 1] = 0;
  strncpy(sPass, (password && password[0]) ? password : "showduino", sizeof(sPass) - 1);
  sPass[sizeof(sPass) - 1] = 0;
  return startAp();
}

void nodeSoftApRetitle(const char *ssid) {
  if (!ssid || !ssid[0]) return;
  if (!strcmp(sSsid, ssid)) return;
  strncpy(sSsid, ssid, sizeof(sSsid) - 1);
  sSsid[sizeof(sSsid) - 1] = 0;
  if (!sStarted) return;
  Serial.printf("[NODE-AP] retitle ssid=%s (reassert ESP-NOW, no deinit)\n", sSsid);
  startAp();
}

void nodeSoftApFollowChannel(uint8_t channel) {
  if (channel < 1 || channel > 13) return;
  if (channel == sChannel && sStarted) return;
  sChannel = channel;
  if (!sStarted) return;
  (void)esp_wifi_set_channel(sChannel, WIFI_SECOND_CHAN_NONE);
  wifi_config_t conf = {};
  if (esp_wifi_get_config(WIFI_IF_AP, &conf) == ESP_OK) {
    conf.ap.channel = sChannel;
    (void)esp_wifi_set_config(WIFI_IF_AP, &conf);
  }
  Serial.printf("[NODE-AP] follow ch%u (no AP restart)\n", (unsigned)sChannel);
}

void nodeSoftApService() {
  if (!sStarted) return;
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;
  const uint32_t now = millis();
  if ((now - sLastCheck) < 2000UL) return;
  sLastCheck = now;

  uint8_t ch = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&ch, &second);
  wifi_mode_t mode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&mode);
  const bool apOn = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);

  if (!apOn) {
    Serial.println("[NODE-AP] AP gone — restarting on ESP-NOW channel");
    startAp();
    return;
  }
  if (ch != sChannel) {
    Serial.printf("[NODE-AP] channel %u -> %u (no AP restart)\n",
                  (unsigned)ch, (unsigned)sChannel);
    nodeSoftApLockChannel();
    if (sHook) sHook();
  }
}

bool nodeSoftApReady() { return sReady; }
bool nodeSoftApStarted() { return sStarted; }
const char *nodeSoftApSsid() { return sSsid; }
const char *nodeSoftApIp() { return sIp; }
const char *nodeSoftApPassword() { return sPass; }
uint8_t nodeSoftApChannel() { return sChannel; }
