# Showduino Repository Status

Classification of firmware projects after the **25 August 2026 lean Stage Controller hardware decision**.

No firmware folders are deleted by this document. Compatibility code stays available until replacement hardware paths are qualified.

Related:

- [Constitution](constitution.md)
- [Architecture](architecture.md)
- [Hardware baseline — 25 Aug 2026](hardware-baseline-2026-08-25.md)
- [Final hardware architecture](final-hardware-architecture.md)
- [Hardware pin/resource map](hardware-pinout.md)

## 1. Canonical hardware target

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine — onboard ESP32-C6
    → integrated P4/C6 transport
Show Engine — ESP32-P4
```

```text
Showduino Node
    → ESP-NOW
Onboard ESP32-C6
    → Show Engine ESP32-P4
```

Browser target: Wi-Fi → onboard C6 → P4 Show Engine services.

The separate C3/SUE board and separate DS3231 are no longer part of the physical Stage Controller baseline.

## 2. Status values

```text
ACTIVE          canonical firmware currently developed for its role
TARGET          selected replacement/target, still needs qualification
COMPATIBILITY   previous known-good path retained for migration/rollback
LEGACY          earlier architecture; reference only
EXPERIMENTAL    prototype, not a selected product path
DIAGNOSTIC      bring-up/probe utility
INCOMPLETE      stub/non-operational placeholder
ARCHIVE CANDIDATE suitable for later archive move
```

## 3. Firmware classification

| Folder | Status | Target hardware | Purpose | Action |
|--------|--------|-----------------|---------|--------|
| `firmware/director-esp32-8048s050/` | **ACTIVE** | ESP32-S3 800×480 Director | Canonical operator UI / requests / display | Keep; repoint pairing to onboard C6 as migration lands |
| `firmware/stage-engine-p4/` | **ACTIVE** | Waveshare ESP32-P4 Stage Controller | Canonical Show Engine | Keep; integrate RTC, onboard audio and onboard-C6 transport |
| `firmware/p4-c6-espnow-bridge/` | **TARGET** | Stage Controller onboard ESP32-C6 | Communications Engine replacement / bring-up | Develop to parity; do not ship current placeholder transport as final |
| `firmware/c3-supermini-espnow-bridge/` | **COMPATIBILITY** | External ESP32-C3 SuperMini | Previous working Communications Engine | Keep as rollback/reference until C6 parity; no longer required in final enclosure |
| `firmware/relay-node-esp32/` | **ACTIVE** | ESP32 relay node | Canonical remote relay actuator | Keep |
| `firmware/director-s3/` | **LEGACY** | Older ESP32-S3 | Earlier UART Director scaffold | Reference only |
| `firmware/espnow-bridge/` | **LEGACY** | ESP32 variants | Earlier bridge scaffold | Reference only |
| `firmware/touch-probe-8048/` | **DIAGNOSTIC** | Director hardware | Touch bring-up | Keep as tool |
| `firmware/sue-esp32s3-node/` | **INCOMPLETE** | Historical separate SUE concept | Multi-function node placeholder | Do not treat as Stage Controller requirement |
| `firmware/controller-cyd/` | **ARCHIVE CANDIDATE** | CYD 2.8" | Pre-P4 Director stack | Archive later |
| `firmware/executor-mega/` | **ARCHIVE CANDIDATE** | Arduino Mega | Pre-P4 executor | Archive later |

## 4. Current migration truth

### Hardware

Canonical now:

```text
P4 board + onboard C6 + P4 RTC/VBAT + onboard system audio + external PCM show audio
```

### Communications firmware

The last fully exercised path in the repository used:

```text
Director → external C3 → UART → P4
```

That path is now **compatibility**, not the final physical design.

The onboard C6 target must reach parity before the external-C3 code can be archived.

### Time firmware

The old C3 tree contains DS3231-specific services. They remain compatibility code only.

The final time owner is the P4 Show Engine using the P4 RTC/system time. RTC backup retention must be tested before it is marked qualified.

### Audio firmware

The final Stage Controller has two local audio roles:

```text
onboard ES8311 + amplifier → Showduino/system sounds
external PCM5102A         → show/programme audio
```

I2S arbitration still needs implementation/qualification.

## 5. Project boundaries

### Director

Owns:

- Operator UI
- Input handling
- Display state
- ESP-NOW client transport
- Director-local assets/diagnostics

Must not own:

- Show truth
- Timeline execution
- Global safety policy
- Node completion assumptions

### Communications Engine — onboard C6 target

Owns:

- ESP-NOW fabric
- Wi-Fi transport
- Link health
- Transport framing/address resolution
- P4-side transport

Must not own:

- Show decisions
- Timeline/cue state
- Safety authority
- Physical-effect completion assumptions

### Show Engine — P4

Owns:

- Authoritative state
- Timeline/cues
- Project/runtime storage
- Safety/emergency policy
- Node coordination
- Local pixels/audio/IO as implemented
- Web services
- System time

### Relay Node

Owns:

- Relay GPIO
- Local fail-safe enforcement
- Actual relay state reporting

## 6. Onboard C6 acceptance gate

Do not promote `firmware/p4-c6-espnow-bridge/` from **TARGET** to **ACTIVE** until it demonstrates:

1. Director → C6 → P4 command delivery.
2. P4 → C6 → Director ACK/state delivery.
3. Node command forwarding and node replies.
4. Emergency activate/clear traffic.
5. Wi-Fi/WebUI transport required by the product.
6. Stable link recovery.
7. No false-success responses.
8. A documented C6 flashing and factory-firmware recovery path.

## 7. Archive plan

Do not delete compatibility code just to make the tree look tidy.

After C6 parity and the first complete scenario are proven, candidates can move under:

```text
archive/
├── legacy-directors/
├── legacy-executors/
├── compatibility-bridges/
├── diagnostic-sketches/
└── incomplete-prototypes/
```

The external C3 bridge should only move after the onboard C6 has passed the acceptance gate above.

## 8. Known naming/implementation debt

| Debt | Location | Resolution |
|------|----------|------------|
| Folder `stage-engine-p4` | `firmware/stage-engine-p4/` | Rename later to Show Engine wording |
| Macro `SHOWDUINO_P4_C6_MAC_*` | Director `BoardConfig.h` | Historical name; values should become onboard C6 peer MAC after migration |
| Old external-C3 UART assumptions | P4 and C6 bridge code | Keep only as compatibility until SDIO/internal transport lands |
| DS3231 service/UI labels | compatibility code / old Studio path | Replace with P4 time service as migration lands |
| `SUE` as physical board | older docs/code | Treat as historical role name, not required hardware |

## 9. Status summary

The **hardware architecture is now simpler than the current firmware tree**. That is intentional.

The repository must preserve working rollback code while making the target unambiguous: the Stage Controller is a P4 board using its onboard C6, RTC and system-audio hardware, plus the external PCM5102A for show audio.
