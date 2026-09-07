#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
g++ -std=c++17 -Wall -Wextra -I"$here/../../protocol" -o "$here/storage_tests" "$here/test_storage.cpp"
"$here/storage_tests"
