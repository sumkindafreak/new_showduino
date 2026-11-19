# Phase 1 — Baseline before visual changes

**State:** Started as isolated development. GitHub source reconciliation #72 is merged; actual hardware benchmark and installed-bench comparison are still outstanding.

## Reason

The Director already has functional screen rendering, nodes, diagnostics, touch calibration, on-board ambient pixels and emergency-aware animation rules. A faster-looking interface is no improvement if it damages interrupt scheduling, memory stability or operator response.

## Step A — Capture existing serial evidence

Use the verified bench firmware, USB UART/CH340 at 115200 baud, USB CDC On Boot disabled. Record separate sessions:
- Cold boot with SD present and all intended nodes present.
- Cold boot with SD absent, then recovery.
- Normal page navigation, tap and scroll use.
- Loss and restoration of the separate Communications S3 link.
- Emergency assertion, Director acknowledgement, confirmation-based clear, and no automatic resume.
- Continuous idle/operation endurance.

Save serial captures outside the repository unless reviewed for sensitive network or identifying strings. The parser in tools/bench_baseline.py operates offline on a log file. Example:

    python director-v1.1-evolution/tools/bench_baseline.py path/to/director_uart.log --format markdown

Or write a machine-readable observation:

    python director-v1.1-evolution/tools/bench_baseline.py path/to/director_uart.log --format json --output path/to/observation.json

What the first-pass parser can observe: boot banners, init outcomes, boot-time PSRAM free messages, connection retries, touch/scroll log counts, panic/watchdog/brownout signatures.

What it **cannot** infer: FPS, touch latency, live heap low-water mark, mean reconnect time, reliable emergency operation, a completed endurance test or performance safety. Those require explicit instrumentation and bench evidence. Never report unsupported numbers.

## Step B — Trial the new performance component only on a separate test build

An allocation-free fixed-size timing collector is now available at `components/DirectorV11Performance.h`, with native host tests. It is **not** included by the active firmware. Further instrumentation candidates:
- UI frame timing and maximum/p95 LVGL handler duration.
- Timestamped touch down → visual response latency (only with trustworthy tracing).
- Internal heap free, largest free allocation, allocation failures, PSRAM free.
- Dropped/stale message counts and actual reconnect intervals.
- Reboot reasons and loop stall count.

Instrumentation must use bounded work and must not block emergency or ESP-NOW service. Avoid logging every frame over UART during a performance test. Add compile-time control and make it OFF in the stable build.

## Step C — Declare acceptance budgets

Compare against actual baseline runs on the same board, toolchain, power supply and test scenario. Establish acceptable limits from measurements; no invented FPS or memory thresholds. A new feature is blocked if any safety regression appears, if practical control response deteriorates or if long-running memory trends indicate leakage.

Use ACCEPTANCE.md to record the test evidence before a merge.

## Phase 1 exit criteria

- [x] GitHub source integration merged as PR #72.
- [ ] Physical flashed-bench version and revision verified.
- [ ] Baseline firmware revision and board/library versions recorded.
- [ ] Cold boot, normal operation, comms-loss and emergency logs captured.
- [ ] Actual FPS/frame duration, touch response and memory readings measured.
- [ ] Baseline screenshots and independent rollback image captured.
- [ ] Bench metrics and thresholds reviewed before UI work.
- [ ] No change to authoritative P4 or safety behaviour.

The presence of host-tested components or documentation is not a successful device build or physical bench test.
