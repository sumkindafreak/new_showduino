#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "../../protocol/showduino_shdo.h"

static bool loadFile(const char *path, std::string *out) {
  if (!path || !out) return false;
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !out->empty();
}

static bool loadFirstAudioExample(std::string *out, std::string *usedPath) {
  static const char *kCandidates[] = {
      "../../examples/productions/first-audio-test.shdo",
      "../examples/productions/first-audio-test.shdo",
      "examples/productions/first-audio-test.shdo",
      "first-audio-test.shdo",
      nullptr};
  for (int i = 0; kCandidates[i]; ++i) {
    if (loadFile(kCandidates[i], out)) {
      if (usedPath) *usedPath = kCandidates[i];
      return true;
    }
  }
  return false;
}

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

  static const char *kMixedPixels =
      "{\n"
      "  \"schema\": \"showduino-production-v2\",\n"
      "  \"package\": { \"version\": 2 },\n"
      "  \"project\": { \"id\": \"mixed_pixels\", \"name\": \"Mixed Pixels\" },\n"
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
      "  \"devices\": [\n"
      "    { \"id\": \"p4-show-pixels\", \"type\": \"p4-pixel-line\",\n"
      "      \"binding\": { \"route\": \"p4-show-pixels\", \"nodeId\": \"p4\", \"pixelStart\": 0, \"pixelCount\": 80 } },\n"
      "    { \"id\": \"LED-01\", \"type\": \"pixel-node\",\n"
      "      \"binding\": { \"route\": \"pixel-node\", \"nodeId\": \"LED-01\", \"pixelStart\": 0, \"pixelCount\": 100 } },\n"
      "    { \"id\": \"LED-02\", \"type\": \"pixel-node\",\n"
      "      \"binding\": { \"route\": \"pixel-node\", \"nodeId\": \"LED-02\", \"pixelStart\": 0, \"pixelCount\": 80 } },\n"
      "    { \"id\": \"audio-1\", \"type\": \"audio-node\",\n"
      "      \"binding\": { \"route\": \"audio-node\" } }\n"
      "  ],\n"
      "  \"clips\": [\n"
      "    { \"id\": \"p4-fire\", \"type\": \"pixel\", \"targetDeviceId\": \"p4-show-pixels\",\n"
      "      \"startMs\": 0, \"params\": { \"effect\": \"FIRE\", \"startPixel\": 0, \"count\": 10 } },\n"
      "    { \"id\": \"led1-flicker\", \"type\": \"pixel\", \"targetDeviceId\": \"LED-01\",\n"
      "      \"startMs\": 2500, \"params\": { \"effect\": \"FLICKER\", \"startPixel\": 0, \"count\": 20 } },\n"
      "    { \"id\": \"led2-warn\", \"type\": \"pixel\", \"targetDeviceId\": \"LED-02\",\n"
      "      \"startMs\": 4000, \"params\": { \"effect\": \"WARNING\", \"startPixel\": 0, \"count\": 8 } },\n"
      "    { \"id\": \"boom\", \"type\": \"audio\", \"targetDeviceId\": \"audio-1\",\n"
      "      \"startMs\": 1000, \"params\": { \"file\": \"door_slam.wav\", \"volume\": 80 } }\n"
      "  ]\n"
      "}\n";

  ShdoCue mixedCues[64]{};
  uint16_t mixedCount = 0;
  const ShdoStatus mixedOk = shdoCompile(kMixedPixels, std::strlen(kMixedPixels), &manifest,
                                         mixedCues, 64, &mixedCount, err, sizeof(err));
  expect(mixedOk == SHDO_OK, "mixed P4 + Pixel Node + audio SHDO compiles");
  bool sawP4 = false, sawLed01 = false, sawLed02 = false, sawAudio = false;
  for (uint16_t i = 0; i < mixedCount; ++i) {
    if (std::strstr(mixedCues[i].command, "PIXEL:SEGMENT:") != nullptr &&
        std::strstr(mixedCues[i].command, "PIXEL:NODE:") == nullptr) sawP4 = true;
    if (std::strstr(mixedCues[i].command, "PIXEL:NODE:LED-01:") != nullptr) sawLed01 = true;
    if (std::strstr(mixedCues[i].command, "PIXEL:NODE:LED-02:") != nullptr) sawLed02 = true;
    if (std::strstr(mixedCues[i].command, "AUDIO:NODE:PLAY:door_slam.wav") != nullptr) sawAudio = true;
  }
  expect(sawP4, "mixed production keeps P4 PIXEL:SEGMENT commands");
  expect(sawLed01, "mixed production keeps LED-01 PIXEL:NODE commands");
  expect(sawLed02, "mixed production keeps LED-02 PIXEL:NODE commands");
  expect(sawAudio, "mixed production keeps Audio Node commands");

  static const char *kLampNode =
      "{\n"
      "  \"schema\": \"showduino-production-v2\",\n"
      "  \"package\": { \"version\": 2 },\n"
      "  \"project\": { \"id\": \"lamp_bench\", \"name\": \"Lamp Bench\" },\n"
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
      "    \"id\": \"carbide\",\n"
      "    \"type\": \"lamp\",\n"
      "    \"binding\": { \"route\": \"lamp-node\", \"nodeId\": \"LAMP-01\" }\n"
      "  }],\n"
      "  \"clips\": [{\n"
      "    \"id\": \"light\",\n"
      "    \"type\": \"lamp\",\n"
      "    \"targetDeviceId\": \"carbide\",\n"
      "    \"startMs\": 12000,\n"
      "    \"durationMs\": 5000,\n"
      "    \"params\": { \"effect\": \"IGNITE\" }\n"
      "  },{\n"
      "    \"id\": \"flutter\",\n"
      "    \"type\": \"lamp\",\n"
      "    \"targetDeviceId\": \"carbide\",\n"
      "    \"startMs\": 14000,\n"
      "    \"params\": { \"effect\": \"UNSTABLE_FLAME\", \"brightness\": 80 }\n"
      "  }]\n"
      "}\n";
  ShdoCue lampCues[16]{};
  uint16_t lampCount = 0;
  const ShdoStatus lampOk = shdoCompile(kLampNode, std::strlen(kLampNode), &manifest,
                                        lampCues, 16, &lampCount, err, sizeof(err));
  expect(lampOk == SHDO_OK, "lamp-node SHDO v2 compiles");
  bool sawIgnite = false, sawExt = false, sawUnstable = false;
  for (uint16_t i = 0; i < lampCount; ++i) {
    if (std::strstr(lampCues[i].command, "LAMP:NODE:LAMP-01:IGNITE") != nullptr) sawIgnite = true;
    if (std::strstr(lampCues[i].command, "LAMP:NODE:LAMP-01:EXTINGUISH") != nullptr) sawExt = true;
    if (std::strstr(lampCues[i].command, "LAMP:NODE:LAMP-01:FX:UNSTABLE_FLAME") != nullptr) {
      sawUnstable = true;
    }
  }
  expect(sawIgnite, "IGNITE compiles to LAMP:NODE:LAMP-01:IGNITE");
  expect(sawExt, "duration end compiles EXTINGUISH");
  expect(sawUnstable, "UNSTABLE_FLAME compiles as FX token");

  std::string example;
  std::string examplePath;
  expect(loadFirstAudioExample(&example, &examplePath),
         "canonical first-audio-test.shdo is loadable");
  if (!example.empty()) {
    ShdoManifest hello{};
    ShdoCue helloCues[16]{};
    uint16_t helloCount = 0;
    const ShdoStatus helloOk = shdoCompile(example.c_str(), example.size(), &hello,
                                           helloCues, 16, &helloCount, err, sizeof(err));
    expect(helloOk == SHDO_OK, "first-audio-test.shdo compiles");
    expect(std::strcmp(hello.productionId, "first-audio-test") == 0,
           "first-audio-test keeps production id");
    expect(std::strcmp(hello.name, "First Audio Test") == 0,
           "first-audio-test keeps production name");
    expect(helloCount == 4, "first-audio-test compiles four runtime cues");

    bool sawVol = false, sawPlay = false, sawStop = false, sawEnd = false;
    bool sawPixel = false, sawLamp = false, sawGpio = false, sawEmerg = false;
    uint32_t lastMs = 0;
    std::printf("first-audio-test compiled from %s\n", examplePath.c_str());
    for (uint16_t i = 0; i < helloCount; ++i) {
      std::printf("  %lu ms -> %s\n",
                  (unsigned long)helloCues[i].timeMs, helloCues[i].command);
      if (helloCues[i].timeMs == 3000 &&
          std::strcmp(helloCues[i].command, "AUDIO:NODE:VOLUME:80") == 0) {
        sawVol = true;
      }
      if (helloCues[i].timeMs == 3000 &&
          std::strcmp(helloCues[i].command, "AUDIO:NODE:PLAY:test.wav") == 0) {
        sawPlay = true;
      }
      if (helloCues[i].timeMs == 25000 &&
          std::strcmp(helloCues[i].command, "AUDIO:NODE:STOP") == 0) {
        sawStop = true;
      }
      if (helloCues[i].timeMs == 30000 &&
          std::strstr(helloCues[i].command, "INTERNAL:LOG:") != nullptr) {
        sawEnd = true;
      }
      if (std::strstr(helloCues[i].command, "PIXEL:") != nullptr) sawPixel = true;
      if (std::strstr(helloCues[i].command, "LAMP:") != nullptr) sawLamp = true;
      if (std::strstr(helloCues[i].command, "GPIO") != nullptr) sawGpio = true;
      if (std::strstr(helloCues[i].command, "EMERGENCY") != nullptr) sawEmerg = true;
      if (helloCues[i].timeMs > lastMs) lastMs = helloCues[i].timeMs;
    }
    expect(sawVol, "first-audio-test emits VOLUME:80 at 3000 ms");
    expect(sawPlay, "first-audio-test emits PLAY:test.wav at 3000 ms");
    expect(sawStop, "first-audio-test emits STOP at 25000 ms");
    expect(sawEnd, "first-audio-test ends at 30000 ms");
    expect(lastMs == 30000, "first-audio-test duration is last cue at 30000 ms");
    expect(!sawPixel && !sawLamp && !sawGpio && !sawEmerg,
           "first-audio-test has no pixel, lamp, GPIO, or emergency cues");
    expect(example.find("pixel-node") == std::string::npos &&
               example.find("lamp-node") == std::string::npos &&
               example.find("\"type\": \"pixel\"") == std::string::npos &&
               example.find("\"type\": \"lamp\"") == std::string::npos,
           "first-audio-test authoring binds only the Audio Node");
  }

  return failures ? 1 : 0;
}
