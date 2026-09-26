#include "showduino_emergency_node.h"
#include "showduino_state_wire.h"
#include <cstdio>
#include <cstring>

static int gFails = 0;

static void expect(bool ok, const char *msg) {
  if (ok) std::printf("PASS  %s\n", msg);
  else {
    std::printf("FAIL  %s\n", msg);
    gFails++;
  }
}

static void expect_str(const char *got, const char *want, const char *msg) {
  expect(got && want && strcmp(got, want) == 0, msg);
}

int main() {
  std::printf("Emergency Node host tests\n\n");

  expect(showduino_emergency_id_ok("ESTOP-01"), "ESTOP-01 valid");
  expect(showduino_emergency_id_ok("ESTOP-06"), "ESTOP-06 valid");
  {
    char next[16];
    ShowduinoEmergencyUpdateGate g;
    expect(showduino_emergency_next_id("ESTOP-01", next, sizeof(next)) == 1,
           "ESTOP-01 next id");
    expect_str(next, "ESTOP-02", "ESTOP-01 -> ESTOP-02");
    expect(showduino_emergency_next_id("ESTOP-02", next, sizeof(next)) == 1,
           "ESTOP-02 next id");
    expect_str(next, "ESTOP-03", "ESTOP-02 -> ESTOP-03");
    showduino_emergency_update_gate(&g, "ESTOP-01", 0, 0);
    expect(!g.allow_next, "update hold before reboot/link");
    showduino_emergency_update_gate(&g, "ESTOP-01", 1, 0);
    expect(!g.allow_next, "healthy but not linked holds ESTOP-02");
    showduino_emergency_update_gate(&g, "ESTOP-01", 1, 1);
    expect(g.allow_next == 1, "ESTOP-01 healthy+linked allows ESTOP-02");
    expect_str(g.next_id, "ESTOP-02", "gate next is ESTOP-02");
    showduino_emergency_update_gate(&g, "ESTOP-02", 1, 1);
    expect(g.allow_next == 1, "ESTOP-02 healthy+linked allows ESTOP-03");
    expect_str(g.next_id, "ESTOP-03", "gate next is ESTOP-03");
  }
  expect(!showduino_emergency_id_ok("LAMP-01"), "LAMP-01 is not an Emergency ID");
  expect(!showduino_emergency_id_ok("e8:06:90:9b:7b:a8"), "MAC is not operator identity");
  expect(strstr(SHOWDUINO_EMERGENCY_CAPS, "MOMENTARY_BUTTON") != NULL,
         "caps declare momentary button");
  expect(strstr(SHOWDUINO_EMERGENCY_CAPS, "NC_INPUT") == NULL,
         "caps no longer declare NC_INPUT");

  /* TEST 1 — NORMAL BOOT */
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 0);
    expect(!m.latched && m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "BOOT + button RELEASED -> NORMAL");
    expect(!m.pending_assert, "boot released: no assert");
  }
  /* TEST 2 — BUTTON HELD DURING BOOT */
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 1);
    expect(m.latched && m.state == SHOWDUINO_ESTOP_ST_LATCHED,
           "BOOT + button PRESSED -> LOCAL EMERGENCY LATCH");
    expect(m.pending_assert, "boot-pressed pending assert");
  }
  /* TEST 3 — PRESS */
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 0);
    showduino_emergency_machine_input(&m, 1);
    expect(m.latched && m.pending_assert, "RELEASED then PRESSED -> latch+assert");
  }
  /* TEST 4 — RELEASE DOES NOT CLEAR */
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 0);
    showduino_emergency_machine_input(&m, 1);
    showduino_emergency_machine_input(&m, 0);
    expect(m.latched, "button RELEASED -> latch remains");
    expect(m.state == SHOWDUINO_ESTOP_ST_LATCHED, "still LATCHED after release");
    expect(!showduino_emergency_is_clear_token("ESTOP:ASSERT:ESTOP-01:X"),
           "assert is not a clear token");
  }

  /* TEST 8 — RADIO LOSS */
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 1);
    showduino_emergency_machine_set_radio(&m, 0);
    expect(m.latched && !showduino_emergency_machine_want_tx(&m, 10),
           "radio unavailable during activation -> latch retained, no TX");
    showduino_emergency_machine_set_radio(&m, 1);
    expect(showduino_emergency_machine_want_tx(&m, 10),
           "radio returns -> assertion transmitted");
  }

  {
    ShowduinoEmergencyMachine m;
    char cmd[48];
    showduino_emergency_machine_init(&m, 1);
    showduino_emergency_machine_set_radio(&m, 1);
    expect(showduino_emergency_machine_want_tx(&m, 0), "first assert immediate");
    showduino_emergency_machine_sent(&m, 0);
    expect(showduino_emergency_machine_want_tx(&m, SHOWDUINO_EMERGENCY_BURST_MS),
           "burst retry scheduled");
    showduino_emergency_format_assert("ESTOP-03", "MAZE EXIT", cmd, sizeof(cmd));
    expect_str(cmd, "ESTOP:ASSERT:ESTOP-03:MAZE EXIT", "assert wire");
  }

  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 1);
    showduino_emergency_machine_ack(&m);
    expect(m.acked && m.latched, "P4 ACK acknowledges without clearing");
    expect(showduino_emergency_machine_rearm(&m) == 0,
           "ACK alone is not a maintenance reset while latched without clear");
    expect(m.latched, "latched after ACK");
  }

  /* TEST 5 — GLOBAL CLEAR + RELEASED BUTTON */
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 1);
    showduino_emergency_machine_input(&m, 0);
    showduino_emergency_machine_global_observed(&m);
    expect(!m.latched && m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "global clear + RELEASED -> auto NORMAL");
    expect(!showduino_emergency_machine_want_tx(&m, 5000),
           "after global clear released, node stops asserting");
  }

  /* TEST 6 — GLOBAL CLEAR WHILE BUTTON HELD */
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 1);
    showduino_emergency_machine_set_radio(&m, 1);
    showduino_emergency_machine_global_observed(&m);
    expect(m.latched && m.state == SHOWDUINO_ESTOP_ST_LATCHED,
           "global clear while PRESSED -> remain latched");
    expect(m.pending_assert, "re-assert while button held");
    expect(showduino_emergency_machine_want_tx(&m, 0),
           "button held after clear still wants TX");
  }

  /* TEST 7 — SECOND PRESS after clear */
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 0);
    showduino_emergency_machine_input(&m, 1);
    showduino_emergency_machine_input(&m, 0);
    showduino_emergency_machine_global_observed(&m);
    expect(m.state == SHOWDUINO_ESTOP_ST_NORMAL, "ready after clear");
    showduino_emergency_machine_input(&m, 1);
    expect(m.latched && m.pending_assert, "second press asserts again");
  }

  /* Repeatability */
  {
    ShowduinoEmergencyMachine m;
    int i;
    showduino_emergency_machine_init(&m, 0);
    for (i = 0; i < 3; i++) {
      showduino_emergency_machine_input(&m, 1);
      expect(m.latched, "repeat press latches");
      showduino_emergency_machine_input(&m, 0);
      expect(m.latched, "repeat release keeps latch");
      showduino_emergency_machine_global_observed(&m);
      expect(m.state == SHOWDUINO_ESTOP_ST_NORMAL, "repeat clear -> READY");
    }
  }

  /* TEST 9 — NO CLEAR TOKENS */
  expect(showduino_emergency_parse_command("ESTOP:CLEAR") ==
             SHOWDUINO_ESTOP_CMD_REJECT_CLEAR,
         "ESTOP:CLEAR rejected");
  expect(showduino_emergency_parse_command("EMERGENCY:CLEAR_REQUEST") ==
             SHOWDUINO_ESTOP_CMD_REJECT_CLEAR,
         "CLEAR_REQUEST rejected");
  expect(showduino_emergency_parse_command("EMERGENCY:CLEAR_CONFIRM") ==
             SHOWDUINO_ESTOP_CMD_REJECT_CLEAR,
         "CLEAR_CONFIRM rejected");
  expect(showduino_emergency_parse_command("EMERGENCY:CLEAR") ==
             SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED,
         "P4 fan-out CLEAR is observed, not a node clear command");
  expect(showduino_emergency_is_clear_token("ESTOP:CLEAR"), "clear token detected");
  expect(showduino_emergency_authority_apply_node_clear(NULL, "ESTOP:CLEAR") == 0,
         "authority rejects node clear");

  {
    ShowduinoEmergencyAuthority a;
    showduino_emergency_authority_init(&a);
    expect(showduino_emergency_authority_assert(&a, "ESTOP-01", "ENTRANCE") == 1,
           "first wireless assert latches");
    expect(a.locked && a.wireless, "authority locked by ESTOP-01");
    expect(showduino_emergency_authority_assert(&a, "ESTOP-02", "CONTROL ROOM") == 0,
           "second assert is duplicate / additional source");
    expect(a.locked, "multiple nodes keep emergency active");
    expect(showduino_emergency_authority_apply_node_clear(&a, "ESTOP:CLEAR") == 0,
           "node packet cannot clear");
    expect(a.locked, "still locked after illegal clear");
    showduino_emergency_authority_offline(&a, 1);
    expect(a.offline_fault && a.locked, "offline is fault, not an unlock");
    showduino_emergency_authority_legitimate_clear(&a);
    expect(!a.locked, "legitimate policy may clear");
  }

  {
    ShowduinoEmergencyAuthority a;
    showduino_emergency_authority_init(&a);
    showduino_emergency_authority_hardwired(&a);
    expect(a.locked && a.hardwired, "hardwired path still latches");
    expect(showduino_emergency_authority_assert(&a, "ESTOP-01", "ENTRANCE") == 0,
           "wireless after hardwired is additional, not a second latch mode");
  }

  {
    ShowduinoEmergencyAnnounce an;
    char line[96];
    expect(showduino_emergency_parse_announce(
               "ANNOUNCE:AA:BB:CC:DD:EE:FF:0.2.0:LATCHED:ID=ESTOP-03:N=MAZE EXIT:IN=PRESSED:L=1:A=0",
               &an) == 1,
           "parse announce PRESSED");
    expect_str(an.id, "ESTOP-03", "announce id");
    expect_str(an.name, "MAZE EXIT", "announce name");
    expect(an.latched && an.input_open, "announce latch/pressed");
    expect(showduino_emergency_parse_announce(
               "ANNOUNCE:AA:BB:CC:DD:EE:FF:0.2.0:LATCHED:ID=ESTOP-03:N=X:IN=OPEN:L=1:A=0",
               &an) == 1,
           "legacy IN=OPEN still parses as pressed");
    expect(an.input_open, "OPEN compat = pressed");
    showduino_emergency_format_announce(&an, line, sizeof(line));
    expect(strstr(line, "ID=ESTOP-03") != NULL, "format announce keeps ID");
    expect(strstr(line, "IN=PRESSED") != NULL, "format announce uses PRESSED");
  }

  {
    ShowduinoEmergencyRoute rt;
    expect(showduino_emergency_parse_route("ROUTE:EMERGENCY:ESTOP-02:9:ESTOP:ACK:LATCHED",
                                           &rt) == 1,
           "parse route");
    expect_str(rt.id, "ESTOP-02", "route id");
    expect_str(rt.command, "ESTOP:ACK:LATCHED", "route command");
  }

  {
    ShowduinoEmergencyDetailWire d;
    expect(showduino_parse_state_node_emergency("STATE:NODE:EMERGENCY:FAULT") ==
               SHOWDUINO_EMERGENCY_NODE_WIRE_FAULT,
           "node wire FAULT");
    expect(showduino_parse_state_node_emergency_detail(
               "STATE:NODE:EMERGENCY:D:2:3:1:1:ESTOP-03:MAZE EXIT:LATCHED", &d) == 1,
           "detail parse");
    expect(d.online == 2 && d.seen == 3 && d.asserting == 1 && d.offline == 1,
           "detail counts");
    expect_str(d.firstId, "ESTOP-03", "detail first id");
  }

  {
    ShowduinoEmergencySourceWire s;
    expect(showduino_parse_state_emergency_source(
               "STATE:EMERGENCY:SOURCE:WIRELESS:ESTOP-03:MAZE EXIT", &s) == 1,
           "source wire");
    expect_str(s.kind, "WIRELESS", "source kind");
    expect_str(s.id, "ESTOP-03", "source id");
    expect(showduino_parse_state_emergency("STATE:EMERGENCY:SOURCE:WIRELESS:ESTOP-03:X") ==
               SHOWDUINO_EMERGENCY_WIRE_INVALID,
           "source line is not ACTIVE/CLEAR");
    expect(showduino_parse_state_safety_estop_fault("STATE:SAFETY:ESTOP:FAULT") == 1,
           "safety fault wire");
    expect(showduino_parse_state_safety_estop_fault("STATE:SAFETY:ESTOP:OK") == 0,
           "safety ok wire");
  }

  /* TEST 10 — OLED failure independence (conceptual host proof) */
  expect(!showduino_emergency_is_clear_token("OLED:FAIL"),
         "OLED failure is not a clear token");
  {
    ShowduinoEmergencyMachine m;
    showduino_emergency_machine_init(&m, 0);
    showduino_emergency_machine_input(&m, 1);
    expect(m.latched && m.pending_assert,
           "latch/assert path does not require OLED");
  }

  std::printf("\n");
  if (gFails) {
    std::printf("%d test(s) failed.\n", gFails);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
