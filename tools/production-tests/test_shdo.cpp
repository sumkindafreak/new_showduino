#include <cstdio>
#include <cstring>

#include "../../protocol/showduino_shdo.h"

static int failures = 0;

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

static const char *kMinimal =
    "{\n"
    "  \"schema\": \"showduino-production-v2\",\n"
    "  \"package\": { \"version\": 2 },\n"
    "  \"project\": { \"id\": \"bench_one\", \"name\": \"Bench One\" },\n"
    "  \"architecture\": {\n"
    "    \"runtimeAuthority\": \"esp32-p4-show-engine\",\n"
    "    \"transport\": \"esp32-s3-comms-controller\"\n"
    "  },\n"
    "  \"safety\": {\n"
    "    \"policy\": \"firmware-authoritative\",\n"
    "    \"productionCannotDisable\": true,\n"
    "    \"emergency\": {\n"
    "      \"autoResume\": false,\n"
    "      \"requiresManualClear\": true,\n"
    "      \"stopTimeline\": true,\n"
    "      \"pixelOverride\": \"all-white\"\n"
    "    }\n"
    "  },\n"
    "  \"clips\": [{ \"id\": \"c1\", \"type\": \"test\", \"startMs\": 0, \"name\": \"hello\" }]\n"
    "}\n";

static const char *kWeakSafety =
    "{\n"
    "  \"schema\": \"showduino-production-v2\",\n"
    "  \"package\": { \"version\": 2 },\n"
    "  \"project\": { \"id\": \"unsafe\", \"name\": \"Unsafe\" },\n"
    "  \"safety\": {\n"
    "    \"policy\": \"firmware-authoritative\",\n"
    "    \"productionCannotDisable\": true,\n"
    "    \"emergency\": { \"autoResume\": true, \"requiresManualClear\": true, \"stopTimeline\": true, \"pixelOverride\": \"all-white\" }\n"
    "  },\n"
    "  \"clips\": [{ \"id\": \"c1\", \"type\": \"test\", \"startMs\": 0, \"name\": \"hello\" }]\n"
    "}\n";

int main() {
  ShdoManifest manifest{};
  ShdoCue cues[8]{};
  uint16_t count = 0;
  char err[96] = "";
  const ShdoStatus ok = shdoCompile(kMinimal, std::strlen(kMinimal), &manifest, cues, 8,
                                    &count, err, sizeof(err));
  expect(ok == SHDO_OK, "minimal SHDO compiles");
  expect(std::strcmp(manifest.productionId, "bench_one") == 0, "production id");
  expect(count == 1, "one compiled cue");

  char manJson[1024];
  char tlJson[2048];
  expect(shdoWriteManifestJson(manJson, sizeof(manJson), &manifest) > 0, "writes manifest json");
  expect(shdoWriteTimelineJson(tlJson, sizeof(tlJson), cues, count) > 0, "writes timeline json");
  expect(std::strstr(manJson, "\"formatVersion\": 1") != nullptr, "manifest format v1");
  expect(std::strstr(tlJson, "INTERNAL:") != nullptr, "timeline has INTERNAL command");

  const ShdoStatus weak = shdoCompile(kWeakSafety, std::strlen(kWeakSafety), &manifest, cues, 8,
                                      &count, err, sizeof(err));
  expect(weak == SHDO_SAFETY_WEAKENED, "rejects autoResume");
  return failures ? 1 : 0;
}
