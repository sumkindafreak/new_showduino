/*
 * Host tests for the persistent P4 emergency latch policy.
 *
 * Exercises protocol/showduino_emergency_latch.h against a simulated NVS
 * namespace that can be made to fail open, read or write. The simulated
 * write mirrors the P4 wrapper: write, then read back and verify.
 *
 * Scope: the decision logic only. The Arduino/ESP-IDF wiring in the Stage
 * Engine sketch is NOT covered here and needs hardware acceptance.
 */
#include "showduino_emergency_latch.h"
#include "showduino_station_blockers.h"
#include "showduino_emergency_engage.h"
#include <cstdio>
#include <cstring>

static int gFails = 0;

static bool isCmd(const char *got, const char *want) {
  return got != NULL && std::strcmp(got, want) == 0;
}

static void expect(bool ok, const char *msg) {
  if (ok) std::printf("PASS  %s\n", msg);
  else { std::printf("FAIL  %s\n", msg); gFails++; }
}

/* Simulated NVS namespace. Power loss is modelled by discarding everything
 * that is not in `value`/`present` (each write is atomic here). */
struct FakeNvs {
  bool present = false;
  uint8_t value = 0;
  int writes = 0;
  bool failOpen = false;
  bool failRead = false;
  bool failWrite = false;
  bool corruptReadback = false;

  /* Mirrors EmergencyLatchStore: open, read, decode. */
  ShowduinoLatchRead boot() const {
    ShowduinoLatchReadInput in{};
    in.storeOpened = !failOpen;
    in.readOk = !failRead;
    in.keyPresent = present;
    in.value = value;
    return showduino_latch_decode(in);
  }

  /* Mirrors EmergencyLatchStore write: putUChar, then read back. */
  bool write(bool active) {
    writes++;
    if (failWrite) return false;
    present = true;
    value = active ? SHOWDUINO_LATCH_VALUE_ACTIVE : SHOWDUINO_LATCH_VALUE_CLEAR;
    if (corruptReadback) return false;
    return !failRead && value == (active ? SHOWDUINO_LATCH_VALUE_ACTIVE : SHOWDUINO_LATCH_VALUE_CLEAR);
  }
};

/* Firmware-shaped flow: an assertion persists before outputs engage. */
static bool assertLatch(FakeNvs &nvs, ShowduinoLatchTrack &t) {
  if (showduino_latch_needs_write(&t, true)) {
    const bool ok = nvs.write(true);
    showduino_latch_record_write(&t, true, ok);
  }
  return true; /* RAM latch is always engaged on assertion */
}

/* Firmware-shaped flow: a clear is released only after a verified CLEAR. */
static bool clearLatch(FakeNvs &nvs, ShowduinoLatchTrack &t, bool *ramReleased) {
  bool verified = true;
  if (showduino_latch_needs_write(&t, false)) {
    verified = nvs.write(false);
    showduino_latch_record_write(&t, false, verified);
  }
  *ramReleased = showduino_latch_clear_may_release(verified);
  return verified;
}

static bool bootLatched(const FakeNvs &nvs, ShowduinoLatchTrack *t) {
  const ShowduinoLatchRead r = nvs.boot();
  showduino_latch_track_from_boot(t, r);
  return showduino_latch_boot_latched(r);
}

