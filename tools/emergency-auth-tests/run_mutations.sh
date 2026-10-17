#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 tools/emergency-auth-tests/run_mutations.py
