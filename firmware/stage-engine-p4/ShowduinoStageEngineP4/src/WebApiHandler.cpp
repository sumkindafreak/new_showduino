#include "WebApiHandler.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <esp_heap_caps.h>
#include "../BoardConfig.h"
#include "../ShowRuntimeOwner.h"
#include "../../../protocol/showduino_web_tunnel.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "StageStorage.h"
#include "StageAudio.h"
#include "StageDiagnostics.h"
#include "EmergencyInput.h"
#include "EmergencyPixels.h"
#include "WebApiLogger.h"
#include "WebJson.h"
#include "ProductionStore.h"
#include "plugin/PluginBus.h"
#include "StageTime.h"
#include "network/ShowNetwork.h"
#include "e131/E131Receiver.h"
#include "storage/StageStore.h"
#include "nodes/AudioNodeLink.h"

#ifndef SHOWDUINO_P4_STATIC_WEBUI
#define SHOWDUINO_P4_STATIC_WEBUI 0
#endif

extern bool emergencyLocked;
extern uint8_t gEmergencySourceId;
extern ShowRuntimeOwner gRuntime;
extern ProductionStore gProductionStore;

static unsigned long sBootMs = 0;
static bool sOriginReady = false;
static WebApiReplyFn sReplySink = nullptr;

static const char *sourceName() {
  if (!emergencyLocked) return "";
  if (gEmergencySourceId == 2) return "physical";
  if (gEmergencySourceId == 1) return "director";
  if (gEmergencySourceId == 3) return "usb";
  return "unknown";
}

#if SHOWDUINO_P4_STATIC_WEBUI
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
#endif

static void sendWebrUart(int status, const char *mime, const char *body, size_t len) {
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
  if (len > 0 && body) Serial1.write((const uint8_t *)body, len);
  Serial1.flush();
  Serial.printf("[WEB] TX WEBR %d len=%u mime=%s\n",
                status, (unsigned)len, (mime && mime[0]) ? mime : "-");
}

static void sendWebr(int status, const char *mime, const char *body, size_t len) {
  if (sReplySink) {
    sReplySink(status, mime, body, len);
    return;
  }
  sendWebrUart(status, mime, body, len);
}