int main() {
  std::printf("== Emergency latch policy tests ==\n");

  /* 1. Normal boot, nothing stored: clear, no write needed. */
  {
    FakeNvs nvs; ShowduinoLatchTrack t{};
    expect(!bootLatched(nvs, &t), "T1 fresh boot with no key is clear");
    expect(!showduino_latch_needs_write(&t, false), "T1 no write needed when clear");
    expect(nvs.writes == 0, "T1 no storage write on normal boot");
  }

  /* 2. Assertion persists and survives simulated reboot. */
  {
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    assertLatch(nvs, t);
    expect(nvs.writes == 1 && nvs.value == SHOWDUINO_LATCH_VALUE_ACTIVE,
           "T2 assertion writes ACTIVE once");
    ShowduinoLatchTrack t2{};
    expect(bootLatched(nvs, &t2), "T2/T3 latch survives simulated reboot");
  }

  /* 4. Reboot after the physical button was released. The boot decision takes
   * no button input, so a persisted latch stays latched. */
  {
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    assertLatch(nvs, t);
    /* button pressed then released; power cut; reboot */
    ShowduinoLatchTrack t2{};
    expect(bootLatched(nvs, &t2), "T4 reboot after button released stays latched");
  }

  /* 5. Reboot while the physical input remains asserted. Same decision: the
   * stored latch is restored. The live button then re-asserts idempotently. */
  {
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    assertLatch(nvs, t);
    ShowduinoLatchTrack t2{};
    expect(bootLatched(nvs, &t2), "T5 reboot while button held stays latched");
    expect(!showduino_latch_needs_write(&t2, true),
           "T5 re-assertion while held needs no extra write");
  }

  /* 6. Invalid clear: nothing persisted, RAM untouched. Modelled as a clear
   * attempt that never reaches storage because the firmware gate rejects it
   * (latch not active, or button still asserted). Storage must be untouched. */
  {
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    assertLatch(nvs, t);
    const int before = nvs.writes;
    /* Firmware gate rejects here; no call to clearLatch(). */
    expect(nvs.writes == before, "T6 rejected clear does not touch storage");
    expect(nvs.value == SHOWDUINO_LATCH_VALUE_ACTIVE, "T6 latch still ACTIVE");
  }

  /* 7. Valid deliberate manual clear persists CLEAR and releases RAM. */
  {
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    assertLatch(nvs, t);
    bool released = false;
    clearLatch(nvs, t, &released);
    expect(released, "T7 valid clear releases RAM latch");
    ShowduinoLatchTrack t2{};
    expect(!bootLatched(nvs, &t2), "T7 cleared latch stays clear after reboot");
  }

  /* 8. Storage errors. */
  {
    FakeNvs nvs; nvs.failOpen = true; ShowduinoLatchTrack t{};
    expect(bootLatched(nvs, &t), "T8a open failure at boot is fail-safe LATCHED");
  }
  {
    FakeNvs nvs; nvs.failRead = true; nvs.present = true; nvs.value = SHOWDUINO_LATCH_VALUE_CLEAR;
    ShowduinoLatchTrack t{};
    expect(bootLatched(nvs, &t), "T8b read failure at boot is fail-safe LATCHED");
  }
  {
    FakeNvs nvs; nvs.present = true; nvs.value = 0x00; ShowduinoLatchTrack t{};
    expect(bootLatched(nvs, &t), "T8c corrupt stored byte is fail-safe LATCHED");
  }
  {
    /* Clear write fails: the latch must NOT release. */
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    assertLatch(nvs, t);
    nvs.failWrite = true;
    bool released = true;
    clearLatch(nvs, t, &released);
    expect(!released, "T8d failed CLEAR write does not release the latch");
    expect(t.dirty, "T8d failed CLEAR is recorded as dirty");
    ShowduinoLatchTrack t2{};
    expect(bootLatched(nvs, &t2), "T8d reboot after failed CLEAR stays latched");
  }
  {
    /* Assertion write fails: RAM latch still engaged; retry on next trigger. */
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    nvs.failWrite = true;
    assertLatch(nvs, t);
    expect(t.dirty, "T8e failed ACTIVE write is recorded as dirty");
    nvs.failWrite = false;
    assertLatch(nvs, t); /* the retry path writes again because dirty */
    expect(!t.dirty && nvs.value == SHOWDUINO_LATCH_VALUE_ACTIVE,
           "T8e retry persists ACTIVE and clears dirty");
  }
  {
    /* Read-back mismatch counts as a failed write. */
    FakeNvs nvs; nvs.corruptReadback = true; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    assertLatch(nvs, t);
    expect(t.dirty, "T8f read-back mismatch is treated as a failed write");
  }

  /* 9. No automatic return to normal playback after emergency. Policy
   * level: a restored latch is ACTIVE, so the firmware must not start a show
   * on boot. The runtime gate is checked in firmware; here we assert the
   * latch decision that gate depends on. */
  {
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    assertLatch(nvs, t);
    ShowduinoLatchTrack t2{};
    expect(bootLatched(nvs, &t2), "T9 latched after reboot (playback gate input)");
    expect(nvs.value == SHOWDUINO_LATCH_VALUE_ACTIVE, "T9 no auto clear written on boot");
  }

  /* Writes happen only on transitions, not per loop iteration. */
  {
    FakeNvs nvs; ShowduinoLatchTrack t{};
    bootLatched(nvs, &t);
    for (int i = 0; i < 1000; i++) assertLatch(nvs, t);
    expect(nvs.writes == 1, "transition-only: 1000 repeated assertions write once");
  }

  /* 10. Wireless station gate. A pressed Emergency Node blocks clear. A
   * released station must not block (that would deadlock a wireless latch). */
  expect(showduino_station_clear_blocked(0u) == 0,
         "T10a no pressed station: clear allowed");
  expect(showduino_station_clear_blocked(1u) != 0,
         "T10b one pressed station: clear refused");
  expect(showduino_station_clear_blocked(3u) != 0,
         "T10c several pressed stations: clear refused");
  {
    /* Station pressed -> assert -> released -> Director clear must be allowed.
     * The station's own release is reported through input_open=0. */
    unsigned pressed = 1u;
    expect(showduino_station_clear_blocked(pressed) != 0,
           "T10d station still pressed during latch: refused");
    pressed = 0u;
    expect(showduino_station_clear_blocked(pressed) == 0,
           "T10e station released (input_open=0): clear allowed, no deadlock");
  }

  /* 11. Node emergency-state reconciliation (Pixel and MOSFET). A mismatch
   * sends one command. A node that already agrees gets nothing, so repeated
   * effect restarts cannot happen. */
  expect(isCmd(showduino_node_emergency_resync_cmd(1, 0), "EMERGENCY:STOP"),
         "T11a P4 locked, node not in EMERGENCY: EMERGENCY:STOP");
  expect(showduino_node_emergency_resync_cmd(1, 1) == NULL,
         "T11b P4 locked, node already EMERGENCY: no command (no restart)");
  expect(isCmd(showduino_node_emergency_resync_cmd(0, 1), "EMERGENCY:CLEAR"),
         "T11c P4 clear, node still EMERGENCY: EMERGENCY:CLEAR");
  expect(showduino_node_emergency_resync_cmd(0, 0) == NULL,
         "T11d both clear: no command");

  /* 12. Wireless station re-acknowledge and global-clear resync. */
  expect(showduino_station_needs_ack(1, 1, 1, 0) == 1, "T12a locked, online, unacked: ack");
  expect(showduino_station_needs_ack(1, 1, 1, 1) == 0, "T12b already acked: no ack repeat");
  expect(showduino_station_needs_ack(1, 0, 1, 0) == 0, "T12c offline: no ack until seen");
  expect(showduino_station_needs_global_clear(0, 1, 1) == 1,
         "T12d P4 clear, station still latched: GLOBAL:OBSERVED");
  expect(showduino_station_needs_global_clear(1, 1, 1) == 0,
         "T12e P4 locked: no global-clear resync");
  expect(showduino_station_needs_global_clear(0, 1, 0) == 0,
         "T12f station already clear: no resync");

  /* 13. Wireless release policy (latest report wins; stale is never released). */
  {
    ShowduinoStationView st[1] = {{1, 1, 1, 1}};          /* pressed and online */
    expect(showduino_stations_pressed(st, 1) == 1, "T13a asserted station blocks clear");
    st[0].online = 0;                                      /* disconnect while asserted */
    expect(showduino_stations_pressed(st, 1) == 1,
           "T13b disconnect while asserted keeps blocking");
    st[0].online = 1;                                      /* restart, no release report */
    expect(showduino_stations_pressed(st, 1) == 1,
           "T13c restart alone does not release the block");
    st[0].input_open = 0; st[0].latched = 0;               /* fresh report: released */
    expect(showduino_stations_pressed(st, 1) == 0,
           "T13d fresh valid report input_open=0 releases the block");
    st[0].input_open = 1;                                  /* pressed again: contradiction */
    expect(showduino_stations_pressed(st, 1) == 1,
           "T13e later assert overrides earlier release");
    st[0].online = 0;                                      /* then silent for a long time */
    expect(showduino_stations_pressed(st, 1) == 1,
           "T13f stale pressed station still blocks (no timeout release)");
  }
  {
    ShowduinoStationView st[2] = {{1, 1, 0, 0}, {0, 0, 1, 0}};  /* unused slot ignored */
    expect(showduino_stations_pressed(st, 2) == 0,
           "T13g unused slot does not block; released station does not block");
    st[1].used = 1; st[1].online = 1; st[1].input_open = 1;
    expect(showduino_stations_pressed(st, 2) == 1,
           "T13h second station pressed blocks clear");
  }

  /* ---- MISSION 001D: persistent wireless clear blockers ----
   * Persistence sequences use a fake store that mirrors the NVS wrapper:
   * whole-string write, then read-back compare. The node-link glue is
   * mirrored here as the same calls in the same order. Target code is not run. */
  struct BlockerStore {
    bool present = false;
    char val[SHOWDUINO_BLOCKER_BUF] = "";
    int writes = 0;
    bool failWrite = false;
    bool corrupt = false; /* simulates a damaged stored string */
  };
  auto storeSave = [](BlockerStore &st, const char *enc) -> bool {
    st.writes++;
    if (st.failWrite) return false;
    std::snprintf(st.val, sizeof(st.val), "%s", enc);
    st.present = true;
    return std::strcmp(st.val, enc) == 0;
  };
  auto storeLoad = [](BlockerStore &st, ShowduinoBlocker *out, uint8_t *n) {
    *n = 0;
    if (!st.present) return SHOWDUINO_BLOCKER_ABSENT;
    return showduino_blocker_decode(st.corrupt ? "B1;garbage" : st.val, out, n);
  };
  auto mk = [](ShowduinoBlocker &b, const char *id, const char *mac) {
    std::snprintf(b.id, sizeof(b.id), "%s", id);
    std::snprintf(b.mac, sizeof(b.mac), "%s", mac);
  };
  const char *MAC1 = "AA:BB:CC:DD:EE:01";
  const char *MAC2 = "AA:BB:CC:DD:EE:02";

  /* 14. Codec: round trip, and every damaged form is CORRUPT. */
  {
    ShowduinoBlocker in[2], out[SHOWDUINO_BLOCKER_MAX];
    uint8_t n = 0;
    mk(in[0], "ESTOP-01", MAC1); mk(in[1], "ESTOP-02", "-");
    char enc[SHOWDUINO_BLOCKER_BUF];
    int len = showduino_blocker_encode(in, 2, enc, sizeof(enc));
    expect(len > 0, "T14a encode a two-station set");
    expect(showduino_blocker_decode(enc, out, &n) == SHOWDUINO_BLOCKER_OK && n == 2,
           "T14b round trip restores both stations");
    expect(out[1].mac[0] == '\0', "T14c MOSFET-style '-' mac round-trips as empty");
    char empty[64];
    int el = showduino_blocker_encode(in, 0, empty, sizeof(empty));
    expect(el > 0 && showduino_blocker_decode(empty, out, &n) == SHOWDUINO_BLOCKER_OK && n == 0,
           "T14d empty set round trips as OK with zero stations");
    char trunc[SHOWDUINO_BLOCKER_BUF];
    std::snprintf(trunc, sizeof(trunc), "%.*s", (int)std::strlen(enc) - 5, enc);
    expect(showduino_blocker_decode(trunc, out, &n) == SHOWDUINO_BLOCKER_CORRUPT,
           "T14e incomplete string (no checksum) is CORRUPT");
    char flip[SHOWDUINO_BLOCKER_BUF];
    std::snprintf(flip, sizeof(flip), "%s", enc);
    flip[10] = (flip[10] == '1') ? '2' : '1'; /* digit inside a valid ID: only the checksum catches it */
    expect(showduino_blocker_decode(flip, out, &n) == SHOWDUINO_BLOCKER_CORRUPT,
           "T14f altered byte fails the checksum (CORRUPT)");
    expect(showduino_blocker_decode("XX;#0000", out, &n) == SHOWDUINO_BLOCKER_CORRUPT,
           "T14g wrong prefix is CORRUPT");
    /* Nine entries with a valid checksum: over the station limit. */
    char nine[SHOWDUINO_BLOCKER_BUF] = "B1;";
    for (int i = 0; i < 9; i++) {
      std::snprintf(nine + std::strlen(nine), sizeof(nine) - std::strlen(nine),
                    "ESTOP-%02d,%s;", i + 1, MAC1);
    }
    static char withCrc[1024];
    std::snprintf(withCrc, sizeof(withCrc), "%s#%04X", nine,
                  (unsigned)showduino_blocker_crc16(nine, std::strlen(nine)));
    expect(showduino_blocker_decode(withCrc, out, &n) == SHOWDUINO_BLOCKER_CORRUPT,
           "T14h more than the station limit is CORRUPT");
  }

  /* 15. Reboot after a wireless assertion: the restored record blocks clear. */
  {
    ShowduinoBlocker set[1], back[SHOWDUINO_BLOCKER_MAX];
    mk(set[0], "ESTOP-01", MAC1);
    BlockerStore st; uint8_t n = 0;
    char enc[SHOWDUINO_BLOCKER_BUF];
    showduino_blocker_encode(set, 1, enc, sizeof(enc));
    storeSave(st, enc);
    ShowduinoBlockerStatus bs = storeLoad(st, back, &n);
    ShowduinoStationView rv[1] = {{1, 0, 1, 0}};         /* restored: pressed, offline */
    expect(bs == SHOWDUINO_BLOCKER_OK && n == 1, "T15a stored set reloads after reboot");
    expect(showduino_station_clear_blocked_full(showduino_stations_pressed(rv, 1), 0, 0, 1),
           "T15b clear blocked after P4 reboot by restored wireless assertion");
  }

  /* 16. Clear attempted before the station reconnects. */
  {
    ShowduinoStationView rv[1] = {{1, 0, 1, 0}};
    expect(showduino_station_clear_blocked_full(showduino_stations_pressed(rv, 1), 0, 0, 1),
           "T16a clear before reconnect is blocked");
    expect(showduino_station_clear_blocked_full(0u, 0, 0, 0),
           "T16b clear before the station table is applied is blocked");
  }

  /* 17. Reconnection with the input still asserted. */
  {
    expect(showduino_station_apply_report(1, 1, MAC1, MAC1) == 1,
           "T17a reconnect with input asserted stays pressed");
    expect(showduino_station_clear_blocked_full(1u, 0, 0, 1),
           "T17b clear blocked after reconnect while asserted");
  }

  /* 18. Reconnection with a fresh, valid released report. */
  {
    expect(showduino_station_apply_report(1, 0, MAC1, MAC1) == 0,
           "T18a fresh released report from the same MAC releases");
    expect(!showduino_station_clear_blocked_full(0u, 0, 0, 1),
           "T18b clear allowed after confirmed release");
    expect(showduino_station_apply_report(1, 0, MAC1, MAC2) == 1,
           "T18c released report from a different MAC does not release");
    expect(showduino_station_apply_report(1, 0, MAC1, "") == 1,
           "T18d released report with no MAC does not release a MAC-identified station");
    expect(showduino_station_apply_report(0, 0, MAC1, MAC2) == 0,
           "T18e a different MAC cannot create a block from a released state");
  }

  /* 19. Station disconnected indefinitely: no report, no release. */
  {
    ShowduinoStationView rv[1] = {{1, 0, 1, 1}};
    int blocked = 1;
    for (int tick = 0; tick < 100000; tick++) {
      blocked = showduino_station_clear_blocked_full(showduino_stations_pressed(rv, 1), 0, 0, 1);
    }
    expect(blocked, "T19 station offline indefinitely keeps clear blocked (no timeout release)");
  }

  /* 20. Multiple asserting stations, only one released. */
  {
    ShowduinoStationView rv[2] = {{1, 1, 1, 1}, {1, 1, 1, 1}};
    rv[0].input_open = showduino_station_apply_report(rv[0].input_open, 0, MAC1, MAC1);
    expect(showduino_station_clear_blocked_full(showduino_stations_pressed(rv, 2), 0, 0, 1),
           "T20a one of two stations released: clear still blocked");
    rv[1].input_open = showduino_station_apply_report(rv[1].input_open, 0, MAC2, MAC2);
    expect(!showduino_station_clear_blocked_full(showduino_stations_pressed(rv, 2), 0, 0, 1),
           "T20b both released: clear allowed");
  }

  /* 21. NVS write failure: unverified blockers keep clear blocked, then retry. */
  {
    ShowduinoBlocker set[1];
    mk(set[0], "ESTOP-03", MAC1);
    BlockerStore st; st.failWrite = true;
    char enc[SHOWDUINO_BLOCKER_BUF];
    showduino_blocker_encode(set, 1, enc, sizeof(enc));
    int dirty = !storeSave(st, enc);
    expect(dirty == 1, "T21a failed blocker write is reported as unverified");
    expect(showduino_station_clear_blocked_full(0u, 0, dirty, 1),
           "T21b unverified blocker write keeps clear blocked");
    st.failWrite = false;
    dirty = !storeSave(st, enc);
    expect(dirty == 0 && !showduino_station_clear_blocked_full(0u, 0, dirty, 1),
           "T21c successful retry clears the dirty state");
  }

  /* 22. Corrupt or missing persisted data while latched: fail closed. */
  {
    ShowduinoBlocker back[SHOWDUINO_BLOCKER_MAX]; uint8_t n = 0;
    BlockerStore st; st.corrupt = true; st.present = true;
    ShowduinoBlockerStatus bs = storeLoad(st, back, &n);
    expect(bs == SHOWDUINO_BLOCKER_CORRUPT, "T22a corrupt stored string is CORRUPT");
    expect(showduino_station_clear_blocked_full(0u, bs != SHOWDUINO_BLOCKER_OK, 0, 1),
           "T22b corrupt record while latched blocks clear (fail closed)");
    BlockerStore none;
    bs = storeLoad(none, back, &n);
    expect(bs == SHOWDUINO_BLOCKER_ABSENT &&
           showduino_station_clear_blocked_full(0u, 1, 0, 1),
           "T22c absent record while latched blocks clear (fail closed)");
    expect(!showduino_station_clear_blocked_full(0u, 0, 0, 1),
           "T22d latch clear at boot: no unknown record, no block");
  }

  /* 23. Repeated announcements write nothing; only a change writes. */
  {
    ShowduinoBlocker set[1];
    mk(set[0], "ESTOP-01", MAC1);
    BlockerStore st;
    char enc[SHOWDUINO_BLOCKER_BUF];
    showduino_blocker_encode(set, 1, enc, sizeof(enc));
    int valid = 1, dirty = 0;
    ShowduinoBlocker persisted[1]; uint8_t persistedN = 1;
    mk(persisted[0], "ESTOP-01", MAC1);
    int open = 1;
    for (int i = 0; i < 1000; i++) {                   /* repeated pressed announces */
      open = showduino_station_apply_report(open, 1, MAC1, MAC1);
      ShowduinoBlocker snap[1]; mk(snap[0], "ESTOP-01", MAC1);
      if (showduino_station_persist_needed(valid, dirty,
              showduino_blocker_sets_equal(snap, 1, persisted, persistedN))) {
        storeSave(st, enc);
      }
    }
    expect(st.writes == 0, "T23a 1000 repeated pressed announces: no NVS writes");
    open = showduino_station_apply_report(open, 0, MAC1, MAC1);
    {
      ShowduinoBlocker snap[1]; uint8_t n = 0;
      if (!open) n = 0;
      if (showduino_station_persist_needed(valid, dirty,
              showduino_blocker_sets_equal(snap, n, persisted, persistedN))) {
        char e2[SHOWDUINO_BLOCKER_BUF];
        showduino_blocker_encode(snap, n, e2, sizeof(e2));
        storeSave(st, e2);
      }
    }
    expect(st.writes == 1, "T23b one genuine release transition writes exactly once");
  }

  /* ---- MISSION 001E: one coherent safety-state model ----
   * Latch byte and blocker record are written in order (blockers, then latch).
   * The latch byte records whether the blocker record was verified. Boot treats
   * an unverified latch, an unreadable latch, or a latch with a missing or corrupt
   * blocker record as unknown, and refuses clear. */
  struct ByteStore {
    bool present = false;
    uint8_t v = 0;
    int writes = 0;
    bool failWrite = false;
  };
  auto latchWrite = [](ByteStore &b, uint8_t val) -> bool {
    b.writes++;
    if (b.failWrite) return false;
    b.present = true; b.v = val; return true;
  };
  auto latchBoot = [](const ByteStore &b) -> ShowduinoLatchRead {
    ShowduinoLatchReadInput in{};
    in.storeOpened = true; in.readOk = true;
    in.keyPresent = b.present; in.value = b.v;
    return showduino_latch_decode(in);
  };

  /* 24. Unverified value decodes as latched and unknown. */
  {
    ByteStore b; b.present = true; b.v = SHOWDUINO_LATCH_VALUE_ACTIVE_UNVERIFIED;
    ShowduinoLatchRead r = latchBoot(b);
    expect(r == SHOWDUINO_LATCH_READ_ACTIVE_UNVERIFIED, "T24a 0xA6 decodes as ACTIVE_UNVERIFIED");
    expect(showduino_latch_boot_latched(r) && showduino_latch_boot_unknown(r),
           "T24b unverified latch restores latched and unknown");
    b.v = SHOWDUINO_LATCH_VALUE_ACTIVE;
    expect(!showduino_latch_boot_unknown(latchBoot(b)),
           "T24c verified 0xA5 is not unknown");
  }

  /* 25. Value mapping. */
  expect(showduino_latch_value_for(true, true) == SHOWDUINO_LATCH_VALUE_ACTIVE, "T25a verified ACTIVE is 0xA5");
  expect(showduino_latch_value_for(true, false) == SHOWDUINO_LATCH_VALUE_ACTIVE_UNVERIFIED, "T25b unverified ACTIVE is 0xA6");
  expect(showduino_latch_value_for(false, false) == SHOWDUINO_LATCH_VALUE_CLEAR, "T25c CLEAR is 0x5A regardless");

  /* 26. Transitions write once; repeats never write. */
  {
    ByteStore b; ShowduinoLatchTrack t{};
    showduino_latch_track_from_boot(&t, SHOWDUINO_LATCH_READ_CLEAR);
    for (int i = 0; i < 500; i++) {
      if (showduino_latch_needs_write_v2(&t, true, true)) {
        bool ok = latchWrite(b, showduino_latch_value_for(true, true));
        showduino_latch_record_write_v2(&t, true, true, ok);
      }
    }
    expect(b.writes == 1, "T26a repeated verified ACTIVE: one write");
    for (int i = 0; i < 500; i++) {
      if (showduino_latch_needs_write_v2(&t, true, false)) {
        bool ok = latchWrite(b, showduino_latch_value_for(true, false));
        showduino_latch_record_write_v2(&t, true, false, ok);
      }
    }
    expect(b.writes == 2 && b.v == SHOWDUINO_LATCH_VALUE_ACTIVE_UNVERIFIED,
           "T26b verified to unverified is one write, stored as 0xA6");
    for (int i = 0; i < 500; i++) {
      if (showduino_latch_needs_write_v2(&t, true, true)) {
        bool ok = latchWrite(b, showduino_latch_value_for(true, true));
        showduino_latch_record_write_v2(&t, true, true, ok);
      }
    }
    expect(b.writes == 3 && b.v == SHOWDUINO_LATCH_VALUE_ACTIVE,
           "T26c unverified converges to verified with one write");
  }

  /* 27. A failed unverified write stays dirty and is retried. */
  {
    ByteStore b; b.failWrite = true; ShowduinoLatchTrack t{};
    showduino_latch_track_from_boot(&t, SHOWDUINO_LATCH_READ_CLEAR);
    bool ok = latchWrite(b, showduino_latch_value_for(true, false));
    showduino_latch_record_write_v2(&t, true, false, ok);
    expect(!ok && t.dirty && showduino_latch_needs_write_v2(&t, true, false),
           "T27a failed unverified write is dirty and needs retry");
    b.failWrite = false;
    ok = latchWrite(b, showduino_latch_value_for(true, false));
    showduino_latch_record_write_v2(&t, true, false, ok);
    expect(ok && !t.dirty && b.v == SHOWDUINO_LATCH_VALUE_ACTIVE_UNVERIFIED,
           "T27b retry stores the unverified latch");
  }

  /* 28. Interrupted assertion: blockers fail, latch engages unverified, restart. */
  {
    ByteStore b;
    /* Assertion: the blocker write failed, so the latch is written unverified. */
    const bool blockersVerified = false;
    bool ok = latchWrite(b, showduino_latch_value_for(true, blockersVerified));
    expect(ok, "T28a latch written during blocker failure");
    ShowduinoLatchRead r = latchBoot(b);
    expect(showduino_boot_state_unknown(showduino_latch_boot_latched(r) ? 1 : 0,
                                        SHOWDUINO_BLOCKER_ABSENT,
                                        showduino_latch_boot_unknown(r) ? 1 : 0),
           "T28b restart after unverified assertion: unknown, clear blocked");
    /* Even with a valid but stale empty record, the unverified byte blocks clear. */
    expect(showduino_boot_state_unknown(1, SHOWDUINO_BLOCKER_OK,
                                        showduino_latch_boot_unknown(r) ? 1 : 0) == 1,
           "T28c stale valid blocker record cannot release an unverified latch");
  }

  /* 29. Upgrade / missing record: latch ACTIVE with no blocker record. */
  {
    ByteStore b; latchWrite(b, SHOWDUINO_LATCH_VALUE_ACTIVE);
    ShowduinoLatchRead r = latchBoot(b);
    expect(showduino_boot_state_unknown(1, SHOWDUINO_BLOCKER_ABSENT, showduino_latch_boot_unknown(r) ? 1 : 0),
           "T29a ACTIVE latch with no blocker record: unknown (fail closed)");
    expect(!showduino_boot_state_unknown(0, SHOWDUINO_BLOCKER_ABSENT, 0),
           "T29b no latch and no record: nothing unknown");
  }

  /* 30. Corrupt latch byte with an otherwise valid empty record: unknown. */
  {
    ShowduinoLatchReadInput in{}; in.storeOpened = true; in.readOk = true;
    in.keyPresent = true; in.value = 0x33;
    ShowduinoLatchRead r = showduino_latch_decode(in);
    expect(r == SHOWDUINO_LATCH_READ_FAULT, "T30a unknown latch byte decodes as FAULT");
    expect(showduino_boot_state_unknown(showduino_latch_boot_latched(r) ? 1 : 0,
                                        SHOWDUINO_BLOCKER_OK,
                                        showduino_latch_boot_unknown(r) ? 1 : 0),
           "T30b corrupt latch with valid record: unknown, clear blocked");
  }

  /* 31. Crash between writes: blockers written, latch not. Restart is CLEAR, so
   * nothing is latched; the next assertion rewrites the record first. */
  {
    ByteStore b;
    expect(!showduino_boot_state_unknown(showduino_latch_boot_latched(latchBoot(b)) ? 1 : 0,
                                         SHOWDUINO_BLOCKER_OK, 0),
           "T31 crash after blocker write, before latch: clean CLEAR restart");
  }

  /* 32. Interrupted verified-to-unverified transition: latch still 0xA5 and the
   * blocker record is then unverified in RAM. Restart must still not trust the
   * blocker record unless it is stored as verified. */
  {
    ByteStore b; latchWrite(b, SHOWDUINO_LATCH_VALUE_ACTIVE);
    ShowduinoLatchTrack t{};
    showduino_latch_track_from_boot(&t, SHOWDUINO_LATCH_READ_ACTIVE);
    /* A later unverified blocker set fails to become durable. The byte stays 0xA5. */
    b.failWrite = true;
    bool ok = latchWrite(b, showduino_latch_value_for(true, false));
    showduino_latch_record_write_v2(&t, true, false, ok);
    expect(!ok && t.dirty, "T32a failed unverified transition leaves storage dirty");
    /* While dirty, the RAM side must keep blocking clear. */
    expect(showduino_station_clear_blocked_full(0u, 0, t.dirty ? 1 : 0, 1),
           "T32b dirty latch keeps clear blocked in RAM");
  }

  /* ---- MISSION 001F: actuation order and storage delay ---- */
  struct EngageSim {
    int order[4] = {0, 0, 0, 0};
    int n = 0;
    int ramCalls = 0;
    unsigned long clock = 0;
    unsigned long ramAt = 0xFFFFFFFFul;
    unsigned long blockersDelay = 0;
    unsigned long latchDelay = 0;
    bool blockersFail = false;
    bool latchFail = false;
    int latchArgBlockersVerified = -1;
  };
  auto simRam = [](void *c) {
    EngageSim *s = static_cast<EngageSim *>(c);
    s->ramCalls++;
    s->ramAt = s->clock;
    s->order[s->n++] = 1;
  };
  auto simBlockers = [](void *c) -> int {
    EngageSim *s = static_cast<EngageSim *>(c);
    s->clock += s->blockersDelay;
    s->order[s->n++] = 2;
    return s->blockersFail ? 0 : 1;
  };
  auto simLatch = [](void *c, int bv) -> int {
    EngageSim *s = static_cast<EngageSim *>(c);
    s->clock += s->latchDelay;
    s->latchArgBlockersVerified = bv;
    s->order[s->n++] = 3;
    return s->latchFail ? 0 : 1;
  };
  auto runEngage = [&](EngageSim &sim) -> int {
    ShowduinoEngageOps ops{&sim, simRam, simBlockers, simLatch};
    return showduino_emergency_engage(&ops);
  };

  /* 33a. Outputs run first, at t=0, even when the blocker write takes 60 s. */
  {
    EngageSim sim; sim.blockersDelay = 60000ul; sim.latchDelay = 60000ul;
    runEngage(sim);
    expect(sim.order[0] == 1 && sim.ramAt == 0ul,
           "T33a RAM engage is first and happens before any storage delay");
  }
  /* 33b. Storage fails on both writes: RAM still engaged once, return reports it. */
  {
    EngageSim sim; sim.blockersFail = true; sim.latchFail = true;
    const int ok = runEngage(sim);
    expect(sim.ramCalls == 1 && ok == 0 && sim.order[0] == 1 && sim.order[1] == 2 && sim.order[2] == 3,
           "T33b storage failure: RAM engaged once, order 1-2-3, result not verified");
  }
  /* 33c. Blocker record unverified: latch is told so and stored as unverified. */
  {
    EngageSim sim; sim.blockersFail = true;
    runEngage(sim);
    expect(sim.latchArgBlockersVerified == 0,
           "T33c unverified blocker record is passed to the latch write");
  }
  /* 33d. Everything verifies: result 1. */
  {
    EngageSim sim;
    const int ok = runEngage(sim);
    expect(ok == 1 && sim.latchArgBlockersVerified == 1,
           "T33d verified storage returns 1 with verified blocker record");
  }
  /* 33e. Delayed storage does not cause a second RAM engage in the same call. */
  {
    EngageSim sim; sim.blockersDelay = 60000ul; sim.latchFail = true;
    runEngage(sim);
    expect(sim.ramCalls == 1, "T33e delayed or failed storage never re-runs outputs");
  }
  /* 33f. Missing callbacks: nothing is engaged and the call reports failure. */
  {
    ShowduinoEngageOps ops{nullptr, nullptr, simBlockers, simLatch};
    expect(showduino_emergency_engage(&ops) == 0, "T33f incomplete ops are rejected");
  }

  /* ---- MISSION 001F: station identity pinning ---- */
  auto pinnedApply = [](char *id, char *mac, int *open, const char *rid,
                        const char *rmac, int ropen) -> int {
    return showduino_station_apply_identified_report(id, 16, mac, 18, open, rid, rmac, ropen);
  };
  const char *legitMac = "AA:BB:CC:DD:EE:01";
  const char *forgedMac = "11:22:33:44:55:66";

  /* 34. Forged released report with the pinned ID and another MAC is rejected:
   * the press stays, the pinned MAC does not change. */
  {
    char id[16] = "ESTOP-01"; char mac[18]; std::strcpy(mac, legitMac); int open = 1;
    const int ok = pinnedApply(id, mac, &open, "ESTOP-01", forgedMac, 0);
    expect(ok == 0 && open == 1 && isCmd(mac, legitMac),
           "T34 forged released report (same ID, other MAC) is rejected and pin is kept");
    /* 35. Repeat forged releases, including one that copies the forged MAC. */
    const int ok2 = pinnedApply(id, mac, &open, "ESTOP-01", forgedMac, 0);
    const int ok3 = pinnedApply(id, mac, &open, "ESTOP-01", forgedMac, 0);
    expect(ok2 == 0 && ok3 == 0 && open == 1 && isCmd(mac, legitMac),
           "T35 repeated forged releases never overwrite the pinned MAC or release");
    /* 36. Forged report with a new ID and the forged MAC is rejected. */
    const int ok4 = pinnedApply(id, mac, &open, "ESTOP-99", forgedMac, 0);
    expect(ok4 == 0 && open == 1 && isCmd(id, "ESTOP-01") && isCmd(mac, legitMac),
           "T36 forged report with a different ID is rejected and ID is kept");
    /* 37. Pinned ID with empty MAC cannot release either. */
    const int ok5 = pinnedApply(id, mac, &open, "ESTOP-01", "", 0);
    expect(ok5 == 1 && open == 1 && isCmd(mac, legitMac),
           "T37 released report with empty MAC keeps the press (no release)");
    /* 38. The original station can still report a legitimate release afterwards. */
    const int ok6 = pinnedApply(id, mac, &open, "ESTOP-01", legitMac, 0);
    expect(ok6 == 1 && open == 0 && isCmd(mac, legitMac),
           "T38 original station releases after forged attempts");
    /* 39. After the release, the pin still holds against a forged MAC. */
    const int ok7 = pinnedApply(id, mac, &open, "ESTOP-01", forgedMac, 1);
    expect(ok7 == 0 && open == 0 && isCmd(mac, legitMac),
           "T39 pin survives after release: forged MAC cannot re-press or change the record");
  }
  /* 40. The original station presses and releases normally from a fresh record. */
  {
    char id[16] = ""; char mac[18] = ""; int open = 0;
    expect(pinnedApply(id, mac, &open, "ESTOP-02", legitMac, 1) == 1 && open == 1 &&
           isCmd(id, "ESTOP-02") && isCmd(mac, legitMac),
           "T40a first announce pins ID and MAC and records the press");
    expect(pinnedApply(id, mac, &open, "ESTOP-02", legitMac, 0) == 1 && open == 0,
           "T40b matching released report from pinned identity releases");
  }
  /* 41. Repeated announcements from the pinned identity do not change the record. */
  {
    char id[16] = "ESTOP-03"; char mac[18]; std::strcpy(mac, legitMac); int open = 1;
    bool allOk = true;
    for (int i = 0; i < 5; ++i) {
      allOk = allOk && pinnedApply(id, mac, &open, "ESTOP-03", legitMac, 1) == 1 && open == 1;
    }
    expect(allOk, "T41 repeated pressed announcements keep the same pinned record");
  }

  std::printf(gFails == 0 ? "ALL PASS\n" : "FAILURES: %d\n", gFails);
  return gFails == 0 ? 0 : 1;
}
