/*
 * Host-side Showduino protocol validation tests (no Arduino / ESP32 SDK).
 *
 * Build (from repo root or this directory):
 *   g++ -std=c++17 -Wall -Wextra -I../../protocol -o protocol_tests test_protocol.cpp
 *   ./protocol_tests
 *
 * Windows (MinGW / clang):
 *   g++ -std=c++17 -Wall -Wextra -I../../protocol -o protocol_tests.exe test_protocol.cpp
 *   .\protocol_tests.exe
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "showduino_protocol_version.h"
#include "showduino_desk_packet.h"
#include "showduino_node_packet.h"
#include "showduino_validation.h"
#include "showduino_legacy_strings.h"
#include "showduino_message_types.h"
#include "showduino_state_wire.h"

static int g_failures = 0;

static void expect(bool ok, const char *name) {
  if (ok) {
    std::printf("PASS  %s\n", name);
  } else {
    std::printf("FAIL  %s\n", name);
    g_failures++;
  }
}

static void expect_eq_int(int got, int want, const char *name) {
  if (got == want) {
    std::printf("PASS  %s\n", name);
  } else {
    std::printf("FAIL  %s (got %d want %d)\n", name, got, want);
    g_failures++;
  }
}

int main() {
  std::printf("Showduino protocol host tests\n");
  std::printf("Package %d.%d wire desk version %d\n\n",
              SHOWDUINO_PROTOCOL_VERSION_MAJOR,
              SHOWDUINO_PROTOCOL_VERSION_MINOR,
              (int)SHOWDUINO_DESK_WIRE_VERSION);

  /* ---- sizes ---- */
  expect(sizeof(ShowduinoDeskPacket) == SHOWDUINO_DESK_PACKET_SIZE_EXPECTED,
         "desk packet size == 108");
  expect(sizeof(ShowduinoNodePacket) == SHOWDUINO_NODE_PACKET_SIZE_EXPECTED,
         "node packet size == 116");

  /* ---- magic / version ---- */
  {
    ShowduinoDeskPacket p;
    showduino_desk_packet_init(&p, 1, 1000);
    expect(showduino_desk_set_command(&p, "HELLO") == 0, "desk set_command HELLO");
    expect_eq_int(showduino_validate_desk_rx(&p, sizeof(p)), SHOWDUINO_VALID,
                  "desk valid HELLO");
  }

  {
    ShowduinoDeskPacket p;
    showduino_desk_packet_init(&p, 1, 1000);
    showduino_desk_set_command(&p, "HELLO");
    p.magic = 0xDEADBEEF;
    expect_eq_int(showduino_validate_desk_rx(&p, sizeof(p)), SHOWDUINO_INVALID_MAGIC,
                  "desk invalid magic");
  }

  {
    ShowduinoDeskPacket p;
    showduino_desk_packet_init(&p, 1, 1000);
    showduino_desk_set_command(&p, "HELLO");
    p.version = 99;
    expect_eq_int(showduino_validate_desk_rx(&p, sizeof(p)), SHOWDUINO_UNSUPPORTED_VERSION,
                  "desk unsupported major/wire version");
  }

  {
    ShowduinoDeskPacket p;
    showduino_desk_packet_init(&p, 1, 1000);
    showduino_desk_set_command(&p, "HELLO");
    expect_eq_int(showduino_validate_desk_rx(&p, sizeof(p) - 1), SHOWDUINO_INVALID_SIZE,
                  "desk invalid size");
  }

  /* ---- empty / max / oversized payload ---- */
  {
    ShowduinoDeskPacket p;
    showduino_desk_packet_init(&p, 1, 1000);
    expect_eq_int(showduino_validate_desk_rx(&p, sizeof(p)), SHOWDUINO_EMPTY_PAYLOAD,
                  "desk empty payload");
  }

  {
    char maxCmd[SHOWDUINO_DESK_COMMAND_MAX];
    memset(maxCmd, 'A', SHOWDUINO_DESK_COMMAND_MAX - 1);
    maxCmd[SHOWDUINO_DESK_COMMAND_MAX - 1] = '\0';
    ShowduinoDeskPacket p;
    showduino_desk_packet_init(&p, 1, 1000);
    expect(showduino_desk_set_command(&p, maxCmd) == 0, "desk set max valid payload");
    expect_eq_int(showduino_validate_desk_rx(&p, sizeof(p)), SHOWDUINO_VALID,
                  "desk max valid payload validates");
  }

  {
    char tooLong[SHOWDUINO_DESK_COMMAND_MAX + 8];
    memset(tooLong, 'B', sizeof(tooLong) - 1);
    tooLong[sizeof(tooLong) - 1] = '\0';
    ShowduinoDeskPacket p;
    showduino_desk_packet_init(&p, 1, 1000);
    expect(showduino_desk_set_command(&p, tooLong) == -3, "desk reject oversized set_command");
  }

  /* ---- null termination ---- */
  {
    ShowduinoDeskPacket p;
    showduino_desk_packet_init(&p, 1, 1000);
    memset(p.command, 'X', SHOWDUINO_DESK_COMMAND_MAX);
    expect_eq_int(showduino_validate_desk_rx(&p, sizeof(p)), SHOWDUINO_PAYLOAD_NOT_TERMINATED,
                  "desk unterminated payload");
  }

  /* ---- safe copy ---- */
  {
    char buf[8];
    expect_eq_int(showduino_safe_copy_command(buf, sizeof(buf), "OK"), SHOWDUINO_VALID,
                  "safe_copy short");
    expect(strcmp(buf, "OK") == 0, "safe_copy contents");
    expect_eq_int(showduino_safe_copy_command(buf, sizeof(buf), "TOO_LONG!"),
                  SHOWDUINO_PAYLOAD_TOO_LONG, "safe_copy reject oversized");
    expect_eq_int(showduino_safe_copy_command(buf, sizeof(buf), ""), SHOWDUINO_EMPTY_PAYLOAD,
                  "safe_copy empty");
  }

  /* ---- node packet ---- */
  {
    ShowduinoNodePacket n;
    showduino_node_packet_init(&n, "RELAY", 7);
    expect(showduino_node_set_command(&n, "RELAY:1:ON") == 0, "node set_command");
    expect_eq_int(showduino_validate_node_rx(&n, sizeof(n)), SHOWDUINO_VALID, "node valid");
  }

  {
    ShowduinoNodePacket n;
    showduino_node_packet_clear(&n);
    showduino_node_set_command(&n, "RELAY:1:ON");
    expect_eq_int(showduino_validate_node_rx(&n, sizeof(n)), SHOWDUINO_INVALID_NODE_TYPE,
                  "node empty type");
  }

  /* ---- legacy string mapping ---- */
  expect_eq_int(showduino_legacy_map_command(SHOWDUINO_LEGACY_SHOW_START),
                SHOWDUINO_MSG_SHOW_START_REQUEST, "map SHOW:START");
  expect_eq_int(showduino_legacy_map_command(SHOWDUINO_LEGACY_SHOW_STOP),
                SHOWDUINO_MSG_SHOW_STOP_REQUEST, "map SHOW:STOP");
  expect_eq_int(showduino_legacy_map_command(SHOWDUINO_LEGACY_EMERGENCY_STOP),
                SHOWDUINO_MSG_EMERGENCY_ACTIVATE_REQUEST, "map EMERGENCY:STOP");
  expect_eq_int(showduino_legacy_map_command(SHOWDUINO_LEGACY_EMERGENCY_CLEAR),
                SHOWDUINO_MSG_EMERGENCY_CLEAR_REQUEST, "map EMERGENCY:CLEAR");
  expect_eq_int(showduino_legacy_map_command("RELAY:1:ON"), SHOWDUINO_MSG_RELAY_SET_REQUEST,
                "map RELAY:1:ON");
  expect_eq_int(showduino_legacy_map_command("RELAY:1:OFF"), SHOWDUINO_MSG_RELAY_SET_REQUEST,
                "map RELAY:1:OFF");
  expect_eq_int(showduino_legacy_map_command("RELAY:3:TOGGLE"),
                SHOWDUINO_MSG_RELAY_TOGGLE_DEPRECATED, "map deprecated TOGGLE");
  expect_eq_int(showduino_legacy_map_command("NOT_A_REAL_CMD"), SHOWDUINO_MSG_UNKNOWN,
                "map unknown command");
  expect(showduino_legacy_is_toggle("RELAY:2:TOGGLE") != 0, "is_toggle true");
  expect(showduino_legacy_is_toggle("RELAY:2:ON") == 0, "is_toggle false for ON");

  /* ---- relay parse ---- */
  {
    int ch = 0;
    ShowduinoRelayState st = SHOWDUINO_RELAY_OFF;
    expect(showduino_legacy_parse_relay_set("RELAY:1:ON", &ch, &st) == 0, "parse RELAY:1:ON");
    expect(ch == 1 && st == SHOWDUINO_RELAY_ON, "parse ON values");
    expect(showduino_legacy_parse_relay_set("RELAY:8:OFF", &ch, &st) == 0, "parse RELAY:8:OFF");
    expect(ch == 8 && st == SHOWDUINO_RELAY_OFF, "parse OFF values");
    expect(showduino_legacy_parse_relay_set("RELAY:1:TOGGLE", &ch, &st) != 0,
           "parse rejects TOGGLE");
    expect(showduino_legacy_parse_relay_set("RELAY:9:ON", &ch, &st) != 0,
           "parse rejects channel 9");
    expect_eq_int(showduino_validate_relay_channel(1), SHOWDUINO_VALID, "relay ch 1 valid");
    expect_eq_int(showduino_validate_relay_channel(0), SHOWDUINO_INVALID_RELAY_CHANNEL,
                  "relay ch 0 invalid");
  }

  /* ---- Stage 3 state wire parsing ---- */
  {
    expect_eq_int(showduino_parse_state_show("STATE:SHOW:IDLE"), SHOWDUINO_SHOW_WIRE_IDLE,
                  "parse STATE:SHOW:IDLE");
    expect_eq_int(showduino_parse_state_show("STATE:SHOW:PLAYING"), SHOWDUINO_SHOW_WIRE_PLAYING,
                  "parse STATE:SHOW:PLAYING");
    expect_eq_int(showduino_parse_state_show("STATE:SHOW:EMERGENCY"), SHOWDUINO_SHOW_WIRE_EMERGENCY,
                  "parse STATE:SHOW:EMERGENCY");
    expect_eq_int(showduino_parse_state_emergency("STATE:EMERGENCY:ACTIVE"),
                  SHOWDUINO_EMERGENCY_WIRE_ACTIVE, "parse STATE:EMERGENCY:ACTIVE");
    expect_eq_int(showduino_parse_state_emergency("STATE:EMERGENCY:CLEAR"),
                  SHOWDUINO_EMERGENCY_WIRE_CLEAR, "parse STATE:EMERGENCY:CLEAR");
    expect_eq_int(showduino_parse_state_node_relay("STATE:NODE:RELAY:ONLINE"),
                  SHOWDUINO_NODE_WIRE_ONLINE, "parse NODE ONLINE");
    expect_eq_int(showduino_parse_state_node_relay("STATE:NODE:RELAY:OFFLINE"),
                  SHOWDUINO_NODE_WIRE_OFFLINE, "parse NODE OFFLINE");

    int ch = 0;
    ShowduinoRelayKnowledgeWire rk = SHOWDUINO_RELAY_WIRE_INVALID;
    expect(showduino_parse_state_relay("STATE:RELAY:1:ON", &ch, &rk) == 0, "parse STATE:RELAY:1:ON");
    expect(ch == 1 && rk == SHOWDUINO_RELAY_WIRE_ON, "STATE relay ON values");
    expect(showduino_parse_state_relay("STATE:RELAY:2:OFF", &ch, &rk) == 0, "parse STATE:RELAY:2:OFF");
    expect(ch == 2 && rk == SHOWDUINO_RELAY_WIRE_OFF, "STATE relay OFF values");
    expect(showduino_parse_state_relay("STATE:RELAY:3:UNKNOWN", &ch, &rk) == 0,
           "parse STATE:RELAY:3:UNKNOWN");
    expect(rk == SHOWDUINO_RELAY_WIRE_UNKNOWN, "STATE relay UNKNOWN value");

    char reason[32];
    expect(showduino_parse_relay_outcome("REJECTED:RELAY:4:EMERGENCY_ACTIVE",
                                         SHOWDUINO_WIRE_REJECTED_RELAY_PREFIX,
                                         &ch, reason, sizeof(reason)) == 0,
           "parse REJECTED:RELAY");
    expect(ch == 4 && strcmp(reason, "EMERGENCY_ACTIVE") == 0, "REJECTED reason");
    expect(showduino_parse_relay_outcome("FAILED:RELAY:5:TIMEOUT",
                                         SHOWDUINO_WIRE_FAILED_RELAY_PREFIX,
                                         &ch, reason, sizeof(reason)) == 0,
           "parse FAILED:RELAY");
    expect(ch == 5 && strcmp(reason, "TIMEOUT") == 0, "FAILED reason");

    expect(strcmp(SHOWDUINO_WIRE_SNAPSHOT_BEGIN, "SNAPSHOT:BEGIN") == 0, "SNAPSHOT:BEGIN token");
    expect(strcmp(SHOWDUINO_WIRE_SNAPSHOT_END, "SNAPSHOT:END") == 0, "SNAPSHOT:END token");
    expect(strncmp(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX, "UNSUPPORTED:", 12) == 0,
           "UNSUPPORTED prefix");
    expect(strncmp(SHOWDUINO_WIRE_ACCEPTED_RELAY_PREFIX, "ACCEPTED:RELAY:", 15) == 0,
           "ACCEPTED:RELAY prefix");
  }

  /* Absolute relay requests preferred; TOGGLE rejected by set-parser when unknown */
  {
    int ch = 0;
    ShowduinoRelayState st = SHOWDUINO_RELAY_OFF;
    expect(showduino_legacy_parse_relay_set("RELAY:1:ON", &ch, &st) == 0, "absolute ON request");
    expect(showduino_legacy_parse_relay_set("RELAY:1:OFF", &ch, &st) == 0, "absolute OFF request");
    expect(showduino_legacy_map_command("RELAY:1:TOGGLE") == SHOWDUINO_MSG_RELAY_TOGGLE_DEPRECATED,
           "TOGGLE remains deprecated mapping");
  }

  /* Invalid packet rejected before payload use */
  {
    ShowduinoDeskPacket bad = {};
    bad.magic = 0;
    bad.version = 1;
    strcpy(bad.command, "HELLO");
    expect_eq_int(showduino_validate_desk_rx(&bad, sizeof(bad)), SHOWDUINO_INVALID_MAGIC,
                  "invalid desk rejected before parse");
  }

  std::printf("\n");
  if (g_failures == 0) {
    std::printf("All tests passed.\n");
    return 0;
  }
  std::printf("%d test(s) failed.\n", g_failures);
  return 1;
}
