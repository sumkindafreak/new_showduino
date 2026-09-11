#include "showduino_pixel_node.h"
#include "showduino_pixel_fx.h"
#include "showduino_state_wire.h"
#include <cstdio>
#include <cstring>

static int gFails = 0;

static void expect(bool ok, const char *msg) {
  if (!ok) {
    std::printf("FAIL: %s\n", msg);
    gFails++;
  } else {
    std::printf("PASS  %s\n", msg);
  }
}

int main() {
  expect(showduino_pixel_id_ok("LED-01") == 1, "LED-01 is a valid logical id");
  expect(showduino_pixel_id_ok("led_02") == 1, "lowercase id allowed");
  expect(showduino_pixel_id_ok("1LED") == 0, "id must start with a letter");
  expect(showduino_pixel_id_ok("L") == 0, "id too short");
  expect(showduino_pixel_id_ok("LED-THISISTOOLONG") == 0, "id too long");
  expect(showduino_pixel_id_equal("LED-01", "led-01") == 1, "id compare is case-insensitive");
  expect(showduino_pixel_name_ok("CORRIDOR") == 1, "friendly name ok");
  expect(showduino_pixel_name_ok("bad:name") == 0, "colon rejected in name");

  expect(showduino_pixel_count_ok(0, SHOWDUINO_PIXEL_NODE_MAX_PIXELS) == 0,
         "zero count is unconfigured");
  expect(showduino_pixel_count_ok(1, SHOWDUINO_PIXEL_NODE_MAX_PIXELS) == 1, "min count 1");
  expect(showduino_pixel_count_ok(512, SHOWDUINO_PIXEL_NODE_MAX_PIXELS) == 1, "C3 max 512");
  expect(showduino_pixel_count_ok(513, SHOWDUINO_PIXEL_NODE_MAX_PIXELS) == 0,
         "513 exceeds C3 max");
  expect(showduino_pixel_count_ok(1024, 1024) == 1, "P4 local max remains 1024");

  expect(showduino_pixel_cmd_requires_init(SHOWDUINO_PIXEL_CMD_TEST) == 1,
         "TEST requires init");
  expect(showduino_pixel_cmd_requires_init(SHOWDUINO_PIXEL_CMD_SEGMENT) == 1,
         "SEGMENT requires init");
  expect(showduino_pixel_cmd_requires_init(SHOWDUINO_PIXEL_CMD_BLACKOUT) == 1,
         "BLACKOUT requires init");
  expect(showduino_pixel_cmd_requires_init(SHOWDUINO_PIXEL_CMD_COUNT) == 0,
         "COUNT does not require init");
  expect(showduino_pixel_cmd_requires_init(SHOWDUINO_PIXEL_CMD_INIT) == 0,
         "INIT does not require init");
  expect(showduino_pixel_gate_init(0, SHOWDUINO_PIXEL_CMD_TEST) ==
             SHOWDUINO_PIXEL_FAIL_NOT_INITIALISED,
         "uninit TEST is NOT_INITIALISED");
  expect(showduino_pixel_gate_init(0, SHOWDUINO_PIXEL_CMD_COUNT) == SHOWDUINO_PIXEL_FAIL_NONE,
         "uninit COUNT allowed");
  expect(showduino_pixel_gate_init(1, SHOWDUINO_PIXEL_CMD_TEST) == SHOWDUINO_PIXEL_FAIL_NONE,
         "init TEST allowed");

  expect(showduino_pixel_segment_id_ok(0) == 1, "segment 0 ok");
  expect(showduino_pixel_segment_id_ok(15) == 1, "segment 15 ok");
  expect(showduino_pixel_segment_id_ok(16) == 0, "segment 16 rejected");
  expect(showduino_pixel_range_ok(0, 20, 100) == 1, "0-19 in 100 ok");
  expect(showduino_pixel_range_ok(20, 20, 100) == 1, "overlap candidate 20-39 ok");
  expect(showduino_pixel_range_ok(90, 20, 100) == 0, "range past end rejected");
  expect(showduino_pixel_range_ok(0, 0, 100) == 0, "zero-length range rejected");
  /* Overlap is allowed; later slot IDs win. Both ranges independently valid. */
  expect(showduino_pixel_range_ok(0, 30, 100) == 1 &&
             showduino_pixel_range_ok(20, 20, 100) == 1,
         "overlapping ranges are both valid (later slot wins at render)");

  ShowduinoPixelFx fx = ShowduinoPixelFx::Off;
  expect(showduinoPixelFxFromName("FLICKER", &fx) && fx == ShowduinoPixelFx::Flicker,
         "FLICKER id");
  expect(showduinoPixelFxFromName("FIRE", &fx) && fx == ShowduinoPixelFx::Fire, "FIRE id");
  expect(showduinoPixelFxFromName("CUSTOM_SEQUENCE", &fx) &&
             fx == ShowduinoPixelFx::CustomSequence,
         "CUSTOM_SEQUENCE id");
  expect(!showduinoPixelFxFromName("NOT_A_REAL_FX", &fx), "unknown FX rejected");
  expect((uint8_t)ShowduinoPixelFx::Count == 25, "25 Showduino pixel FX");

  expect(showduino_pixel_classify_command("PIXEL:STATUS") == SHOWDUINO_PIXEL_CMD_STATUS,
         "STATUS");
  expect(showduino_pixel_classify_command("PIXEL:NODE:LED-01:COUNT:60") ==
             SHOWDUINO_PIXEL_CMD_COUNT,
         "NODE COUNT");
  expect(showduino_pixel_classify_command("PIXEL:NODE:LED-01:INIT") ==
             SHOWDUINO_PIXEL_CMD_INIT,
         "NODE INIT");
  expect(showduino_pixel_classify_command("PIXEL:NODE:LED-01:LOCATE") ==
             SHOWDUINO_PIXEL_CMD_LOCATE,
         "NODE LOCATE");
  expect(showduino_pixel_classify_command("EMERGENCY:STOP") ==
             SHOWDUINO_PIXEL_CMD_EMERGENCY_STOP,
         "emergency stop");
  expect(showduino_pixel_classify_command("PIXEL:NODE:LED-01:OWN:GRANT") ==
             SHOWDUINO_PIXEL_CMD_OWN_GRANT,
         "grant");

  expect(showduino_pixel_can_accept_ex(SHOWDUINO_PIXEL_ST_UNINIT,
                                      SHOWDUINO_PIXEL_CMD_TEST,
                                      SHOWDUINO_CMD_ORIGIN_LOCAL, 0) ==
             SHOWDUINO_PIXEL_FAIL_NOT_INITIALISED,
         "local TEST locked before init");
  expect(showduino_pixel_can_accept_ex(SHOWDUINO_PIXEL_ST_EMERGENCY,
                                      SHOWDUINO_PIXEL_CMD_LOCATE,
                                      SHOWDUINO_CMD_ORIGIN_SHOW, 1) ==
             SHOWDUINO_PIXEL_FAIL_EMERGENCY,
         "Locate loses to emergency");
  expect(showduino_pixel_locate_allowed(1, 1) == 0, "locate blocked in emergency");
  expect(showduino_pixel_locate_allowed(0, 0) == 0, "locate blocked before init");
  expect(showduino_pixel_locate_allowed(0, 1) == 1, "locate allowed when ready");
  expect(showduino_pixel_can_accept_ex(SHOWDUINO_PIXEL_ST_EMERGENCY,
                                      SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR,
                                      SHOWDUINO_CMD_ORIGIN_WEB, 1) ==
             SHOWDUINO_PIXEL_FAIL_EMERGENCY,
         "webpage cannot clear emergency");
  expect(showduino_pixel_can_accept_ex(SHOWDUINO_PIXEL_ST_EMERGENCY,
                                      SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR,
                                      SHOWDUINO_CMD_ORIGIN_SHOW, 1) ==
             SHOWDUINO_PIXEL_FAIL_NONE,
         "P4 may clear emergency");

  ShowduinoPixelAnnounce an{};
  expect(showduino_pixel_parse_announce(
             "ANNOUNCE:AA:BB:CC:DD:EE:FF:0.1.0:SEARCHING:ID=LED-03:N=CRYPT:P=100:I=1:S=5",
             &an) == 1,
         "announce parses");
  expect(std::strcmp(an.id, "LED-03") == 0, "announce id");
  expect(std::strcmp(an.name, "CRYPT") == 0, "announce name");
  expect(an.pixelCount == 100 && an.initialised == 1 && an.segments == 5,
         "announce line fields");

  ShowduinoPixelRoute rt{};
  expect(showduino_pixel_parse_route("ROUTE:PIXEL:LED-03:42:PIXEL:SEGMENT:2:START", &rt) == 1,
         "route parses");
  expect(std::strcmp(rt.id, "LED-03") == 0 && rt.sequence == 42, "route id/seq");
  expect(std::strcmp(rt.command, "PIXEL:SEGMENT:2:START") == 0, "route inner command");
  expect(showduino_pixel_parse_route("ROUTE:PIXEL:99BAD:1:PIXEL:STATUS", &rt) == 0,
         "route rejects bad id");

  char id[16] = "";
  char inner[96] = "";
  expect(showduino_pixel_inner_command("PIXEL:NODE:LED-03:COUNT:60", id, sizeof(id),
                                      inner, sizeof(inner)) == 1,
         "inner COUNT");
  expect(std::strcmp(id, "LED-03") == 0 && std::strcmp(inner, "PIXEL:COUNT:60") == 0,
         "inner COUNT rewrite");
  expect(showduino_pixel_inner_command("PIXEL:NODE:LED-03:PIXEL:INIT", id, sizeof(id),
                                      inner, sizeof(inner)) == 1,
         "inner already PIXEL:");
  expect(std::strcmp(inner, "PIXEL:INIT") == 0, "inner INIT passthrough");

  char offline[48];
  expect(showduino_pixel_format_offline(offline, sizeof(offline), "LED-03") == 1,
         "offline reply formats");
  expect(std::strcmp(offline, "REJECTED:PIXEL:OFFLINE:LED-03") == 0, "offline reply text");

  expect(showduino_parse_state_node_pixel("STATE:NODE:PIXEL:ONLINE") ==
             SHOWDUINO_PIXEL_NODE_WIRE_ONLINE,
         "pixel wire ONLINE");
  expect(showduino_parse_state_node_pixel("STATE:NODE:PIXEL:D:1:2:LED-01:READY") ==
             SHOWDUINO_PIXEL_NODE_WIRE_INVALID,
         "detail wire skipped by coarse parse");
  ShowduinoPixelDetailWire det{};
  expect(showduino_parse_state_node_pixel_detail("STATE:NODE:PIXEL:D:2:3:LED-01:ONLINE",
                                                &det) == 1,
         "detail parse");
  expect(det.online == 2 && det.seen == 3 && std::strcmp(det.firstId, "LED-01") == 0,
         "detail fields");

  char ann[96];
  expect(showduino_pixel_format_announce(ann, sizeof(ann), "AA:BB:CC:DD:EE:FF", "0.1.0",
                                        "SEARCHING", "LED-01", "ENTRANCE HALL", 60, 0, 0) ==
             1,
         "announce formats within 96 bytes");
  expect(std::strstr(ann, "N=ENTRANCE") != nullptr, "friendly name truncated to 8");

  if (gFails) {
    std::printf("%d Pixel Node host tests failed\n", gFails);
    return 1;
  }
  std::printf("All Pixel Node host tests passed\n");
  return 0;
}
