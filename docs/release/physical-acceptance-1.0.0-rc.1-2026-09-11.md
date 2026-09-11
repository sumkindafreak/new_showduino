# Showduino 1.0.0-rc.1 — Physical bench acceptance record

**Product:** Showduino `1.0.0-rc.1`  
**Gate date:** 2026-09-11  
**Operator environment:** development host `new_showduino` (Windows), not the Showduino SoftAP  
**Rule applied:** no paper passes. Visual, audio and electrical behaviour is PASS only if physically observed.

Do **not** tag `v1.0.0` from this record.

---

## 1. Baseline

| Item | Value |
|------|--------|
| User-reported SHA | `51644cb6cbcb007868149e3300aa35c46950dc63` |
| Starting local HEAD before fetch | `51644cb6cbcb007868149e3300aa35c46950dc63` |
| Current intended `origin/main` | `bc00861aee072abc5772e3dbbc739d36769c9b05` |
| Newer commit | `bc00861` — GitHub Actions `Regenerate S3 Studio WebUI bundle` |
| Overlay still present in that bundle | **YES** — `/studio/js/showduino-pixel-authoring.js` and `web/studio-v4-overlay/` recorded in `tools/embed-webui/last-build.json` |
| Product | `1.0.0-rc.1` (`SHOWDUINO_PLATFORM_VERSION`) |
| Protocol | `1.0` |
| SHDO | package v2 (`SHOWDUINO_SHDO_PACKAGE_VERSION`) |
| P4 | `0.6.0` |
| Comms | `0.4.0` |
| Director | `0.9.2-director` |
| Pixel Node | `0.1.0` |
| Audio Node | `0.4.2` |
| Lamp Node | `0.2.1` |

Source-verified Pixel Node pins (not a physical PASS):

```text
OLED SDA GPIO5
OLED SCL GPIO6
OLED 0x3C @ 400 kHz
128×64, 180° rotation
Pixel DATA GPIO2
C3 max 512 / P4 GPIO23 max 1024
25 FX in protocol/showduino_pixel_fx.h
```

No firmware versions were changed. No `v1.0.0` tag was created.

---

## 2. Hardware reachability (this session)

| Check | Result |
|-------|--------|
| Host Wi-Fi | Connected to `PLUSNET-NFFHPJ` (5 GHz, channel 36) — **not** `Showduino` |
| `192.168.4.1` (Comms SoftAP) | ICMP timeout |
| `192.168.5.1` (Pixel Node SoftAP) | ICMP timeout |
| Device Manager Ports | Stale/ghost entries (Status `Unknown`): COM3–COM8, COM17–COM22, COM24, COM27, COM28 |
| Serial open of those COMs | **Every port: “The port does not exist.”** |
| Identified live P4 / Comms / Director / nodes | **None** |

Historical note only: a previous session mapped Audio Node bring-up to a CP210x port, likely COM7. That port does not exist on this host now.

**Conclusion:** this environment cannot flash, cannot capture live serial banners, and cannot observe pixels, OLED, Director LCD, audio or emergency hardware.

---

## 3. Classification key

| Label | Meaning |
|-------|---------|
| **PASS — PHYSICALLY VERIFIED** | Required hardware behaviour was seen on the real bench |
| **PASS — HOST TEST ONLY** | Automated/host/browser test only. Not a physical PASS |
| **FAIL** | Physical test was run and failed |
| **BLOCKED** | Required hardware or access was not available |
| **NOT RUN** | Test was not performed |

No item below is **PASS — PHYSICALLY VERIFIED**.

---

## 4. Flash / device versions

| Test ID | Hardware | Steps | Expected | Observed | Result |
|---------|----------|-------|----------|----------|--------|
| FLASH-01 | P4 | Build + flash RC binary; read boot version | Reports `0.6.0` | Not flashed. No P4 serial. | **BLOCKED** |
| FLASH-02 | Comms | Build + flash `bc00861` embed; read boot version | Reports `0.4.0` | Not flashed. No Comms serial/SoftAP. | **BLOCKED** |
| FLASH-03 | Director | Build + flash; read version | Reports `0.9.2-director` | Not flashed. | **BLOCKED** |
| FLASH-04 | Audio Node | Build + flash; read version | Reports `0.4.2` | Not flashed. | **BLOCKED** |
| FLASH-05 | Lamp Node | Build + flash; read version | Reports `0.2.1` | Not flashed. | **BLOCKED** |
| FLASH-06 | LED-01 | Flash Pixel Node `0.1.0` | Boot banner `0.1.0` | Not flashed. | **BLOCKED** |
| FLASH-07 | LED-02 | Same binary as LED-01 | Boot banner `0.1.0` | Not flashed. | **BLOCKED** |

