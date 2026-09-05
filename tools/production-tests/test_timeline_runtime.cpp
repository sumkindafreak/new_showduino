#include <cstdio>
#include <cstring>

#include "ShowRuntimeOwner.h"

uint32_t gShowduinoTestMillis = 0;
ShowduinoTestSerial Serial;

static int failures = 0;
static int dispatchCount = 0;
static char lastCommand[64] = {};

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

static void dispatch(const char *command) {
  ++dispatchCount;
  std::strncpy(lastCommand, command, sizeof(lastCommand) - 1);
}

static void loadTwoCueTimeline(ShowRuntimeOwner &owner, ShowEngineState &legacy,
                               const char *name) {
  expect(owner.handleTlBegin(false), "begin staged timeline load");
  expect(owner.handleTlCue(0, "INTERNAL:TEST:first:start"), "stage first cue");
  expect(owner.handleTlCue(1000, "INTERNAL:TEST:second:end"), "stage second cue");
  expect(owner.handleTlEnd(gShowduinoTestMillis, &legacy, name, false),
         "commit staged timeline");
}

int main() {
  ShowRuntimeOwner owner;
  ShowEngineState legacy;
  owner.begin(nullptr);
  owner.setDispatch(dispatch);
  owner.bootToIdle();
  loadTwoCueTimeline(owner, legacy, "System Test");

  expect(owner.rt.state == SHOW_STATE_SHOW_LOADED, "runtime becomes SHOW_LOADED");
  expect(owner.timeline.cueTotal() == 2, "committed timeline has two cues");
  expect(owner.handleRun(gShowduinoTestMillis, &legacy), "start loaded timeline");
  owner.service(gShowduinoTestMillis, &legacy);
  expect(dispatchCount == 1 && std::strcmp(lastCommand, "INTERNAL:TEST:first:start") == 0,
         "zero-time cue fires on first service");

  gShowduinoTestMillis = 400;
  owner.service(gShowduinoTestMillis, &legacy);
  owner.onEmergencyStop(gShowduinoTestMillis, &legacy);
  expect(owner.rt.state == SHOW_STATE_EMERGENCY_STOP, "emergency owns runtime state");
  expect(owner.timeline.state() == TimelinePlayState::Paused,
         "emergency pauses timeline");
  expect(!owner.handleRun(gShowduinoTestMillis, &legacy),
         "show start is rejected while emergency is active");

  gShowduinoTestMillis = 2000;
  owner.service(gShowduinoTestMillis, &legacy);
  expect(dispatchCount == 1, "paused emergency timeline does not dispatch");
  owner.onEmergencyCleared(gShowduinoTestMillis, &legacy);
  expect(owner.rt.state == SHOW_STATE_PAUSED,
         "emergency clear does not automatically resume");

  expect(owner.handleResume(gShowduinoTestMillis, &legacy), "operator resumes after clear");
  gShowduinoTestMillis = 2600;
  owner.service(gShowduinoTestMillis, &legacy);
  expect(dispatchCount == 2, "remaining cue fires after explicit resume");
  expect(owner.rt.state == SHOW_STATE_FINISHED, "runtime reaches FINISHED");

  owner.handleStop(gShowduinoTestMillis, &legacy);
  expect(owner.rt.state == SHOW_STATE_IDLE, "stop returns runtime to IDLE");

  loadTwoCueTimeline(owner, legacy, "Original");
  expect(owner.handleTlBegin(false), "begin replacement transaction");
  expect(owner.handleTlCue(0, "INTERNAL:TEST:new:new"), "stage replacement cue");
  owner.abortTimelineLoad();
  expect(owner.timeline.cueTotal() == 2, "aborted replacement preserves active timeline");
  expect(std::strcmp(owner.timeline.currentShowName(), "Original") == 0,
         "aborted replacement preserves production name");

  expect(owner.handleRun(gShowduinoTestMillis, &legacy),
         "start original timeline for emergency-stop test");
  owner.onEmergencyStop(gShowduinoTestMillis, &legacy);
  expect(!owner.handleUnload(gShowduinoTestMillis, &legacy),
         "unload is rejected while emergency owns runtime");
  expect(owner.handleStop(gShowduinoTestMillis, &legacy),
         "safe stop remains available during emergency");
  expect(owner.timeline.state() == TimelinePlayState::Stopped &&
             owner.rt.state == SHOW_STATE_EMERGENCY_STOP,
         "safe stop halts playback without clearing emergency latch");
  owner.onEmergencyCleared(gShowduinoTestMillis, &legacy);
  expect(owner.rt.state == SHOW_STATE_SHOW_LOADED,
         "emergency clear after safe stop leaves production loaded");

  expect(owner.handleUnload(gShowduinoTestMillis, &legacy), "unload idle production");
  expect(owner.timeline.cueTotal() == 0 && owner.rt.showName[0] == '\0',
         "unload clears timeline and production identity");
  owner.service(gShowduinoTestMillis, &legacy);
  expect(owner.rt.showName[0] == '\0',
         "runtime service does not restore an unloaded production name");
  expect(!owner.handleRun(gShowduinoTestMillis, &legacy),
         "show start is rejected when no timeline is loaded");

  if (failures == 0) {
    std::printf("All timeline/runtime tests passed.\n");
    return 0;
  }
  std::printf("%d timeline/runtime test(s) failed.\n", failures);
  return 1;
}
