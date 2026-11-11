// Phase 3: one pixel segment rule for live upload and persisted shows.
//
// studio-expected.tsv in each fixture folder is what website Studio's own live-upload
// compiler (showduino.com js/studio-deploy.js compileShdoForStage) produced for the .shdo
// files next to it. This test requires shdoCompile() to produce the same commands, at the
// same times, with the SAME segment numbers. The two compilers are separate
// implementations, so agreement here is the evidence that a show addresses pixels the
// same way on both paths.
//
// Also checked: shows without a `segment` field still get 0, 1, 2 ... as before; no pixel
// cue can produce an emergency or show-control command; raw emergency commands are refused.
//
// Host test only. LEDs, nodes and timing on a P4 are not exercised.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../../protocol/showduino_shdo.h"

static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
  ++checks;
  if (!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}
static bool readFile(const std::string &path, std::string *out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}
struct Compiled { ShdoStatus status; std::vector<ShdoCue> cues; uint16_t count; };
static Compiled compile(const std::string &doc) {
  Compiled c{SHDO_INVALID_JSON, std::vector<ShdoCue>(SHOWDUINO_SHDO_MAX_CUES), 0};
  ShdoManifest manifest{};
  char err[128] = "";
  c.status = shdoCompile(doc.c_str(), doc.size(), &manifest, c.cues.data(), SHOWDUINO_SHDO_MAX_CUES, &c.count, err, sizeof(err));
  return c;
}
static std::string segmentsOf(const Compiled &c) {
  std::string out;
  for (uint16_t i = 0; c.status == SHDO_OK && i < c.count; ++i) {
    const char *range = std::strstr(c.cues[i].command, ":RANGE:");
    const char *seg = std::strstr(c.cues[i].command, "SEGMENT:");
    if (range && seg) { if (!out.empty()) out += ","; out.append(seg + 8, range - (seg + 8)); }
  }
  return out;
}

static void compareWithStudio(const std::string &dir, int *shows) {
  std::string table;
  if (!readFile(dir + "/studio-expected.tsv", &table)) { expect(false, "cannot read " + dir + "/studio-expected.tsv"); return; }
  std::vector<std::string> order;
  std::map<std::string, std::pair<std::string, std::vector<std::string>>> want;
  size_t pos = 0; std::string current;
  while (pos < table.size()) {
    size_t end = table.find('\n', pos); if (end == std::string::npos) end = table.size();
    const std::string line = table.substr(pos, end - pos); pos = end + 1;
    if (line.rfind("SHOW\t", 0) == 0) { const size_t tab = line.find('\t', 5); current = line.substr(5, tab - 5); want[current].first = line.substr(tab + 1); order.push_back(current); }
    else if (line.rfind("CUE\t", 0) == 0 && !current.empty()) want[current].second.push_back(line.substr(4));
  }
  for (const std::string &name : order) {
    std::string doc;
    if (!readFile(dir + "/" + name + ".shdo", &doc)) { expect(false, name + ": fixture is readable"); continue; }
    ++*shows;
    const Compiled c = compile(doc);
    const bool studioOk = want[name].first == "OK";
    expect((c.status == SHDO_OK) == studioOk, name + ": P4 " + shdoStatusName(c.status) + ", Studio " + want[name].first);
    if (c.status != SHDO_OK || !studioOk) continue;
    const std::vector<std::string> &lines = want[name].second;
    bool same = c.count == lines.size();
    std::string firstDifference;
    for (uint16_t i = 0; i < c.count && i < lines.size(); ++i) {
      const std::string mine = std::to_string(c.cues[i].timeMs) + "\t" + c.cues[i].command;
      if (mine != lines[i]) { same = false; firstDifference = " (cue " + std::to_string(i) + ": P4 \"" + mine + "\", Studio \"" + lines[i] + "\")"; break; }
    }
    expect(same, name + ": same commands, times and segment numbers as Studio's live upload" + firstDifference);
    for (uint16_t i = 0; i < c.count; ++i) {
      const char *cmd = c.cues[i].command;
      const bool emergencyRing = std::strncmp(cmd, "ESTOP:NODE:PIXEL:SEGMENT:", 25) == 0;
      const bool control = std::strncmp(cmd, "EMERGENCY", 9) == 0 || std::strncmp(cmd, "SHOW:", 5) == 0 ||
                           std::strncmp(cmd, "PRODUCTION:", 11) == 0 || (std::strncmp(cmd, "ESTOP", 5) == 0 && !emergencyRing);
      if (control) expect(false, name + ": a pixel show produced a control command: " + cmd);
    }
  }
}

static std::string show(const std::string &clips, const char *route = "p4-show-pixels") {
  return std::string("{\"schema\":\"showduino-production-v2\",\"package\":{\"version\":2},"
                     "\"project\":{\"id\":\"segment_unit\",\"name\":\"Segment unit\"},"
                     "\"devices\":[{\"id\":\"line\",\"type\":\"p4-pixel-line\",\"binding\":{\"route\":\"") + route + "\"}}],"
         "\"clips\":[" + clips + "]}";
}
static std::string clip(int start, int duration, const char *segmentField) {
  return std::string("{\"type\":\"pixel\",\"targetDeviceId\":\"line\",\"startMs\":") + std::to_string(start) +
         ",\"durationMs\":" + std::to_string(duration) + ",\"params\":{" + segmentField +
         "\"startPixel\":0,\"length\":4,\"effect\":\"SOLID\"}}";
}

