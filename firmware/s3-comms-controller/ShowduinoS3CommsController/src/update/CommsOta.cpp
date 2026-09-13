#include "CommsOta.h"

#include "../../BoardConfig.h"
#include "../CommsUart.h"
#include "../EspNowTransport.h"
#include "../ProtocolBridge.h"
#include "../network/CommsGateway.h"
#include "../web/CommsWebServer.h"
#include "../../../protocol/showduino_gateway_wire.h"

#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <string.h>

#ifndef SHOWDUINO_OTA_TEST_FAIL_HEALTH
#define SHOWDUINO_OTA_TEST_FAIL_HEALTH 0
#endif
#ifndef SHOWDUINO_OTA_ALLOW_FORCE
#define SHOWDUINO_OTA_ALLOW_FORCE 0
#endif

static Preferences sTx;
static char sState[36] = SHOWDUINO_OTA_STATE_IDLE;
static char sError[48] = "";
static char sFrom[24] = "";
static char sTo[24] = "";
static char sUrl[192] = "";
static char sSha[SHOWDUINO_OTA_SHA256_HEX_LEN + 1] = "";
static uint32_t sSize = 0;
static uint32_t sGot = 0;
static uint32_t sValidateFromMs = 0;
static bool sPendingValidate = false;
static bool sRolledBack = false;
static bool sWantJob = false;
static bool sRebootArmed = false;
static uint32_t sRebootAtMs = 0;
static ShowduinoOtaCandidate sCand;

static void setState(const char *st) {
  showduino_update_copy(sState, sizeof(sState), st);
}

static void setError(const char *e) {
  showduino_update_copy(sError, sizeof(sError), e);
}

static void persistFlags() {
  sTx.begin("sdotx", false);
  sTx.putBool("pending", sPendingValidate);
  sTx.putBool("rolled", sRolledBack);
  sTx.putString("from", sFrom);
  sTx.putString("to", sTo);
  sTx.end();
}

static String jsonField(const String &body, const char *key) {
  String out;
  String k = String("\"") + key + "\"";
  int i = body.indexOf(k);
  if (i < 0) return out;
  int colon = body.indexOf(':', i + k.length());
  if (colon < 0) return out;
  int p = colon + 1;
  while (p < (int)body.length() && (body[p] == ' ' || body[p] == '\n')) p++;
  if (p < (int)body.length() && (body[p] == 't' || body[p] == 'f' ||
                                 (body[p] >= '0' && body[p] <= '9'))) {
    int e = p;
    while (e < (int)body.length() && body[e] != ',' && body[e] != '}') e++;
    out = body.substring(p, e);
    out.trim();
    return out;
  }
  int q1 = body.indexOf('"', colon);
  if (q1 < 0) return out;
  int q2 = q1 + 1;
  while (q2 < (int)body.length() && !(body[q2] == '"' && body[q2 - 1] != '\\')) q2++;
  if (q2 > q1) out = body.substring(q1 + 1, q2);
  return out;
}

static void hexSha(const uint8_t digest[32], char out[65]) {
  static const char *h = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out[i * 2] = h[(digest[i] >> 4) & 0xF];
    out[i * 2 + 1] = h[digest[i] & 0xF];
  }
  out[64] = 0;
}

static int shaEq(const char *a, const char *b) {
  if (!a || !b) return 0;
  for (int i = 0; i < 64; i++) {
    char ca = a[i], cb = b[i];
    if (ca >= 'A' && ca <= 'F') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'F') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return 0;
  }
  return a[64] == 0 && b[64] == 0;
}

static ShowduinoCommsHealth healthNow() {
  ShowduinoCommsHealth h;
  memset(&h, 0, sizeof(h));
  h.booted = 1;
  h.uart = commsUartReady() ? 1 : 0;
  h.p4_link = protocolBridgeP4Alive() ? 1 : 0;
  h.espnow = espNowTransportReady() ? 1 : 0;
  h.network = commsGatewayApOnline() ? 1 : 0;
  h.webui = commsWebReady() ? 1 : 0;
  h.fatal = commsWebFault() ? 1 : 0;
#if SHOWDUINO_OTA_TEST_FAIL_HEALTH
  h.fatal = 1;
#endif
  return h;
}

