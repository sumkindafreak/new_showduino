#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "ProductionStore.h"

uint32_t gShowduinoTestMillis = 0;
ShowduinoTestSerial Serial;

static int failures = 0;

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

static void writeText(const std::filesystem::path &path, const char *text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << text;
}

static void writeSized(const std::filesystem::path &path, size_t size) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  const std::string block(1024, 'x');
  while (size >= block.size()) {
    output.write(block.data(), static_cast<std::streamsize>(block.size()));
    size -= block.size();
  }
  output.write(block.data(), static_cast<std::streamsize>(size));
}

static std::string manifest(const char *id, const char *timeline = "timeline.json") {
  return std::string("{\"formatVersion\":1,\"productionId\":\"") + id +
      "\",\"name\":\"Fixture\",\"timeline\":\"" + timeline + "\"}";
}

static void addFixture(const std::filesystem::path &root, const char *id,
                       const char *timelineJson, bool addManifest = true,
                       bool addTimeline = true) {
  const auto folder = root / "showduino" / "productions" / id;
  std::filesystem::create_directories(folder);
  if (addManifest) {
    const std::string text = manifest(id);
    writeText(folder / "manifest.json", text.c_str());
  }
  if (addTimeline) writeText(folder / "timeline.json", timelineJson);
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    "showduino_production_store_tests";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);

  const char *validTimeline =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"one\",\"timeMs\":0,\"type\":\"TEST\",\"value\":\"start\"}]}";
  addFixture(root, "system_test", validTimeline);
  addFixture(root, "missing_manifest", validTimeline, false, true);
  addFixture(root, "missing_timeline", validTimeline, true, false);
  addFixture(root, "empty_timeline", "");
  addFixture(root, "bad_timeline", "{broken");
  addFixture(root, "duplicate", "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"same\",\"timeMs\":0,\"type\":\"TEST\"},"
      "{\"id\":\"same\",\"timeMs\":1,\"type\":\"TEST\"}]}");
  addFixture(root, "descending", "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"late\",\"timeMs\":2,\"type\":\"TEST\"},"
      "{\"id\":\"early\",\"timeMs\":1,\"type\":\"TEST\"}]}");

  const auto badManifestFolder = root / "showduino" / "productions" / "bad_manifest";
  std::filesystem::create_directories(badManifestFolder);
  writeText(badManifestFolder / "manifest.json", "{broken");
  const auto emptyManifestFolder = root / "showduino" / "productions" /
                                   "empty_manifest";
  std::filesystem::create_directories(emptyManifestFolder);
  writeText(emptyManifestFolder / "manifest.json", "");
  const auto largeManifestFolder = root / "showduino" / "productions" /
                                   "large_manifest";
  std::filesystem::create_directories(largeManifestFolder);
  writeSized(largeManifestFolder / "manifest.json",
             SHOWDUINO_MANIFEST_MAX_BYTES + 1U);

  const auto largeTimelineFolder = root / "showduino" / "productions" /
                                   "large_timeline";
  std::filesystem::create_directories(largeTimelineFolder);
  const std::string largeTimelineManifest = manifest("large_timeline");
  writeText(largeTimelineFolder / "manifest.json", largeTimelineManifest.c_str());
  writeSized(largeTimelineFolder / "timeline.json",
             SHOWDUINO_TIMELINE_MAX_BYTES + 1U);
  for (unsigned i = 0; i <= SHOWDUINO_PRODUCTION_MAX_FOUND; ++i) {
    char id[24];
    std::snprintf(id, sizeof(id), "limit_%02u", i);
    addFixture(root, id, validTimeline);
  }

  fs::FS fakeFs(root);
  ProductionStore store;
  expect(store.begin(fakeFs), "initialize and scan production storage");
  expect(store.count() == SHOWDUINO_PRODUCTION_MAX_FOUND,
         "production discovery is bounded at 32 entries");

  ProductionPackage package{};
  expect(store.load("system_test", &package) == ProductionStoreResult::Ok,
         "load valid production from SD fixture");
  expect(package.timeline.cueCount == 1,
         "valid production exposes parsed cue count");
  store.markLoaded(package.manifest);
  expect(store.hasLoaded(), "mark valid production loaded only after parse");
  store.release(&package);

  expect(store.load("../escape", &package) == ProductionStoreResult::NotFound,
         "reject invalid production ID");
  expect(store.load("missing_manifest", &package) ==
             ProductionStoreResult::MissingManifest,
         "report missing manifest");
  expect(store.load("bad_manifest", &package) ==
             ProductionStoreResult::InvalidManifest,
         "report malformed manifest");
  expect(store.load("empty_manifest", &package) ==
             ProductionStoreResult::InvalidManifest,
         "report empty manifest");
  expect(store.load("large_manifest", &package) ==
             ProductionStoreResult::TooLarge,
         "reject manifest above the 4 KiB limit");
  expect(store.load("missing_timeline", &package) ==
             ProductionStoreResult::MissingTimeline,
         "report missing timeline");
  expect(store.load("bad_timeline", &package) ==
             ProductionStoreResult::InvalidTimeline,
         "report malformed timeline");
  expect(store.load("empty_timeline", &package) ==
             ProductionStoreResult::InvalidTimeline,
         "report empty timeline");
  expect(store.load("large_timeline", &package) ==
             ProductionStoreResult::TooLarge,
         "reject timeline above the 128 KiB limit");
  expect(store.load("duplicate", &package) ==
             ProductionStoreResult::DuplicateCueId,
         "report duplicate cue ID");
  expect(store.load("descending", &package) ==
             ProductionStoreResult::InvalidCueTime,
         "report descending cue time");
  expect(store.hasLoaded(), "failed load preserves loaded production metadata");

  store.unload();
  expect(!store.hasLoaded(), "unload clears loaded production metadata");
  std::filesystem::remove_all(root, ec);

  if (failures == 0) {
    std::printf("All production store tests passed.\n");
    return 0;
  }
  std::printf("%d production store test(s) failed.\n", failures);
  return 1;
}