void webApiSetReplySink(WebApiReplyFn fn) {
  sReplySink = fn;
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

static bool mapWebuiPath(const String &urlIn, String &sdOut) {
  String url = urlIn;
  int q = url.indexOf('?');
  if (q >= 0) url = url.substring(0, q);
  url.trim();
  while (url.indexOf("//") >= 0) url.replace("//", "/");
  if (url.length() == 0 || url == "/") url = "/index.html";
  if (!url.startsWith("/")) return false;
  if (url.indexOf('\\') >= 0) return false;
  if (pathHasDotDot(url)) return false;

  const String root = PATH_WEBUI;
  sdOut = root + url;
  if (sdOut.endsWith("/")) sdOut += "index.html";

  if (!sdOut.startsWith(root + "/") && sdOut != root) return false;
  if (pathHasDotDot(sdOut)) return false;
  return true;
}

static void appendQuoted(String &json, const char *value) {
  json += '"';
  json += ShowduinoWebJson::escape(String(value ? value : ""));
  json += '"';
}

static void appendCaps(String &json, uint32_t caps) {
  json += '[';
  bool first = true;
  for (uint8_t b = 0; b < 24; b++) {
    const uint32_t mask = 1u << b;
    if ((caps & mask) == 0) continue;
    const char *n = pluginCapName((PluginCap)mask);
    if (!n || !n[0]) continue;
    if (!first) json += ',';
    first = false;
    appendQuoted(json, n);
  }
  json += ']';
}

static void appendIsoTime(String &iso, bool &synced, time_t &epoch) {
  epoch = (time_t)stageTimeEpoch();
  synced = stageTimeSynced();
  char buf[32];
  stageTimeIso(buf, sizeof(buf));
  iso = buf;
}

static void normalizeWebCmd(String &cmd) {
  cmd.trim();
  String upper = cmd;
  upper.toUpperCase();
  if (upper == "PANIC" || upper == "EMERGENCY:PANIC" ||
      upper == "ESTOP" || upper == "E-STOP") {
    cmd = "EMERGENCY:STOP";
  }
}

static bool webCommandAllowed(const String &cmd) {
  if (cmd == "SHOW:START" || cmd == "SHOW:RUN" || cmd == "SHOW:PAUSE" ||
      cmd == "SHOW:RESUME" || cmd == "SHOW:STOP" || cmd == "STOP:ALL") {
    return true;
  }
  if (cmd == "PRODUCTION:UNLOAD" || cmd == "PLUGIN:SCAN" ||
      cmd == "AUDIO:STOP" || cmd == "AUDIO:LOCAL:STOP" ||
      cmd == "EMERGENCY:STOP" || cmd == "EMERGENCY:CLEAR_CONFIRM" ||
      cmd == "EMERGENCY:CLEAR_CANCEL" || cmd == "STATUS:REQUEST") {
    return true;
  }
  if (cmd.startsWith("PRODUCTION:LOAD:")) {
    String id = cmd.substring(16);
    id.trim();
    return productionIdIsValid(id.c_str());
  }
  if (cmd == SHOWDUINO_LEGACY_TIME_REQUEST || cmd == "TIME?") return true;
  if (cmd.startsWith("TIME:SET:")) {
    uint32_t epoch = 0;
    return stageTimeParseSet(cmd.c_str() + 9, &epoch);
  }
  if (cmd == "NET:STATUS" || cmd == "NET:APPLY" ||
      cmd == "NET:MODE:DHCP" || cmd == "NET:MODE:STATIC" ||
      cmd == "E131:STATUS" || cmd == "E131:CHANNELS") {
    return true;
  }
  if (cmd.startsWith("NET:ENABLE:") || cmd.startsWith("NET:STATIC:") ||
      cmd.startsWith("E131:ENABLE:") || cmd.startsWith("E131:UNIVERSE:") ||
      cmd.startsWith("E131:CHANNELS:")) {
    return cmd.length() < 96;
  }
  if (cmd == "STORAGE:STATUS" || cmd == "STORAGE:LIST" ||
      cmd == "STORAGE:CHECK" || cmd == "STORAGE:BACKUP") {
    return true;
  }
  if (cmd.startsWith("AUDIO:NODE:")) {
    char arg[80];
    int vol = -1;
    int fade = -1;
    int pri = -1;
    const ShowduinoAudioCmd parsed =
        showduino_audio_parse_command_ex(cmd.c_str(), arg, sizeof(arg), &vol, &fade, &pri);
    if (parsed == SHOWDUINO_AUDIO_CMD_NONE || parsed == SHOWDUINO_AUDIO_CMD_LOCAL_REJECT) {
      return false;
    }
    if ((parsed == SHOWDUINO_AUDIO_CMD_PLAY || parsed == SHOWDUINO_AUDIO_CMD_LOOP) && arg[0]) {
      char absPath[SHOWDUINO_AUDIO_PATH_MAX + 1];
      return showduino_audio_resolve_path(arg, absPath, sizeof(absPath)) ==
             SHOWDUINO_AUDIO_PATH_OK;
    }
    return true;
  }
  return false;
}

static void appendPluginDevices(String &json) {
  json += "[\n";
  const uint8_t n = pluginBusInstanceCount();
  bool first = true;
  for (uint8_t i = 0; i < n; i++) {
    const PluginInstance *inst = pluginBusInstanceAt(i);
    if (!inst) continue;
    if (!first) json += ",\n";
    first = false;
    char addr[8];
    pluginFormatAddress(inst->loc.address, addr, sizeof(addr));
    const bool online = inst->status == PluginStatus::Online ||
                        inst->status == PluginStatus::Unknown ||
                        inst->status == PluginStatus::Ambiguous;
    json += "      {\n";
    json += "        \"id\": \"";
    json += ShowduinoWebJson::escape(String(inst->instanceId[0] ? inst->instanceId : addr));
    json += "\",\n";
    json += "        \"bus\": " + String((unsigned)inst->loc.busId) + ",\n";
    json += "        \"address\": \"" + String(addr) + "\",\n";
    json += "        \"chip\": \"" + String(pluginChipName(inst->chip)) + "\",\n";
    json += "        \"role\": \"" + String(pluginRoleName(inst->role)) + "\",\n";
    json += "        \"name\": ";
    appendQuoted(json, inst->friendly[0] ? inst->friendly : "Unknown I2C Device");
    json += ",\n";
    json += "        \"class\": \"" + String(pluginClassName(inst->deviceClass)) + "\",\n";
    json += "        \"classification\": \"" + String(pluginClassificationName(inst->classification)) + "\",\n";
    json += "        \"status\": \"" + String(pluginStatusName(inst->status)) + "\",\n";
    json += "        \"configured\": " + String(inst->configured ? "true" : "false") + ",\n";
    json += "        \"online\": " + String(online ? "true" : "false") + ",\n";
    json += "        \"capabilities\": ";
    appendCaps(json, inst->capabilities);
    json += "\n      }";
  }
  json += "\n    ]";
}

static void handleApiSystem() {
  gWebApiLogger.logHttpRequest("GET", "/api/system");
  const StageStorageStatus &st = stageStorageStatus();
  const StageAudioStatus &au = stageAudioStatus();
  const ShowRuntime &rt = gRuntime.rt;
  String iso;
  bool timeSynced = false;
  time_t epoch = 0;
  appendIsoTime(iso, timeSynced, epoch);

  String json = "{\n";
  json += "  \"firmwareVersion\": \"0.4.0\",\n";
  json += "  \"protocolVersion\": \"1.0\",\n";
  json += "  \"boardName\": \"ESP32-P4 Stage Engine\",\n";
  json += "  \"role\": \"stage\",\n";
  json += "  \"p4Online\": true,\n";
  json += "  \"stageLink\": \"online\",\n";
  json += "  \"uptime\": " + String(millis() - sBootMs) + ",\n";
  json += "  \"heapFree\": " + String(ESP.getFreeHeap()) + ",\n";
  json += "  \"heapTotal\": " + String(ESP.getHeapSize()) + ",\n";
  json += "  \"psramFree\": " + String(ESP.getFreePsram()) + ",\n";
  json += "  \"psramTotal\": " + String(ESP.getPsramSize()) + ",\n";
  json += "  \"cpuMhz\": " + String(getCpuFrequencyMhz()) + ",\n";
  json += "  \"storageReady\": " + String(st.mounted ? "true" : "false") + ",\n";
  json += "  \"storageState\": \"" + String(stageStoreStateName()) + "\",\n";
  json += "  \"storageWritable\": " + String(stageStoreWritable() ? "true" : "false") + ",\n";
  json += "  \"storageHasWww\": " + String(st.hasWww ? "true" : "false") + ",\n";
  json += "  \"storageCardType\": \"" + String(st.cardType) + "\",\n";
  json += "  \"storageTotalMb\": " + String((unsigned long)(st.totalBytes / (1024ULL * 1024ULL))) + ",\n";
  json += "  \"storageFreeMb\": " + String((unsigned long)(st.freeBytes / (1024ULL * 1024ULL))) + ",\n";
  json += "  \"storageMessage\": ";
  appendQuoted(json, st.message);
  json += ",\n";
  json += "  \"productionsPath\": \"" SHOWDUINO_PRODUCTIONS_ROOT "\",\n";
  json += "  \"showsPath\": \"" SHOWDUINO_PRODUCTIONS_ROOT "\",\n";
  json += "  \"webuiPath\": \"" PATH_WEBUI "\",\n";
  json += "  \"webuiHost\": \"comms-s3\",\n";
  json += "  \"productionLoaded\": " + String(gProductionStore.hasLoaded() ? "true" : "false") + ",\n";
  json += "  \"productionId\": ";
  appendQuoted(json, gProductionStore.hasLoaded() ? gProductionStore.loaded().productionId : "");
  json += ",\n";
  json += "  \"productionName\": ";
  appendQuoted(json, gProductionStore.hasLoaded() ? gProductionStore.loaded().name : "");
  json += ",\n";
  json += "  \"showState\": \"" + String(showStateName(rt.state)) + "\",\n";
  json += "  \"showName\": ";
  appendQuoted(json, rt.showName);
  json += ",\n";
  json += "  \"showElapsedMs\": " + String(rt.elapsedMs) + ",\n";
  json += "  \"showRemainingMs\": " + String(rt.remainingMs) + ",\n";
  json += "  \"showDurationMs\": " + String(rt.totalDurationMs) + ",\n";
  json += "  \"currentCue\": " + String(rt.currentCue) + ",\n";
  json += "  \"totalCues\": " + String(rt.totalCues) + ",\n";
  json += "  \"showLoaded\": " + String(rt.loaded ? "true" : "false") + ",\n";
  json += "  \"showRunning\": " + String(rt.running ? "true" : "false") + ",\n";
  json += "  \"showPaused\": " + String(rt.paused ? "true" : "false") + ",\n";
  json += "  \"showLastError\": ";
  appendQuoted(json, rt.lastError);
  json += ",\n";
  json += "  \"emergencyActive\": " + String(emergencyLocked ? "true" : "false") + ",\n";
  json += "  \"emergencySource\": \"" + String(sourceName()) + "\",\n";
  json += "  \"emergencyPendingClear\": " +
         String(emergencyInputPendingClearValid(millis()) ? "true" : "false") + ",\n";
  json += "  \"emergencyButtonPressed\": " +
         String(emergencyInputLoopOpen() ? "true" : "false") + ",\n";
  json += "  \"emergencyAudioPath\": ";
  appendQuoted(json, au.selectedPath);
  json += ",\n";
  json += "  \"emergencyAudioPlaying\": " + String(au.emergencyPlaying ? "true" : "false") + ",\n";
  json += "  \"commsLink\": " + String(stageCommsLinkUp() ? "true" : "false") + ",\n";
  json += "  \"directorTrafficSeen\": " + String(stageCommsSawDirectorTraffic() ? "true" : "false") + ",\n";
  json += "  \"timeSynced\": " + String(timeSynced ? "true" : "false") + ",\n";
  json += "  \"timeIso\": \"" + iso + "\",\n";
  json += "  \"timeEpoch\": " + String((unsigned long)epoch) + ",\n";
  json += "  \"timeSource\": \"" + String(stageTimeSource()) + "\",\n";
  json += "  \"rtcStatus\": \"" + String(stageTimeHealth()) + "\",\n";
  json += "  \"timezone\": \"UTC\",\n";
  json += "  \"audio\": {\n";
  json += "    \"role\": \"system\",\n";
  json += "    \"codec\": \"ES8311\",\n";
  json += "    \"output\": \"ONBOARD SPEAKER\",\n";
  json += "    \"codecReady\": " + String(au.codecReady ? "true" : "false") + ",\n";
  json += "    \"codecDetected\": " + String(au.codecDetected ? "true" : "false") + ",\n";
  json += "    \"i2sReady\": " + String(au.i2sReady ? "true" : "false") + ",\n";
  json += "    \"amplifierEnabled\": " + String(au.amplifierEnabled ? "true" : "false") + ",\n";
  json += "    \"storageReady\": " + String(au.storageReady ? "true" : "false") + ",\n";
  json += "    \"wavPresent\": " + String(au.wavPresent ? "true" : "false") + ",\n";
  json += "    \"mp3Present\": false,\n";
  json += "    \"emergencyPlaying\": " + String(au.emergencyPlaying ? "true" : "false") + ",\n";
  json += "    \"showPlaying\": false,\n";
  json += "    \"playing\": " + String(stageAudioIsPlaying() ? "true" : "false") + ",\n";
  json += "    \"current\": \"";
  json += au.currentName;
  json += "\",\n    \"state\": \"";
  json += au.healthName;
  json += "\",\n    \"assets\": " + String((unsigned)au.activeAssets) + ",\n";
  json += "    \"assetsTotal\": 7,\n";
  json += "    \"shutdownReserved\": " + String(au.shutdownAssetPresent ? "true" : "false") + ",\n";
  json += "    \"wavPath\": ";
  appendQuoted(json, au.wavPath);
  json += ",\n    \"selectedPath\": ";
  appendQuoted(json, au.selectedPath);
  json += ",\n    \"lastError\": ";
  appendQuoted(json, au.lastError);
  json += "\n  },\n";
  json += "  \"capabilities\": {\n";
  json += "    \"dmx\": \"planned\",\n";
  json += "    \"pixels\": \"planned\",\n";
  json += "    \"ethernet\": \"" + String(showNetworkLive().hardwareInit ? "ready" : "optional") + "\",\n";
  json += "    \"e131\": \"test\",\n";
    json += "    \"audio\": \"" + String(au.codecReady ? "ready" : "fault") + "\",\n";
  json += "    \"inputs\": \"planned\",\n";
    json += "    \"sd\": \"" + String(stageStoreStateName()) + "\",\n";
    json += "    \"pluginBus\": " + String(pluginBusReady() ? "true" : "false") + ",\n";
    json += "    \"audioNode\": \"" + String(audioNodeLinkStatus().online ? "ready" : "searching") + "\"\n";
    json += "  },\n";
  json += "  \"ethernet\": {\n";
  json += "    \"link\": \"" + String(showNetworkLive().hasIp ? "UP" : (showNetworkLive().linkUp ? "UP" : "DOWN")) + "\",\n";
  json += "    \"ip\": \"" + String(showNetworkLive().ip) + "\",\n";
  json += "    \"mode\": \"" + String(showNetModeName(showNetworkLive().liveMode)) + "\"\n";
  json += "  },\n";
  json += "  \"e131\": {\n";
  json += "    \"state\": \"" + String(e131RxStateName(e131ReceiverStatus().state)) + "\",\n";
  json += "    \"universe\": " + String((unsigned)e131ReceiverStatus().universe) + "\n";
  json += "  },\n";
  PluginBusSelfTest pst;
  pluginBusCaptureSelfTest(&pst);
  json += "  \"pluginBus\": {\n";
  json += "    \"ready\": " + String(pluginBusReady() ? "true" : "false") + ",\n";
  json += "    \"internal\": " + String((unsigned)pst.internal) + ",\n";
  json += "    \"configuredPlugins\": " + String((unsigned)pst.configuredPlugins) + ",\n";
  json += "    \"unconfiguredPlugins\": " + String((unsigned)pst.unconfiguredPlugins) + ",\n";
  json += "    \"unknown\": " + String((unsigned)pst.unknown) + ",\n";
  json += "    \"offlineConfigured\": " + String((unsigned)pst.offlineConfigured) + ",\n";
  json += "    \"total\": " + String((unsigned)pluginBusInstanceCount()) + ",\n";
  json += "    \"devices\": ";
  appendPluginDevices(json);
  json += "\n  },\n";
  json += "  \"storage\": ";
  stageStoreAppendJson(json);
  json += ",\n  \"audioNode\": ";
  audioNodeLinkAppendJson(json);
  json += "\n}\n";
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
  String json = "{\n  \"devices\": [\n";
  json += "    {\n";
  json += "      \"id\": \"p4\",\n";
  json += "      \"name\": \"Stage Engine\",\n";
  json += "      \"friendlyName\": \"ESP32-P4 Show Engine\",\n";
  json += "      \"board\": \"ESP32-P4\",\n";
  json += "      \"role\": \"stage\",\n";
  json += "      \"online\": true,\n";
  json += "      \"connectionStatus\": \"uart-authoritative\",\n";
  json += "      \"firmwareVersion\": \"0.2.0\"\n";
  json += "    }";
  const uint8_t n = pluginBusInstanceCount();
  for (uint8_t i = 0; i < n; i++) {
    const PluginInstance *inst = pluginBusInstanceAt(i);
    if (!inst) continue;
    char addr[8];
    pluginFormatAddress(inst->loc.address, addr, sizeof(addr));
    const bool online = inst->status == PluginStatus::Online ||
                        inst->status == PluginStatus::Unknown ||
                        inst->status == PluginStatus::Ambiguous;
    json += ",\n    {\n";
    json += "      \"id\": \"";
    json += ShowduinoWebJson::escape(String(inst->instanceId[0] ? inst->instanceId : addr));
    json += "\",\n";
    json += "      \"name\": ";
    appendQuoted(json, inst->friendly[0] ? inst->friendly : "I2C plugin");
    json += ",\n      \"board\": \"";
    json += pluginChipName(inst->chip);
    json += "\",\n      \"role\": \"";
    json += pluginRoleName(inst->role);
    json += "\",\n      \"online\": ";
    json += online ? "true" : "false";
    json += ",\n      \"connectionStatus\": \"p4-plugin-bus\",\n";
    json += "      \"mac\": \"";
    json += addr;
    json += "\"\n    }";
  }
  const AudioNodeStatus &an = audioNodeLinkStatus();
  if (an.seen) {
    json += ",\n    {\n";
    json += "      \"id\": \"audio-node\",\n";
    json += "      \"name\": \"Audio Node\",\n";
    json += "      \"friendlyName\": \"Audio Node\",\n";
    json += "      \"board\": \"ESP32-A1S Audio Kit\",\n";
    json += "      \"role\": \"AUDIO\",\n";
    json += "      \"online\": ";
    json += an.online ? "true" : "false";
    json += ",\n      \"connectionStatus\": \"espnow-node\",\n";
    json += "      \"mac\": \"";
    json += an.mac;
    json += "\",\n      \"firmwareVersion\": \"";
    json += an.firmware;
    json += "\",\n      \"state\": \"";
    json += an.state;
    json += "\",\n      \"capabilities\": \"";
    json += an.capabilities;
    json += "\"\n    }";
  }
  json += "\n  ]\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiProductions() {
  gWebApiLogger.logHttpRequest("GET", "/api/productions");
  gProductionStore.scan();
  String json = "{\n";
  json += "  \"path\": \"" SHOWDUINO_PRODUCTIONS_ROOT "\",\n";
  json += "  \"storageReady\": " + String(stageStorageIsReady() ? "true" : "false") + ",\n";
  json += "  \"loadedId\": ";
  appendQuoted(json, gProductionStore.hasLoaded() ? gProductionStore.loaded().productionId : "");
  json += ",\n  \"error\": ";
  appendQuoted(json, gProductionStore.lastError());
  json += ",\n  \"productions\": [\n";
  for (uint8_t i = 0; i < gProductionStore.count(); i++) {
    const ProductionManifest *m = gProductionStore.at(i);
    if (!m) continue;
    json += "    {\n";
    json += "      \"id\": ";
    appendQuoted(json, m->productionId);
    json += ",\n      \"name\": ";
    appendQuoted(json, m->name);
    json += ",\n      \"description\": ";
    appendQuoted(json, m->description);
    json += ",\n      \"author\": ";
    appendQuoted(json, m->author);
    json += ",\n      \"timeline\": ";
    appendQuoted(json, m->timeline);
    json += ",\n      \"revision\": " + String((unsigned long)m->revision) + "\n";
    json += "    }";
    json += (i + 1 < gProductionStore.count()) ? ",\n" : "\n";
  }
  json += "  ]\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiShow() {
  gWebApiLogger.logHttpRequest("GET", "/api/show");
  const ShowRuntime &rt = gRuntime.rt;
  String json = "{\n";
  json += "  \"state\": \"" + String(showStateName(rt.state)) + "\",\n";
  json += "  \"name\": ";
  appendQuoted(json, rt.showName);
  json += ",\n  \"productionId\": ";
  appendQuoted(json, gProductionStore.hasLoaded() ? gProductionStore.loaded().productionId : "");
  json += ",\n  \"elapsedMs\": " + String(rt.elapsedMs) + ",\n";
  json += "  \"remainingMs\": " + String(rt.remainingMs) + ",\n";
  json += "  \"durationMs\": " + String(rt.totalDurationMs) + ",\n";
  json += "  \"currentCue\": " + String(rt.currentCue) + ",\n";
  json += "  \"totalCues\": " + String(rt.totalCues) + ",\n";
  json += "  \"loaded\": " + String(rt.loaded ? "true" : "false") + ",\n";
  json += "  \"running\": " + String(rt.running ? "true" : "false") + ",\n";
  json += "  \"paused\": " + String(rt.paused ? "true" : "false") + ",\n";
  json += "  \"emergency\": " + String(emergencyLocked ? "true" : "false") + ",\n";
  json += "  \"lastError\": ";
  appendQuoted(json, rt.lastError);
  json += ",\n  \"cues\": [\n";
  const uint16_t total = gRuntime.timeline.cueTotal();
  const uint16_t limit = total > 64 ? 64 : total;
  for (uint16_t i = 0; i < limit; i++) {
    const TimelineCue *cue = gRuntime.timeline.cueAt(i);
    if (!cue) continue;
    json += "    {\"index\": " + String((unsigned)i) +
            ", \"timeMs\": " + String((unsigned long)cue->timeMs) +
            ", \"command\": ";
    appendQuoted(json, cue->command);
    json += "}";
    json += (i + 1 < limit) ? ",\n" : "\n";
  }
  json += "  ],\n";
  json += "  \"cuesTruncated\": " + String(total > 64 ? "true" : "false") + "\n";
  json += "}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiPlugins() {
  gWebApiLogger.logHttpRequest("GET", "/api/plugins");
  PluginBusSelfTest pst;
  pluginBusCaptureSelfTest(&pst);
  String json = "{\n";
  json += "  \"ready\": " + String(pluginBusReady() ? "true" : "false") + ",\n";
  json += "  \"internal\": " + String((unsigned)pst.internal) + ",\n";
  json += "  \"configuredPlugins\": " + String((unsigned)pst.configuredPlugins) + ",\n";
  json += "  \"unconfiguredPlugins\": " + String((unsigned)pst.unconfiguredPlugins) + ",\n";
  json += "  \"unknown\": " + String((unsigned)pst.unknown) + ",\n";
  json += "  \"offlineConfigured\": " + String((unsigned)pst.offlineConfigured) + ",\n";
  json += "  \"total\": " + String((unsigned)pluginBusInstanceCount()) + ",\n";
  json += "  \"devices\": ";
  appendPluginDevices(json);
  json += "\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiAudio() {
  gWebApiLogger.logHttpRequest("GET", "/api/audio");
  const StageAudioStatus &au = stageAudioStatus();
  String json = "{\n";
  json += "  \"role\": \"system\",\n";
  json += "  \"codec\": \"ES8311\",\n";
  json += "  \"output\": \"ONBOARD SPEAKER\",\n";
  json += "  \"codecReady\": " + String(au.codecReady ? "true" : "false") + ",\n";
  json += "  \"i2sReady\": " + String(au.i2sReady ? "true" : "false") + ",\n";
  json += "  \"amplifierEnabled\": " + String(au.amplifierEnabled ? "true" : "false") + ",\n";
  json += "  \"wavPresent\": " + String(au.wavPresent ? "true" : "false") + ",\n";
  json += "  \"mp3Present\": false,\n";
  json += "  \"mp3Decoded\": false,\n";
  json += "  \"emergencyPlaying\": " + String(au.emergencyPlaying ? "true" : "false") + ",\n";
  json += "  \"showPlaying\": false,\n";
  json += "  \"playing\": " + String(stageAudioIsPlaying() ? "true" : "false") + ",\n";
  json += "  \"current\": \"";
  json += au.currentName;
  json += "\",\n  \"state\": \"";
  json += au.healthName;
  json += "\",\n  \"assets\": " + String((unsigned)au.activeAssets) + ",\n";
  json += "  \"emergencyActive\": " + String(emergencyLocked ? "true" : "false") + ",\n";
  json += "  \"wavPath\": ";
  appendQuoted(json, au.wavPath);
  json += ",\n  \"selectedPath\": ";
  appendQuoted(json, au.selectedPath);
  json += ",\n  \"lastError\": ";
  appendQuoted(json, au.lastError);
  json += "\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiTime() {
  gWebApiLogger.logHttpRequest("GET", "/api/time");
  String iso;
  bool synced = false;
  time_t epoch = 0;
  appendIsoTime(iso, synced, epoch);
  char clock[16];
  char date[16];
  stageTimeClock(clock, sizeof(clock));
  stageTimeDate(date, sizeof(date));
  String json = "{\n";
  json += "  \"iso\": \"" + iso + "\",\n";
  json += "  \"time\": \"" + String(clock) + "\",\n";
  json += "  \"date\": \"" + String(date) + "\",\n";
  json += "  \"epoch\": " + String((unsigned long)epoch) + ",\n";
  json += "  \"timezone\": \"UTC\",\n";
  json += "  \"synced\": " + String(synced ? "true" : "false") + ",\n";
  json += "  \"source\": \"" + String(stageTimeSource()) + "\",\n";
  json += "  \"rtcStatus\": \"" + String(stageTimeHealth()) + "\",\n";
  json += "  \"uptime\": " + String(millis() - sBootMs) + ",\n";
  json += "  \"note\": \"No DS3231 on this P4 generation. Clock is the internal RTC.\"\n";
  json += "}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiCapabilities() {
  gWebApiLogger.logHttpRequest("GET", "/api/capabilities");
  const StageAudioStatus &au = stageAudioStatus();
  String json = "{\n";
  json += "  \"engine\": [\n";
  json += "    {\"name\":\"show-runtime\",\"state\":\"ready\"},\n";
  json += "    {\"name\":\"productions\",\"state\":\"" +
         String(stageStorageIsReady() ? "ready" : "unavailable") + "\"},\n";
  json += "    {\"name\":\"audio\",\"state\":\"" + String(au.codecReady ? "ready" : "fault") + "\"},\n";
  json += "    {\"name\":\"plugin-bus\",\"state\":\"" + String(pluginBusReady() ? "ready" : "planned") + "\"},\n";
  json += "    {\"name\":\"emergency\",\"state\":\"ready\"},\n";
  json += "    {\"name\":\"rtc\",\"state\":\"ready\"},\n";
  json += "    {\"name\":\"ethernet\",\"state\":\"" +
         String(showNetworkLive().hardwareInit ? "ready" : "optional") + "\"},\n";
  json += "    {\"name\":\"e131-test\",\"state\":\"ready\"},\n";
  json += "    {\"name\":\"dmx\",\"state\":\"planned\"},\n";
  json += "    {\"name\":\"pixels\",\"state\":\"planned\"},\n";
  json += "    {\"name\":\"espnow-nodes\",\"state\":\"planned\"}\n";
  json += "  ],\n";
  json += "  \"devices\": ";
  appendPluginDevices(json);
  json += "\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static uint32_t queryU32(const String &path, const char *key, uint32_t fallback) {
  String needle = String(key) + "=";
  int q = path.indexOf('?');
  if (q < 0) return fallback;
  int k = path.indexOf(needle, q);
  if (k < 0) return fallback;
  return (uint32_t)path.substring(k + needle.length()).toInt();
}

static void handleApiNetwork() {
  gWebApiLogger.logHttpRequest("GET", "/api/network");
  const ShowNetLive &live = showNetworkLive();
  const ShowNetConfig &saved = showNetworkSavedConfig();
  const uint32_t now = millis();
  String json = "{\n";
  json += "  \"role\": \"stage\",\n";
  json += "  \"uartReady\": " + String(stageCommsUartReady() ? "true" : "false") + ",\n";
  json += "  \"commsLink\": " + String(stageCommsLinkUp() ? "true" : "false") + ",\n";
  json += "  \"directorTrafficSeen\": " + String(stageCommsSawDirectorTraffic() ? "true" : "false") + ",\n";
  json += "  \"lastRxAgeMs\": " +
         String(stageCommsLastRxMs() ? (unsigned long)(now - stageCommsLastRxMs()) : 0) + ",\n";
  json += "  \"note\": \"P4 Ethernet is optional. Browser SoftAP remains the Communications S3.\",\n";
  json += "  \"ethernet\": {\n";
  json += "    \"enabled\": " + String(live.enabled ? "true" : "false") + ",\n";
  json += "    \"hardwareInit\": " + String(live.hardwareInit ? "true" : "false") + ",\n";
  json += "    \"link\": \"" + String(live.linkUp ? "UP" : "DOWN") + "\",\n";
  json += "    \"mode\": \"" + String(showNetModeName(live.liveMode)) + "\",\n";
  json += "    \"mac\": \"" + String(live.mac) + "\",\n";
  json += "    \"ip\": \"" + String(live.ip) + "\",\n";
  json += "    \"subnet\": \"" + String(live.subnet) + "\",\n";
  json += "    \"gateway\": \"" + String(live.gateway) + "\",\n";
  json += "    \"dns\": \"" + String(live.dns) + "\",\n";
  json += "    \"speedMbps\": " + String((unsigned)live.speedMbps) + ",\n";
  json += "    \"fullDuplex\": " + String(showNetworkFullDuplex() ? "true" : "false") + ",\n";
  json += "    \"linkAgeMs\": " + String(live.linkUp && live.linkUpMs ? (unsigned long)(now - live.linkUpMs) : 0) + ",\n";
  json += "    \"httpListening\": " + String(live.httpListening ? "true" : "false") + ",\n";
  json += "    \"url\": \"" + String(live.webUrl) + "\"\n";
  json += "  },\n";
  json += "  \"saved\": {\n";
  json += "    \"formatVersion\": 1,\n";
  json += "    \"ethernet\": {\n";
  json += "      \"enabled\": " + String(saved.enabled ? "true" : "false") + ",\n";
  json += "      \"mode\": \"" + String(showNetModeName(saved.mode)) + "\",\n";
  json += "      \"ip\": \"" + String(saved.ip) + "\",\n";
  json += "      \"subnet\": \"" + String(saved.subnet) + "\",\n";
  json += "      \"gateway\": \"" + String(saved.gateway) + "\",\n";
  json += "      \"dns\": \"" + String(saved.dns) + "\"\n";
  json += "    },\n";
  json += "    \"e131\": {\n";
  json += "      \"enabled\": " + String(saved.e131Enabled ? "true" : "false") + ",\n";
  json += "      \"universe\": " + String((unsigned)saved.e131Universe) + "\n";
  json += "    }\n";
  json += "  }\n";
  json += "}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiE131() {
  gWebApiLogger.logHttpRequest("GET", "/api/e131");
  const E131RxStatus &rx = e131ReceiverStatus();
  String json = "{\n";
  json += "  \"enabled\": " + String(rx.enabled ? "true" : "false") + ",\n";
  json += "  \"state\": \"" + String(e131RxStateName(rx.state)) + "\",\n";
  json += "  \"universe\": " + String((unsigned)rx.universe) + ",\n";
  json += "  \"multicast\": \"" + String(rx.multicast) + "\",\n";
  json += "  \"multicastJoined\": " + String(rx.multicastJoined ? "true" : "false") + ",\n";
  json += "  \"source\": ";
  appendQuoted(json, rx.sourceName);
  json += ",\n";
  json += "  \"cid\": \"" + String(rx.cid) + "\",\n";
  json += "  \"priority\": " + String((unsigned)rx.priority) + ",\n";
  json += "  \"sequence\": " + String((unsigned)rx.lastSequence) + ",\n";
  json += "  \"packets\": " + String((unsigned long)rx.packetsOk) + ",\n";
  json += "  \"rejected\": " + String((unsigned long)rx.packetsRejected) + ",\n";
  json += "  \"rateFps\": " + String(rx.rateFps, 1) + ",\n";
  json += "  \"lastPacketAgeMs\": " + String((unsigned long)rx.lastAgeMs) + ",\n";
  json += "  \"lastReject\": \"" + String(rx.lastRejectName) + "\",\n";
  json += "  \"loopUs\": " + String((unsigned long)rx.lastLoopUs) + ",\n";
  json += "  \"maxLoopUs\": " + String((unsigned long)rx.maxLoopUs) + ",\n";
  json += "  \"readOnly\": true,\n";
  json += "  \"note\": \"E1.31 test receiver is observation only. It does not control the show.\"\n";
  json += "}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiE131Channels(const String &path) {
  gWebApiLogger.logHttpRequest("GET", "/api/e131/channels");
  uint32_t from = queryU32(path, "from", 1);
  uint32_t count = queryU32(path, "count", 64);
  if (from < 1) from = 1;
  if (from > SHOWDUINO_E131_MAX_SLOTS) from = SHOWDUINO_E131_MAX_SLOTS;
  if (count < 1) count = 1;
  if (count > 128) count = 128;
  if (from + count - 1 > SHOWDUINO_E131_MAX_SLOTS) {
    count = SHOWDUINO_E131_MAX_SLOTS - from + 1;
  }
  uint8_t buf[128];
  e131ReceiverCopySlots(buf, (uint16_t)from, (uint16_t)count);
  String json = "{\n";
  json += "  \"from\": " + String((unsigned)from) + ",\n";
  json += "  \"count\": " + String((unsigned)count) + ",\n";
  json += "  \"readOnly\": true,\n";
  json += "  \"channels\": [";
  for (uint32_t i = 0; i < count; i++) {
    if (i) json += ',';
    json += "{\"ch\":";
    json += String((unsigned)(from + i));
    json += ",\"v\":";
    json += String((unsigned)buf[i]);
    json += '}';
  }
  json += "]\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiStorage() {
  gWebApiLogger.logHttpRequest("GET", "/api/storage");
  String json = "{\n  \"role\": \"stage\",\n  \"storage\": ";
  stageStoreAppendJson(json);
  json += "\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiLighting() {
  gWebApiLogger.logHttpRequest("GET", "/api/lighting");
  String json = "{\n";
  json += "  \"dmx\": \"unsupported\",\n";
  json += "  \"pixelNodes\": \"unsupported\",\n";
  json += "  \"emergencyPixelsReady\": " + String(emergencyPixelsReady() ? "true" : "false") + ",\n";
  json += "  \"emergencyPixelsWhite\": " + String(emergencyPixelsWhiteActive() ? "true" : "false") + ",\n";
  json += "  \"emergencyActive\": " + String(emergencyLocked ? "true" : "false") + ",\n";
  json += "  \"devices\": ";
  appendPluginDevices(json);
  json += "\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleApiCommand(const String &cmdIn) {
  String cmd = cmdIn;
  cmd.trim();
  normalizeWebCmd(cmd);
  gWebApiLogger.logHttpRequest("POST", "/api/command");
  if (!webCommandAllowed(cmd)) {
    const char *err =
        "{\"ok\":false,\"error\":\"command_not_allowed\","
        "\"note\":\"WebUI may only dispatch the P4-validated whitelist\"}\n";
    sendWebr(403, "application/json", err, strlen(err));
    return;
  }
  String replies;
  replies.reserve(512);
  stageWebDispatchCommand(cmd.c_str(), &replies);
  String json = "{\n  \"ok\": true,\n  \"cmd\": ";
  appendQuoted(json, cmd.c_str());
  json += ",\n  \"replies\": ";
  appendQuoted(json, replies.c_str());
  json += ",\n  \"showState\": \"";
  json += showStateName(gRuntime.rt.state);
  json += "\",\n  \"emergencyActive\": ";
  json += emergencyLocked ? "true" : "false";
  json += "\n}\n";
  sendWebr(200, "application/json", json.c_str(), json.length());
}

static void handleStaticFile(const String &urlPath) {
#if !SHOWDUINO_P4_STATIC_WEBUI
  (void)urlPath;
  const char *err =
      "{\"error\":\"frontend_moved\",\"host\":\"comms-s3\","
      "\"note\":\"Static WebUI is served from Communications S3 PROGMEM\"}\n";
  sendWebr(410, "application/json", err, strlen(err));
#else
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
#endif
}

void webApiBegin(unsigned long bootMs) {
  sBootMs = bootMs;
  sOriginReady = true;
  gWebApiLogger.log(WEB_LOG_INFO, "WebUI", "P4 HTTP origin ready");
  Serial.println("[WEB] P4 API origin ready (static frontend lives on Comms S3 PROGMEM)");
  if (stageStorageIsReady()) {
    Serial.println("[WEB] SD mounted — productions/logs remain P4-owned");
  } else {
    Serial.println("[WEB] SD not mounted — API still answers without static files");
  }
}

bool webApiDispatch(const char *method, const char *pathIn, const char *body) {
  String m = method ? method : "GET";
  m.toUpperCase();
  String path = pathIn ? pathIn : "/";
  path.trim();
  while (path.startsWith("//")) path = path.substring(1);
  if (path.length() == 0) path = "/";

  if (m == "POST") {
    if (path.startsWith("/api/command/")) {
      handleApiCommand(path.substring(strlen("/api/command/")));
      return true;
    }
    if (path.startsWith("/api/command")) {
      String cmd;
      if (body && body[0]) {
        const String b = body;
        const int key = b.indexOf("\"cmd\"");
        const int colon = key >= 0 ? b.indexOf(':', key) : -1;
        const int q1 = colon >= 0 ? b.indexOf('"', colon) : -1;
        const int q2 = q1 >= 0 ? b.indexOf('"', q1 + 1) : -1;
        if (q1 >= 0 && q2 > q1) cmd = b.substring(q1 + 1, q2);
      }
      if (cmd.length() == 0) {
        const char *err = "{\"ok\":false,\"error\":\"missing_cmd\"}\n";
        sendWebr(400, "application/json", err, strlen(err));
        return true;
      }
      handleApiCommand(cmd);
      return true;
    }
    const char *err = "{\"error\":\"not_found\"}\n";
    sendWebr(404, "application/json", err, strlen(err));
    return true;
  }

  if (m != "GET") {
    const char *err = "{\"error\":\"method_not_allowed\"}\n";
    sendWebr(405, "application/json", err, strlen(err));
    return true;
  }

  if (path.startsWith("/api/system")) { handleApiSystem(); return true; }
  if (path.startsWith("/api/logs")) { handleApiLogs(); return true; }
  if (path.startsWith("/api/devices")) { handleApiDevices(); return true; }
  if (path.startsWith("/api/productions")) { handleApiProductions(); return true; }
  if (path.startsWith("/api/show")) { handleApiShow(); return true; }
  if (path.startsWith("/api/plugins")) { handleApiPlugins(); return true; }
  if (path.startsWith("/api/audio")) { handleApiAudio(); return true; }
  if (path.startsWith("/api/time")) { handleApiTime(); return true; }
  if (path.startsWith("/api/capabilities")) { handleApiCapabilities(); return true; }
  if (path.startsWith("/api/network")) { handleApiNetwork(); return true; }
  if (path.startsWith("/api/e131/channels")) { handleApiE131Channels(path); return true; }
  if (path.startsWith("/api/e131")) { handleApiE131(); return true; }
  if (path.startsWith("/api/storage")) { handleApiStorage(); return true; }
  if (path.startsWith("/api/lighting")) { handleApiLighting(); return true; }
  if (path.startsWith("/api/")) {
    const char *err = "{\"error\":\"not_found\"}\n";
    sendWebr(404, "application/json", err, strlen(err));
    return true;
  }

  handleStaticFile(path);
  return true;
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

bool webApiOriginReady() {
  return sOriginReady;
}

static bool probeMappedUrl(const char *url, char *resolved, size_t resolvedLen,
                           char *err, size_t errLen) {
  String sdPath;
  if (!mapWebuiPath(String(url), sdPath)) {
    if (err && errLen) {
      strncpy(err, "origin refused URL", errLen - 1);
      err[errLen - 1] = '\0';
    }
    return false;
  }
  if (resolved && resolvedLen) {
    strncpy(resolved, sdPath.c_str(), resolvedLen - 1);
    resolved[resolvedLen - 1] = '\0';
  }
  if (!stageStorageIsReady()) {
    if (err && errLen) {
      strncpy(err, "SD not mounted", errLen - 1);
      err[errLen - 1] = '\0';
    }
    return false;
  }
  if (!stageStorageFs().exists(sdPath.c_str())) {
    if (err && errLen) {
      strncpy(err, "mapped file missing", errLen - 1);
      err[errLen - 1] = '\0';
    }
    return false;
  }
  File f = stageStorageFs().open(sdPath.c_str(), FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    if (err && errLen) {
      strncpy(err, "mapped file open failed", errLen - 1);
      err[errLen - 1] = '\0';
    }
    return false;
  }
  uint8_t buf[64];
  int n = f.read(buf, sizeof(buf));
  f.close();
  if (n <= 0) {
    if (err && errLen) {
      strncpy(err, "mapped file empty", errLen - 1);
      err[errLen - 1] = '\0';
    }
    return false;
  }
  return true;
}

bool webApiProbePublicUrl(const char *url, char *resolved, size_t resolvedLen,
                          char *err, size_t errLen) {
  if (err && errLen) err[0] = '\0';
  if (resolved && resolvedLen) resolved[0] = '\0';
  if (!url || !url[0]) {
    if (err && errLen) {
      strncpy(err, "empty URL", errLen - 1);
      err[errLen - 1] = '\0';
    }
    return false;
  }
  if (!sOriginReady) {
    if (err && errLen) {
      strncpy(err, "origin not initialised", errLen - 1);
      err[errLen - 1] = '\0';
    }
    return false;
  }
  return probeMappedUrl(url, resolved, resolvedLen, err, errLen);
}

bool webApiProbeIndex(char *err, size_t errLen) {
  char resolved[64];
  if (webApiProbePublicUrl("/", resolved, sizeof(resolved), err, errLen)) return true;
  return webApiProbePublicUrl("/index.html", resolved, sizeof(resolved), err, errLen);
}
