/*
 * MISSION 001L host tests: lost-latch-record detection, key separation between
 * station and Director authority, maintenance recovery policy, loop statistics.
 * Uses the same headers as the P4 firmware.
 * Build: g++ -std=c++17 -Wall -Wextra -Werror -Iprotocol test_recovery_001l.cpp
 */
#include "showduino_emergency_latch.h"
#include "showduino_station_auth.h"
#include "showduino_maintenance_recovery.h"
#include "showduino_loop_stats.h"
#include <cstdio>
#include <cstring>

static int gFails = 0, gPass = 0;
static void expect(bool ok, const char *msg) {
  if (ok) { std::printf("PASS  %s\n", msg); gPass++; }
  else { std::printf("FAIL  %s\n", msg); gFails++; }
}

static ShowduinoLatchRead decode(bool opened, bool key, uint8_t value, bool marker) {
  ShowduinoLatchReadInput in{};
  in.storeOpened = opened;
  in.readOk = opened;
  in.keyPresent = key;
  in.value = value;
  in.markerPresent = marker;
  return showduino_latch_decode(in);
}

int main() {
  /* ===== L1: lost latch record ===== */
  {
    expect(decode(true, false, 0, false) == SHOWDUINO_LATCH_READ_CLEAR,
           "L1a fresh device (no latch key, no marker) boots CLEAR");
    expect(decode(true, false, 0, true) == SHOWDUINO_LATCH_READ_FAULT,
           "L1b marker present but latch key missing: record lost -> FAULT (fail-safe latched)");
    expect(showduino_latch_boot_latched(decode(true, false, 0, true)) &&
           showduino_latch_boot_unknown(decode(true, false, 0, true)),
           "L1c a lost record restores LATCHED and blocks clear until recovery");
    expect(decode(true, true, SHOWDUINO_LATCH_VALUE_ACTIVE, true) == SHOWDUINO_LATCH_READ_ACTIVE &&
           decode(true, true, SHOWDUINO_LATCH_VALUE_CLEAR, true) == SHOWDUINO_LATCH_READ_CLEAR &&
           decode(true, true, SHOWDUINO_LATCH_VALUE_ACTIVE, false) == SHOWDUINO_LATCH_READ_ACTIVE,
           "L1d readable records decode as before (with or without the marker)");
    expect(decode(false, false, 0, false) == SHOWDUINO_LATCH_READ_FAULT,
           "L1e namespace that cannot be opened is still FAULT");
  }

  /* ===== L2: key separation (station vs Director) ===== */
  {
    uint8_t keys[3][SHOWDUINO_AUTH_KEY_BYTES];
    for (int k = 0; k < 3; ++k) for (int i = 0; i < 32; ++i) keys[k][i] = (uint8_t)(k * 50 + i + 1);
    uint8_t cand[32];
    std::memcpy(cand, keys[1], 32);
    expect(showduino_auth_key_conflicts(cand, keys, 3) == 1,
           "L2a a Director key equal to a station key is refused");
    expect(showduino_auth_key_conflicts(keys[2], keys, 2) == 0,
           "L2b a key not in the other authority's set is accepted");
    cand[31] ^= 1;
    expect(showduino_auth_key_conflicts(cand, keys, 3) == 0,
           "L2c a key differing only in the last byte is distinct");
    std::memcpy(cand, keys[2], 32);
    expect(showduino_auth_key_conflicts(cand, keys, 3) == 1,
           "L2d the last entry of the set is checked too");
    uint8_t zero[32] = {0};
    expect(showduino_auth_key_conflicts(zero, keys, 0) == 1 && showduino_auth_key_conflicts(nullptr, keys, 3) == 1,
           "L2e an all-zero or missing key is always refused");
    uint8_t dir[1][SHOWDUINO_AUTH_KEY_BYTES];
    std::memcpy(dir[0], keys[0], 32);
    expect(showduino_auth_key_conflicts(keys[0], dir, 1) == 1,
           "L2f the reverse direction (station key equal to the Director key) is refused");
  }

  /* ===== L3: blocker recovery gate ===== */
  {
    const char *T = SHOWDUINO_RECOVER_BLOCKERS_TOKEN;
    expect(showduino_recover_blockers_allowed(0, 1, 1, 0, T) == SHOWDUINO_RECOVER_REFUSE_SOURCE,
           "L3a recovery from the Comms UART / web is refused");
    expect(showduino_recover_blockers_allowed(1, 0, 1, 0, T) == SHOWDUINO_RECOVER_REFUSE_NOT_LATCHED,
           "L3b nothing to recover when not latched");
    expect(showduino_recover_blockers_allowed(1, 1, 0, 0, T) == SHOWDUINO_RECOVER_REFUSE_NOT_NEEDED,
           "L3c refused when the blocker state is known");
    expect(showduino_recover_blockers_allowed(1, 1, 1, 0, nullptr) == SHOWDUINO_RECOVER_REFUSE_TOKEN &&
           showduino_recover_blockers_allowed(1, 1, 1, 0, "INSPECTED") == SHOWDUINO_RECOVER_REFUSE_TOKEN &&
           showduino_recover_blockers_allowed(1, 1, 1, 0, "inspected-all-stations") == SHOWDUINO_RECOVER_REFUSE_TOKEN,
           "L3d the exact inspection confirmation is required");
    expect(showduino_recover_blockers_allowed(1, 1, 1, 1, T) == SHOWDUINO_RECOVER_REFUSE_BUTTON,
           "L3e refused while the P4 physical loop is open");
    expect(showduino_recover_blockers_allowed(1, 1, 1, 0, T) == SHOWDUINO_RECOVER_OK,
           "L3f local, latched, unknown, confirmed, loop closed: allowed");
  }

  /* ===== L4: conservative blocker set ===== */
  {
    ShowduinoBlocker stored[2]{};
    std::strcpy(stored[0].id, "ESTOP-09");   /* legacy station that asserted */
    std::strcpy(stored[1].id, "ESTOP-01");
    char prov[3][SHOWDUINO_BLOCKER_ID_CHARS + 1] = {"ESTOP-01", "ESTOP-02", "ESTOP-03"};
    ShowduinoBlocker out[SHOWDUINO_BLOCKER_MAX];
    int n = 0;
    expect(showduino_recover_build_blockers(stored, 2, prov, 3, out, SHOWDUINO_BLOCKER_MAX, &n) ==
               SHOWDUINO_RECOVER_OK && n == 4,
           "L4a union of readable stored blockers and provisioned stations, no duplicates");
    bool has09 = false;
    for (int i = 0; i < n; ++i) has09 |= std::strcmp(out[i].id, "ESTOP-09") == 0;
    expect(has09, "L4b a stored unprovisioned blocker is never discarded");
    expect(showduino_recover_build_blockers(nullptr, 0, prov, 3, out, SHOWDUINO_BLOCKER_MAX, &n) ==
               SHOWDUINO_RECOVER_OK && n == 3,
           "L4c unreadable stored record: every provisioned station becomes a blocker");
    expect(showduino_recover_build_blockers(stored, 2, prov, 3, out, 3, &n) ==
               SHOWDUINO_RECOVER_REFUSE_OVERFLOW && n == 0,
           "L4d a set that does not fit is refused, never truncated");
  }

  /* ===== L5: counter recovery gate ===== */
  {
    const char *T = SHOWDUINO_RECOVER_COUNTER_TOKEN;
    expect(showduino_recover_counter_allowed(0, 1, T) == SHOWDUINO_RECOVER_REFUSE_SOURCE &&
           showduino_recover_counter_allowed(1, 0, T) == SHOWDUINO_RECOVER_REFUSE_NOT_NEEDED &&
           showduino_recover_counter_allowed(1, 1, "LOST") == SHOWDUINO_RECOVER_REFUSE_TOKEN &&
           showduino_recover_counter_allowed(1, 1, T) == SHOWDUINO_RECOVER_OK,
           "L5a counter recovery: USB only, only when unreadable, exact confirmation");
    expect(SHOWDUINO_RECOVER_COUNTER_BASE > 0x80000000u,
           "L5b recovered counter starts far above any value a normal installation reaches");
  }

  /* ===== L6: loop statistics ===== */
  {
    ShowduinoLoopStats s;
    showduino_loop_stats_reset(&s);
    const uint32_t samples[] = {200, 1000, 1001, 4999, 20000, 49999, 100000, 100001, 250000};
    for (uint32_t us : samples) showduino_loop_stats_add(&s, us);
    expect(s.count == 9 && s.maxUs == 250000 && s.lastUs == 250000,
           "L6a count, last and worst loop recorded");
    expect(s.bucket[0] == 2 && s.bucket[1] == 2 && s.bucket[2] == 1 && s.bucket[3] == 1 &&
           s.bucket[4] == 1 && s.bucket[5] == 2,
           "L6b bucket boundaries (<=1, <=5, <=20, <=50, <=100 ms, >100 ms)");
    showduino_loop_stats_reset(&s);
    expect(s.count == 0 && s.maxUs == 0, "L6c reset");
  }

  std::printf("\n%d passed, %d failed\n", gPass, gFails);
  std::printf(gFails ? "FAILURES\n" : "ALL PASS\n");
  return gFails ? 1 : 0;
}