---

## 5. Physical tests

Date/time for all rows: **2026-09-11**. Firmware column is the *intended* RC set above. Observed result for every physical row: **no bench hardware on the validation host**.

| Test ID | Test | Hardware | Expected | Observed | Result |
|---------|------|----------|----------|----------|--------|
| A-01 | Core cold boot (P4+Comms+Director only) | Core | Stable links, P4 authoritative, no DEGRADED/reboot loop, no false emergency | Not run — no devices | **BLOCKED** |
| A-02 | Core cold boot 10× | Core | 10/10 | 0/10 attempted | **BLOCKED** |
| A-03 | Director clean boot UI | Director | Dark backlight until ready; no old UI flash; clean boot screen; emergency can interrupt | Not observed | **BLOCKED** |
| B-01 | GPIO23 not initialised | P4 + strip | `NOT INITIALISED`; pixels dark | Not observed | **BLOCKED** |
| B-02 | GPIO23 Save Count (e.g. 30) | P4 + strip | Persists; no unexpected light | Not observed | **BLOCKED** |
| B-03 | GPIO23 Initialise Line | P4 + strip | Configured count addressable; safe start | Not observed | **BLOCKED** |
| B-04 | GPIO23 segments 0–2 independent + simultaneous | P4 + strip | Boundaries hold; no cross-corruption | Not observed | **BLOCKED** |
| B-05 | GPIO23 all 25 FX | P4 + strip | Each FX runs/stops; emergency not blocked; no P4 crash | Not observed | **BLOCKED** |
| B-06 | GPIO23 emergency all-white | P4 + E-stop + strip | Entire configured GPIO23 line bright white | Not observed | **BLOCKED** |
| B-07 | GPIO24 normal signage | P4 + signage strip | Groups of 10; pixel 0/10/… green | Not observed | **BLOCKED** |
| B-08 | GPIO24 emergency white | P4 + E-stop + signage | Entire applicable GPIO24 line bright white | Not observed | **BLOCKED** |
| C-01 | LED-01 first boot / OLED / AP / discovery | LED-01 | OLED crop/orientation; AP; no Director disruption | Not observed | **BLOCKED** |
| C-02 | LED-01 OLED states | LED-01 | Readable BOOTING / UNINIT / SEARCHING / CONNECTED / LOCATE / EMERGENCY / LOST | Not observed | **BLOCKED** |
| C-03 | LED-01 commissioning WebUI | LED-01 SoftAP | ID/name/count/init/test; ESP-NOW stays up | SoftAP unreachable | **BLOCKED** |
| C-04 | LED-01 identity persist `LED-01` / `Entrance` | LED-01 + P4 + Comms + Director + Studio | Logical ID persists; MAC not production identity | Not observed | **BLOCKED** |
| C-05 | LED-01 Save Count (e.g. 60) | LED-01 | Persists; no unexpected light | Not observed | **BLOCKED** |
| C-06 | LED-01 Initialise Line | LED-01 | Correct length; tests unlock; no C3/ESP-NOW loss | Not observed | **BLOCKED** |
| C-07 | LED-01 reboot persistence | LED-01 | ID/name/count persist; reconnect; no stale FX | Not observed | **BLOCKED** |
| C-08 | LED-01 segments + later-slot-wins | LED-01 | Independent + simultaneous; overlap later wins | Not observed | **BLOCKED** |
| C-09 | LED-01 all 25 FX | LED-01 | Same vocabulary/params as P4; no WDT/radio starve | Not observed | **BLOCKED** |
| C-10 | LED-01 Locate | LED-01 | Temporary pattern; non-blocking; then resync | Not observed | **BLOCKED** |
| C-11 | LED-01 emergency all-white | LED-01 + E-stop | Entire configured line white; OLED EMERGENCY; Locate loses | Not observed | **BLOCKED** |
| C-12 | LED-01 emergency while not initialised | LED-01 + E-stop | Configured count forces white; count 0 stays dark | Not observed | **BLOCKED** |
| C-13 | LED-01 emergency clear | Full authority path | Node does not self-clear; no stale FX resume | Not observed | **BLOCKED** |
| C-14 | LED-01 comms-loss blackout | LED-01 isolated | Line black; core/Audio/Lamp/Director continue; resync on rejoin | Not observed | **BLOCKED** |
| D-01 | Embedded `/studio/` pixel picker | Comms SoftAP | P4 + LED-01 — Entrance visible; not stale bundle | SoftAP unreachable | **BLOCKED** |
| D-02 | Author mixed production | Studio | P4 FIRE seg 1 + LED-01 FLICKER seg 2 (+ audio); save/reload exact IDs | Not run on Comms `/studio/` | **BLOCKED** |
| D-03 | Deploy mixed production | Studio→SHDO v2→Comms→P4→SD | Physical P4 + LED-01 + audio execute; browser not timing | Not run | **BLOCKED** |
| D-04 | Disconnect browser during show | Running show | Show continues; P4 authoritative | Not run | **BLOCKED** |
| D-05 | Disconnect Director during show | Running show | Show continues; Director recovers | Not run | **BLOCKED** |
| E-01 | LED-02 commission `Corridor` | LED-02 | Same binary; independent ID/count | Node not present | **BLOCKED** |
| E-02 | Multi-node discovery | LED-01 + LED-02 | Both IDs/names; neither replaces the other | Not run | **BLOCKED** |
| E-03 | Multi-node mixed production | P4 + LED-01 + LED-02 + Audio/Lamp | Each target independent | Not run | **BLOCKED** |
| E-04 | Power-cycle LED-01 during show | Full system | Others continue; LED-01 resyncs | Not run | **BLOCKED** |
| E-05 | Power-cycle LED-02 during show | Full system | LED-01 unaffected | Not run | **BLOCKED** |
| E-06 | Pixel join torture 10× LED-01 | Core + LED-01 | 10/10 no Director/core drop | 0/10 | **BLOCKED** |
| E-07 | Pixel join torture 10× LED-02 | Core + LED-02 | 10/10 | 0/10 | **BLOCKED** |
| F-01 | Audio power-on while Director linked 10× | Core + Audio | 10/10 no `SHOWDUINO CONNECTION LOST` | 0/10 — **existing release blocker remains untested** | **BLOCKED** |
| F-02 | Combined node power-up order torture | All nodes | Radio stable for listed orders | Not run | **BLOCKED** |
| F-03 | Follow-channel (Comms STA ≠ ch 1) | Full radio set | All peers follow; SoftAP usable per design | Host is on home 5 GHz AP, not Comms STA | **BLOCKED** |
| F-04 | SoftAP while show connected | Comms + node WebUIs | No ESP-NOW deinit / peer delete / Director drop | SoftAP unreachable | **BLOCKED** |
| G-01 | Full-system emergency | All outputs + E-stop | GPIO23/24 + LED-01/02 all-white; audio/lamp override; Director shows emergency | Not observed | **BLOCKED** |
| G-02 | Emergency clear rejected while input held | E-stop | CLEAR rejected until released | Not observed | **BLOCKED** |
| G-03 | Legitimate emergency clear | Authority path | Authoritative resync; no stale FX | Not observed | **BLOCKED** |
| G-04 | Emergency during node join | LED-01/02 + E-stop | White once emergency state received | Not observed | **BLOCKED** |
| G-05 | Node failure during emergency | LED-01/02 + E-stop | Rejoin stays emergency; no show FX first | Not observed | **BLOCKED** |
| H-01 | P4 standalone (no Director/browser/Wi-Fi/internet) | P4 + nodes | Show continues | Not run | **BLOCKED** |
| H-02 | Soak | Full representative show | Record duration; no invented PASS | **NOT RUN** — no duration | **NOT RUN** |
| H-03 | C3 pixel performance toward 512 | LED-01 safe PSU | FX/emergency/radio/OLED/WDT remain responsive | Not run | **BLOCKED** |
| H-04 | Power / thermal / emergency-white current | PSU + wiring | Safe voltages; no undocumented reset | Not observed | **BLOCKED** |

