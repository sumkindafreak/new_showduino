#include <cstdio>
#include <cstring>

#include "../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src/WebTimelineUploadPolicy.h"

static int failures = 0;

static void expect(bool condition, const char *name) {
  std::printf("%s  %s\n", condition ? "PASS" : "FAIL", name);
  if (!condition) ++failures;
}

int main() {
  using namespace ShowduinoWebTimelineUploadPolicy;

  expect(envelopeAllowed("SHOW:TL:BEGIN"), "allows timeline begin");
  expect(envelopeAllowed("SHOW:TL:END"), "allows timeline end");

  const char *payload = nullptr;
  expect(cueEnvelopeAllowed("SHOW:TL:C:0:PIXEL:SEGMENT:0:START", &payload),
         "allows pixel cue envelope");
  expect(payload && std::strcmp(payload, "PIXEL:SEGMENT:0:START") == 0,
         "returns nested pixel payload");

  payload = nullptr;
  expect(cueEnvelopeAllowed("SHOW:TL:C:2500:AUDIO:NODE:STOP", &payload),
         "allows audio-node cue envelope");
  expect(payload && std::strcmp(payload, "AUDIO:NODE:STOP") == 0,
         "returns nested audio payload");

  expect(!cueEnvelopeAllowed("SHOW:TL:C::PIXEL:OFF"),
         "rejects missing timestamp");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:-1:PIXEL:OFF"),
         "rejects negative timestamp");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:abc:PIXEL:OFF"),
         "rejects non-numeric timestamp");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:4294967296:PIXEL:OFF"),
         "rejects timestamp above uint32");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:10:"),
         "rejects empty nested command");

  expect(!cueEnvelopeAllowed("SHOW:TL:C:10:EMERGENCY:STOP"),
         "rejects nested emergency command");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:10:SHOW:START"),
         "rejects nested show command");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:10:PRODUCTION:UNLOAD"),
         "rejects nested production command");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:10:NET:APPLY"),
         "rejects nested network command");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:10:AUDIO:TEST:EMERGENCY"),
         "rejects P4 system-audio command");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:10:RELAY:1:ON"),
         "rejects legacy relay command");
  expect(!cueEnvelopeAllowed("SHOW:TL:C:10:DMX:1:255"),
         "rejects parked DMX command");

  const char *longPayload =
      "SHOW:TL:C:10:PIXEL:SEGMENT:0:COLOR:123:123:123:EXTRA:EXTRA:EXTRA:EXTRA:EXTRA:EXTRA";
  expect(!cueEnvelopeAllowed(longPayload),
         "rejects nested command over TimelineEngine limit");

  expect(!envelopeAllowed("SHOW:TL:ABORT"),
         "rejects unknown timeline envelope");
  expect(!envelopeAllowed("PIXEL:OFF"),
         "policy is envelope-only, not a general web whitelist");

  if (failures == 0) {
    std::printf("All web timeline upload policy tests passed.\n");
    return 0;
  }

  std::printf("%d web timeline upload policy test(s) failed.\n", failures);
  return 1;
}
