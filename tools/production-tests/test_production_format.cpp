#include <cstdio>
#include <cstring>
#include <string>

#include "ProductionFormat.h"

static int failures = 0;

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

static bool parseManifest(const char *json, ProductionParseResult *result = nullptr) {
  ProductionManifest manifest;
  return productionParseManifest(json, std::strlen(json), &manifest, result);
}

static bool parseTimeline(const char *json, ProductionCue *cues,
                          ProductionTimeline *timeline,
                          ProductionParseResult *result = nullptr) {
  return productionParseTimeline(json, std::strlen(json), cues,
                                 SHOWDUINO_PRODUCTION_MAX_CUES,
                                 timeline, result);
}

int main() {
  const char *validManifest =
      "{\"formatVersion\":1,\"productionId\":\"system_test\","
      "\"name\":\"System Test\",\"description\":\"Safe test\","
      "\"author\":\"Showduino\",\"timeline\":\"timeline.json\","
      "\"revision\":1}";
  ProductionManifest manifest;
  ProductionParseResult result = ProductionParseResult::Ok;
  expect(productionParseManifest(validManifest, std::strlen(validManifest),
                                 &manifest, &result), "valid manifest");
  expect(std::strcmp(manifest.productionId, "system_test") == 0,
         "manifest preserves production ID");
  expect(!parseManifest("{\"formatVersion\":1}"), "manifest rejects missing fields");
  expect(!parseManifest("{\"formatVersion\":2,\"productionId\":\"x\","
                        "\"name\":\"X\",\"timeline\":\"timeline.json\"}", &result) &&
             result == ProductionParseResult::UnsupportedVersion,
         "manifest rejects unsupported version");
  expect(!parseManifest("{\"formatVersion\":1,\"productionId\":\"../bad\","
                        "\"name\":\"X\",\"timeline\":\"timeline.json\"}"),
         "manifest rejects path-like production ID");
  expect(!parseManifest("{\"formatVersion\":1,\"productionId\":\"safe\","
                        "\"name\":\"X\",\"timeline\":\"../timeline.json\"}"),
         "manifest rejects timeline traversal");
  expect(!parseManifest("{broken"), "manifest rejects malformed JSON");
  std::string longNameManifest =
      "{\"formatVersion\":1,\"productionId\":\"safe\",\"name\":\"" +
      std::string(SHOWDUINO_PRODUCTION_NAME_MAX, 'n') +
      "\",\"timeline\":\"timeline.json\"}";
  expect(!parseManifest(longNameManifest.c_str()),
         "manifest rejects strings above their fixed bounds");
  expect(!parseManifest("{\"formatVersion\":1,\"productionId\":\"safe\"," 
                        "\"name\":\"X\",\"timeline\":\"timeline.json\"," 
                        "\"extra\":[[[[[[[[[[0]]]]]]]]]]}"),
         "manifest rejects excessive JSON nesting");

  const char *validTimeline =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"a\",\"timeMs\":0,\"type\":\"TEST\",\"value\":\"start\"},"
      "{\"id\":\"b\",\"timeMs\":1000,\"type\":\"LOG\","
      "\"target\":\"diagnostic.timeline\",\"action\":\"LOG\","
      "\"value\":\"next\",\"parameters\":{\"level\":\"info\"}}]}";
  ProductionCue cues[SHOWDUINO_PRODUCTION_MAX_CUES]{};
  ProductionTimeline timeline;
  result = ProductionParseResult::Ok;
  expect(parseTimeline(validTimeline, cues, &timeline, &result), "valid timeline");
  expect(timeline.cueCount == 2, "timeline cue count");
  expect(std::strcmp(cues[1].target, "diagnostic.timeline") == 0,
         "timeline preserves logical target");
  expect(std::strcmp(cues[0].command, "INTERNAL:TEST:a:start") == 0,
         "timeline builds safe internal command");
  expect(std::strcmp(cues[1].command, "INTERNAL:LOG:b:next") == 0,
         "timeline preserves LOG cue type");

  const char *optionalValue =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"no_message\",\"timeMs\":0,\"type\":\"TEST\"}]}";
  expect(parseTimeline(optionalValue, cues, &timeline, &result),
         "timeline permits optional cue value");
  expect(std::strcmp(cues[0].command, "INTERNAL:TEST:no_message:") == 0,
         "optional value produces a safe empty message");

  const char *duplicate =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"same\",\"timeMs\":0,\"type\":\"TEST\",\"value\":\"a\"},"
      "{\"id\":\"same\",\"timeMs\":1,\"type\":\"TEST\",\"value\":\"b\"}]}";
  result = ProductionParseResult::Ok;
  expect(!parseTimeline(duplicate, cues, &timeline, &result) &&
             result == ProductionParseResult::DuplicateCueId,
         "timeline rejects duplicate cue IDs");

  const char *descending =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"late\",\"timeMs\":100,\"type\":\"TEST\",\"value\":\"a\"},"
      "{\"id\":\"early\",\"timeMs\":99,\"type\":\"TEST\",\"value\":\"b\"}]}";
  result = ProductionParseResult::Ok;
  expect(!parseTimeline(descending, cues, &timeline, &result) &&
             result == ProductionParseResult::InvalidCueTime,
         "timeline rejects descending cue times");

  const char *negativeTime =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"bad_time\",\"timeMs\":-1,\"type\":\"TEST\"}]}";
  result = ProductionParseResult::Ok;
  expect(!parseTimeline(negativeTime, cues, &timeline, &result) &&
             result == ProductionParseResult::InvalidCueTime,
         "timeline rejects negative cue times explicitly");

  const char *overflowTime =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"bad_time\",\"timeMs\":4294967296,\"type\":\"TEST\"}]}";
  result = ProductionParseResult::Ok;
  expect(!parseTimeline(overflowTime, cues, &timeline, &result) &&
             result == ProductionParseResult::InvalidCueTime,
         "timeline rejects time values above uint32_t");

  const char *longCommand =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"abcdefghijklmnopqrstuvwxyz123456789abcd\",\"timeMs\":0,"
      "\"type\":\"TEST\",\"value\":\"command_overflow\"}]}";
  result = ProductionParseResult::Ok;
  expect(!parseTimeline(longCommand, cues, &timeline, &result) &&
             result == ProductionParseResult::CommandTooLong,
         "timeline rejects generated commands above the fixed limit");

  const char *maxCommand =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"abcdefghijklmnopqrstuvwxyz123456789abcd\",\"timeMs\":0,"
      "\"type\":\"TEST\",\"value\":\"1234567890\"}]}";
  expect(parseTimeline(maxCommand, cues, &timeline, &result) &&
             std::strlen(cues[0].command) == SHOWDUINO_CUE_COMMAND_MAX - 1U,
         "timeline accepts a scheduler command exactly at 63 characters");

  std::string tooMany = "{\"formatVersion\":1,\"cues\":[";
  for (unsigned i = 0; i <= SHOWDUINO_PRODUCTION_MAX_CUES; ++i) {
    if (i) tooMany += ',';
    tooMany += "{\"id\":\"cue_" + std::to_string(i) +
               "\",\"timeMs\":" + std::to_string(i) +
               ",\"type\":\"TEST\"}";
  }
  tooMany += "]}";
  result = ProductionParseResult::Ok;
  expect(!productionParseTimeline(tooMany.c_str(), tooMany.size(), cues,
                                  SHOWDUINO_PRODUCTION_MAX_CUES,
                                  &timeline, &result) &&
             result == ProductionParseResult::TooManyCues,
         "timeline rejects cue counts above the fixed limit");

  const char *unsupported =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"relay\",\"timeMs\":0,\"type\":\"RELAY\",\"value\":\"ON\"}]}";
  result = ProductionParseResult::Ok;
  expect(!parseTimeline(unsupported, cues, &timeline, &result) &&
             result == ProductionParseResult::UnsupportedCueType,
         "timeline rejects unsupported physical cue type");

  const char *unsupportedAction =
      "{\"formatVersion\":1,\"cues\":["
      "{\"id\":\"bad_action\",\"timeMs\":0,\"type\":\"TEST\"," 
      "\"action\":\"RELAY\"}]}";
  result = ProductionParseResult::Ok;
  expect(!parseTimeline(unsupportedAction, cues, &timeline, &result) &&
             result == ProductionParseResult::InvalidCueAction,
         "timeline rejects unsupported cue actions");

  expect(!parseTimeline("{\"formatVersion\":1,\"cues\":[]}", cues, &timeline, &result),
         "timeline rejects empty cue array");
  expect(!parseTimeline("{\"formatVersion\":1,\"cues\":[{broken]}",
                        cues, &timeline, &result),
         "timeline rejects malformed JSON");

  if (failures == 0) {
    std::printf("All production format tests passed.\n");
    return 0;
  }
  std::printf("%d production format test(s) failed.\n", failures);
  return 1;
}
