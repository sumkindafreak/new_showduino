#include "WebApiHandler.h"
#include "../BoardConfig.h"

#if SHOWDUINO_WEBUI_ENABLED

#include "WebApiLogger.h"
#include "WebJson.h"
#include "StageStorage.h"
#include "../ShowEngineState.h"
#include "../ShowRuntimeOwner.h"
#include "../../../protocol/showduino_web_tunnel.h"
#include "../../../protocol/showduino_show_runtime.h"

extern ShowEngineState gState;
extern ShowRuntimeOwner gRuntime;

static unsigned long sBootMs = 0;

static void sendWebResponse(int status, const String &body) {
  if (body.length() > SHOWDUINO_WEB_TUNNEL_BODY_MAX) {
    const char *err = "{\"error\":\"response too large\"}\n";
    Serial1.print(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX);
    Serial1.print(500);
    Serial1.print(':');
    Serial1.print((unsigned)strlen(err));
    Serial1.print('\n');
    Serial1.print(err);
    Serial1.flush();
    return;
  }
  Serial1.print(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX);
  Serial1.print(status);
  Serial1.print(':');
  Serial1.print((unsigned)body.length());
  Serial1.print('\n');
  Serial1.print(body);
  Serial1.flush();
  Serial.printf("[WebAPI] WEBR:%d len=%u\n", status, (unsigned)body.length());
}

static String nodeConnectionStatus(NodeAvailability n) {
  switch (n) {
    case NodeAvailability::Online: return "online";
    case NodeAvailability::Offline: return "offline";
    case NodeAvailability::Fault: return "fault";
    default: return "unknown";
  }
}

static String buildSystemJson() {
  const ShowRuntime &rt = gRuntime.rt;
  const StageStorageStatus &st = stageStorageStatus();
  String json = "{\n";
  json += "  \"firmwareVersion\": \"" + String(STAGE_FW_VERSION) + "\",\n";
  json += "  \"protocolVersion\": \"" + String(SHOWDUINO_PROTOCOL_VERSION_MAJOR) + "." +
          String(SHOWDUINO_PROTOCOL_VERSION_MINOR) + "\",\n";
  json += "  \"boardName\": \"" SHOWDUINO_BOARD_NAME "\",\n";
  json += "  \"role\": \"show-engine\",\n";
  json += "  \"uptime\": " + String(millis() - sBootMs) + ",\n";
  json += "  \"heapFree\": " + String(ESP.getFreeHeap()) + ",\n";
  json += "  \"heapTotal\": " + String(ESP.getHeapSize()) + ",\n";
  json += "  \"psramFree\": " + String(ESP.getFreePsram()) + ",\n";
  json += "  \"psramTotal\": " + String(ESP.getPsramSize()) + ",\n";
  json += "  \"cpuMhz\": " + String(getCpuFrequencyMhz()) + ",\n";
  json += "  \"storageReady\": " + String(st.mounted ? "true" : "false") + ",\n";
  json += "  \"storageWritable\": " + String(st.writable ? "true" : "false") + ",\n";
  json += "  \"storageHasWww\": " + String(st.hasWww ? "true" : "false") + ",\n";
  json += "  \"storageMessage\": \"" + ShowduinoWebJson::escape(String(st.message)) + "\",\n";
  json += "  \"storageCardType\": \"" + ShowduinoWebJson::escape(String(st.cardType)) + "\",\n";
  json += "  \"storageTotalMb\": " + String((unsigned long)(st.totalBytes / (1024ULL * 1024ULL))) + ",\n";
  json += "  \"storageFreeMb\": " + String((unsigned long)(st.freeBytes / (1024ULL * 1024ULL))) + ",\n";
  json += "  \"showsPath\": \"/showduino/shows\",\n";
  json += "  \"showPackagesPath\": \"/showduino/shows/packages\",\n";
  json += "  \"webuiPath\": \"" PATH_WEBUI_WWW "\",\n";
  json += "  \"webuiHost\": \"c3-front-door\",\n";
  json += "  \"mdnsHost\": \"showduino-studio\",\n";
  json += "  \"apSsid\": \"Showduino-Studio\",\n";
  json += "  \"showState\": \"" + ShowduinoWebJson::escape(String(showStateName(rt.state))) + "\",\n";
  json += "  \"emergencyActive\": " + String(gState.emergency == EmergencyState::Active ? "true" : "false") + ",\n";
  json += "  \"relayNodeStatus\": \"" + nodeConnectionStatus(gState.relayNode) + "\",\n";
  json += "  \"wifi\": {\n";
  json += "    \"mode\": \"UART\",\n";
  json += "    \"ssid\": \"Showduino-Studio\",\n";
  json += "    \"ip\": \"192.168.4.1\",\n";
  json += "    \"hostname\": \"showduino-studio\",\n";
  json += "    \"connected\": true\n";
  json += "  }\n";
  json += "}\n";
  return json;
}

