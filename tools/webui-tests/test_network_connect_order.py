#!/usr/bin/env python3
"""Source-order regression: Connect must capture draft credentials before paint()."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
src = (ROOT / "web" / "showduino-studio" / "js" / "pages" / "Network.js").read_text(encoding="utf-8")
failures = 0


def check(name, ok):
    global failures
    if ok:
        print("PASS  " + name)
    else:
        failures += 1
        print("FAIL  " + name)


check("imports gateway draft helpers", "createGatewayDraft" in src and "captureConnectCredentials" in src)
check("password input is type=password", "type: 'password'" in src or 'type: "password"' in src)
check("does not console.log password", "console.log" not in src)
idx_cap = src.find("captureConnectCredentials(gatewayDraft)")
idx_busy = src.find("gwBusy = 'connect'")
check("captureConnectCredentials exists", idx_cap >= 0)
check("connect sets gwBusy", idx_busy >= 0)
check("CONNECT ORDER: capture before gwBusy/paint", idx_cap >= 0 and idx_busy >= 0 and idx_cap < idx_busy)
check("scan selection writes draft", "applyScanSelection(gatewayDraft" in src)
check("SSID input updates draft", "applySsidInput(gatewayDraft" in src)
check("password input updates draft", "applyPasswordInput(gatewayDraft" in src)
check("form is not wiped with innerHTML", "host.innerHTML" not in src)
check("password not passed through el() value attr", not re.search(r"gw-pass'[^;]*value:", src))

if failures:
    print(f"\n{failures} FAILED")
    sys.exit(1)
print("\nALL PASS")