static void markValid() {
  esp_ota_mark_app_valid_cancel_rollback();
  sPendingValidate = false;
  sRolledBack = false;
  persistFlags();
  setState(SHOWDUINO_OTA_STATE_COMPLETE);
  Serial.printf("[OTA] COMMS UPDATE COMPLETE VERSION=%s\n", SHOWDUINO_COMMS_FIRMWARE_VERSION);
}

static void requestRollback() {
  Serial.println("[OTA] UPDATE ROLLED BACK — switching to previous slot");
  sPendingValidate = false;
  sRolledBack = true;
  persistFlags();
  if (Update.canRollBack()) {
    Update.rollBack();
  }
  delay(50);
  ESP.restart();
}

static bool ensureMaintenance() {
  if (protocolBridgeMaintenanceObserved()) return true;
  if (!protocolBridgeP4Alive() || !commsUartReady()) return false;
  commsUartWriteLine("UPDATE:MAINTENANCE:ON");
  const uint32_t t0 = millis();
  while ((millis() - t0) < 1600UL) {
    protocolBridgeLoop();
    if (protocolBridgeMaintenanceObserved()) return true;
    delay(15);
  }
  return protocolBridgeMaintenanceObserved();
}

void commsOtaBegin() {
  memset(&sCand, 0, sizeof(sCand));
  sTx.begin("sdotx", true);
  sPendingValidate = sTx.getBool("pending", false);
  sRolledBack = sTx.getBool("rolled", false);
  String from = sTx.getString("from", "");
  String to = sTx.getString("to", "");
  sTx.end();
  strncpy(sFrom, from.c_str(), sizeof(sFrom) - 1);
  strncpy(sTo, to.c_str(), sizeof(sTo) - 1);
  if (sPendingValidate) {
    setState(SHOWDUINO_OTA_STATE_PENDING);
    sValidateFromMs = millis();
    Serial.println("[OTA] candidate PENDING VALIDATION");
  } else if (sRolledBack) {
    setState(SHOWDUINO_OTA_STATE_ROLLED_BACK);
    Serial.println("[OTA] previous firmware restored after rollback");
    Serial.println("[OTA] UPDATE ROLLED BACK");
  } else {
    setState(SHOWDUINO_OTA_STATE_IDLE);
    /* USB-flashed or already-valid slot is the known-good rollback target. */
    esp_ota_mark_app_valid_cancel_rollback();
  }
}

void commsOtaLoop() {
  if (sRebootArmed && (int32_t)(millis() - sRebootAtMs) >= 0) {
    sRebootArmed = false;
    ESP.restart();
    return;
  }
  if (!sPendingValidate) return;
  const ShowduinoCommsHealth h = healthNow();
  if (showduino_comms_health_pass(&h)) {
    markValid();
    commsOtaPushDirector();
    return;
  }
  if (sValidateFromMs && (millis() - sValidateFromMs) > SHOWDUINO_COMMS_OTA_VALIDATE_MS) {
    setError("HEALTH_CHECK_FAILED");
    setState(SHOWDUINO_OTA_STATE_FAILED);
    commsOtaPushDirector();
    requestRollback();
  }
}

bool commsOtaBusy() {
  return strcmp(sState, SHOWDUINO_OTA_STATE_DOWNLOADING) == 0 ||
         strcmp(sState, SHOWDUINO_OTA_STATE_VERIFYING) == 0 ||
         strcmp(sState, SHOWDUINO_OTA_STATE_INSTALLING) == 0 ||
         sWantJob || sRebootArmed;
}

const char *commsOtaState() { return sState; }
const char *commsOtaError() { return sError; }

