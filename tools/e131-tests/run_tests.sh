#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
g++ -std=c++17 -Wall -Wextra -I"$HERE/../../protocol" -o "$HERE/e131_tests" "$HERE/test_e131_parser.cpp"
"$HERE/e131_tests"
