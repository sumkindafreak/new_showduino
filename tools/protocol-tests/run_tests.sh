#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
g++ -std=c++17 -Wall -Wextra -I"$HERE/../../protocol" -o "$HERE/protocol_tests" "$HERE/test_protocol.cpp"
"$HERE/protocol_tests"
