#include "CommsWebServer.h"
#include "../../../protocol/showduino_web_timeline_upload_policy.h"

/* Preserve the current S3 WebUI server unchanged and add only the dedicated
 * Studio timeline route after the normal server has been initialised. */
#define commsWebBegin commsWebBeginBase
#include "CommsWebServerBase.h"
#undef commsWebBegin

#if SHOWDUINO_WEBUI_ENABLED

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

  /* S3 is transport, not show authority. It performs only the shared narrow
   * envelope check before proxying. P4 repeats this check, performs detailed
   * PIXEL/AUDIO validation and owns the actual timeline/emergency decision. */
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

  /* Dedicated Studio deployment endpoint. The ordinary /api/command route and
   * its whitelist are intentionally left exactly as they were. */
  sServer.on("/api/studio-timeline", HTTP_GET, handleStudioTimelineCapability);
  sServer.on("/api/studio-timeline", HTTP_POST, handleStudioTimelineUpload);
  sServer.on("/api/studio-timeline", HTTP_OPTIONS, sendCors);

  Serial.println("[WEBUI] Studio timeline deploy endpoint ready: /api/studio-timeline");
}

#else

void commsWebBegin() {
  commsWebBeginBase();
}

#endif
