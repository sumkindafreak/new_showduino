#!/usr/bin/env bash
set -euo pipefail
TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
PLAYER_DIR="$TEST_DIR/../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src"
TEST_BINARY="$(mktemp)"
trap 'rm -f "$TEST_BINARY"' EXIT
g++ -std=gnu++17 -Wall -Wextra -I"$TEST_DIR/stubs" -I"$PLAYER_DIR" \
  -include "$TEST_DIR/stubs/HostConfig.h" "$TEST_DIR/test_ambience.cpp" \
  "$PLAYER_DIR/StageAmbience.cpp" -o "$TEST_BINARY"
"$TEST_BINARY"