int main(int argc, char **argv) {
  const std::string base = argc > 1 ? argv[1] : "fixtures";
  int shows = 0;
  compareWithStudio(base + "/studio-phase3", &shows);
  compareWithStudio(base + "/studio-phase3-segments", &shows);
  expect(shows == 33, "33 Studio shows compared (21 original, 12 segment cases)");

  // No `segment` field: numbering is unchanged from before this rule existed.
  expect(segmentsOf(compile(show(clip(0, 500, "") + "," + clip(1000, 500, "") + "," + clip(2000, 500, "")))) == "0,1,2",
         "cues without a segment field still get 0, 1, 2");
  std::string seventeen;
  for (int i = 0; i < 17; ++i) seventeen += (i ? "," : "") + clip(i * 1000, 500, "");
  expect(compile(show(seventeen)).status == SHDO_TOO_MANY_CUES, "17 cues without a segment field are still refused");

  // Explicit segments.
  expect(segmentsOf(compile(show(clip(0, 5000, "\"segment\":7,") + "," + clip(100, 5000, "\"segment\":2,")))) == "7,2", "named segments are honoured");
  expect(segmentsOf(compile(show(clip(0, 5000, "\"segment\":7,") + "," + clip(100, 5000, "\"segment\":7,")))) == "7,0", "a busy named segment moves the later cue to the lowest free one");
  expect(segmentsOf(compile(show(clip(0, 1000, "\"segment\":7,") + "," + clip(1000, 1000, "\"segment\":7,")))) == "7,7", "a segment is free again exactly when its cue ends");
  expect(segmentsOf(compile(show(clip(0, 0, "\"segment\":0,") + "," + clip(60000, 500, "\"segment\":0,")))) == "0,1", "a cue with no duration holds its segment");
  expect(segmentsOf(compile(show(clip(0, 500, "\"segment\":400,")))) == "15", "a segment above the range uses the last one");
  expect(segmentsOf(compile(show(clip(0, 500, "\"segment\":null,") + "," + clip(1000, 500, "\"segment\":\"\",") + "," + clip(2000, 500, "\"segment\":\"3\",")))) == "0,1,2",
         "null, empty and text segments count as not chosen");
  expect(compile(show(clip(0, 500, "\"segment\":1.5,"))).status != SHDO_OK, "a fractional segment is refused, not guessed");

  // A held cue on a segment that last ran a timed cue must clear the stale duration.
  {
    const Compiled c = compile(show(clip(0, 1000, "\"segment\":0,") + "," + clip(2000, 0, "\"segment\":0,")));
    bool reset = false; int durations = 0;
    for (uint16_t i = 0; c.status == SHDO_OK && i < c.count; ++i) {
      if (std::strstr(c.cues[i].command, ":DURATION:")) ++durations;
      if (c.cues[i].timeMs == 2000 && std::strcmp(c.cues[i].command, "PIXEL:SEGMENT:0:DURATION:0") == 0) reset = true;
    }
    expect(reset && durations == 2, "a held cue reusing a timed segment sends DURATION:0");
    const Compiled plain = compile(show(clip(0, 0, "\"segment\":0,")));
    bool any = false;
    for (uint16_t i = 0; plain.status == SHDO_OK && i < plain.count; ++i) if (std::strstr(plain.cues[i].command, ":DURATION:")) any = true;
    expect(!any, "a held cue on a fresh segment sends no DURATION, as before");
  }

  // Emergency signage is not reachable from an ordinary pixel cue or a raw command.
  {
    const Compiled line = compile(show(clip(0, 500, "\"segment\":0,")));
    bool leak = false;
    for (uint16_t i = 0; line.status == SHDO_OK && i < line.count; ++i) if (std::strncmp(line.cues[i].command, "PIXEL:SEGMENT:", 14) != 0) leak = true;
    expect(line.status == SHDO_OK && !leak, "a P4 line cue compiles only to PIXEL:SEGMENT commands");
    const char *raw[] = {"EMERGENCY:CLEAR", "EMERGENCY:STOP", "ESTOP:CLEAR", "ESTOP:NODE:CLEAR", "SHOW:START", "SHOW:STOP", "PRODUCTION:LOAD:other", "ROUTE:LAMP:IGNITE"};
    for (const char *command : raw) {
      const std::string doc = show(std::string("{\"type\":\"pixel\",\"targetDeviceId\":\"line\",\"startMs\":0,\"durationMs\":0,\"command\":\"") + command + "\"}");
      expect(compile(doc).status == SHDO_UNSUPPORTED_ACTION, std::string("raw command ") + command + " is refused");
    }
    expect(compile(show(clip(0, 500, "\"segment\":0,"), "emergency-override")).status != SHDO_OK, "an unknown pixel route is refused");
  }

  std::printf("%s  Pixel segment policy: %d checks, %d failed\n", failures ? "FAIL" : "PASS", checks, failures);
  return failures ? 1 : 0;
}
