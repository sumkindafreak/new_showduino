#!/usr/bin/env python3
"""Source regression: Live page Emergency instructions must match P4/Director authority.

Checks only browser copy and command boundaries. It cannot certify physical safety.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
live = (ROOT / "web/showduino-studio/js/pages/Live.js").read_text(encoding="utf-8")
p4 = (ROOT / "firmware/stage-engine-p4/ShowduinoStageEngineP4/src/EmergencyInput.cpp").read_text(encoding="utf-8")
director = (ROOT / "firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorEmergencyClearDialog.cpp").read_text(encoding="utf-8")
checks = {
    "obsolete 3-second hold text removed": not re.search(r"3\s*[- ]?s(?:econd)?s?\s*[- ]?hold|3s-hold", live, re.I),
    "8-second hold described as Locate, not clear": "holding it for 8 seconds requests Locate, not clearance" in live,
    "operator must release physical button": "To clear Emergency, release the button" in live,
    "Director request and explicit confirmation shown": "Director Emergency → CLEAR EMERGENCY → CONFIRM CLEAR" in live,
    "P4 authorisation is explicit": "The P4 must authorise clearance" in live,
    "no automatic resume stated": "Clearing never automatically resumes a show" in live,
    "browser cannot send Emergency clear": not re.search(r"send\(\s*['\"](?:EMERGENCY|ESTOP):CLEAR", live),
    "browser PANIC still asserts Emergency": "send('EMERGENCY:STOP'" in live,
    "P4 main button uses hold/clear policy": "SHOWDUINO_ESTOP_LOCATE_HOLD_MS" in p4 and "emergencyInputLoopHealthy" in p4,
    "Director confirmation exists": "CONFIRM CLEAR" in director and "confirmFn_" in director,
}
failed = []
for label, ok in checks.items():
    print(("PASS  " if ok else "FAIL  ") + label)
    if not ok:
        failed.append(label)
if failed:
    print(f"{len(failed)} FAILURES")
    sys.exit(1)
print("ALL PASS — source-only UI safety wording check; physical acceptance remains separate")
