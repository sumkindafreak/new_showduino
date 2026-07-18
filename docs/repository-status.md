# Showduino Repository Status

Classification of firmware projects. **No folders were moved or deleted** for this document. Status labels guide development priority only.

Related:

- [Constitution](constitution.md)
- [Architecture](architecture.md)
- [Command protocol](command-protocol.md)
- [State synchronisation (Stage 3)](state-synchronisation.md)
- [Final hardware architecture](final-hardware-architecture.md)

**Roadmap note:** Stage 0–3 complete for documentation / constitution / shared protocol / authoritative state. Stage 4+ (timeline, storage migration, Web UI, etc.) not started.

---

## 1. Canonical active stack

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine ESP32-C3
    → UART
Show Engine ESP32-P4
```

```text
Showduino Node
    → ESP-NOW
Communications Engine ESP32-C3
    → UART
Show Engine ESP32-P4
```

Browser / phone access (conceptual target): Wi‑Fi → Communications Engine → Show Engine services. The Director does not host the primary Web UI.

### Active firmware

| Folder | Role |
|--------|------|
| `firmware/director-esp32-8048s050/` | Director — operator UI, requests, display |
| `firmware/c3-supermini-espnow-bridge/` | Communications Engine — ESP‑NOW + UART transport |
| `firmware/stage-engine-p4/` | Show Engine on Stage Controller (folder name legacy) |
| `firmware/relay-node-esp32/` | Relay Node — local relay actuation |

---

## 2. Complete firmware classification table

Status values: `ACTIVE` · `LEGACY` · `EXPERIMENTAL` · `DIAGNOSTIC` · `INCOMPLETE` · `ARCHIVE CANDIDATE`

| Folder | Status | Target hardware | Current purpose | Reason for classification | Relationship to active architecture | Recommended future action |
|--------|--------|-----------------|-----------------|---------------------------|--------------------------------------|---------------------------|
| `firmware/director-esp32-8048s050/` | **ACTIVE** | ESP32-S3 800×480 (8048S043/S050) | Canonical Director LVGL + ESP‑NOW client | Supported operator desk | Desk → Comms via ESP‑NOW | Keep; align UI to constitution in later stages |
| `firmware/c3-supermini-espnow-bridge/` | **ACTIVE** | ESP32-C3 SuperMini | Canonical Communications Engine | Live desk + node bridge | Centre of ESP‑NOW/UART fabric | Keep; add Wi‑Fi later without owning show state |
| `firmware/stage-engine-p4/` | **ACTIVE** | ESP32-P4 Stage Controller | Canonical Show Engine (early hub) | Authoritative path for decisions | UART peer of Comms Engine | Keep; grow SoT features; rename folder later |
| `firmware/relay-node-esp32/` | **ACTIVE** | ESP32 + relay module | Canonical Relay Node | Working node actuator | Node → Comms → Show Engine | Keep; device-ID addressing later |
| `firmware/director-s3/` | **LEGACY** | ESP32-S3 + TFT_eSPI | Earlier UART-only Director scaffold | Older topology (Director↔UART↔engine) | Superseded by 8048 ESP‑NOW Director | Retain for reference; do not extend |
| `firmware/espnow-bridge/` | **LEGACY** | ESP32-C3/C6/S3/ESP32 | Early P4↔node ESP‑NOW scaffold | Pre–dual-role C3 design | Superseded by C3 SuperMini | Retain for packet ideas; do not ship |
| `firmware/p4-c6-espnow-bridge/` | **EXPERIMENTAL** | ESP32-C6 (P4 companion radio) | Alternate desk bridge prototype | Incomplete vs current C3 path (desk replies) | Not canonical Comms Engine | Extract useful notes only; do not use as product bridge |
| `firmware/touch-probe-8048/` | **DIAGNOSTIC** | ESP32-8048S043/S050 | GT911 / XPT2046 touch probe | Hardware bring-up only | Supports Director hardware debug | Keep as tool; not runtime |
| `firmware/sue-esp32s3-node/` | **INCOMPLETE** | ESP32-S3 (planned) | SUE multi-function node placeholder | README / intent only; no operational sketch set | Future node family candidate | Implement under `nodes/` later or archive stub |
| `firmware/controller-cyd/` | **ARCHIVE CANDIDATE** | ESP32-2432S028R CYD | CYD front panels for Mega era | Pre–S3/P4 product direction | Parallel legacy stack | Future move to `archive/legacy-directors/` |
| `firmware/executor-mega/` | **ARCHIVE CANDIDATE** | Arduino Mega 2560 | Legacy show executor | Replaced by Show Engine on P4 | Parallel legacy stack | Future move to `archive/legacy-executors/` |

### Nested projects (not first-level, listed for completeness)

| Path | Status | Notes |
|------|--------|-------|
| `firmware/director-esp32-8048s050/ShowduinoSdTouchTest/` | **DIAGNOSTIC** | SD + touch bring-up; not production Director |
| `firmware/controller-cyd/showduino_cyd_director_v1/` | **ARCHIVE CANDIDATE** | Parent folder status applies |
| `firmware/controller-cyd/showduino_cyd_director_web_sd_v1/` | **ARCHIVE CANDIDATE** | Parent folder status applies |
| `firmware/touch-probe-8048/TouchProbe8048/` | **DIAGNOSTIC** | Sketch under diagnostic folder |
| `firmware/executor-mega/showduino_mega_v1/` | **ARCHIVE CANDIDATE** | Sketch under archive-candidate folder |

---

## 3. Active project boundaries

### Director (`firmware/director-esp32-8048s050/`)

**Owns:**

* Operator UI
* Input handling
* Display state
* ESP‑NOW client transport
* Director-local assets and diagnostics

**Must not own:**

* Authoritative show state
* Authoritative projects
* Timeline execution
* Node routing policy
* Physical completion assumptions

### Communications Engine (`firmware/c3-supermini-espnow-bridge/`)

**Owns:**

* ESP‑NOW fabric
* Wi‑Fi transport (planned in this role)
* UART transport
* Packet routing
* Link health
* Transport-address resolution

**Must not own:**

* Show decisions
* Timelines
* Cue state
* Physical effects
* False completion acknowledgements

### Show Engine (`firmware/stage-engine-p4/`)

**Owns:**

* Authoritative state
* Timeline and cue execution (as implemented)
* Project storage (target; early today)
* Safety policy
* Node coordination
* Local DMX, pixels, audio, storage and Web services **as implemented**

**Must not own:**

* ESP‑NOW radio implementation
* Director visual logic
* Node-local hardware drivers

### Relay Node (`firmware/relay-node-esp32/`)

**Owns:**

* Relay GPIO
* Local output enforcement
* Local fail-safe behaviour
* Reporting actual relay state

**Must not own:**

* Show state
* Timeline decisions
* Operator UI state
* Global emergency policy

---

## 4. Archive plan — proposal only

**Do not move files yet.** Suggested future layout:

```text
archive/
├── legacy-directors/
├── legacy-executors/
├── experimental-bridges/
├── diagnostic-sketches/
└── incomplete-prototypes/
```

| Current folder | Proposed future archive path | Reason | Extract first |
|----------------|------------------------------|--------|---------------|
| `firmware/controller-cyd/` | `archive/legacy-directors/controller-cyd/` | CYD+Mega era UI | Any still-useful SD/web patterns into docs |
| `firmware/executor-mega/` | `archive/legacy-executors/executor-mega/` | Mega no longer Show Engine | Cue/timing ideas worth citing in docs |
| `firmware/director-s3/` | `archive/legacy-directors/director-s3/` (optional later) | UART Director superseded | Confirm no unique UI patterns needed |
| `firmware/espnow-bridge/` | `archive/experimental-bridges/espnow-bridge/` | Superseded by C3 SuperMini | Node packet comments if any unique |
| `firmware/p4-c6-espnow-bridge/` | `archive/experimental-bridges/p4-c6-espnow-bridge/` | Incomplete alternate desk path | Pin/UART notes for Waveshare C6 if useful |
| `firmware/sue-esp32s3-node/` | `archive/incomplete-prototypes/sue-esp32s3-node/` or revive under `nodes/` | Stub only | Source-repo links from README |
| `firmware/touch-probe-8048/` | `archive/diagnostic-sketches/touch-probe-8048/` (optional) | Or keep beside Director as lab tool | None required |
| `.../ShowduinoSdTouchTest/` | `archive/diagnostic-sketches/ShowduinoSdTouchTest/` (optional) | Or keep under Director tree | None required |

Active four folders stay under `firmware/`.

---

## 5. Known naming debt

Do **not** rename in Stage 1. Recorded for later stages:

| Debt | Location | Issue |
|------|----------|-------|
| Folder `stage-engine-p4` | `firmware/stage-engine-p4/` | Should eventually reflect **Show Engine** |
| Sketch `ShowduinoStageEngineP4` | Same | “Stage Engine” retired; product is Stage Controller running Show Engine |
| Macro names `SHOWDUINO_P4_C6_MAC_*` | Director `BoardConfig.h` | Historical; peer is Communications Engine **C3**, not P4-C6 |
| Term “Stage Engine” | Older docs / comments / Serial strings | Replace with Show Engine / Stage Controller as edited |
| Director↔UART↔P4 diagrams | Older docs (mostly corrected in Stage 0) | Must not reappear as “current” topology |
| Bridge folder `p4-c6-espnow-bridge` | Firmware tree | Implies canonical C6 desk path; it is experimental only |
| Root/history “ESP-NOW Bridge” as vague role | Mixed docs | Official role name is **Communications Engine** |

---

## Status legend (quick)

| Status | Meaning |
|--------|---------|
| ACTIVE | Canonical runtime / supported path |
| LEGACY | Earlier working architecture, reference only |
| EXPERIMENTAL | Prototype / alternate, not canonical |
| DIAGNOSTIC | Bring-up / probe utility |
| INCOMPLETE | Stub or non-operational placeholder |
| ARCHIVE CANDIDATE | Suitable for a future archive move; still present in-tree |
