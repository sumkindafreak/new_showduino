#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$HERE/../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src"
g++ -std=c++17 -Wall -Wextra -I"$SRC_DIR" \
  -o "$HERE/plugin_role_tests" \
  "$HERE/test_plugin_roles.cpp" "$SRC_DIR/plugin/PluginRoles.cpp"
"$HERE/plugin_role_tests"
