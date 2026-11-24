# Development 02 — pre-bench engineering

Status: isolated source, not hardware approved. Nothing in the production Director, Comms, P4, protocols or GPIO mapping is changed.

## Additions
- `previews/DirectorV11Preview.h`: original local RGB565 24x12 simulations of fire, rain, snow, lightning, flicker, fade, chase and rainbow. Fixed buffer and 10 Hz maximum update rate. No output commands. All six safety inputs must permit operation; unknown state must block rendering. These are illustrative previews, not yet matched to firmware effect algorithms.
- `components/DirectorV11Health.h`: conservative display-only health model, change tracking and rollover-safe age tracking.
- `ui/DirectorV11DesignTokens.h`: proposed shared palette and sizing, not a replacement for the active theme.
- `components/DirectorV11CardLvgl.h`: update style only when status changes. Owner must destroy the card before deleting its parent; never retain a copied handle or update a deleted page.
- `tools/bench_capture.py`: Windows-compatible three-port receive-only capture and offline report; raw RX chunks retained as base64 JSONL. No serial commands are sent. Host timestamps approximate arrival, not execution or precise cross-board order. USB driver buffering, loss and DTR/RTS opening transients are possible.
- Native C++ and Python tests in `tests/`.

## Host checks
From repository root:
```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic director-v1.1-evolution/tests/test_development_02.cpp -o /tmp/director_v11_dev02
/tmp/director_v11_dev02
python -m unittest discover -s director-v1.1-evolution/tests -p "test_*.py"
```
On Windows replace /tmp output with a local .exe. These checks do not compile LVGL or exercise hardware.

## Bench capture (example only; substitute actual ports and baud rates)
Install Python 3.9+ and `py -m pip install pyserial`. Close Arduino serial monitors. Disconnect effect loads or put the system in a safe bench state before opening ports; the USB driver may reset a board despite disabled DTR/RTS. Use the actual firmware Serial.begin rates, not the inter-board UART rate by assumption.
```powershell
py director-v1.1-evolution/tools/bench_capture.py capture --director COM7:115200 --comms COM8:115200 --p4 COM9:115200 --output baseline.jsonl
py director-v1.1-evolution/tools/bench_capture.py report --input baseline.jsonl --output baseline.txt
```
Ctrl+C ends capture. Output files are never overwritten. A report flags candidate lines, not verified faults. Keep raw JSONL as evidence and review for private data before sharing.

## Integration order (separate test build, after baseline)
1. Record exact flashed source SHA, binary recovery path, ESP32/LVGL versions, board options and actual ports. Capture startup, READY handshake, reconnect, repeated playback and emergency test with effect loads isolated.
2. Active shell is SHOWDUINO_OS2_SHELL=0; retain existing page APIs, actions, themes and safety overlays. Start with instrumentation only and collect loop/LVGL timing, internal free heap/largest block, PSRAM and page transitions. No per-frame serial logs.
3. Trial a single read-only node card using fresh P4 evidence; invalidate cached state on disconnect/reboot. Leave existing node detail/settings and emergency controls intact. Test repeated page creation/destruction, scrolling and touch.
4. Trial health display separately. Unknown memory/storage/emergency is not READY; display health is never an authorisation check. Gather SD information from existing diagnostics rather than mounting storage again or writing files.
5. Integrate preview only on visible cue-editor page while show idle, fresh P4 state, no emergency, sufficient memory and UI headroom. Run in existing UI task after processing safety state. Call tick every loop even when blocked, hide on !running(), invalidate canvas only on changed frame. Use a correctly configured RGB565 image/canvas buffer with verified lifetime/stride; do not allocate per frame. Disable preview before page destruction. Do not send cues or call physical effect functions.
6. Measure baseline versus candidate; define thresholds from actual hardware before acceptance. Keep changes individually switchable, default off until tested. Validate boot, emergency, reconnect, playback, 24-hour stability and rollback per ACCEPTANCE.md.

## Deferred
Board-specific LVGL canvas/widget build, navigation restyle and visual approval, exact effect parity, live serial-port testing, storage and heap thresholds, recovery binary and actual performance measurements. No device compilation, flash, or production merge is implied.

## Verified on host, 24 November 2026
- New C++ preview/health and resource tests passed under g++ C++17 with warnings as errors and undefined-behaviour/bounds sanitizers. Source blob hashes matched GitHub read-back.
- Three Python offline capture tests passed; source and tests matched GitHub blob hashes.
- Resource monitor (`diagnostics/DirectorV11Resources.h`) tracks minimum internal heap, largest allocation, PSRAM, maximum loop and transition time; unconfigured budgets deny preview permission. Run `g++ -std=c++17 -Wall -Wextra -Werror -pedantic director-v1.1-evolution/tests/test_resources.cpp -o /tmp/director_v11_resources && /tmp/director_v11_resources`.
- Preview cadence is capped during continuous operation; rapid reselection/re-entry must also be throttled by the future UI adapter. No LVGL, ESP32 or live USB validation yet.
