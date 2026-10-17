#!/usr/bin/env bash
# Build and run the persistent emergency-latch policy host tests.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
proto="$here/../../protocol"
out="$(mktemp -d)/emergency_latch_tests"
g++ -std=c++17 -Wall -Wextra -Werror -I"$proto" -o "$out" "$here/test_emergency_latch.cpp"
"$out"
