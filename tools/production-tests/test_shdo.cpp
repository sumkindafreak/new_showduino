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

  static const char *kPixelNode =
      "{\n"
      "  \"schema\": \"showduino-production-v2\",\n"
      "  \"package\": { \"version\": 2 },\n"
      "  \"project\": { \"id\": \"pixel_node_bench\", \"name\": \"Pixel Node Bench\" },\n"
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
      "  \"devices\": [{\n"
      "    \"id\": \"crypt-line\",\n"
      "    \"type\": \"pixel\",\n"
      "    \"binding\": { \"route\": \"pixel-node\", \"nodeId\": \"LED-03\", \"pixelStart\": 0, \"pixelCount\": 100 }\n"
      "  }],\n"
      "  \"clips\": [{\n"
      "    \"id\": \"flicker-door\",\n"
      "    \"type\": \"pixel\",\n"
      "    \"targetDeviceId\": \"crypt-line\",\n"
      "    \"startMs\": 0,\n"
      "    \"params\": { \"effect\": \"FLICKER\", \"startPixel\": 0, \"count\": 20, \"r\": 255, \"g\": 0, \"b\": 0, \"brightness\": 160, \"speed\": 40 }\n"
      "  }]\n"
      "}\n";

  ShdoCue pixelCues[32]{};
  uint16_t pixelCount = 0;
  const ShdoStatus pixelOk = shdoCompile(kPixelNode, std::strlen(kPixelNode), &manifest,
                                         pixelCues, 32, &pixelCount, err, sizeof(err));
  expect(pixelOk == SHDO_OK, "pixel-node SHDO compiles");
  expect(pixelCount >= 9, "pixel-node emits segment command chain");
  bool sawNodePrefix = false;
  bool sawFlicker = false;
  for (uint16_t i = 0; i < pixelCount; ++i) {
    if (std::strstr(pixelCues[i].command, "PIXEL:NODE:LED-03:") != nullptr) sawNodePrefix = true;
    if (std::strstr(pixelCues[i].command, ":FX:FLICKER") != nullptr) sawFlicker = true;
  }
  expect(sawNodePrefix, "compiled commands target PIXEL:NODE:LED-03");
  expect(sawFlicker, "compiled FX is FLICKER");

  static const char *kMissingPixel =
      "{\n"
      "  \"schema\": \"showduino-production-v2\",\n"
      "  \"package\": { \"version\": 2 },\n"
      "  \"project\": { \"id\": \"missing_led\", \"name\": \"Missing LED\" },\n"
      "  \"architecture\": {\n"
      "    \"runtimeAuthority\": \"esp32-p4-show-engine\",\n"
      "    \"transport\": \"esp32-s3-comms-controller\"\n"
      "  },\n"
      "  \"safety\": {\n"
      "    \"policy\": \"firmware-authoritative\",\n"
      "    \"productionCannotDisable\": true,\n"
      "    \"emergency\": { \"autoResume\": false, \"requiresManualClear\": true, \"stopTimeline\": true, \"pixelOverride\": \"all-white\" }\n"
      "  },\n"
      "  \"clips\": [{\n"
      "    \"id\": \"c1\",\n"
      "    \"type\": \"pixel\",\n"
      "    \"targetDeviceId\": \"led-03\",\n"
      "    \"startMs\": 0,\n"
      "    \"params\": { \"effect\": \"FLICKER\" }\n"
      "  }]\n"
      "}\n";
  const ShdoStatus missing = shdoCompile(kMissingPixel, std::strlen(kMissingPixel), &manifest,
                                         pixelCues, 32, &pixelCount, err, sizeof(err));
  expect(missing == SHDO_MISSING_DEVICE, "pixel clip without bound device is SHDO_MISSING_DEVICE");

  return failures ? 1 : 0;
}
