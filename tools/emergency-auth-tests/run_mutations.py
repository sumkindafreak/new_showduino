#!/usr/bin/env python3
"""MISSION 001J mutation check for the station authentication rejection paths.

Each mutant removes or weakens one check in a temporary copy of protocol/. Both
the auth suite and the station integration suite are rebuilt against the copy
(-Wall -Wextra -Werror). A mutant is KILLED when compilation succeeds and at
least one suite fails. A mutant that does not compile is reported as INVALID
(the script fails, so the mutant must be rewritten). A SURVIVING mutant fails
the script unless it is listed in EQUIVALENT with a reason.

Run from the repository root:  python3 tools/emergency-auth-tests/run_mutations.py
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SUITES = [
    "tools/emergency-auth-tests/test_station_auth.cpp",
    "tools/emergency-station-integration-tests/test_station_integration.cpp",
]

# (id, file, old, new, occurrence index (0-based) or None for "must be unique")
MUTANTS = [
    ("M01 nonce single-use bit ignored", "showduino_station_auth.h",
     "    if (s->usedMask & bit) return 0;\n", "", None),
    ("M02 freshness window ignored", "showduino_station_auth.h",
     "    if (age > SHOWDUINO_AUTH_WINDOW_MS) return 0;\n", "    (void)age;\n", None),
    ("M03 challenge episode ignored", "showduino_station_auth.h",
     "    if (s->episode != episodeNow) return 0;\n", "    (void)episodeNow;\n", None),
    ("M04 AUTH tag not compared", "showduino_station_auth.h",
     "  return showduino_auth_ct_equal(expect, m->tag, SHOWDUINO_AUTH_TAG_BYTES);",
     "  (void)expect; return 1;", 0),
    ("M05 CMD tag not compared", "showduino_station_auth.h",
     "  return showduino_auth_ct_equal(expect, m->tag, SHOWDUINO_AUTH_TAG_BYTES);",
     "  (void)expect; return 1;", 1),
    ("M06 counter freshness >=", "showduino_station_auth.h",
     "  return counter > lastAccepted;", "  return counter >= lastAccepted;", None),
    ("M07 counter block persisted after use", "showduino_station_auth.h",
     "    if (!persist(ctx, newLimit)) return 0;", "    (void)persist(ctx, newLimit);", None),
    ("M08 challenge reset rewinds sequence", "showduino_station_auth.h",
     "  t->lastSeq = keep;", "  (void)keep; t->lastSeq = 0;", None),
    ("M09 ct_equal checks first byte only", "showduino_station_auth.h",
     "  for (size_t i = 0; i < n; ++i) acc |= (uint8_t)(a[i] ^ b[i]);",
     "  for (size_t i = 0; i < n && i < 1; ++i) acc |= (uint8_t)(a[i] ^ b[i]);", None),
    ("M10 ASSERT does not latch with a full table", "showduino_station_model.h",
     "    res->globalLatch = 1;\n    int slot = showduino_station_model_find_id(t, n, id);",
     "    int slot = showduino_station_model_find_id(t, n, id);", None),
    ("M11 offline station does not block", "showduino_station_model.h",
     "  return (m->inputOpen || !m->reportKnown || !m->online) ? 1 : 0;",
     "  return (m->inputOpen || !m->reportKnown) ? 1 : 0;", None),
    ("M12 proof age not checked", "showduino_station_model.h",
     "        m->proofChalSeq == 0 || (int32_t)(m->proofChalSeq - sinceSeq) < 0) {",
     "        m->proofChalSeq == 0) { (void)sinceSeq;", None),
    ("M13 missing provisioned station not counted", "showduino_station_model.h",
     "    if (slot < 0) { missing++; continue; }", "    if (slot < 0) { continue; }", None),
    ("M14 table overflow ignored", "showduino_station_model.h",
     "  if (tableOverflow) return SHOWDUINO_CLEAR_BLOCK_OVERFLOW;", "  (void)tableOverflow;", None),
    ("M15 proof window ignores the request", "showduino_station_model.h",
     "  return (int32_t)(requestSeq - recent) > 0 ? requestSeq : recent;",
     "  return recent;", None),
    ("M16 unverified AUTH accepted (tag result ignored)", "showduino_station_model.h",
     "    if (!tagOk) return 1;", "    (void)tagOk;", None),
    ("M17 dismiss allowed for provisioned IDs", "showduino_station_model.h",
     "  if (idIsProvisioned) return 0;", "  (void)idIsProvisioned;", None),
    ("M18 station GCL episode not checked", "showduino_station_responder.h",
     "  } else if (!showduino_auth_gcl_matches_episode(st->latchedEpisode, m.episode)) {",
     "  } else if (0) {", None),
    ("M19 station applies before persisting", "showduino_station_responder.h",
     "  if (!persist(ctx, m.counter)) return SHOWDUINO_RESP_REJECT_PERSIST;",
     "  (void)persist(ctx, m.counter);", None),
    ("M20 station ignores CMD identity", "showduino_station_responder.h",
     "  if (!showduino_emergency_id_equal(m.id, id)) return SHOWDUINO_RESP_REJECT_IDENTITY;",
     "", None),
    ("M21 fresh latch keeps the old episode", "showduino_station_responder.h",
     "  if (l != st->prevLatched) st->latchedEpisode = 0;", "", None),
    ("M22 station accepts commands while unlatched", "showduino_station_responder.h",
     "  if (!latched) return SHOWDUINO_RESP_REJECT_STATE;", "  (void)latched;", None),
]

# Mutants shown to be equivalent, with the reason. Must stay empty unless argued.
EQUIVALENT = {}


def nth_replace(text, old, new, occ):
    if occ is None:
        if text.count(old) != 1:
            return None
        return text.replace(old, new, 1)
    idx = -1
    for _ in range(occ + 1):
        idx = text.find(old, idx + 1)
        if idx < 0:
            return None
    return text[:idx] + new + text[idx + len(old):]


def run_suites(inc, work):
    for i, src in enumerate(SUITES):
        exe = os.path.join(work, "t%d" % i)
        c = subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I" + inc,
                            os.path.join(ROOT, src), "-lcrypto", "-o", exe],
                           capture_output=True, text=True)
        if c.returncode != 0:
            return "INVALID", c.stderr.strip().splitlines()[:3]
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode != 0:
            fails = [l for l in r.stdout.splitlines() if l.startswith("FAIL")]
            return "KILLED", fails[:2]
    return "SURVIVED", []


def main():
    work = tempfile.mkdtemp(prefix="sd_mut_")
    inc = os.path.join(work, "protocol")
    bad = 0
    try:
        shutil.copytree(os.path.join(ROOT, "protocol"), inc)
        status, _ = run_suites(inc, work)
        if status != "SURVIVED":
            print("BASELINE FAILS: fix the suites before running mutations")
            return 2
        for mid, fname, old, new, occ in MUTANTS:
            shutil.rmtree(inc)
            shutil.copytree(os.path.join(ROOT, "protocol"), inc)
            path = os.path.join(inc, fname)
            text = open(path).read()
            mutated = nth_replace(text, old, new, occ)
            if mutated is None:
                print("NOMATCH   %s (pattern not found: the mutant list is stale)" % mid)
                bad += 1
                continue
            open(path, "w").write(mutated)
            status, detail = run_suites(inc, work)
            if status == "SURVIVED" and mid in EQUIVALENT:
                status = "EQUIV"
            print("%-9s %s" % (status, mid))
            for d in detail:
                print("            " + d)
            if status in ("SURVIVED", "INVALID"):
                bad += 1
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("\n%d mutants, %d problems" % (len(MUTANTS), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
