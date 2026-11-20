#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$ROOT/.host-test-cache/director"
mkdir -p "$OUT"
cd "$ROOT"
node "$HERE/test_live_status_refresh.cjs"
for test in live_status_refresh lamp_sheet emergency_sheet audio_inventory cue_library emergency_clear_flow system_sound_volume runtime_freshness touch_calibration; do
  g++ -std=c++17 -Wall -Wextra -I"$ROOT/protocol" \
    -I"$ROOT/firmware/director-esp32-8048s050/ShowduinoDirector8048S050" \
    -o "$OUT/$test" "$HERE/test_$test.cpp"
  "$OUT/$test"
done
