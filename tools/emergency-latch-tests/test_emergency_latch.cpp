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
#include <cstdio>
#include <cstring>

static int gFails = 0;

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

  std::printf(gFails == 0 ? "ALL PASS\n" : "FAILURES: %d\n", gFails);
  return gFails == 0 ? 0 : 1;
}