void commsOtaAppendStatusJson(String &json) {
  json += "  \"commsOta\": {\n";
  json += "    \"state\": \"";
  json += sState;
  json += "\",\n    \"installed\": \"" SHOWDUINO_COMMS_FIRMWARE_VERSION "\",\n";
  json += "    \"candidate\": \"";
  json += sTo[0] ? sTo : "";
  json += "\",\n    \"previous\": \"";
  json += sFrom[0] ? sFrom : "";
  json += "\",\n    \"hardwareId\": \"" SHOWDUINO_COMMS_HARDWARE_ID "\",\n";
  json += "    \"otaCapable\": true,\n";
  json += "    \"bytesReceived\": ";
  json += String((unsigned long)sGot);
  json += ",\n    \"totalBytes\": ";
  json += String((unsigned long)sSize);
  json += ",\n    \"percent\": ";
  if (sSize) json += String((unsigned)((sGot * 100UL) / sSize));
  else json += "null";
  json += ",\n    \"lastError\": \"";
  json += sError;
  json += "\",\n    \"pendingValidation\": ";
  json += sPendingValidate ? "true" : "false";
  json += ",\n    \"rolledBack\": ";
  json += sRolledBack ? "true" : "false";
  json += ",\n    \"validationTimeoutMs\": ";
  json += String((unsigned)SHOWDUINO_COMMS_OTA_VALIDATE_MS);
  json += "\n  }";
}

bool commsOtaParseApply(const String &body, ShowduinoOtaCandidate *c, String &url) {
  if (!c) return false;
  memset(c, 0, sizeof(*c));
  const String role = jsonField(body, "component");
  const String hw = jsonField(body, "hardwareId");
  const String fw = jsonField(body, "firmware");
  const String sha = jsonField(body, "sha256");
  const String file = jsonField(body, "filename");
  const String sz = jsonField(body, "size");
  const String confirm = jsonField(body, "confirm");
  const String force = jsonField(body, "force");
  url = jsonField(body, "url");
  showduino_update_copy(c->role, sizeof(c->role),
                        role.length() ? role.c_str() : "");
  showduino_update_copy(c->hardware_id, sizeof(c->hardware_id), hw.c_str());
  showduino_update_copy(c->firmware, sizeof(c->firmware), fw.c_str());
  showduino_update_copy(c->sha256, sizeof(c->sha256), sha.c_str());
  showduino_update_copy(c->filename, sizeof(c->filename), file.c_str());
  c->size = (uint32_t)sz.toInt();
  c->ota_capable = 1;
  c->confirm = (confirm == "true" || confirm == "1") ? 1 : 0;
#if SHOWDUINO_OTA_ALLOW_FORCE
  c->force = (force == "true" || force == "1") ? 1 : 0;
#else
  (void)force;
  c->force = 0;
#endif
  return c->role[0] != 0;
}

bool commsOtaRequestApply(const ShowduinoOtaCandidate &c, const char *url, String &err) {
  if (c.confirm && protocolBridgeP4Alive() && !protocolBridgeMaintenanceObserved()) {
    (void)ensureMaintenance();
  }
  const char *why = showduino_comms_ota_reject_reason(
      &c, SHOWDUINO_COMMS_FIRMWARE_VERSION, SHOWDUINO_COMMS_HARDWARE_ID,
      protocolBridgeShowRunning() ? 1 : 0,
      protocolBridgeEmergencyActive() ? 1 : 0,
      protocolBridgeMaintenanceObserved() ? 1 : 0,
      protocolBridgeP4Alive() ? 1 : 0);
  if (why) {
    err = why;
    return false;
  }
  if (!url || !url[0] || strncmp(url, "https://", 8) != 0) {
    err = "BAD_MANIFEST";
    return false;
  }
  if (commsOtaBusy() || sPendingValidate) {
    err = "BUSY";
    return false;
  }
  sCand = c;
  showduino_update_copy(sUrl, sizeof(sUrl), url);
  showduino_update_copy(sSha, sizeof(sSha), c.sha256);
  showduino_update_copy(sFrom, sizeof(sFrom), SHOWDUINO_COMMS_FIRMWARE_VERSION);
  showduino_update_copy(sTo, sizeof(sTo), c.firmware);
  sSize = c.size;
  sGot = 0;
  sError[0] = 0;
  sWantJob = true;
  setState(SHOWDUINO_OTA_STATE_DOWNLOADING);
  return true;
}

