/*
 * Host-side E1.31 parser tests (no Arduino / Ethernet).
 *
 *   g++ -std=c++17 -Wall -Wextra -I../../protocol -o e131_tests test_e131_parser.cpp
 *   ./e131_tests
 *
 * Windows:
 *   g++ -std=c++17 -Wall -Wextra -I../../protocol -o e131_tests.exe test_e131_parser.cpp
 *   .\e131_tests.exe
 */

#include <cstdio>
#include <cstring>
#include <cstdint>

#include "showduino_e131.h"

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

static void put16(uint8_t *p, size_t o, uint16_t v) {
  p[o] = (uint8_t)(v >> 8);
  p[o + 1] = (uint8_t)(v & 0xff);
}

static void put32(uint8_t *p, size_t o, uint32_t v) {
  p[o] = (uint8_t)(v >> 24);
  p[o + 1] = (uint8_t)(v >> 16);
  p[o + 2] = (uint8_t)(v >> 8);
  p[o + 3] = (uint8_t)(v & 0xff);
}

static void setFlagsLen(uint8_t *p, size_t o, size_t pduLen) {
  put16(p, o, (uint16_t)(0x7000u | (pduLen & 0x0fffu)));
}

static size_t buildPacket(uint8_t *p, size_t cap, uint16_t universe,
                          uint16_t slotCount, uint8_t startCode,
                          uint8_t sequence, const uint8_t *slots) {
  const size_t len = (size_t)SHOWDUINO_E131_HEADER_BYTES + (size_t)slotCount;
  if (len > cap) return 0;
  memset(p, 0, len);
  put16(p, SHOWDUINO_E131_OFF_PREAMBLE, 0x0010);
  put16(p, SHOWDUINO_E131_OFF_POSTAMBLE, 0x0000);
  memcpy(p + SHOWDUINO_E131_OFF_ACN_ID, SHOWDUINO_E131_ACN_ID, SHOWDUINO_E131_ACN_ID_LEN);
  setFlagsLen(p, SHOWDUINO_E131_OFF_ROOT_FLAGS, len - SHOWDUINO_E131_OFF_ROOT_FLAGS);
  put32(p, SHOWDUINO_E131_OFF_ROOT_VECTOR, SHOWDUINO_E131_ROOT_VECTOR);
  for (int i = 0; i < 16; i++) p[SHOWDUINO_E131_OFF_CID + i] = (uint8_t)(0xA0 + i);
  setFlagsLen(p, SHOWDUINO_E131_OFF_FRAMING_FLAGS, len - SHOWDUINO_E131_OFF_FRAMING_FLAGS);
  put32(p, SHOWDUINO_E131_OFF_FRAMING_VECTOR, SHOWDUINO_E131_FRAMING_VECTOR);
  memcpy(p + SHOWDUINO_E131_OFF_SOURCE_NAME, "QLC+", 4);
  p[SHOWDUINO_E131_OFF_PRIORITY] = 100;
  p[SHOWDUINO_E131_OFF_SEQUENCE] = sequence;
  put16(p, SHOWDUINO_E131_OFF_UNIVERSE, universe);
  setFlagsLen(p, SHOWDUINO_E131_OFF_DMP_FLAGS, len - SHOWDUINO_E131_OFF_DMP_FLAGS);
  p[SHOWDUINO_E131_OFF_DMP_VECTOR] = SHOWDUINO_E131_DMP_VECTOR;
  p[SHOWDUINO_E131_OFF_DMP_TYPE] = SHOWDUINO_E131_DMP_TYPE;
  put16(p, SHOWDUINO_E131_OFF_FIRST_ADDR, 0);
  put16(p, SHOWDUINO_E131_OFF_INCREMENT, 1);
  put16(p, SHOWDUINO_E131_OFF_PROP_COUNT, (uint16_t)(slotCount + 1));
  p[SHOWDUINO_E131_OFF_START_CODE] = startCode;
  if (slots && slotCount) memcpy(p + SHOWDUINO_E131_OFF_SLOTS, slots, slotCount);
  return len;
}

