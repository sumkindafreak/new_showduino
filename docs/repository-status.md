# Showduino Repository Status

Classification of firmware projects. **No folders were moved or deleted** for this document. Status labels guide development priority only.

Related:

- [Constitution](constitution.md)
- [Architecture](architecture.md)
- [Command protocol](command-protocol.md)
- [State synchronisation (Stage 3)](state-synchronisation.md)
- [Final hardware architecture](final-hardware-architecture.md)

**Roadmap note:** Stages 0–3 established documentation, constitution, shared protocol, and authoritative state. The active P4 firmware now also contains the Stage 6 RAM timeline/runtime plus transactional loading of versioned TEST/LOG productions from P4 SD. Broader asset storage, a complete authoritative Web UI product, structured v2 routing, and completion-driven node state remain future work.

---

## 1. Canonical active stack

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine ESP32-S3 (dedicated Dev Module)
    → UART
Show Engine ESP32-P4
```

The supported current stack ends at the P4. Node firmware and node routing remain experimental/future work.

Browser / phone access remains a conceptual target. The current S3 Comms Controller does not host SoftAP. The Director does not host the primary Web UI.

### Active firmware

| Folder | Role |
|--------|------|
| `firmware/director-esp32-8048s050/` | Director — operator UI, requests, display |
| `firmware/s3-comms-controller/` | Communications Engine — dedicated ESP32-S3 ESP‑NOW + UART |
| `firmware/stage-engine-p4/` | Show Engine on Stage Controller (folder name legacy) |

---

## 2. Complete firmware classification table

Status values: `ACTIVE` · `LEGACY` · `EXPERIMENTAL` · `DIAGNOSTIC` · `INCOMPLETE` · `ARCHIVE CANDIDATE`

| Folder | Status | Target hardware | Current purpose | Reason for classification | Relationship to active architecture | Recommended future action |
|--------|--------|-----------------|-----------------|---------------------------|--------------------------------------|---------------------------|
| `firmware/director-esp32-8048s050/` | **ACTIVE** | ESP32-S3 800×480 (8048S043/S050) | Canonical Director LVGL + ESP‑NOW client | Supported operator desk | Desk → Comms via ESP‑NOW | Keep; align UI to constitution in later stages |
| `firmware/s3-comms-controller/` | **ACTIVE** | ESP32-S3 Dev Module | Canonical Communications Engine | Dedicated ESP‑NOW + UART bridge | Centre of ESP‑NOW/UART fabric | Keep; do not move show logic here |
| `firmware/p4-c6-espnow-bridge/` | **UNUSED / RESERVED** | Onboard ESP32-C6 (Waveshare P4 module) | Historical C6 ESP‑NOW UART bridge | Superseded by dedicated S3 Comms Controller | Physical C6 nets remain reserved; do not flash | Retain source; not current architecture |
| `firmware/stage-engine-p4/` | **ACTIVE** | ESP32-P4 Stage Controller | Canonical Show Engine runtime + timeline hub | Authoritative state, safety, SD production loading, and cue scheduling | UART peer of Comms Engine | Keep; grow SoT features; rename folder later |
| `firmware/relay-node-esp32/` | **EXPERIMENTAL / FUTURE** | ESP32 + relay module | Relay-node prototype | Source exists, but completion semantics and logical-ID routing are incomplete | Outside the supported current stack | Retain for a future node milestone; do not present as shipping |
| `firmware/c3-supermini-espnow-bridge/` | **LEGACY / SUPERSEDED** | ESP32-C3 SuperMini (SUE) | Previous external Communications Engine | External C3 + Wi‑Fi AP generation | Superseded first by onboard C6, now by dedicated S3 | Retain as reference; do not treat as current path |
| `firmware/director-s3/` | **LEGACY** | ESP32-S3 + TFT_eSPI | Earlier UART-only Director scaffold | Older topology (Director↔UART↔engine) | Superseded by 8048 ESP‑NOW Director | Retain for reference; do not extend |
| `firmware/espnow-bridge/` | **LEGACY** | ESP32-C3/C6/S3/ESP32 | Early P4↔node ESP‑NOW scaffold | Pre–dual-role C3 design | Superseded by C3 then by onboard C6 | Retain for packet ideas; do not ship |
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

### Communications Engine (`firmware/s3-comms-controller/`)

**Owns:**

* ESP‑NOW fabric
* UART transport to the P4
* Packet routing
* Link health
* Transport-address resolution

**Must not own:**

* Show decisions
* Timelines
* Cue state
* Physical effects
* SoftAP / WebUI / BLE / OTA in this phase
* False completion acknowledgements

The onboard C6 firmware (`firmware/p4-c6-espnow-bridge/`) is **UNUSED / RESERVED**. The previous Communications Engine (`firmware/c3-supermini-espnow-bridge/`) remains **LEGACY / SUPERSEDED**.

### Show Engine (`firmware/stage-engine-p4/`)

**Owns:**

* Authoritative state
* Timeline and cue execution (as implemented)
* Versioned SD production manifests and TEST/LOG timelines (implemented foundation); broader project/assets storage remains a target
* Safety policy
* Node coordination
* Local DMX, pixels, audio, storage and Web services **as implemented**

**Must not own:**

* ESP‑NOW radio implementation
* Director visual logic
* Node-local hardware drivers

### Relay Node prototype (`firmware/relay-node-esp32/`)

**Classification:** EXPERIMENTAL / FUTURE. It documents the intended node boundary, but it is not part of the supported current product path.

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
| `firmware/espnow-bridge/` | `archive/experimental-bridges/espnow-bridge/` | Superseded by C3 then onboard C6 | Node packet comments if any unique |
| `firmware/c3-supermini-espnow-bridge/` | `archive/legacy-bridges/c3-supermini-espnow-bridge/` (optional later) | Previous external SUE C3 | Confirm no unique Wi‑Fi/Web tunnel notes needed |
| `firmware/sue-esp32s3-node/` | `archive/incomplete-prototypes/sue-esp32s3-node/` or revive under `nodes/` | Stub only | Source-repo links from README |
| `firmware/touch-probe-8048/` | `archive/diagnostic-sketches/touch-probe-8048/` (optional) | Or keep beside Director as lab tool | None required |
| `.../ShowduinoSdTouchTest/` | `archive/diagnostic-sketches/ShowduinoSdTouchTest/` (optional) | Or keep under Director tree | None required |

The three active folders stay under `firmware/`. The relay prototype remains in place as experimental source.

---

## 5. Known naming debt

Do **not** rename in Stage 1. Recorded for later stages:

| Debt | Location | Issue |
|------|----------|-------|
| Folder `stage-engine-p4` | `firmware/stage-engine-p4/` | Should eventually reflect **Show Engine** |
| Sketch `ShowduinoStageEngineP4` | Same | “Stage Engine” retired; product is Stage Controller running Show Engine |
| Macro names `SHOWDUINO_COMMS_MAC_*` | Director `BoardConfig.h` | Values must be copied from the S3 Comms Controller boot Serial (`SHOWDUINO_P4_C6_MAC_*` remains an alias) |
| Term “Stage Engine” | Older docs / comments / Serial strings | Replace with Show Engine / Stage Controller as edited |
| Director↔UART↔P4 diagrams | Older docs (mostly corrected in Stage 0) | Must not reappear as “current” topology |
| External C3 / SUE as “current Comms Engine” | Older docs / C3 firmware README | Previous generation; current path is dedicated ESP32-S3 |
| Onboard C6 as “current Comms Engine” | Older docs / `firmware/p4-c6-espnow-bridge/` | Superseded; C6 hardware remains reserved unused |
| Root/history “ESP-NOW Bridge” as vague role | Mixed docs | Official role name is **Communications Engine** |

---

## Status legend (quick)

| Status | Meaning |
|--------|---------|
| UNUSED / RESERVED | Hardware present but not used by Showduino application firmware |
| ACTIVE | Canonical runtime / supported path |
| LEGACY | Earlier working architecture, reference only |
| EXPERIMENTAL | Prototype / alternate, not canonical |
| DIAGNOSTIC | Bring-up / probe utility |
| INCOMPLETE | Stub or non-operational placeholder |
| ARCHIVE CANDIDATE | Suitable for a future archive move; still present in-tree |

---

## Implementation maturity

| Maturity | Current repository scope |
|----------|--------------------------|
| **IMPLEMENTED** | Director → ESP-NOW → dedicated S3 Comms → UART → P4 transport; P4 authoritative runtime/emergency state; RAM timeline; P4 SD discovery and transactional loading of versioned TEST/LOG productions |
| **PARTIAL** | P4 storage/assets beyond the production manifest and timeline; audio and local output services; host-side Web UI/API integration; compatibility relay state surfaces |
| **PLANNED** | Supported node products; logical device-ID routing end to end; structured/versioned transport messages; completion-driven node state and faults; broader production cue types |
| **LEGACY** | C3/SUE Communications Engine, earlier Director/bridge firmware, CYD/Mega generation, and Director-authoritative show storage assumptions |
