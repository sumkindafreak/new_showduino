#include "ProductionDeploy.h"

#include "ProductionStore.h"
#include "storage/StageStore.h"
#include "../../../protocol/showduino_deploy.h"
#include "../../../protocol/showduino_shdo.h"
#include "../../../protocol/showduino_version.h"

#include <esp_heap_caps.h>
#include <string.h>

extern ProductionStore gProductionStore;
extern bool emergencyLocked;

enum class DeploySession : uint8_t { Idle = 0, Open };

static DeploySession sSession = DeploySession::Idle;
static uint8_t *sDoc = nullptr;
static size_t sExpected = 0;
static size_t sUsed = 0;
static uint32_t sCrc = 0;
static uint32_t sStartMs = 0;
static char sLastId[SHOWDUINO_SHDO_ID_MAX] = "";
static char sLastError[96] = "idle";
static uint16_t sLastCues = 0;

static void freeDoc() {
  if (sDoc) {
    heap_caps_free(sDoc);
    sDoc = nullptr;
  }
  sExpected = 0;
  sUsed = 0;
  sCrc = 0;
  sSession = DeploySession::Idle;
}

static void setErr(const char *msg) {
  strncpy(sLastError, msg ? msg : "error", sizeof(sLastError) - 1);
  sLastError[sizeof(sLastError) - 1] = '\0';
}

static void reply(int *statusOut, String *jsonOut, int status, const String &json) {
  if (statusOut) *statusOut = status;
  if (jsonOut) *jsonOut = json;
}