bool commsOtaWantsNetJob() { return sWantJob; }

void commsOtaRunNetJob() {
  sWantJob = false;
  if (protocolBridgeEmergencyActive()) {
    setError(SHOWDUINO_UPDATE_FAULT_EMERGENCY);
    setState(SHOWDUINO_OTA_STATE_EMERGENCY);
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(8);
  HTTPClient http;
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, sUrl)) {
    setError("DOWNLOAD_FAILED");
    setState(SHOWDUINO_OTA_STATE_FAILED);
    return;
  }
  const int code = http.GET();
  if (code != 200) {
    http.end();
    setError("DOWNLOAD_FAILED");
    setState(SHOWDUINO_OTA_STATE_FAILED);
    return;
  }
  const int clen = http.getSize();
  if (clen > 0 && (uint32_t)clen != sSize) {
    http.end();
    setError(SHOWDUINO_UPDATE_BLOCK_SIZE);
    setState(SHOWDUINO_OTA_STATE_FAILED);
    return;
  }
  if (!Update.begin(sSize, U_FLASH)) {
    http.end();
    setError("WRITE_FAILED");
    setState(SHOWDUINO_OTA_STATE_FAILED);
    return;
  }
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[1024];
  sGot = 0;
  while (sGot < sSize) {
    if (protocolBridgeEmergencyActive()) {
      Update.abort();
      mbedtls_sha256_free(&sha);
      http.end();
      setError(SHOWDUINO_UPDATE_FAULT_EMERGENCY);
      setState(SHOWDUINO_OTA_STATE_EMERGENCY);
      return;
    }
    const int avail = stream ? stream->available() : 0;
    if (avail <= 0) {
      if (!http.connected()) break;
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    int n = avail;
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    if (sGot + (uint32_t)n > sSize) n = (int)(sSize - sGot);
    n = stream->readBytes((char *)buf, n);
    if (n <= 0) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    mbedtls_sha256_update(&sha, buf, (size_t)n);
    if (Update.write(buf, (size_t)n) != (size_t)n) {
      Update.abort();
      mbedtls_sha256_free(&sha);
      http.end();
      setError("WRITE_FAILED");
      setState(SHOWDUINO_OTA_STATE_FAILED);
      return;
    }
    sGot += (uint32_t)n;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  http.end();
  setState(SHOWDUINO_OTA_STATE_VERIFYING);
  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  char gotHex[65];
  hexSha(digest, gotHex);
  if (!shaEq(gotHex, sSha)) {
    Update.abort();
    setError(SHOWDUINO_UPDATE_FAULT_INTEGRITY);
    setState(SHOWDUINO_OTA_STATE_FAILED);
    return;
  }
  setState(SHOWDUINO_OTA_STATE_INSTALLING);
  if (!Update.end(true)) {
    setError("WRITE_FAILED");
    setState(SHOWDUINO_OTA_STATE_FAILED);
    return;
  }
  sPendingValidate = true;
  sRolledBack = false;
  persistFlags();
  setState(SHOWDUINO_OTA_STATE_REBOOT);
  commsOtaPushDirector();
  Serial.println("[OTA] candidate written — COMMS RESTARTING");
  sRebootArmed = true;
  sRebootAtMs = millis() + 400;
}

void commsOtaPushDirector() {
  if (!espNowTransportHaveDirector()) return;
  char line[SHOWDUINO_DESK_COMMAND_MAX];
  snprintf(line, sizeof(line), SHOWDUINO_WIRE_STATE_UPDATE_PREFIX "COMMS:%s", sState);
  espNowTransportSendToDirector(line);
}
