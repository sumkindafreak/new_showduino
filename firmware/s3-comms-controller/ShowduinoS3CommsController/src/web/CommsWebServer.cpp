#include "CommsWebServer.h"
#include "../../../protocol/showduino_web_timeline_upload_policy.h"
#include "../../../protocol/showduino_deploy.h"
#include "../network/CommsGateway.h"

/* Preserve the current S3 WebUI server unchanged and add only extra routes
 * after the normal server has been initialised. */
#define commsWebBegin commsWebBeginBase
#include "CommsWebServerBase.h"
#undef commsWebBegin

#if SHOWDUINO_WEBUI_ENABLED

static String jsonField(const String &body, const char *key) {
  String out;
  String k = String("\"") + key + "\"";
  int i = body.indexOf(k);
  if (i < 0) return out;
  int colon = body.indexOf(':', i + k.length());
  int q1 = body.indexOf('"', colon);
  if (q1 < 0) return out;
  int q2 = q1 + 1;
  while (q2 < (int)body.length() && !(body[q2] == '"' && body[q2 - 1] != '\\')) q2++;
  if (q2 > q1) out = body.substring(q1 + 1, q2);
  return out;
}

static void handleGatewayGet() {
  String json = "{\n";
  json += "  \"productName\": \"" SHOWDUINO_PRODUCT_NAME "\",\n";
  json += "  \"productVersion\": \"" SHOWDUINO_PLATFORM_VERSION "\",\n";
  commsGatewayAppendStatusJson(json);
  json += "\n}\n";
  sendJson(200, json);
}

static void handleGatewayScanGet() {
  sendJson(200, String(commsGatewayScanJson()) + "\n");
}

static void handleGatewayScanPost() {
  if (!commsGatewayScanStart()) {
    sendJson(409, "{\"ok\":false,\"error\":\"busy\"}\n");
    return;
  }
  sendJson(202, "{\"ok\":true,\"state\":\"scanning\"}\n");
}

static void handleGatewayConnect() {
  const String body = sServer.arg("plain");
  const String ssid = jsonField(body, "ssid");
  const String pass = jsonField(body, "password");
  if (!ssid.length()) {
    sendJson(400, "{\"ok\":false,\"error\":\"missing_ssid\"}\n");
    return;
  }
  if (!commsGatewayConnect(ssid.c_str(), pass.length() ? pass.c_str() : nullptr)) {
    sendJson(400, "{\"ok\":false,\"error\":\"invalid_credentials\"}\n");
    return;
  }
  sendJson(200, "{\"ok\":true,\"state\":\"connecting\"}\n");
}

static void handleGatewayDisconnect() {
  commsGatewayDisconnect();
  sendJson(200, "{\"ok\":true,\"state\":\"ap_only\"}\n");
}

static void handleGatewayForget() {
  commsGatewayForget();
  sendJson(200, "{\"ok\":true,\"state\":\"forgotten\"}\n");
}

static void handleGatewayMode() {
  const String body = sServer.arg("plain");
  const String mode = jsonField(body, "mode");
  if (mode == "ap_sta") commsGatewaySetMode(COMMS_WIFI_AP_STA);
  else if (mode == "ap_only") commsGatewaySetMode(COMMS_WIFI_AP_ONLY);
  else {
    sendJson(400, "{\"ok\":false,\"error\":\"invalid_mode\"}\n");
    return;
  }
  sendJson(200, "{\"ok\":true}\n");
}

static void handleUpdatesGet() {
  String json;
  commsGatewayUpdatesJson(json);
  sendJson(200, json);
}

static void handleUpdatesCheck() {
  commsGatewayRequestUpdateCheck();
  sendJson(202, "{\"ok\":true,\"state\":\"checking\",\"otaInstall\":false}\n");
}

static void proxyDeploy(const char *path) {
  const String body = sServer.arg("plain");
  String resp;
  String mime;
  int status = 0;
  if (!commsWebTunnelPostBody("POST", path,
                              (const uint8_t *)body.c_str(), body.length(),
                              resp, status, mime, 8000)) {
    sendJson(503, "{\"ok\":false,\"error\":\"p4_offline\",\"note\":\"P4 OFFLINE\"}\n");
    return;
  }
  if (mime.length() == 0) mime = "application/json";
  sServer.sendHeader("Access-Control-Allow-Origin", "*");
  sServer.sendHeader("Cache-Control", "no-store");
  sServer.send(status > 0 ? status : 200, mime.c_str(), resp);
}

static void handleDeployBegin() { proxyDeploy(SHOWDUINO_DEPLOY_BEGIN_PATH); }
static void handleDeployChunk() { proxyDeploy(SHOWDUINO_DEPLOY_CHUNK_PATH); }
static void handleDeployCommit() { proxyDeploy(SHOWDUINO_DEPLOY_COMMIT_PATH); }
static void handleDeployAbort() { proxyDeploy(SHOWDUINO_DEPLOY_ABORT_PATH); }

