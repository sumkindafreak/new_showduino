# Showduino V1 feature matrix

Product version: **1.0.0-rc.1**. Do not call hardware items complete from source inspection.

Legend:

| Status | Meaning |
|--------|---------|
| YES | Implemented and proven by host tests and/or prior accepted hardware |
| PARTIAL | Code exists; scope is intentionally incomplete |
| NEEDS HARDWARE TEST | Implemented in this tree; not signed off on the bench |
| BLOCKER | Release of `v1.0.0` must not happen until this is physically proven |
| NO | Not in V1 |

| Feature | Director | Comms | P4 | WebUI / Studio | Protocol | Nodes | V1 status |
|---------|----------|-------|----|----------------|----------|-------|-----------|
| Studio → SHDO v2 → Comms → P4 → SD persist | Display/load only | WEB/BODY proxy | Compile + staging commit; no auto-load | Commissioning persist + RAM `/studio/` | `showduino_shdo.h` / `showduino_deploy.h` | n/a | **PARTIAL** + **NEEDS HARDWARE TEST** (RAM timeline YES in prior work; SD persist new) |
| Unified 1.0.0-rc.x product version | About + footer | `/api/comms` + USB | `/api/system` | Brand + Software card | `showduino_version.h` | Component FW still independent | **YES** (host tests) |
| Director boot draw-order | First complete frame then backlight | n/a | n/a | n/a | n/a | n/a | **NEEDS HARDWARE TEST** |
| Boot sound + atmospheric LEDs | Ambient engine present | n/a | System audio separate | n/a | n/a | Lamp/Audio not boot-atmosphere | **NEEDS HARDWARE TEST** |
| Director Nodes-style visual refresh | Status LINK/WIFI/UPD; COMMS footer; Settings Network/Software | Gateway wires | Unchanged authority | Nodes page not restyled | GATEWAY/UPDATE wires | n/a | **PARTIAL** (UI copy/status; not a full visual reskin) |
| Audio power-on / Director disconnect | Follow scan while unlinked; no recover-to-ch1 | Peer channel 0; no deinit | n/a | n/a | Radio policy | Audio follow; SoftAP follow channel | **BLOCKER** until 10× physical test |
| Deterministic ESP-NOW channel/peer | Follow Comms; peer channel 0 | Channel authority; never yank to 1 while STA up | n/a | Reports channel | `showduino_radio.h` | Audio + Lamp follow | **NEEDS HARDWARE TEST** |
| Comms home/venue Wi-Fi + retained AP | WIFI slot ≠ LINK | NVS `sdnet`, AP+STA | Not Wi-Fi authority | Network page | GATEWAY wire | Follow Showduino AP | **NEEDS HARDWARE TEST** |
| WebUI Network configuration/status | Network dialog (display) | `/api/gateway*` | Ethernet/E1.31 still P4 | Gateway first, Ethernet below | n/a | n/a | **PARTIAL** (implemented; needs bench) |
| GitHub Releases Check for Updates | UPD hint, no prompt | Discovery only | Not GitHub authority | System Software card | Version compare | n/a | **PARTIAL** (no OTA install; no releases published yet) |
| Future LAN devices / paludarium | No UI | No dispatcher | No `NETWORK_HTTP` | Not advertised as shipping | Documented only | n/a | **NO** (foundation doc only) |
| Cross-component sync rule | Checked this pass | Checked | Checked | Checked | Headers added | Audio/Lamp radio | **YES** (process + this change set) |
| Emergency/safety regression | Dual-action clear unchanged | Transport only | Latch, pixels white, no auto-resume; persist abort | PANIC still P4 | Safety cannot weaken in SHDO | Lamp emergency white | **NEEDS HARDWARE TEST** (code not weakened) |
| Stale C3/SUE/C6/DMX | Footer COMMS not SUE | No C3 path | Transport reject obsolete SHDO | Copy updated | SHDO rejects C3/SUE/C6 | Lamp is a node, not comms | **PARTIAL** (legacy folders remain, not live path) |
| Host compile/tests | n/a | n/a | Format/store/SHDO/WEB-BODY/version | n/a | Headers host-tested | n/a | **YES** after `run_tests.ps1` |
| Firmware compile | Director 0.9.1 | Comms 0.3.0 | P4 0.5.1 | Embedded WebUI `d74a7aac072d` | n/a | Audio 0.4.2, Lamp 0.2.1 | **YES** (this pass) |
| Physical-test checklist | `docs/physical-test-checklist.md` | same | same | same | same | same | **NEEDS HARDWARE TEST** |

## Shipping tonight (operator path)

Power P4 (SD) → Comms → UART → Director. Optional Studio at `http://192.168.4.1/studio/`. START only after P4 ack. Home Wi-Fi is not required.

## Must not tag `v1.0.0` until

1. Audio Node power-on 10× does not drop the Director.
2. Emergency physical latch still works.
3. STA join on a non-1 channel does not drop the Director.
4. SHDO persist + Director load proven on SD.
