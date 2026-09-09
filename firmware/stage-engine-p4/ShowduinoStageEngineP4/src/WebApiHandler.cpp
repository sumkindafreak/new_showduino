#include "WebApiHandler.h"
#include "../../../protocol/showduino_web_timeline_upload_policy.h"
#include "ProductionDeploy.h"
#include "../../../protocol/showduino_version.h"

/*
 * Keep the existing P4 Web API implementation byte-for-byte as the base and
 * wrap only the public dispatch entry points. This avoids widening or
 * refactoring the normal /api/command whitelist just to support Studio deploy.
 */
#define webApiDispatch webApiDispatchBase
#define webApiHandleTunnelRequest webApiHandleTunnelRequestBase
#include "WebApiHandlerBase.h"
#undef webApiHandleTunnelRequest
#undef webApiDispatch

static bool extractStudioTimelineBodyCommand(const char *body, String &cmd) {
  if (!body || !body[0]) return false;
  const String value(body);
  const int key = value.indexOf("\"cmd\"");
  const int colon = key >= 0 ? value.indexOf(':', key) : -1;
  const int q1 = colon >= 0 ? value.indexOf('"', colon) : -1;
  const int q2 = q1 >= 0 ? value.indexOf('"', q1 + 1) : -1;
  if (q1 < 0 || q2 <= q1) return false;
  cmd = value.substring(q1 + 1, q2);
  cmd.trim();
  return cmd.length() > 0;
}

static void sendStudioTimelineCapability() {
  const char *json =
      "{\n"
      "  \"ok\": true,\n"
      "  \"capability\": \"studio-ram-timeline-upload\",\n"
      "  \"state\": \"ready\",\n"
      "  \"endpoint\": \"/api/studio-timeline\",\n"
      "  \"transport\": \"show-timeline-ram-v1\",\n"
      "  \"allowedPayloadFamilies\": [\"PIXEL\", \"AUDIO:NODE\"],\n"
      "  \"autoStart\": false,\n"
      "  \"runtimeAuthority\": \"esp32-p4-show-engine\"\n"
      "}\n";
  sendWebr(200, "application/json", json, strlen(json));
}

static void handleStudioTimelineCommand(const String &cmdIn) {
  String cmd = cmdIn;
  cmd.trim();

  const char *payload = nullptr;
  if (!ShowduinoWebTimelineUploadPolicy::envelopeAllowed(cmd.c_str(), &payload)) {
    const char *err =
        "{\"ok\":false,\"error\":\"timeline_envelope_not_allowed\","
        "\"note\":\"Studio timeline endpoint accepts only BEGIN/END and PIXEL/AUDIO:NODE cues\"}\n";
    sendWebr(403, "application/json", err, strlen(err));
    return;
  }

  /* The shared envelope policy checks the command family and TimelineEngine
   * length. The existing P4 web validator then performs the detailed command
   * validation (including Audio Node path resolution) on the nested payload.
   */
  if (payload && !webCommandAllowed(String(payload))) {
    const char *err =
        "{\"ok\":false,\"error\":\"timeline_payload_not_allowed\","
        "\"note\":\"Nested command failed the normal P4 output validator\"}\n";
    sendWebr(403, "application/json", err, strlen(err));
    return;
  }

  String replies;
  replies.reserve(384);
  stageWebDispatchCommand(cmd.c_str(), &replies);

  const bool rejected = replies.indexOf("REJECTED:") >= 0 ||
                        replies.indexOf("ERR:") >= 0 ||
                        replies.indexOf("UNSUPPORTED:") >= 0;
  String json = "{\n  \"ok\": ";
  json += rejected ? "false" : "true";
  json += ",\n  \"cmd\": ";
  appendQuoted(json, cmd.c_str());
  json += ",\n  \"replies\": ";
  appendQuoted(json, replies.c_str());
  json += ",\n  \"loaded\": ";
  json += gRuntime.rt.loaded ? "true" : "false";
  json += ",\n  \"running\": ";
  json += gRuntime.rt.running ? "true" : "false";
  json += ",\n  \"emergencyActive\": ";
  json += emergencyLocked ? "true" : "false";
  json += "\n}\n";
  sendWebr(rejected ? 409 : 200, "application/json", json.c_str(), json.length());
}

bool webApiDispatch(const char *method, const char *pathIn, const char *body) {
  String methodText = method ? method : "GET";
  methodText.toUpperCase();
  String path = pathIn ? pathIn : "/";
  path.trim();
  while (path.startsWith("//")) path = path.substring(1);
  if (path.length() == 0) path = "/";

  String cleanPath = path;
  const int query = cleanPath.indexOf('?');
  if (query >= 0) cleanPath = cleanPath.substring(0, query);

  if (methodText == "GET" && cleanPath == "/api/studio-timeline") {
    gWebApiLogger.logHttpRequest("GET", "/api/studio-timeline");
    sendStudioTimelineCapability();
    return true;
  }

  {
    int deployStatus = 0;
    String deployJson;
    if (productionDeployTry(methodText.c_str(), cleanPath.c_str(),
                            body ? body : "", body ? strlen(body) : 0,
                            &deployStatus, &deployJson)) {
      gWebApiLogger.logHttpRequest(methodText.c_str(), cleanPath.c_str());
      sendWebr(deployStatus, "application/json", deployJson.c_str(), deployJson.length());
      return true;
    }
  }

  if (methodText == "POST" &&
      (cleanPath == "/api/studio-timeline" ||
       cleanPath.startsWith("/api/studio-timeline/"))) {
    gWebApiLogger.logHttpRequest("POST", "/api/studio-timeline");
    String cmd;
    if (cleanPath.startsWith("/api/studio-timeline/")) {
      cmd = cleanPath.substring(strlen("/api/studio-timeline/"));
    } else {
      extractStudioTimelineBodyCommand(body, cmd);
    }

    if (cmd.length() == 0) {
      const char *err = "{\"ok\":false,\"error\":\"missing_cmd\"}\n";
      sendWebr(400, "application/json", err, strlen(err));
      return true;
    }

    handleStudioTimelineCommand(cmd);
    return true;
  }

  return webApiDispatchBase(method, pathIn, body);
}

bool webApiHandleTunnelRequest(const String &command) {
  if (!command.startsWith(SHOWDUINO_WEB_TUNNEL_REQ_PREFIX)) return false;

  String rest = command.substring(strlen(SHOWDUINO_WEB_TUNNEL_REQ_PREFIX));
  if (rest.startsWith("POST")) {
    String path = rest.substring(4);
    if (path.length() == 0) path = "/";
    while (path.startsWith("//")) path = path.substring(1);
    return webApiDispatch("POST", path.c_str(), nullptr);
  }

  if (!rest.startsWith("GET")) {
    const char *err = "{\"error\":\"method_not_allowed\"}\n";
    sendWebr(405, "application/json", err, strlen(err));
    return true;
  }

  String path = rest.substring(3);
  if (path.length() == 0) path = "/";
  while (path.startsWith("//")) path = path.substring(1);
  return webApiDispatch("GET", path.c_str(), nullptr);
}
