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
  expect(SHOWDUINO_LAMP_FX_SOLID == 14, "SOLID numeric id unchanged");
  expect(SHOWDUINO_LAMP_FX_TABLE_LEN == (size_t)SHOWDUINO_LAMP_FX_COUNT,
         "table covers every FX id");

  unsigned carbide = 0, extra = 0;
  for (size_t i = 0; i < SHOWDUINO_LAMP_FX_TABLE_LEN; ++i) {
    if (SHOWDUINO_LAMP_FX_TABLE[i].origin == SHOWDUINO_LAMP_FX_CARBIDE) carbide++;
    else extra++;
  }
  expect(carbide == 19, "carbide origin count includes machine tokens");
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

  expect(showduino_lamp_parse_command("LAMP:NODE:LAMP-01:IGNITE", &c) ==
             SHOWDUINO_LAMP_CMD_IGNITE,
         "logical ignite");
  expect(strcmp(c.logicalId, "LAMP-01") == 0, "logical id extracted");
  expect(showduino_lamp_parse_command("LAMP:NODE:EXTINGUISH", &c) ==
             SHOWDUINO_LAMP_CMD_EXTINGUISH,
         "extinguish");
  expect(c.logicalId[0] == 0, "broadcast extinguish has empty id");
  expect(showduino_lamp_parse_command("LAMP:FX:LOW_FLAME", &c) ==
             SHOWDUINO_LAMP_CMD_FX,
         "low flame token");
  expect(c.fx == SHOWDUINO_LAMP_FX_LOW_FLAME, "low flame id");
  expect(showduino_lamp_fx_from_token("BURNING", &fx) == 0 &&
             fx == SHOWDUINO_LAMP_FX_STEADY_FLAME,
         "BURNING aliases STEADY_FLAME");
  expect(showduino_lamp_fx_from_token("UNSTABLE_FLAME", &fx) == 0 &&
             fx == SHOWDUINO_LAMP_FX_UNSTABLE,
         "UNSTABLE_FLAME alias");
  expect(showduino_lamp_id_ok("LAMP-01"), "LAMP-01 ok");
  expect(!showduino_lamp_id_ok("LED-01"), "LED-01 is not a lamp id");
  expect(showduino_lamp_id_matches("", "LAMP-01"), "empty id matches all");
  expect(!showduino_lamp_id_matches("LAMP-02", "LAMP-01"), "wrong id rejected");
  expect(showduino_lamp_can_accept_ex(SHOWDUINO_LAMP_ST_EMERGENCY,
                                      SHOWDUINO_LAMP_CMD_IGNITE,
                                      SHOWDUINO_CMD_ORIGIN_SHOW) ==
             SHOWDUINO_LAMP_FAIL_EMERGENCY,
         "emergency rejects ignite");
  expect(showduino_lamp_can_accept_ex(SHOWDUINO_LAMP_ST_SEARCHING,
                                      SHOWDUINO_LAMP_CMD_IGNITE,
                                      SHOWDUINO_CMD_ORIGIN_WEB) ==
             SHOWDUINO_LAMP_FAIL_NONE,
         "web commissioning ignite while searching");
  expect(showduino_lamp_comms_loss_extinguish(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED,
                                              1, 0) == 1,
         "show-controlled timeout extinguishes");
  expect(showduino_lamp_comms_loss_extinguish(SHOWDUINO_LAMP_ST_STANDALONE,
                                              0, 0) == 0,
         "standalone local burn continues");
  expect(showduino_lamp_comms_loss_extinguish(SHOWDUINO_LAMP_ST_EMERGENCY,
                                              1, 0) == 0,
         "emergency is not locally cleared by comms loss");

  expect(showduino_lamp_product_mode(SHOWDUINO_LAMP_ST_SEARCHING) ==
             SHOWDUINO_LAMP_PRODUCT_STANDALONE,
         "searching is standalone product mode");
  expect(showduino_lamp_product_mode(SHOWDUINO_LAMP_ST_STANDALONE) ==
             SHOWDUINO_LAMP_PRODUCT_STANDALONE,
         "standalone product mode");
  expect(showduino_lamp_product_mode(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED) ==
             SHOWDUINO_LAMP_PRODUCT_SHOWDUINO,
         "show-controlled is SHOWDUINO product mode");
  expect(showduino_lamp_product_mode(SHOWDUINO_LAMP_ST_EMERGENCY) ==
             SHOWDUINO_LAMP_PRODUCT_EMERGENCY,
         "emergency product mode");
  expect(!strcmp(showduino_lamp_product_mode_name(SHOWDUINO_LAMP_PRODUCT_STANDALONE),
                 "STANDALONE"),
         "standalone label");
  expect(!strcmp(showduino_lamp_product_mode_name(SHOWDUINO_LAMP_PRODUCT_SHOWDUINO),
                 "SHOWDUINO"),
         "showduino label");
  expect(showduino_lamp_web_may_control(SHOWDUINO_LAMP_ST_SEARCHING),
         "web may control while searching");
  expect(showduino_lamp_web_may_control(SHOWDUINO_LAMP_ST_STANDALONE),
         "web may control standalone");
  expect(!showduino_lamp_web_may_control(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED),
         "web may not override P4");
  expect(!showduino_lamp_web_may_control(SHOWDUINO_LAMP_ST_EMERGENCY),
         "web may not control emergency");
  expect(showduino_lamp_local_authority(SHOWDUINO_LAMP_ST_SEARCHING),
         "physical lamp lives during search");

  char ssid[33] = "";
  showduino_lamp_format_ssid("LAMP-01", ssid, sizeof(ssid));
  expect(!strcmp(ssid, "Showduino-Lamp-LAMP-01"), "ssid from logical id");
  showduino_lamp_format_ssid("LED-01", ssid, sizeof(ssid));
  expect(!strcmp(ssid, "Showduino-Lamp-LAMP"), "invalid id falls back");

  expect(showduino_lamp_grant_fresh(1, 4000, 1000, SHOWDUINO_OWNER_KEEPALIVE_MS),
         "short grant gap is still owned");
  expect(!showduino_lamp_grant_fresh(1, 10000, 1000, SHOWDUINO_OWNER_KEEPALIVE_MS),
         "stale grant is not standalone permission by itself");
  expect(!showduino_lamp_grant_fresh(0, 4000, 1000, SHOWDUINO_OWNER_KEEPALIVE_MS),
         "ungranted is not fresh");
  expect(showduino_lamp_light_normalized(50, 0) == -1, "uncalibrated light");
  expect(showduino_lamp_light_normalized(50, 100) == 50, "normalized light");
  expect(showduino_lamp_light_normalized(200, 100) == 100, "normalized clamp");

  ShowduinoOwnerMachine owner;
  showduino_owner_begin(&owner, 0);
  expect(owner.mode == SHOWDUINO_OWNER_SEARCHING, "boot searches");
  showduino_owner_apply(&owner, SHOWDUINO_OWNER_EV_TICK, 1000,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  expect(owner.mode == SHOWDUINO_OWNER_SEARCHING, "still searching at 1s");
  expect(showduino_lamp_web_may_control(showduino_lamp_state_from_owner(owner.mode)),
         "local control during search");
  showduino_owner_apply(&owner, SHOWDUINO_OWNER_EV_TICK, SHOWDUINO_OWNER_DISCOVER_MS,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  expect(owner.mode == SHOWDUINO_OWNER_STANDALONE, "discover timeout is standalone");
  expect(owner.enteredStandalone, "entered standalone edge");
  showduino_owner_apply(&owner, SHOWDUINO_OWNER_EV_GRANT, 9000,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  expect(owner.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED, "grant becomes showduino");
  expect(owner.enteredShow, "entered show edge");
  expect(!showduino_lamp_web_may_control(showduino_lamp_state_from_owner(owner.mode)),
         "no silent web override after grant");
  showduino_owner_apply(&owner, SHOWDUINO_OWNER_EV_TICK, 12000,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  expect(owner.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED,
         "momentary gap is not standalone");
  expect(!owner.lostAuthority, "keepalive window still open");
  showduino_owner_apply(&owner, SHOWDUINO_OWNER_EV_TICK, 9000 + SHOWDUINO_OWNER_KEEPALIVE_MS,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  expect(owner.mode == SHOWDUINO_OWNER_STANDALONE, "grant timeout returns standalone");
  expect(owner.lostAuthority, "lost authority is explicit");
  expect(showduino_lamp_comms_loss_extinguish(
             SHOWDUINO_LAMP_ST_SHOW_CONTROLLED, 1,
             showduino_lamp_grant_fresh(0, 20000, 9000,
                                        SHOWDUINO_OWNER_KEEPALIVE_MS)) == 1,
         "lost grant uses comms-loss extinguish");

  uint16_t start = 0, count = 0;
  showduino_lamp_list_slice(0, &start, &count);
  expect(start == 0 && count == SHOWDUINO_LAMP_LIST_PER_PAGE, "list page 0");
  showduino_lamp_list_slice(10, &start, &count);
  expect(count == 0, "list past end");

  expect(showduino_lamp_volt_mv(-1, SHOWDUINO_LAMP_VOLT_FS_MV,
                                SHOWDUINO_LAMP_VOLT_ADC_MAX) == -1,
         "negative raw stays uncalibrated");
  expect(showduino_lamp_volt_mv(4095, 0, SHOWDUINO_LAMP_VOLT_ADC_MAX) == -1,
         "missing num stays uncalibrated");
  expect(showduino_lamp_volt_mv(4095, SHOWDUINO_LAMP_VOLT_FS_MV,
                                SHOWDUINO_LAMP_VOLT_ADC_MAX) == 5000,
         "12-bit full scale is 5.00 V");
  expect(showduino_lamp_volt_mv(0, SHOWDUINO_LAMP_VOLT_FS_MV,
                                SHOWDUINO_LAMP_VOLT_ADC_MAX) == 0,
         "zero counts is 0 V");
  expect(showduino_lamp_volt_is_default_fs(SHOWDUINO_LAMP_VOLT_FS_MV,
                                           SHOWDUINO_LAMP_VOLT_ADC_MAX),
         "default 5 V FS flag");
  expect(!showduino_lamp_volt_is_default_fs(5000, 484), "one-point is not default FS");

  if (gFails) {
    std::printf("%d FAILED\n", gFails);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