---

## 6. Host-only evidence (not physical)

These already exist in the repository from software work. They do **not** satisfy the release gate.

| Item | Evidence | Result |
|------|----------|--------|
| Shared 25-FX vocabulary + pixel picker logic | `tools/studio-v4-tests/test_pixel_authoring.html` — 16/16 in a previous session | **PASS — HOST TEST ONLY** |
| Mixed P4 + LED-01 + LED-02 + audio SHDO compile | `tools/production-tests/test_shdo.cpp` | **PASS — HOST TEST ONLY** |
| Studio overlay save/reload/duplicate identity | Browser Studio clone in a previous session | **PASS — HOST TEST ONLY** |
| Comms compile after embed | `1334949` flash bytes (39%), `52408` RAM (15%) on `51644cb` embed | **PASS — HOST TEST ONLY** |
| Public showduino.com Pixel Node picker | Overlay exists in this repo; `gh` not authenticated; site not deployed | **NOT RUN** (website deploy) |

---

## 7. Failures, fixes, retests

| Failure | Root cause | Fix commit | Retest |
|---------|------------|------------|--------|
| None this session | Physical bench was not attached to the validation host | None. No firmware change. | Not applicable |

---

## 8. Acceptance matrix

| System | Test | Result |
|--------|------|--------|
| Core | Cold boot 10× | **BLOCKED** |
| Director | Clean boot UI | **BLOCKED** |
| P4 GPIO23 | Count/save/init | **BLOCKED** |
| P4 GPIO23 | Segments/FX | **BLOCKED** |
| GPIO24 | Normal signage | **BLOCKED** |
| GPIO24 | Emergency white | **BLOCKED** |
| LED-01 | OLED | **BLOCKED** |
| LED-01 | Count/save/init | **BLOCKED** |
| LED-01 | Segments/FX | **BLOCKED** |
| LED-01 | Locate | **BLOCKED** |
| LED-01 | Emergency white | **BLOCKED** |
| LED-01 | Comms-loss blackout | **BLOCKED** |
| LED-01 | Join 10× | **BLOCKED** |
| LED-02 | Independent operation | **BLOCKED** |
| Multi-node | Simultaneous operation | **BLOCKED** |
| Audio | Join 10× | **BLOCKED** |
| Radio | Follow-channel | **BLOCKED** |
| Studio | Mixed authoring | **BLOCKED** (embedded `/studio/` not reachable) |
| SHDO | Physical mixed deploy | **BLOCKED** |
| Runtime | Browser removed | **BLOCKED** |
| Runtime | Director removed | **BLOCKED** |
| Emergency | Full-system activation | **BLOCKED** |
| Emergency | Clear policy | **BLOCKED** |
| System | Soak | **NOT RUN** |

