# Showduino Repository Status

Classification of firmware projects. Status labels guide development priority; legacy source is retained where useful.

Related:

- [Constitution](constitution.md)
- [Architecture](architecture.md)
- [Command protocol](command-protocol.md)
- [Final hardware architecture](final-hardware-architecture.md)
- [Audio and Pixel Engine](audio-pixel-engine.md)
- [Specialist Node Roadmap](node-roadmap.md)

**Roadmap note:** the active P4 now owns authoritative runtime/safety, persistent TEST/LOG production loading, onboard system audio, and the local GPIO23 segmented Show Pixel Engine. Persistent production `AUDIO`/`PIXEL` cue types and broader logical target routing remain follow-up work.

---

## 1. Canonical active stack

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine ESP32-S3 (dedicated Dev Module)
    → UART
Show Engine ESP32-P4
```

The supported current stack also includes the first specialist **Audio Node**.

Current specialist-node rollout order:

```text
Audio Node → C3 Lamp Node → C3 Pixel Node → MOSFET Node
```

The Relay Node product role is **retired**. The C3 Lamp Node occupies that Director fabric slot. MOSFET remains a later digital/PWM specialist. DMX remains parked until explicitly reopened.

The S3 Communications Engine now hosts the bench/browser SoftAP WebUI and proxies API requests to the P4. It remains transport/UI hosting only; it does not own show decisions.

### Active firmware

| Folder | Role |
|--------|------|
| `firmware/director-esp32-8048s050/` | Director — operator UI, requests, display |
| `firmware/s3-comms-controller/` | Communications Engine — dedicated ESP32-S3 ESP-NOW + UART + SoftAP/WebUI proxy |
| `firmware/stage-engine-p4/` | Show Engine on Stage Controller |
| `firmware/audio-node-esp32-a1s/` | First specialist Audio Node — implemented, hardware test required |
| `firmware/c3-lamp-node/` | C3 Lamp Node — ACTIVE (replaces retired Relay product role) |

---

## 2. Complete firmware classification table

| Folder | Status | Target hardware | Current purpose | Relationship / next action |
|--------|--------|-----------------|-----------------|----------------------------|
| `firmware/director-esp32-8048s050/` | **ACTIVE** | ESP32-S3 800×480 | Canonical Director LVGL + ESP-NOW client | Keep; Director requests/displays only |
| `firmware/s3-comms-controller/` | **ACTIVE** | ESP32-S3 Dev Module | Communications Engine, Audio Node routing, SoftAP/WebUI/API proxy | Keep transport-only; no show decisions |
| `firmware/stage-engine-p4/` | **ACTIVE** | ESP32-P4 Stage Controller | Authoritative Show Engine | Current platform focus includes P4 pixel bench commissioning |
| `firmware/audio-node-esp32-a1s/` | **ACTIVE / HARDWARE TEST REQUIRED** | Ai-Thinker ESP32-A1S / ES8388 | Attraction/programme audio node | Bench commission before claiming hardware-complete |
| `firmware/c3-lamp-node/` | **ACTIVE** | ESP32-C3 Super Mini OLED / lamp | Carbide / theatrical lamp FX specialist | Replaces retired Relay product role; P4 `LampNodeLink` |
| `firmware/mosfet-node-esp32/` | **PLANNED** | TBD ESP32 + MOSFET outputs | Future switched-output / PWM specialist | Comes after C3 Pixel Node; no implementation yet |
| `firmware/relay-node-esp32/` | **LEGACY / RETIRED** | ESP32 + relay module | Historical relay-node prototype | Retain as reference only; not a production Node |
| `firmware/p4-c6-espnow-bridge/` | **UNUSED / RESERVED** | Onboard ESP32-C6 | Historical C6 bridge qualification | C6 hardware remains unused/reserved |
| `firmware/c3-supermini-espnow-bridge/` | **LEGACY / SUPERSEDED** | ESP32-C3 SuperMini | Previous SUE Communications Engine | Retain as reference; not current path |
| `firmware/director-s3/` | **LEGACY** | ESP32-S3 + TFT_eSPI | Earlier UART Director scaffold | Do not extend |
| `firmware/espnow-bridge/` | **LEGACY** | ESP32 family | Early ESP-NOW bridge scaffold | Retain packet ideas only |
| `firmware/touch-probe-8048/` | **DIAGNOSTIC** | 8048 touchscreen | Touch hardware bring-up | Keep as lab tool |
| `firmware/sue-esp32s3-node/` | **INCOMPLETE** | ESP32-S3 | Old multi-function node placeholder | Do not confuse with current specialist-node roadmap |
| `firmware/controller-cyd/` | **ARCHIVE CANDIDATE** | CYD | Legacy UI generation | Archive later |
| `firmware/executor-mega/` | **ARCHIVE CANDIDATE** | Arduino Mega | Legacy executor | Archive later |

---

## 3. Active project boundaries

### Director

**Owns:** operator UI, input handling, display state, ESP-NOW client transport, Director-local diagnostics.

**Must not own:** authoritative show state, production execution, node routing policy, physical completion assumptions.

### Communications Engine

**Owns:** ESP-NOW fabric, UART transport to P4, transport-address discovery, Audio Node packet forwarding, link health, static browser WebUI hosting and P4 API proxying.

**Must not own:** show decisions, timelines, cue state, physical FX execution, or false completion acknowledgements.

### Show Engine — ESP32-P4

**Owns:**

- authoritative show/emergency state;
- production loading and timeline execution;
- safety policy;
- node coordination;
- P4 system/safety audio;
- GPIO24 emergency/signage pixels;
- GPIO23 local segmented Show Pixel Engine;
- storage and local services as implemented.

The GPIO23 engine currently supports direct/bench `PIXEL:` commands and the shared 25-effect vocabulary. Persistent production format v1 still accepts only TEST/LOG cues, so production-file PIXEL cue parsing remains future work.

### Emergency/signage pixels — GPIO24

Safety-owned, not a production lane.

```text
Each sign = 10 pixels
NORMAL: first pixel GREEN, remaining 9 OFF
EMERGENCY: every pixel WHITE
```

Up to 100 configured pixels gives up to ten sign bundles. All groups are prepared in one frame and transmitted together.

### Show pixels — GPIO23

One local theatrical line with independent segments and non-blocking effects. Emergency overrides every segment and forces the full line bright white. Clearing emergency leaves the show line blacked out; old effects do not auto-resume.

Shared FX vocabulary: `protocol/showduino_pixel_fx.h`.

### Audio Node

**Classification:** implemented / hardware test required.

Owns local ES8388 + microSD programme audio, playback lifecycle, fades/duck/inventory, P4 GRANT ownership, and a channel-1 SoftAP WebUI when standalone. It does not own P4 system/emergency audio or show decisions.

### C3 Lamp Node

**Classification:** ACTIVE. Replaces the retired Relay Node product role on the Director fabric. Carbide / theatrical lamp FX via `firmware/c3-lamp-node/`; P4 `LampNodeLink` / Comms `ROUTE:LAMP:`.

### C3 Pixel Node

Follows Lamp. It should reuse the common Showduino 25-effect vocabulary and implement the same hard emergency rule: **all connected pixels bright white**.

### MOSFET Node

Planned after C3 Pixel. Digital on/off and PWM specialist — not a Relay revival. The old relay firmware tree is legacy/reference only.

---

## 4. Pixel electrical baseline

Each P4 pixel data line uses a **470 Ω series resistor** near the controller/logic buffer:

```text
GPIO23 / buffer → 470 Ω → Show Pixel DIN
GPIO24 / buffer → 470 Ω → Emergency/Signage DIN
```

Common ground is mandatory. A 74AHCT125/74HCT125-class 5 V logic buffer is recommended for final/long-cable installations. The resistor is signal conditioning, not level conversion. Use an appropriately sized external 5 V pixel supply and bulk capacitance near the line start.

---

## 5. Current implementation maturity

| Maturity | Current repository scope |
|----------|--------------------------|
| **IMPLEMENTED** | Director → ESP-NOW → S3 Comms → UART → P4; P4 authoritative emergency/runtime; P4 SD TEST/LOG production loading; P4 ES8311 system audio; Audio Node firmware; P4 GPIO24 grouped emergency signage; P4 GPIO23 segmented 25-FX local pixel engine; Comms SoftAP/WebUI/API proxy |
| **HARDWARE TEST REQUIRED** | Audio Node physical board; P4 GPIO23 Show Pixel line; final GPIO24 grouped signage wiring |
| **PARTIAL** | Persistent production assets/cue types beyond TEST/LOG; completion-driven state across every node type; logical target routing; WebUI surfaces for all new pixel controls |
| **PLANNED** | C3 Lantern Node modifications; C3 Pixel Node; MOSFET Node; production AUDIO/PIXEL cue dispatch |
| **PARKED** | DMX work until explicitly reopened |
| **LEGACY** | Relay Node product concept, C3/SUE Comms, onboard-C6 Comms attempt, CYD/Mega generation |

---

## 6. Naming / repository debt

- `firmware/stage-engine-p4/` retains the older folder name even though the role is Show Engine / Stage Controller.
- Older docs may still describe the Relay Node as future/current; when encountered, update them to MOSFET Node or clearly mark historical context.
- Older docs may still say the S3 Comms Controller does not host SoftAP/WebUI; current Comms firmware does.
- Onboard C6 references must remain clearly marked unused/reserved.

No broad archive/move operation is required for the current pixel milestone.
