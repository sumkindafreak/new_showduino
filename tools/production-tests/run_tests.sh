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

g++ -std=c++17 -Wall -Wextra \
  -o "$HERE/web_timeline_upload_policy_tests" \
  "$HERE/test_web_timeline_upload_policy.cpp"
"$HERE/web_timeline_upload_policy_tests"

g++ -std=c++17 -Wall -Wextra -I"$HERE/../../protocol" \
  -o "$HERE/version_tests" "$HERE/test_version.cpp"
"$HERE/version_tests"

g++ -std=c++17 -Wall -Wextra -I"$HERE/../../protocol" \
  -o "$HERE/shdo_tests" "$HERE/test_shdo.cpp"
"$HERE/shdo_tests"

g++ -std=c++17 -Wall -Wextra -I"$HERE/../../protocol" \
  -o "$HERE/web_body_tests" "$HERE/test_web_body.cpp"
"$HERE/web_body_tests"

# Phase 3: genuine website Studio exports through the compiler and the P4 timeline loader.
g++ -std=c++17 -Wall -Wextra -I"$SRC_DIR" \
  -o "$HERE/studio_export_tests" \
  "$HERE/test_studio_exports.cpp" "$SRC_DIR/ProductionFormat.cpp"
"$HERE/studio_export_tests" "$HERE/fixtures/studio-phase3"

# Phase 3: pixel segment numbers must equal website Studio's live-upload compiler.
g++ -std=c++17 -Wall -Wextra \
  -o "$HERE/pixel_segment_tests" "$HERE/test_pixel_segments.cpp"
"$HERE/pixel_segment_tests" "$HERE/fixtures"
