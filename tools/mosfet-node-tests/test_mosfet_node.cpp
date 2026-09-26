#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "showduino_mosfet_node.h"
#include "showduino_mosfet_engine.h"
#include "showduino_mosfet_identifier.h"

static int gFails = 0;
static void expect(bool ok, const char *msg) {
  if (!ok) { std::printf("FAIL: %s\n", msg); gFails++; }
  else std::printf("PASS: %s\n", msg);
}

static uint8_t gHw[4];
static void writeCb(uint8_t idx, uint8_t pct) {
  if (idx < 4) gHw[idx] = pct;
}

int main() {
  expect(SHOWDUINO_MOSFET_OUT1_GPIO_DEFAULT == 16, "OUT1 GPIO 16");
  expect(SHOWDUINO_MOSFET_OUT2_GPIO_DEFAULT == 17, "OUT2 GPIO 17");
  expect(SHOWDUINO_MOSFET_OUT3_GPIO_DEFAULT == 26, "OUT3 GPIO 26");
  expect(SHOWDUINO_MOSFET_OUT4_GPIO_DEFAULT == 27, "OUT4 GPIO 27");
  expect(SHOWDUINO_MOSFET_GPIO_VERIFIED_DEFAULT == 0, "GPIO verified flag 0");

  expect(showduino_mosfet_id_ok("MOSFET-01"), "id MOSFET-01");
  expect(showduino_mosfet_id_ok("MOSFET-1"), "id MOSFET-1");
  expect(!showduino_mosfet_id_ok("MOSFET-09"), "reject MOSFET-09");
  expect(!showduino_mosfet_id_ok("LED-01"), "reject LED-01");

  ShowduinoMosfetRoute rt = {};
  expect(showduino_mosfet_parse_route(
           "ROUTE:MOSFET:MOSFET-02:31:MOSFET:OUT:3:LEVEL:40", &rt),
         "parse route");
  expect(showduino_mosfet_id_equal(rt.id, "MOSFET-02"), "route id");
  expect(rt.sequence == 31, "route seq");
  expect(std::strcmp(rt.command, "MOSFET:OUT:3:LEVEL:40") == 0, "route cmd");

  ShowduinoMosfetEngine eng;
  std::memset(gHw, 0xFF, sizeof(gHw));
  showduino_mosfet_engine_begin(&eng, writeCb);
  expect(gHw[0]==0 && gHw[1]==0 && gHw[2]==0 && gHw[3]==0, "boot all off");

  expect(showduino_mosfet_engine_set_level(&eng, 1, 50), "level 50");
  expect(eng.ch[0].level == 50 && eng.ch[1].level == 0, "only ch1");
  expect(!showduino_mosfet_engine_set_level(&eng, 5, 10), "reject OUT5");
  expect(!showduino_mosfet_engine_set_level(&eng, 1, 101), "reject level 101");

  expect(showduino_mosfet_engine_pulse(&eng, 2, 75, 500, 1000), "pulse start");
  expect(eng.ch[1].level == 75, "pulse level");
  showduino_mosfet_engine_tick(&eng, 1499);
  expect(eng.ch[1].level == 75, "pulse before deadline");
  showduino_mosfet_engine_tick(&eng, 1500);
  expect(eng.ch[1].level == 0, "pulse done");

  expect(showduino_mosfet_engine_set_level(&eng, 1, 0), "reset before fade");
  expect(showduino_mosfet_engine_fade(&eng, 1, 100, 1000, 0), "fade in");
  showduino_mosfet_engine_tick(&eng, 500);
  expect(eng.ch[0].level >= 45 && eng.ch[0].level <= 55, "fade midpoint");
  showduino_mosfet_engine_tick(&eng, 1000);
  expect(eng.ch[0].level == 100, "fade end 100");

  showduino_mosfet_engine_set_level(&eng, 1, 20);
  showduino_mosfet_engine_pulse(&eng, 2, 80, 2000, 0);
  showduino_mosfet_engine_fade(&eng, 3, 50, 2000, 0);
  showduino_mosfet_engine_set_level(&eng, 4, 100);
  showduino_mosfet_engine_all_off(&eng, "test");
  expect(eng.ch[0].level==0 && eng.ch[1].level==0 && eng.ch[2].level==0 && eng.ch[3].level==0,
         "all off");

  ShowduinoMosfetCmd theatrical = showduino_mosfet_classify_command("MOSFET:OUT:1:ON");
  expect(showduino_mosfet_can_accept(SHOWDUINO_MOSFET_ST_SAFE, theatrical,
                                     SHOWDUINO_CMD_ORIGIN_SHOW) == SHOWDUINO_MOSFET_FAIL_NOT_OWNER,
         "no grant rejects theatrical show cmd");
  expect(showduino_mosfet_can_accept(SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED, theatrical,
                                     SHOWDUINO_CMD_ORIGIN_SHOW) == SHOWDUINO_MOSFET_FAIL_NONE,
         "owned accepts theatrical");
  expect(showduino_mosfet_can_accept(SHOWDUINO_MOSFET_ST_EMERGENCY, theatrical,
                                     SHOWDUINO_CMD_ORIGIN_SHOW) == SHOWDUINO_MOSFET_FAIL_EMERGENCY,
         "emergency rejects ON");
  expect(showduino_mosfet_can_accept(SHOWDUINO_MOSFET_ST_EMERGENCY,
                                     SHOWDUINO_MOSFET_CMD_ALL_OFF,
                                     SHOWDUINO_CMD_ORIGIN_LOCAL) == SHOWDUINO_MOSFET_FAIL_NONE,
         "emergency allows ALL OFF");

  char status[96];
  uint8_t lv[4] = {0,50,0,100};
  expect(showduino_mosfet_format_status(status, sizeof(status), "MOSFET-01", 1, 0, lv),
         "format status");
  expect(std::strstr(status, "O=0,50,0,100") != nullptr, "status levels");

  char idfmt[16];
  expect(showduino_mosfet_format_id(1, idfmt, sizeof(idfmt)), "format id");
  expect(std::strcmp(idfmt, "MOSFET-01") == 0, "canonical MOSFET-01");

  char cmd[80];
  std::snprintf(cmd, sizeof(cmd), "MOSFET:NODE:MOSFET-08:OUT:4:PULSE:100:600000");
  expect(std::strlen(cmd) <= 63, "timeline cmd <= 63");

  expect(showduino_mosfet_classify_command("MOSFET:IDENTIFY") ==
             SHOWDUINO_MOSFET_CMD_IDENTIFY,
         "classify IDENTIFY");
  expect(showduino_mosfet_classify_command("MOSFET:NODE:MOSFET-01:IDENTIFY") ==
             SHOWDUINO_MOSFET_CMD_IDENTIFY,
         "classify NODE IDENTIFY");
  expect(std::strstr(SHOWDUINO_MOSFET_CAPS, "IDENTIFY") != nullptr, "caps has IDENTIFY");
  expect(std::strstr(SHOWDUINO_MOSFET_CAPS, ",PIXEL") == nullptr &&
             std::strstr(SHOWDUINO_MOSFET_CAPS, "PIXEL,") == nullptr &&
             std::strcmp(SHOWDUINO_MOSFET_CAPS, "PIXEL") != 0,
         "caps excludes PIXEL token");

  expect(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO_DEFAULT == 25, "ident GPIO 25");
  expect(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_COUNT_DEFAULT == 4, "ident count 4");
  expect(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_VERIFIED_DEFAULT == 0, "ident unverified");
  expect(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_FOR_OUT(1) == 0, "pixel0↔OUT1");
  expect(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_FOR_OUT(2) == 1, "pixel1↔OUT2");
  expect(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_FOR_OUT(3) == 2, "pixel2↔OUT3");
  expect(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_FOR_OUT(4) == 3, "pixel3↔OUT4");

  uint8_t levelsOff[4] = {0, 0, 0, 0};
  uint8_t greenOff[4] = {1, 1, 1, 1};
  showduino_mosfet_identifier_frame(levelsOff, 64, greenOff);
  expect(greenOff[0]==0 && greenOff[1]==0 && greenOff[2]==0 && greenOff[3]==0,
         "ident all off");

  uint8_t levelsAct[4] = {100, 0, 50, 25};
  uint8_t greenAct[4] = {0};
  showduino_mosfet_identifier_frame(levelsAct, 64, greenAct);
  expect(greenAct[0] == 64, "ident OUT1 full (capped)");
  expect(greenAct[1] == 0, "ident OUT2 off");
  expect(greenAct[2] == 32, "ident OUT3 half");
  expect(greenAct[3] == 16, "ident OUT4 quarter");

  expect(showduino_mosfet_can_accept(SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED,
                                     SHOWDUINO_MOSFET_CMD_IDENTIFY,
                                     SHOWDUINO_CMD_ORIGIN_SHOW) == SHOWDUINO_MOSFET_FAIL_NONE,
         "IDENTIFY allowed while owned");
  expect(showduino_mosfet_can_accept(SHOWDUINO_MOSFET_ST_EMERGENCY,
                                     SHOWDUINO_MOSFET_CMD_IDENTIFY,
                                     SHOWDUINO_CMD_ORIGIN_SHOW) == SHOWDUINO_MOSFET_FAIL_EMERGENCY,
         "IDENTIFY rejected in emergency");

  /* Fade tracking uses engine current level, not target */
  ShowduinoMosfetEngine fadeEng;
  showduino_mosfet_engine_begin(&fadeEng, writeCb);
  showduino_mosfet_engine_fade(&fadeEng, 1, 100, 1000, 0);
  showduino_mosfet_engine_tick(&fadeEng, 500);
  uint8_t fadeLv[4];
  showduino_mosfet_engine_levels(&fadeEng, fadeLv);
  uint8_t fadeGreen[4];
  showduino_mosfet_identifier_frame(fadeLv, 64, fadeGreen);
  expect(fadeGreen[0] >= 28 && fadeGreen[0] <= 36, "ident follows fade midpoint");
  expect(fadeGreen[0] != 64, "ident not jumped to final during fade");

  if (gFails) { std::printf("%d FAILURES\n", gFails); return 1; }
  std::printf("ALL TESTS PASSED\n");
  return 0;
}