static void handleDeployStatus() {
  if (proxyGetToP4(SHOWDUINO_DEPLOY_STATUS_PATH, 2500)) return;
  sendJson(503, "{\"ok\":false,\"error\":\"p4_offline\"}\n");
}

static void handleStudioTimelineCapability() {
  if (proxyGetToP4("/api/studio-timeline", 2500)) return;
  sendJson(503,
           "{\"ok\":false,\"error\":\"p4_offline\","
           "\"capability\":\"studio-ram-timeline-upload\","
           "\"state\":\"offline\"}\n");
}

static void handleStudioTimelineUpload() {
  String cmd;
  if (!extractCmd(sServer.arg("plain"), cmd)) {
    sendJson(400, "{\"ok\":false,\"error\":\"missing_cmd\"}\n");
    return;
  }
  cmd.trim();

  if (!ShowduinoWebTimelineUploadPolicy::envelopeAllowed(cmd.c_str())) {
    sendJson(403,
             "{\"ok\":false,\"error\":\"timeline_envelope_not_allowed\","
             "\"note\":\"Only BEGIN/END and PIXEL/AUDIO:NODE timeline cues are accepted\"}\n");
    return;
  }

  String path = "/api/studio-timeline/" + cmd;
  String body;
  String mime;
  int status = 0;
  if (!commsWebTunnelPost(path.c_str(), body, status, mime, 5000)) {
    sendJson(503,
             "{\"ok\":false,\"error\":\"p4_offline\","
             "\"p4Online\":false,\"note\":\"P4 OFFLINE\"}\n");
    return;
  }

  if (mime.length() == 0) mime = "application/json";
  sServer.sendHeader("Access-Control-Allow-Origin", "*");
  sServer.sendHeader("Cache-Control", "no-store");
  sServer.send(status > 0 ? status : 200, mime.c_str(), body);
}

void commsWebBegin() {
  commsWebBeginBase();

  sServer.on("/api/studio-timeline", HTTP_GET, handleStudioTimelineCapability);
  sServer.on("/api/studio-timeline", HTTP_POST, handleStudioTimelineUpload);
  sServer.on("/api/studio-timeline", HTTP_OPTIONS, sendCors);

  sServer.on("/api/gateway", HTTP_GET, handleGatewayGet);
  sServer.on("/api/gateway", HTTP_OPTIONS, sendCors);
  sServer.on("/api/gateway/scan", HTTP_GET, handleGatewayScanGet);
  sServer.on("/api/gateway/scan", HTTP_POST, handleGatewayScanPost);
  sServer.on("/api/gateway/scan", HTTP_OPTIONS, sendCors);
  sServer.on("/api/gateway/connect", HTTP_POST, handleGatewayConnect);
  sServer.on("/api/gateway/connect", HTTP_OPTIONS, sendCors);
  sServer.on("/api/gateway/disconnect", HTTP_POST, handleGatewayDisconnect);
  sServer.on("/api/gateway/disconnect", HTTP_OPTIONS, sendCors);
  sServer.on("/api/gateway/forget", HTTP_POST, handleGatewayForget);
  sServer.on("/api/gateway/forget", HTTP_OPTIONS, sendCors);
  sServer.on("/api/gateway/mode", HTTP_POST, handleGatewayMode);
  sServer.on("/api/gateway/mode", HTTP_OPTIONS, sendCors);
  sServer.on("/api/updates", HTTP_GET, handleUpdatesGet);
  sServer.on("/api/updates/check", HTTP_POST, handleUpdatesCheck);
  sServer.on("/api/updates", HTTP_OPTIONS, sendCors);
  sServer.on("/api/updates/check", HTTP_OPTIONS, sendCors);

  sServer.on("/api/productions/deploy/begin", HTTP_POST, handleDeployBegin);
  sServer.on("/api/productions/deploy/chunk", HTTP_POST, handleDeployChunk);
  sServer.on("/api/productions/deploy/commit", HTTP_POST, handleDeployCommit);
  sServer.on("/api/productions/deploy/abort", HTTP_POST, handleDeployAbort);
  sServer.on("/api/productions/deploy/status", HTTP_GET, handleDeployStatus);
  sServer.on("/api/productions/deploy/begin", HTTP_OPTIONS, sendCors);
  sServer.on("/api/productions/deploy/chunk", HTTP_OPTIONS, sendCors);
  sServer.on("/api/productions/deploy/commit", HTTP_OPTIONS, sendCors);
  sServer.on("/api/productions/deploy/abort", HTTP_OPTIONS, sendCors);
  sServer.on("/api/productions/deploy/status", HTTP_OPTIONS, sendCors);

  Serial.println("[WEBUI] Studio timeline deploy endpoint ready: /api/studio-timeline");
  Serial.println("[WEBUI] Gateway / updates / SHDO persist endpoints ready");
}

#else

void commsWebBegin() {
  commsWebBeginBase();
}

#endif