static String buildDevicesJson() {
  String json = "{\n  \"devices\": [\n    {\n";
  json += "      \"name\": \"Show Engine\",\n";
  json += "      \"board\": \"" SHOWDUINO_BOARD_NAME "\",\n";
  json += "      \"mac\": \"local\",\n";
  json += "      \"role\": \"show-engine\",\n";
  json += "      \"online\": true,\n";
  json += "      \"rssi\": 0,\n";
  json += "      \"protocolVersion\": \"" + String(SHOWDUINO_PROTOCOL_VERSION_MAJOR) + "." +
          String(SHOWDUINO_PROTOCOL_VERSION_MINOR) + "\",\n";
  json += "      \"connectionStatus\": \"local\"\n";
  json += "    }";

  if (gState.relayNode != NodeAvailability::Unknown) {
    json += ",\n    {\n";
    json += "      \"name\": \"Relay Node\",\n";
    json += "      \"board\": \"ESP32 Relay Node\",\n";
    json += "      \"mac\": \"via-c3\",\n";
    json += "      \"role\": \"relay\",\n";
    json += "      \"online\": " + String(gState.relayNode == NodeAvailability::Online ? "true" : "false") + ",\n";
    json += "      \"rssi\": 0,\n";
    json += "      \"protocolVersion\": \"" + String(SHOWDUINO_PROTOCOL_VERSION_MAJOR) + "." +
            String(SHOWDUINO_PROTOCOL_VERSION_MINOR) + "\",\n";
    json += "      \"connectionStatus\": \"" + nodeConnectionStatus(gState.relayNode) + "\"\n";
    json += "    }";
  }

  json += "\n  ]\n}\n";
  return json;
}

static String buildLogsJson() {
  String json = "{\n  \"logs\": ";
  gWebApiLogger.appendJsonArray(json);
  json += "\n}\n";
  return json;
}

void webApiBegin(unsigned long bootMs) {
  sBootMs = bootMs;
  gWebApiLogger.log(WEB_LOG_INFO, "WebAPI", "Show Engine REST API ready");
}

bool webApiHandleTunnelRequest(const String &command) {
  if (!command.startsWith(SHOWDUINO_WEB_TUNNEL_REQ_PREFIX)) return false;

  int slashMethod = command.indexOf('/', 4);
  if (slashMethod < 0) {
    sendWebResponse(400, "{\"error\":\"bad tunnel request\"}\n");
    return true;
  }

  String method = command.substring(4, slashMethod);
  String path = command.substring(slashMethod);
  /* Accept WEB/GET//api/... (double slash) as well as WEB/GET/api/... */
  while (path.startsWith("//")) path.remove(0, 1);
  if (!path.startsWith("/")) path = "/" + path;
  if (path.length() == 0) path = "/";

  gWebApiLogger.logHttpRequest(method.c_str(), path.c_str());

  if (method != "GET") {
    sendWebResponse(405, "{\"error\":\"method not allowed\"}\n");
    return true;
  }

  if (path == "/api/system") {
    sendWebResponse(200, buildSystemJson());
  } else if (path == "/api/devices") {
    sendWebResponse(200, buildDevicesJson());
  } else if (path == "/api/logs") {
    sendWebResponse(200, buildLogsJson());
  } else {
    sendWebResponse(404, "{\"error\":\"not found\"}\n");
  }
  return true;
}

#else

void webApiBegin(unsigned long bootMs) { (void)bootMs; }

bool webApiHandleTunnelRequest(const String &command) {
  (void)command;
  return false;
}

#endif
