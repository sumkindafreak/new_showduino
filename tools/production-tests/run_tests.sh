#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$HERE/../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src"
g++ -std=c++17 -Wall -Wextra -I"$SRC_DIR" \
  -o "$HERE/production_format_tests" \
  "$HERE/test_production_format.cpp" "$SRC_DIR/ProductionFormat.cpp"
"$HERE/production_format_tests"

g++ -std=c++17 -Wall -Wextra -I"$HERE/stubs" -I"$SRC_DIR" \
  -o "$HERE/production_store_tests" \
  "$HERE/test_production_store.cpp" "$SRC_DIR/ProductionFormat.cpp" \
  "$SRC_DIR/ProductionStore.cpp"
"$HERE/production_store_tests"

g++ -std=c++17 -Wall -Wextra -I"$HERE/stubs" \
  -I"$HERE/../../firmware/stage-engine-p4/ShowduinoStageEngineP4" \
  -o "$HERE/timeline_runtime_tests" "$HERE/test_timeline_runtime.cpp"
"$HERE/timeline_runtime_tests"
