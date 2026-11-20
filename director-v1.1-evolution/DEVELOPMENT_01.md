# Development 01 — measurable performance + read-only card foundation

**Branch:** `feature/director-v1.1-evolution`  
**Stage:** isolated prototype, **not integrated**, not hardware validated.

## Implemented under this folder

- `components/DirectorV11Performance.h` — fixed 64-entry timing windows for UI-loop and LVGL-handler samples, with O(1) observation, computed min/mean/max/p95 and sample count.
- `components/DirectorV11CardModel.h` — deterministic, P4-authority-aware node-card classification. Emergency overrides all status. Stale reports and unapproved nodes cannot display READY.
- `components/DirectorV11CardLvgl.h` — minimal LVGL 9 card using two text labels and a status rail. No global styles, timers, animations, event handlers or network commands.
- `tests/test_director_v11.cpp` — native automated coverage for timing-window rollover, percentile/average, emergency priority, link loss, stale state, and P4 approval gating.

## Host test

From repository root, with `g++` installed:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  director-v1.1-evolution/tests/test_director_v11.cpp \
  -o director-v1.1-evolution/tests/test_director_v11
./director-v1.1-evolution/tests/test_director_v11
```

The LVGL widget header additionally requires an LVGL 9 environment; **native tests do not establish visual/firmware compilation or physical safety.** Its integration remains gated.

## Proposed integration *after* bench acceptance

1. Measure baseline with the stable Director. Record UART log, flashed commit and library versions.
2. On a **separate trial firmware branch**, include `DirectorV11Performance.h`. Time calls to `lvglPortLoop()` and optionally the UI loop with a monotonic microsecond clock. Observe measurements in the same task. Read snapshot only once per second or less; logging per frame is prohibited.
3. Build one opt-in card in the Nodes page with `createCard()`, `updateCard()` and `destroyCard()`, deriving `NodeEvidence` only from confirmed P4/Node state. No transport ACK is accepted as authorization.
4. Compare real device frame time, memory, touch latency and comms recovery against a baseline. Disable the test with a compile-time switch if it cannot meet budgets.
5. Execute the acceptance checklist including emergency and 24-hour endurance before considering merge.

## Explicit non-goals

No edits to production sketch or protocol, no new effects, no implied hardware tests, no direct physical outputs, no change to the boot/SD initialization order, no emergency-clear changes.
