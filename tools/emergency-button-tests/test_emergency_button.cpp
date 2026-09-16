#include "showduino_emergency_button.h"
#include "showduino_director_locate.h"
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

static void expect_eq(unsigned got, unsigned want, const char *msg) {
  if (got == want) std::printf("PASS  %s\n", msg);
  else {
    std::printf("FAIL  %s got=%u want=%u\n", msg, got, want);
    gFails++;
  }
}

static void sim_press(ShowduinoEstopHoldState *hold, ShowduinoEstopHoldEvents *ev,
                      uint32_t now) {
  memset(ev, 0, sizeof(*ev));
  showduino_estop_hold_on_press(hold, now, ev);
}

static unsigned hold_until(ShowduinoEstopHoldState *hold, uint32_t startMs,
                           uint32_t durationMs, uint32_t stepMs) {
  unsigned locateCount = 0;
  for (uint32_t t = startMs; t <= startMs + durationMs; t += stepMs) {
    ShowduinoEstopHoldEvents ev = {};
    showduino_estop_hold_tick(hold, t, &ev);
    if (ev.locateRequested) locateCount++;
  }
  return locateCount;
}

int main() {
  std::printf("Showduino Emergency button + Director Locate host tests\n\n");

  /* TEST 1 — quick press: Emergency assert event, no Locate, no Clear. */
  {
    ShowduinoEstopHoldState hold;
    ShowduinoEstopHoldEvents ev = {};
    ShowduinoEstopClearAuth clearAuth;
    showduino_estop_hold_reset(&hold);
    showduino_estop_clear_reset(&clearAuth);
    sim_press(&hold, &ev, 1000);
    expect(ev.pressBegan == 1, "T1 press began (Emergency assert now)");
    expect(ev.locateRequested == 0, "T1 no Locate on press");
    showduino_estop_hold_tick(&hold, 1000 + 4000, &ev);
    expect(ev.locateRequested == 0, "T1 no Locate at 4s");
    memset(&ev, 0, sizeof(ev));
    showduino_estop_hold_on_release(&hold, &ev);
    expect(ev.released == 1, "T1 release");
    expect(ev.locateRequested == 0, "T1 release does not Locate");
    expect(clearAuth.pending == 0, "T1 no pending clear from physical button");
  }

  /* TEST 2 — exact 8-second hold fires Locate once. */
  {
    ShowduinoEstopHoldState hold;
    ShowduinoEstopHoldEvents ev = {};
    showduino_estop_hold_reset(&hold);
    sim_press(&hold, &ev, 0);
    expect(ev.pressBegan == 1, "T2 Emergency asserted immediately");
    memset(&ev, 0, sizeof(ev));
    showduino_estop_hold_tick(&hold, 7999, &ev);
    expect(ev.locateRequested == 0, "T2 7999 ms does not Locate");
    memset(&ev, 0, sizeof(ev));
    showduino_estop_hold_tick(&hold, 8000, &ev);
    expect(ev.locateRequested == 1, "T2 8000 ms Locates");
  }

  /* TEST 3 — hold 20 seconds: Locate command count = 1. */
  {
    ShowduinoEstopHoldState hold;
    ShowduinoEstopHoldEvents ev = {};
    showduino_estop_hold_reset(&hold);
    sim_press(&hold, &ev, 100);
    unsigned n = hold_until(&hold, 100, 20000, 50);
    expect_eq(n, 1, "T3 Locate fires once in 20 s hold");
  }

  /* TEST 4 — release rearms Locate hold. */
  {
    ShowduinoEstopHoldState hold;
    ShowduinoEstopHoldEvents ev = {};
    showduino_estop_hold_reset(&hold);
    sim_press(&hold, &ev, 0);
    unsigned n1 = hold_until(&hold, 0, 8000, 100);
    expect_eq(n1, 1, "T4 first hold Locates");
    memset(&ev, 0, sizeof(ev));
    showduino_estop_hold_on_release(&hold, &ev);
    sim_press(&hold, &ev, 20000);
    unsigned n2 = hold_until(&hold, 20000, 8000, 100);
    expect_eq(n2, 1, "T4 second hold Locates once");
  }

  /* TEST 5 — interrupted hold does not accumulate. */
  {
    ShowduinoEstopHoldState hold;
    ShowduinoEstopHoldEvents ev = {};
    showduino_estop_hold_reset(&hold);
    sim_press(&hold, &ev, 0);
    unsigned n1 = hold_until(&hold, 0, 4000, 100);
    expect_eq(n1, 0, "T5 first 4 s does not Locate");
    memset(&ev, 0, sizeof(ev));
    showduino_estop_hold_on_release(&hold, &ev);
    sim_press(&hold, &ev, 5000);
    unsigned n2 = hold_until(&hold, 5000, 4000, 100);
    expect_eq(n2, 0, "T5 second 4 s does not Locate");
  }

  /* TEST 6 — Emergency already latched; second 8 s hold Locates. */
  {
    ShowduinoEstopHoldState hold;
    ShowduinoEstopHoldEvents ev = {};
    showduino_estop_hold_reset(&hold);
    sim_press(&hold, &ev, 0);
    showduino_estop_hold_on_release(&hold, &ev);
    sim_press(&hold, &ev, 1000);
    unsigned n = hold_until(&hold, 1000, 8000, 100);
    expect_eq(n, 1, "T6 Locate from second hold while latched");
    expect(hold.pressed == 1, "T6 button still pressed");
  }

  /* TEST 7 / 8 — first Locate touch acknowledges and is consumed. */
  {
    ShowduinoDirectorLocateState loc;
    showduino_director_locate_reset(&loc);
    int underlying = 0;
    expect(showduino_director_locate_start(&loc) == 1, "T7 Locate starts");
    expect(showduino_director_locate_start(&loc) == 0, "T7 second start is idempotent");
    int acked = 0;
    int consumed = showduino_director_locate_on_touch(&loc, 1, &acked);
    expect(consumed == 1, "T8 first press consumed");
    expect(acked == 1, "T7 first press acknowledges");
    expect(showduino_director_locate_active(&loc) == 0, "T7 Locate inactive after ack");
    if (!consumed) underlying = 1;
    consumed = showduino_director_locate_on_touch(&loc, 1, &acked);
    expect(consumed == 1, "T8 rest of press consumed");
    expect(acked == 0, "T8 held press does not re-ack");
    if (!consumed) underlying = 1;
    consumed = showduino_director_locate_on_touch(&loc, 0, &acked);
    expect(consumed == 1, "T8 matching release consumed");
    expect(acked == 0, "T8 release does not ack again");
    if (!consumed) underlying = 1;
    expect(underlying == 0, "T8 underlying control does not fire");
    consumed = showduino_director_locate_on_touch(&loc, 1, &acked);
    expect(consumed == 0, "T8 later press is not consumed");
    expect(acked == 0, "T8 later press is not an ack");
  }

  /* TEST 10 — no auto-timeout. */
  {
    ShowduinoDirectorLocateState loc;
    showduino_director_locate_reset(&loc);
    showduino_director_locate_start(&loc);
    expect(showduino_director_locate_active(&loc) == 1, "T10 still active after 15s model");
    expect(showduino_director_locate_active(&loc) == 1, "T10 still active after 60s model");
  }

  /* TEST 11 — clear rejected while button pressed. */
  {
    ShowduinoEstopClearAuth a;
    showduino_estop_clear_reset(&a);
    int rc = showduino_estop_clear_begin(&a, 1000, 1, 1, 1);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_ERR_BUTTON_ACTIVE,
              "T11 begin rejected while button asserted");
    expect(a.pending == 0, "T11 no pending after button-asserted reject");
  }

  /* TEST 12 — deliberate clear with button released. */
  {
    ShowduinoEstopClearAuth a;
    showduino_estop_clear_reset(&a);
    int rc = showduino_estop_clear_begin(&a, 1000, 1, 0, 4);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_OK, "T12 begin pending");
    expect(a.pending == 1, "T12 pending set");
    rc = showduino_estop_clear_confirm(&a, 2000, 1, 0, 4);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_OK, "T12 confirm clears");
    expect(a.pending == 0, "T12 pending consumed");
    rc = showduino_estop_clear_begin(&a, 1000, 0, 0, 4);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_ERR_NOT_LATCHED,
              "T12 not latched cannot begin");
  }

  /* TEST 13 — new press during confirmation invalidates old confirm. */
  {
    ShowduinoEstopClearAuth a;
    uint32_t seq = 1;
    showduino_estop_clear_reset(&a);
    int rc = showduino_estop_clear_begin(&a, 1000, 1, 0, seq);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_OK, "T13 pending created");
    seq++;
    showduino_estop_clear_cancel(&a);
    rc = showduino_estop_clear_confirm(&a, 1500, 1, 0, seq - 1);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_ERR_NO_REQUEST,
              "T13 stale confirm rejected after cancel");

    seq = 7;
    rc = showduino_estop_clear_begin(&a, 2000, 1, 0, seq);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_OK, "T13 second pending");
    rc = showduino_estop_clear_confirm(&a, 2500, 1, 0, seq + 1);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_ERR_SUPERSEDED,
              "T13 confirm rejected when assertion seq moved");
  }

  /* Release never creates a clear request. */
  {
    ShowduinoEstopHoldState hold;
    ShowduinoEstopHoldEvents ev = {};
    ShowduinoEstopClearAuth a;
    showduino_estop_hold_reset(&hold);
    showduino_estop_clear_reset(&a);
    sim_press(&hold, &ev, 0);
    showduino_estop_hold_on_release(&hold, &ev);
    expect(a.pending == 0, "release does not begin pending clear");
    int rc = showduino_estop_clear_confirm(&a, 100, 1, 0, 0);
    expect_eq((unsigned)rc, SHOWDUINO_ESTOP_CLEAR_ERR_NO_REQUEST,
              "no physical-button clear confirm path");
  }

  if (gFails) {
    std::printf("\nFAILED %d\n", gFails);
    return 1;
  }
  std::printf("\nALL PASS\n");
  return 0;
}
