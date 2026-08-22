#include "WebApiHandler.h"

#include <esp_heap_caps.h>
#include "../BoardConfig.h"
#include "../../../protocol/showduino_web_tunnel.h"
#include "StageStorage.h"
#include "StageAudio.h"
#include "WebApiLogger.h"
#include "WebJson.h"

extern bool emergencyLocked;
extern uint8_t gEmergencySourceId;

static unsigned long sBootMs = 0;

static const char *sourceName() {
  if (!emergencyLocked) return "";
  if (gEmergencySourceId == 2) return "physical";
  if (gEmergencySourceId == 1) return "director";
  return "unknown";
}

static const char *mimeForPath(const String &path) {
  String p = path;
  p.toLowerCase();
  if (p.endsWith(".html") || p.endsWith(".htm")) return "text/html";
  if (p.endsWith(".css")) return "text/css";
  if (p.endsWith(".js") || p.endsWith(".mjs")) return "application/javascript";
  if (p.endsWith(".json")) return "application/json";
  if (p.endsWith(".svg")) return "image/svg+xml";
  if (p.endsWith(".png")) return "image/png";
  if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
  if (p.endsWith(".ico")) return "image/x-icon";
  if (p.endsWith(".woff")) return "font/woff";
  if (p.endsWith(".woff2")) return "font/woff2";
  if (p.endsWith(".txt")) return "text/plain";
  return "application/octet-stream";
}

static void sendWebr(int status, const char *mime, const char *body, size_t len) {
  if (len > SHOWDUINO_WEB_TUNNEL_BODY_MAX) len = SHOWDUINO_WEB_TUNNEL_BODY_MAX;
  Serial1.print(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX);
  Serial1.print(status);
  Serial1.print(':');
  Serial1.print((unsigned long)len);
  if (mime && mime[0]) {
    Serial1.print(':');
    Serial1.print(mime);
  }
  Serial1.print('\n');
  if (len > 0 && body) {
    Serial1.write((const uint8_t *)body, len);
  }
}

static bool pathHasDotDot(const String &p) {
  int i = p.indexOf("..");
  while (i >= 0) {
    bool leftOk = (i == 0) || p.charAt(i - 1) == '/';
    bool rightOk = (i + 2 >= (int)p.length()) || p.charAt(i + 2) == '/';
    if (leftOk && rightOk) return true;
    i = p.indexOf("..", i + 2);
  }
  return false;
}

/* Map a public URL onto /showduino/webui/... and refuse escape. */
static bool mapWebuiPath(const String &urlIn, String &sdOut) {
  String url = urlIn;
  int q = url.indexOf('?');
  if (q >= 0) url = url.substring(0, q);
  url.trim();
  while (url.indexOf("//") >= 0) {
    url.replace("//", "/");
  }
  if (url.length() == 0 || url == "/") url = "/index.html";
  if (!url.startsWith("/")) return false;
  if (url.indexOf('\\') >= 0 || url.indexOf('\0') >= 0) return false;
  if (pathHasDotDot(url)) return false;

  const String root = PATH_WEBUI;
  sdOut = root + url;
  if (sdOut.endsWith("/")) sdOut += "index.html";

  if (!sdOut.startsWith(root + "/") && sdOut != root) return false;
  if (pathHasDotDot(sdOut)) return false;
  return true;
}