static String quoted(const char *s) {
  String out = "\"";
  if (!s) {
    out += "\"";
    return out;
  }
  while (*s) {
    char c = *s++;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((unsigned char)c < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }
  out += '"';
  return out;
}

static bool timedOut() {
  if (sSession != DeploySession::Open) return false;
  return (millis() - sStartMs) > SHOWDUINO_DEPLOY_SESSION_TIMEOUT_MS;
}

static String statusJson(bool ok, const char *state) {
  String json = "{\n  \"ok\": ";
  json += ok ? "true" : "false";
  json += ",\n  \"state\": ";
  json += quoted(state);
  json += ",\n  \"productVersion\": \"" SHOWDUINO_PLATFORM_VERSION "\"";
  json += ",\n  \"autoStart\": false,\n  \"autoLoad\": false";
  json += ",\n  \"bytes\": ";
  json += String((unsigned long)sUsed);
  json += ",\n  \"expected\": ";
  json += String((unsigned long)sExpected);
  json += ",\n  \"productionId\": ";
  json += quoted(sLastId);
  json += ",\n  \"cueCount\": ";
  json += String((unsigned)sLastCues);
  json += ",\n  \"error\": ";
  json += quoted(sLastError);
  json += ",\n  \"emergencyActive\": ";
  json += emergencyLocked ? "true" : "false";
  json += "\n}\n";
  return json;
}

static bool handleBegin(const char *body, int *statusOut, String *jsonOut) {
  if (emergencyLocked) {
    setErr("emergency_active");
    reply(statusOut, jsonOut, 409, statusJson(false, "aborted"));
    return true;
  }
  if (!stageStoreWritable()) {
    setErr("storage_not_writable");
    reply(statusOut, jsonOut, 503, statusJson(false, "storage"));
    return true;
  }
  uint32_t bytes = 0;
  uint32_t crc = 0;
  if (!body || !showduino_json_u32_field(body, "bytes", &bytes) ||
      !showduino_json_u32_field(body, "crc32", &crc) ||
      bytes == 0 || bytes > SHOWDUINO_SHDO_MAX_BYTES) {
    setErr("invalid_begin");
    reply(statusOut, jsonOut, 400, statusJson(false, "error"));
    return true;
  }
  freeDoc();
  sDoc = (uint8_t *)heap_caps_malloc(bytes + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!sDoc) sDoc = (uint8_t *)malloc(bytes + 1);
  if (!sDoc) {
    setErr("no_memory");
    reply(statusOut, jsonOut, 507, statusJson(false, "error"));
    return true;
  }
  sExpected = bytes;
  sUsed = 0;
  sCrc = crc;
  sStartMs = millis();
  sSession = DeploySession::Open;
  sLastId[0] = '\0';
  sLastCues = 0;
  setErr("OK");
  reply(statusOut, jsonOut, 200, statusJson(true, "open"));
  return true;
}

static bool handleChunk(const char *body, int *statusOut, String *jsonOut) {
  if (timedOut()) {
    freeDoc();
    setErr("session_timeout");
    reply(statusOut, jsonOut, 408, statusJson(false, "timeout"));
    return true;
  }
  if (sSession != DeploySession::Open || !sDoc) {
    setErr("no_session");
    reply(statusOut, jsonOut, 409, statusJson(false, "error"));
    return true;
  }
  char hex[1600];
  if (!body || !showduino_json_string_field(body, "hex", hex, sizeof(hex))) {
    setErr("invalid_chunk");
    reply(statusOut, jsonOut, 400, statusJson(false, "error"));
    return true;
  }
  unsigned char decoded[SHOWDUINO_DEPLOY_CHUNK_MAX];
  size_t n = 0;
  if (!showduino_hex_decode(hex, decoded, sizeof(decoded), &n) || n == 0) {
    setErr("invalid_hex");
    reply(statusOut, jsonOut, 400, statusJson(false, "error"));
    return true;
  }
  if (sUsed + n > sExpected) {
    freeDoc();
    setErr("overflow");
    reply(statusOut, jsonOut, 413, statusJson(false, "error"));
    return true;
  }
  memcpy(sDoc + sUsed, decoded, n);
  sUsed += n;
  sStartMs = millis();
  setErr("OK");
  reply(statusOut, jsonOut, 200, statusJson(true, "open"));
  return true;
}

static bool handleCommit(const char *body, int *statusOut, String *jsonOut) {
  if (emergencyLocked) {
    freeDoc();
    setErr("emergency_active");
    reply(statusOut, jsonOut, 409, statusJson(false, "aborted"));
    return true;
  }
  if (timedOut()) {
    freeDoc();
    setErr("session_timeout");
    reply(statusOut, jsonOut, 408, statusJson(false, "timeout"));
    return true;
  }
  if (sSession != DeploySession::Open || !sDoc) {
    setErr("no_session");
    reply(statusOut, jsonOut, 409, statusJson(false, "error"));
    return true;
  }
  uint32_t crc = sCrc;
  if (body) showduino_json_u32_field(body, "crc32", &crc);
  if (sUsed != sExpected) {
    setErr("incomplete");
    reply(statusOut, jsonOut, 409, statusJson(false, "error"));
    return true;
  }
  sDoc[sUsed] = 0;
  const uint32_t got = showduino_crc32_ieee(sDoc, sUsed);
  if (got != crc) {
    setErr("crc_mismatch");
    reply(statusOut, jsonOut, 400, statusJson(false, "error"));
    return true;
  }

  ShdoManifest manifest{};
  ShdoCue *cues = (ShdoCue *)heap_caps_calloc(
      SHOWDUINO_SHDO_MAX_CUES, sizeof(ShdoCue), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!cues) cues = (ShdoCue *)calloc(SHOWDUINO_SHDO_MAX_CUES, sizeof(ShdoCue));
  if (!cues) {
    freeDoc();
    setErr("no_memory");
    reply(statusOut, jsonOut, 507, statusJson(false, "error"));
    return true;
  }

  uint16_t cueCount = 0;
  char compileErr[96];
  const ShdoStatus st = shdoCompile((const char *)sDoc, sUsed, &manifest, cues,
                                    SHOWDUINO_SHDO_MAX_CUES, &cueCount,
                                    compileErr, sizeof(compileErr));
  if (st != SHDO_OK) {
    heap_caps_free(cues);
    freeDoc();
    setErr(compileErr[0] ? compileErr : shdoStatusName(st));
    reply(statusOut, jsonOut, 422, statusJson(false, "rejected"));
    return true;
  }
  if (emergencyLocked) {
    heap_caps_free(cues);
    freeDoc();
    setErr("emergency_active");
    reply(statusOut, jsonOut, 409, statusJson(false, "aborted"));
    return true;
  }

  char *manifestJson = (char *)heap_caps_malloc(SHOWDUINO_MANIFEST_MAX_BYTES + 1,
                                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!manifestJson) manifestJson = (char *)malloc(SHOWDUINO_MANIFEST_MAX_BYTES + 1);
  char *timelineJson = (char *)heap_caps_malloc(SHOWDUINO_TIMELINE_MAX_BYTES + 1,
                                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!timelineJson) timelineJson = (char *)malloc(SHOWDUINO_TIMELINE_MAX_BYTES + 1);
  if (!manifestJson || !timelineJson) {
    if (manifestJson) heap_caps_free(manifestJson);
    if (timelineJson) heap_caps_free(timelineJson);
    free(cues);
    freeDoc();
    setErr("no_memory");
    reply(statusOut, jsonOut, 507, statusJson(false, "error"));
    return true;
  }

  const size_t manLen = shdoWriteManifestJson(manifestJson, SHOWDUINO_MANIFEST_MAX_BYTES,
                                              &manifest);
  const size_t tlLen = shdoWriteTimelineJson(timelineJson, SHOWDUINO_TIMELINE_MAX_BYTES,
                                             cues, cueCount);
  heap_caps_free(cues);
  if (!manLen || !tlLen) {
    heap_caps_free(manifestJson);
    heap_caps_free(timelineJson);
    freeDoc();
    setErr("compile_write_failed");
    reply(statusOut, jsonOut, 500, statusJson(false, "error"));
    return true;
  }

  if (emergencyLocked) {
    heap_caps_free(manifestJson);
    heap_caps_free(timelineJson);
    freeDoc();
    setErr("emergency_active");
    reply(statusOut, jsonOut, 409, statusJson(false, "aborted"));
    return true;
  }

  const ProductionStoreResult stored = gProductionStore.persistRuntimeFiles(
      manifest.productionId, manifestJson, manLen, timelineJson, tlLen);
  heap_caps_free(manifestJson);
  heap_caps_free(timelineJson);
  strncpy(sLastId, manifest.productionId, sizeof(sLastId) - 1);
  sLastCues = cueCount;
  freeDoc();
  if (stored != ProductionStoreResult::Ok) {
    setErr(gProductionStore.lastError());
    reply(statusOut, jsonOut, 500, statusJson(false, "store_failed"));
    return true;
  }
  setErr("OK");
  Serial.printf("[DEPLOY] persisted %s cues=%u (not loaded)\n",
                sLastId, (unsigned)sLastCues);
  reply(statusOut, jsonOut, 200, statusJson(true, "committed"));
  return true;
}

static bool handleAbort(int *statusOut, String *jsonOut) {
  freeDoc();
  setErr("aborted");
  reply(statusOut, jsonOut, 200, statusJson(true, "idle"));
  return true;
}

bool productionDeployTry(const char *method, const char *path,
                         const char *body, size_t len,
                         int *statusOut, String *jsonOut) {
  (void)len;
  if (!path) return false;
  String m = method ? method : "GET";
  m.toUpperCase();
  if (strcmp(path, SHOWDUINO_DEPLOY_STATUS_PATH) == 0 && m == "GET") {
    if (timedOut()) {
      freeDoc();
      setErr("session_timeout");
    }
    reply(statusOut, jsonOut, 200, statusJson(sSession == DeploySession::Open, 
          sSession == DeploySession::Open ? "open" : "idle"));
    return true;
  }
  if (m != "POST") return false;
  if (strcmp(path, SHOWDUINO_DEPLOY_BEGIN_PATH) == 0) {
    return handleBegin(body, statusOut, jsonOut);
  }
  if (strcmp(path, SHOWDUINO_DEPLOY_CHUNK_PATH) == 0) {
    return handleChunk(body, statusOut, jsonOut);
  }
  if (strcmp(path, SHOWDUINO_DEPLOY_COMMIT_PATH) == 0) {
    return handleCommit(body, statusOut, jsonOut);
  }
  if (strcmp(path, SHOWDUINO_DEPLOY_ABORT_PATH) == 0) {
    return handleAbort(statusOut, jsonOut);
  }
  return false;
}