---

## 9. Remaining blockers (unchanged)

All items in the release-blocker list remain blockers because they were not physically passed:

* core cold boot 10/10
* Director clean boot
* Audio power-on while Director linked 10/10
* LED-01 join/power-cycle 10/10
* multi-Pixel-Node stability
* P4 GPIO23 / GPIO24 / Pixel Node GPIO2 physical output
* Pixel Node OLED, Save Count / Initialise, segments, FX, emergency white, comms-loss blackout
* embedded Studio mixed authoring on Comms
* real SHDO mixed deploy
* browser-disconnect and Director-disconnect continuation
* full-system emergency and clear
* follow-channel
* soak

Public showduino.com Studio picker remains a **separate deployment item**, not a runtime architecture change.

---

## 10. Release recommendation

**Keep Showduino `1.0.0-rc.1`.**

Do **not** tag `v1.0.0`.

Resume this gate on a host that has:

1. USB serial to P4, Comms, Director, Audio, Lamp, LED-01, LED-02
2. Correctly powered pixel loads and a fused bench PSU
3. Physical emergency input
4. A person who can watch the Director LCD, OLEDs and pixel lines

Then flash `bc00861` (or newer intended `origin/main`) and fill this matrix with observed PASS/FAIL rows. Preserve any new failures; do not erase them after a later fix.
