#include "showduino_lamp_node.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int gFails = 0;

static void expect(bool ok, const char *msg) {
  if (!ok) {
    std::printf("FAIL: %s\n", msg);
    gFails++;
  }
}

int main() {
  expect(SHOWDUINO_LAMP_FX_CARBIDE_COUNT == 14, "14 original carbide FX");
  expect(SHOWDUINO_LAMP_FX_TABLE_LEN == (size_t)SHOWDUINO_LAMP_FX_COUNT,
         "table covers every FX id");

  unsigned carbide = 0, extra = 0;
  for (size_t i = 0; i < SHOWDUINO_LAMP_FX_TABLE_LEN; ++i) {
    if (SHOWDUINO_LAMP_FX_TABLE[i].origin == SHOWDUINO_LAMP_FX_CARBIDE) carbide++;
    else extra++;
  }
  expect(carbide == 14, "carbide origin count");
  expect(extra == 9, "showduino extra count");

  ShowduinoLampFx fx = SHOWDUINO_LAMP_FX_COUNT;
  expect(showduino_lamp_fx_from_token("CARBIDE_FLAME", &fx) == 0, "lookup carbide flame");
  expect(fx == SHOWDUINO_LAMP_FX_CARBIDE_FLAME, "carbide flame id");
  expect(strcmp(showduino_lamp_fx_info(fx)->display, "Carbide Flame") == 0,
         "display name preserved");
  expect(showduino_lamp_fx_from_token("NOT_A_REAL_FX", &fx) != 0, "unknown fx rejected");

  ShowduinoLampCommand c;
  expect(showduino_lamp_parse_command("LAMP:STATUS", &c) == SHOWDUINO_LAMP_CMD_STATUS,
         "status");
  expect(showduino_lamp_parse_command("LAMP:NODE:OFF", &c) == SHOWDUINO_LAMP_CMD_OFF,
         "node prefix off");
  expect(showduino_lamp_parse_command("LAMP:STOP", &c) == SHOWDUINO_LAMP_CMD_OFF, "stop=off");
  expect(showduino_lamp_parse_command("LAMP:LIST", &c) == SHOWDUINO_LAMP_CMD_LIST, "list");
  expect(showduino_lamp_parse_command("LAMP:TEST", &c) == SHOWDUINO_LAMP_CMD_TEST, "test");

  expect(showduino_lamp_parse_command("LAMP:BRIGHTNESS:80", &c) ==
             SHOWDUINO_LAMP_CMD_BRIGHTNESS,
         "bri cmd");
  expect(c.brightness == 80, "bri 80");
  expect(showduino_lamp_parse_command("LAMP:BRIGHTNESS:101", &c) == SHOWDUINO_LAMP_CMD_NONE,
         "bri overflow");

  expect(showduino_lamp_parse_command("LAMP:SOLID:10,20,30", &c) == SHOWDUINO_LAMP_CMD_SOLID,
         "solid rgb");
  expect(c.r == 10 && c.g == 20 && c.b == 30, "solid values");
  expect(c.fx == SHOWDUINO_LAMP_FX_SOLID, "solid fx id");

  expect(showduino_lamp_parse_command("LAMP:FX:FLICKER:BRI=70:SPD=40", &c) ==
             SHOWDUINO_LAMP_CMD_FX,
         "fx flicker params");
  expect(c.fx == SHOWDUINO_LAMP_FX_FLICKER, "flicker is original carbide fx");
  expect(c.brightness == 70 && c.speed == 40, "fx params");
  expect(showduino_lamp_fx_info(c.fx)->origin == SHOWDUINO_LAMP_FX_CARBIDE,
         "flicker origin carbide");

  expect(showduino_lamp_parse_command("LAMP:FX:FIRE", &c) == SHOWDUINO_LAMP_CMD_FX, "fire extra");
  expect(showduino_lamp_fx_info(c.fx)->origin == SHOWDUINO_LAMP_FX_SHOWDUINO,
         "fire origin extra");

  expect(showduino_lamp_parse_command("EMERGENCY:STOP", &c) ==
             SHOWDUINO_LAMP_CMD_EMERGENCY_STOP,
         "estop");
  expect(showduino_lamp_parse_command("EMERGENCY:CLEAR", &c) ==
             SHOWDUINO_LAMP_CMD_EMERGENCY_CLEAR,
         "eclear");
  expect(showduino_lamp_parse_command("SHOW:START", &c) == SHOWDUINO_LAMP_CMD_NONE,
         "show start not a lamp command");
  expect(showduino_lamp_parse_command("LAMP:FX:FLICKER:BOGUS=1", &c) ==
             SHOWDUINO_LAMP_CMD_NONE,
         "bad fx kv");

  expect(showduino_lamp_can_accept(SHOWDUINO_LAMP_ST_EMERGENCY,
                                   SHOWDUINO_LAMP_CMD_FX) ==
             SHOWDUINO_LAMP_FAIL_EMERGENCY,
         "emergency rejects fx");
  expect(showduino_lamp_can_accept(SHOWDUINO_LAMP_ST_EMERGENCY,
                                   SHOWDUINO_LAMP_CMD_EMERGENCY_CLEAR) ==
             SHOWDUINO_LAMP_FAIL_NONE,
         "emergency allows clear");
  expect(showduino_lamp_can_accept(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED,
                                   SHOWDUINO_LAMP_CMD_FX) ==
             SHOWDUINO_LAMP_FAIL_NONE,
         "show-controlled allows fx");

  uint16_t start = 0, count = 0;
  showduino_lamp_list_slice(0, &start, &count);
  expect(start == 0 && count == SHOWDUINO_LAMP_LIST_PER_PAGE, "list page 0");
  showduino_lamp_list_slice(10, &start, &count);
  expect(count == 0, "list past end");

  if (gFails) {
    std::printf("%d FAILED\n", gFails);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