static void handleApiSystem() {
  gWebApiLogger.logHttpRequest("GET", "/api/system");
  const StageStorageStatus &st = stageStorageStatus();
  const StageAudioStatus &au = stageAudioStatus();

  String json = "{\n";
  json += "  \"firmwareVersion\": \"0.2.0\",\n";
  json += "  \"protocolVersion\": \"1.0\",\n";
  json += "  \"boardName\": \"ESP32-P4-IAN\",\n";
  json += "  \"role\": \"stage\",\n";
  json += "  \"uptime\": " + String(millis() - sBootMs) + ",\n";
  json += "  \"heapFree\": " + String(ESP.getFreeHeap()) + ",\n";
  json += "  \"heapTotal\": " + String(ESP.getHeapSize()) + ",\n";
  json += "  \"psramFree\": " + String(ESP.getFreePsram()) + ",\n";
  json += "  \"psramTotal\": " + String(ESP.getPsramSize()) + ",\n";
  json += "  \"cpuMhz\": " + String(getCpuFrequencyMhz()) + ",\n";
  json += "  \"storageReady\": " + String(st.mounted ? "true" : "false") + ",\n";
  json += "  \"storageWritable\": " + String(st.writable ? "true" : "false") + ",\n";
  json += "  \"storageHasWww\": " + String(st.hasWww ? "true" : "false") + ",\n";
  json += "  \"storageCardType\": \"" + String(st.cardType) + "\",\n";
  json += "  \"storageTotalMb\": " + String((unsigned long)(st.totalBytes / (1024ULL * 1024ULL))) + ",\n";
  json += "  \"storageFreeMb\": " + String((unsigned long)(st.freeBytes / (1024ULL * 1024ULL))) + ",\n";
  json += "  \"storageMessage\": \"" + ShowduinoWebJson::escape(String(st.message)) + "\",\n";
  json += "  \"showsPath\": \"/showduino/shows\",\n";
  json += "  \"webuiPath\": \"" PATH_WEBUI "\",\n";
  json += "  \"webuiReady\": " + String(st.hasWww ? "true" : "false") + ",\n";
  json += "  \"emergencyActive\": " + String(emergencyLocked ? "true" : "false") + ",\n";
  json += "  \"emergencySource\": \"" + String(sourceName()) + "\",\n";
  json += "  \"emergencyAudioPath\": \"" + ShowduinoWebJson::escape(String(au.selectedPath)) + "\",\n";
  json += "  \"emergencyAudioPlaying\": " + String(au.emergencyPlaying ? "true" : "false") + "\n";
  json += "}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiLogs() {
  gWebApiLogger.logHttpRequest("GET", "/api/logs");
  String json = "{\n  \"logs\": ";
  gWebApiLogger.appendJsonArray(json);
  json += "\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiDevices() {
  gWebApiLogger.logHttpRequest("GET", "/api/devices");
  String json = "{\n  \"devices\": [\n    {\n";
  json += "      \"id\": \"ian\",\n";
  json += "      \"name\": \"Stage Controller\",\n";
  json += "      \"board\": \"ESP32-P4\",\n";
  json += "      \"role\": \"stage\",\n";
  json += "      \"online\": true,\n";
  json += "      \"connectionStatus\": \"uart\"\n";
  json += "    }\n  ]\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleStaticFile(const String &urlPath) {
  String sdPath;
  if (!mapWebuiPath(urlPath, sdPath)) {
    const char *err = "{\"error\":\"forbidden\"}\n";
    sendWebr(403, "application/json", err, strlen(err));
    return;
  }

  if (!stageStorageIsReady()) {
    const char *err = "{\"error\":\"sd_unavailable\"}\n";
    sendWebr(503, "application/json", err, strlen(err));
    return;
  }

  if (!stageStorageFs().exists(sdPath.c_str())) {
    const char *err = "{\"error\":\"not_found\"}\n";
    sendWebr(404, "application/json", err, strlen(err));
    return;
  }

  File f = stageStorageFs().open(sdPath.c_str(), FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    const char *err = "{\"error\":\"not_found\"}\n";
    sendWebr(404, "application/json", err, strlen(err));
    return;
  }

  size_t sz = f.size();
  if (sz > SHOWDUINO_WEB_TUNNEL_BODY_MAX) {
    f.close();
    const char *err = "{\"error\":\"file_too_large\"}\n";
    sendWebr(413, "application/json", err, strlen(err));
    return;
  }

  char *buf = (char *)heap_caps_malloc(sz + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) buf = (char *)malloc(sz + 1);
  if (!buf) {
    f.close();
    const char *err = "{\"error\":\"oom\"}\n";
    sendWebr(500, "application/json", err, strlen(err));
    return;
  }

  size_t n = f.read((uint8_t *)buf, sz);
  f.close();
  sendWebr(200, mimeForPath(sdPath), buf, n);
  free(buf);
}

void webApiBegin(unsigned long bootMs) {
  sBootMs = bootMs;
  gWebApiLogger.log(WEB_LOG_INFO, "WebUI", "P4 HTTP origin ready");
  Serial.println("[WEB] HTTP origin ready (SUE UART tunnel)");
  if (stageStorageIsReady()) {
    Serial.println("[WEB] Serving WebUI from SD: " PATH_WEBUI "/");
  } else {
    Serial.println("[WEB] WebUI origin up; SD not mounted — static files unavailable");
  }
}

bool webApiHandleTunnelRequest(const String &command) {
  if (!command.startsWith(SHOWDUINO_WEB_TUNNEL_REQ_PREFIX)) return false;

  String rest = command.substring(strlen(SHOWDUINO_WEB_TUNNEL_REQ_PREFIX));
  if (!rest.startsWith("GET")) {
    const char *err = "{\"error\":\"method_not_allowed\"}\n";
    sendWebr(405, "application/json", err, strlen(err));
    return true;
  }

  String path = rest.substring(3);
  if (path.length() == 0) path = "/";
  while (path.startsWith("//")) path = path.substring(1);

  if (path.startsWith("/api/system")) {
    handleApiSystem();
    return true;
  }
  if (path.startsWith("/api/logs")) {
    handleApiLogs();
    return true;
  }
  if (path.startsWith("/api/devices")) {
    handleApiDevices();
    return true;
  }
  if (path.startsWith("/api/")) {
    const char *err = "{\"error\":\"not_found\"}\n";
    sendWebr(404, "application/json", err, strlen(err));
    return true;
  }

  handleStaticFile(path);
  return true;
}