int main() {
  uint8_t pkt[SHOWDUINO_E131_MAX_PACKET + 8];
  uint8_t slots[SHOWDUINO_E131_MAX_SLOTS];
  ShowduinoE131View view;
  size_t len;

  std::printf("Showduino E1.31 host parser tests\n\n");

  {
    uint8_t mcast[4];
    showduino_e131_multicast_ipv4(1, mcast);
    expect(mcast[0] == 239 && mcast[1] == 255 && mcast[2] == 0 && mcast[3] == 1,
           "multicast universe 1 = 239.255.0.1");
    showduino_e131_multicast_ipv4(513, mcast);
    expect(mcast[0] == 239 && mcast[1] == 255 && mcast[2] == 2 && mcast[3] == 1,
           "multicast universe 513 = 239.255.2.1");
  }

  {
    const uint8_t data[4] = {255, 0, 128, 64};
    len = buildPacket(pkt, sizeof(pkt), 1, 4, 0x00, 10, data);
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view), SHOWDUINO_E131_OK,
                  "valid Universe 1 packet");
    expect_eq_int(view.universe, 1, "universe field");
    expect_eq_int(view.priority, 100, "priority");
    expect_eq_int(view.sequence, 10, "sequence");
    expect_eq_int(view.slotCount, 4, "slot count");
    expect_eq_int(view.startCode, 0, "start code 0");
    expect(view.slots && view.slots[0] == 255 && view.slots[2] == 128,
           "slot values");
    expect(strcmp(view.sourceName, "QLC+") == 0, "source name");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    pkt[SHOWDUINO_E131_OFF_ACN_ID] = 'X';
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_BAD_IDENTIFIER, "wrong ACN identifier");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    put32(pkt, SHOWDUINO_E131_OFF_ROOT_VECTOR, 0x00000008u);
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_BAD_ROOT_VECTOR, "wrong root vector");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    put32(pkt, SHOWDUINO_E131_OFF_FRAMING_VECTOR, 0x00000001u);
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_BAD_FRAMING_VECTOR, "wrong framing vector");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    pkt[SHOWDUINO_E131_OFF_DMP_VECTOR] = 0x01;
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_BAD_DMP_VECTOR, "wrong DMP vector");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    expect_eq_int(showduino_e131_parse(pkt, 80, 1, &view),
                  SHOWDUINO_E131_TRUNCATED, "truncated packet");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    put16(pkt, SHOWDUINO_E131_OFF_PROP_COUNT, 0);
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_BAD_PROPERTY_COUNT, "property count 0");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    put16(pkt, SHOWDUINO_E131_OFF_PROP_COUNT, 600);
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_BAD_PROPERTY_COUNT, "property count too large");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x17, 1, data);
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_UNSUPPORTED_START_CODE, "unsupported start code");
    expect_eq_int(view.startCode, 0x17, "start code preserved on reject");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 2, 2, 0x00, 1, data);
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_WRONG_UNIVERSE, "wrong universe");
  }

  expect_eq_int(showduino_e131_sequence_check(10, 11, 1), SHOWDUINO_E131_OK,
                "sequence progress 10 -> 11");
  expect_eq_int(showduino_e131_sequence_check(255, 0, 1), SHOWDUINO_E131_OK,
                "sequence wrap 255 -> 0");
  expect_eq_int(showduino_e131_sequence_check(40, 40, 1),
                SHOWDUINO_E131_DUPLICATE_SEQUENCE, "duplicate sequence");
  expect_eq_int(showduino_e131_sequence_check(40, 38, 1),
                SHOWDUINO_E131_OLD_SEQUENCE, "old sequence");
  expect_eq_int(showduino_e131_sequence_check(0, 1, 0), SHOWDUINO_E131_OK,
                "first sequence accepted");

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    setFlagsLen(pkt, SHOWDUINO_E131_OFF_ROOT_FLAGS, 20);
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_BAD_LENGTH, "malformed root length");
  }

  {
    const uint8_t data[2] = {1, 2};
    len = buildPacket(pkt, sizeof(pkt), 1, 2, 0x00, 1, data);
    pkt[SHOWDUINO_E131_OFF_ROOT_FLAGS] = 0x00;
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view),
                  SHOWDUINO_E131_BAD_LENGTH, "bad flags nibble");
  }

  {
    memset(slots, 0x5A, sizeof(slots));
    slots[0] = 1;
    slots[511] = 9;
    len = buildPacket(pkt, sizeof(pkt), 1, 512, 0x00, 7, slots);
    expect_eq_int((int)len, SHOWDUINO_E131_MAX_PACKET, "max packet size 638");
    expect_eq_int(showduino_e131_parse(pkt, len, 1, &view), SHOWDUINO_E131_OK,
                  "maximum 512-slot frame");
    expect_eq_int(view.slotCount, 512, "512 slots");
    expect(view.slots && view.slots[0] == 1 && view.slots[511] == 9,
           "first and last slot");
  }

  {
    expect_eq_int(showduino_e131_parse(pkt, SHOWDUINO_E131_MAX_PACKET + 1, 1, &view),
                  SHOWDUINO_E131_TOO_LARGE, "oversize packet rejected");
  }

  if (g_failures) {
    std::printf("\n%d FAIL\n", g_failures);
    return 1;
  }
  std::printf("\nAll E1.31 parser tests passed\n");
  return 0;
}
