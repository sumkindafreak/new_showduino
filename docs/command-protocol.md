# Showduino Command Protocol

Canonical documentation for the shared protocol package under `protocol/`.

**Constitution:** Show Engine decides · Communications Engine transports · Director commands and displays · Nodes act.

Current transport topology:

```text
Director ESP32-S3
    → ESP-NOW
Dedicated ESP32-S3 Communications Engine
    → UART
ESP32-P4 Show Engine
```

Browser commands use the S3-hosted Studio/WebUI and are proxied to the P4. ESP-NOW, UART, Wi-Fi and HTTP are transports; application messages describe intent/state.

The first specialist Node is the Audio Node. Current Node roadmap after Audio is C3 Lamp → C3 Pixel → MOSFET. The Relay Node product is **retired** (Lamp occupies that Director slot). DMX is parked/out of scope.

---

## Version policy

Defined in `protocol/showduino_protocol_version.h`.

| Symbol | Role |
|--------|------|
| `SHOWDUINO_PROTOCOL_VERSION_MAJOR` | Breaking wire changes |
| `SHOWDUINO_PROTOCOL_VERSION_MINOR` | Backward-compatible package additions |
| `SHOWDUINO_DESK_WIRE_VERSION` | Value in desk packet `version` field |

Rules:

- major mismatch is rejected;
- reserved fields are zero-initialised;
- packet magic/version/size are validated before payload use;
- the current transport still carries legacy colon-delimited application strings in several places.

---

## ESP-NOW packets

### Desk packet

Director ↔ Communications Engine uses `ShowduinoDeskPacket` / historical alias `ShowduinoEspNowPacket`.

Typical v1 fields:

```text
magic       uint32_t
version     uint16_t
sequence    uint16_t
sentMillis  uint32_t
command     char[96]
```

Expected size is 108 bytes on the current ESP32 toolchain layout.

### Node packet

Communications Engine ↔ specialist Nodes uses `ShowduinoNodePacket`.

```text
nodeType    char[16]
command     char[96]
sequence    uint32_t
```

Expected current size is 116 bytes. Stronger framing/node magic remains future protocol work.

---

## Show control

Live compatibility commands include:

```text
SHOW:START
SHOW:RUN
SHOW:STOP
SHOW:PAUSE
SHOW:RESUME
SHOW:STATE?
STATUS:REQUEST
HEARTBEAT
HELLO
```

The P4 is authoritative. A Director/Studio request does not become authoritative state until the P4 accepts/applies it.

---

## Emergency control

Live commands include:

```text
EMERGENCY:STOP
EMERGENCY:CLEAR
EMERGENCY:CLEAR_CONFIRM
EMERGENCY:CLEAR_CANCEL
```

Emergency state is published as authoritative state, including:

```text
STATE:EMERGENCY:ACTIVE
STATE:EMERGENCY:CLEAR
```

Emergency clear does not automatically restart/resume the show.

### Global pixel emergency law

Every pixel-capable output must implement:

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

On the P4 this applies to both GPIO23 Show Pixels and GPIO24 emergency/signage pixels. The C3 Lamp Node and any later pixel-capable Node must follow the same rule.

Normal FX/segment commands are subordinate to this safety state.

---

## P4 local pixel commands — implemented commissioning/runtime surface

The local P4 GPIO23 Show Pixel Engine is no longer an `UNSUPPORTED:PIXEL` placeholder.

Current direct commands:

```text
PIXEL:STATUS
PIXEL:TEST
PIXEL:TEST:STOP
PIXEL:OFF
PIXEL:BLACKOUT
PIXEL:SOLID:<r>:<g>:<b>
PIXEL:BRIGHTNESS:<0-255>
```

### Segment commands

```text
PIXEL:SEGMENT:<id>:RANGE:<start>:<count>
PIXEL:SEGMENT:<id>:FX:<name>
PIXEL:SEGMENT:<id>:COLOR:<r>:<g>:<b>
PIXEL:SEGMENT:<id>:COLOR2:<r>:<g>:<b>
PIXEL:SEGMENT:<id>:BRIGHTNESS:<0-255>
PIXEL:SEGMENT:<id>:SPEED:<1-100>
PIXEL:SEGMENT:<id>:INTENSITY:<0-100>
PIXEL:SEGMENT:<id>:RANDOMNESS:<0-100>
PIXEL:SEGMENT:<id>:DURATION:<milliseconds>
PIXEL:SEGMENT:<id>:REVERSE:<0|1>
PIXEL:SEGMENT:<id>:START
PIXEL:SEGMENT:<id>:STOP
PIXEL:SEGMENT:<id>:STATUS
```

The current P4 configuration provides up to 16 simultaneous segment slots on the one GPIO23 line.

### Shared 25-FX vocabulary

Canonical enum/names are defined in `protocol/showduino_pixel_fx.h`. P4 GPIO23 and the C3 Pixel Node use the same language.

```text
OFF / BLACKOUT
SOLID
FADE_IN
FADE_OUT
PULSE
BREATHE
FLICKER
CANDLE
FIRE
LIGHTNING
STROBE
RANDOM_STROBE
CHASE
BOUNCE
COMET
WIPE
REVERSE_WIPE
BUILD
SPARKLE
TWINKLE
GLITCH
WARNING / WARNING_RED
PORTAL / PORTAL_GLOW
RAINBOW
CUSTOM_SEQUENCE
```

These commands are **commissioning/runtime control**, not proof that production-format-v1 supports PIXEL timeline cues. Persistent production v1 still accepts TEST/LOG cue types; production PIXEL cue parsing, named segment persistence and logical target resolution remain future integration work.

While emergency is active, normal pixel commands must be rejected/overridden and all pixels remain white.

---

## GPIO24 emergency/signage behavior

GPIO24 is safety-owned; it is not a general show-control lane.

It is organised as 10-pixel emergency-sign groups, up to 100 pixels/10 signs by the current configuration.

Normal frame per sign:

```text
pixel 0 of group = GREEN
pixels 1-9       = OFF
```

Emergency frame per sign:

```text
all 10 pixels = WHITE
```

All sign groups are composed before one frame transmission so they visually change together.

---

## Audio commands

### P4 system/safety audio

P4 onboard ES8311 is system/safety audio only.

```text
AUDIO:STATUS
AUDIO:TEST:BOOT
AUDIO:TEST:EMERGENCY
AUDIO:TEST:BEEP
AUDIO:TEST:TONE
AUDIO:TEST:ERROR
AUDIO:TEST:ACCEPTED
AUDIO:TEST:COMPLETE
AUDIO:STOP
```

Attraction/programme playback is not routed to the P4 speaker.

### Specialist Audio Node

Programme audio uses `AUDIO:NODE:*`, including the currently implemented playback/control family such as:

```text
AUDIO:NODE:PLAY:<path>
AUDIO:NODE:LOOP:<path>
AUDIO:NODE:STOP
AUDIO:NODE:PAUSE
AUDIO:NODE:RESUME
AUDIO:NODE:VOLUME:<0-100>
AUDIO:NODE:DUCK
AUDIO:NODE:UNDUCK
AUDIO:NODE:INVENTORY:...
AUDIO:NODE:SOUND:...
AUDIO:NODE:OWN:GRANT
```

P4 ownership GRANT is required before the Audio Node accepts theatrical commands from the show path. Hearing ESP-NOW is not ownership. See [`standalone-node-architecture.md`](standalone-node-architecture.md).

Transport envelope:

```text
P4 → ROUTE:AUDIO:<seq>:<cmd> → S3 → Audio Node
Audio Node → S3 → NODE:AUDIO:<report> → P4
```

Audio Node acceptance/started/completed/failed reports are distinct lifecycle events. Transmit/route is not completion.

---

## Browser / Studio command path

Canonical static frontend source:

```text
web/showduino-studio/
```

Runtime host:

```text
S3 Communications Engine PROGMEM
```

Authoritative API/state:

```text
P4 Show Engine
```

So a browser pixel request follows:

```text
Studio control
→ S3 HTTP/API proxy
→ UART/Web tunnel
→ P4 command whitelist/validation
→ P4 Show Pixel Engine
```

The S3 must not execute the pixel effect itself.

After frontend source changes the generated S3 asset bundle must be regenerated before flashing the S3:

```text
python tools/embed-webui/embed_webui.py
```

Generated target:

```text
firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h
```

---

## Request lifecycle model

```text
REQUEST_RECEIVED
  → REQUEST_ACCEPTED | REQUEST_REJECTED
  → ACTION_STARTED
  → ACTION_COMPLETED | ACTION_FAILED
  → STATE_CHANGED
```

**Acceptance does not imply completion.**

For P4-local pixels, the P4 owns the physical engine directly. For remote Nodes, completion must come from the Node/result lifecycle rather than an optimistic transport ACK.

---

## Logical IDs

Application-level addressing is intended to use logical Showduino device IDs. Transport may resolve peers through MAC addresses internally, but MAC addresses must not become the meaning of a show cue.

`ShowduinoRequestContext` and message catalog structures exist as foundations for future structured protocol work. Full logical-ID routing is not yet complete end to end.

---

## Legacy Relay compatibility

Relay protocol strings remain in the repository for compatibility/history:

```text
RELAY:<channel>:ON
RELAY:<channel>:OFF
RELAY:<channel>:TOGGLE    deprecated
```

The Relay Node is **not** the current product roadmap. The future MOSFET Node supersedes it. Do not build new application behavior around Relay-specific assumptions.

---

## Unsupported / parked behavior

`UNSUPPORTED:*` / `NODE_UNAVAILABLE:*` remain valid responses when a requested capability genuinely does not exist.

Do **not** document all `PIXEL:*` as unsupported: P4-local GPIO23 segment commands are implemented.

Distributed C3 Pixel Node routing is implemented:

```text
PIXEL:NODE:<LED-01>:COUNT:<n>
PIXEL:NODE:<LED-01>:INIT
PIXEL:NODE:<LED-01>:STATUS
PIXEL:NODE:<LED-01>:LOCATE
PIXEL:NODE:<LED-01>:SEGMENT:<slot>:...
ROUTE:PIXEL:<LED-01>:<seq>:<inner PIXEL command>
```

P4 addresses logical IDs. Comms holds up to eight Pixel Node ESP-NOW peers. A missing node returns `REJECTED:PIXEL:OFFLINE:<id>` / `NODE_UNAVAILABLE:PIXEL:<id>` and does **not** freeze the timeline.

GPIO24 remains the dedicated emergency-signage line and is not part of this Pixel Node model.

DMX production work is explicitly parked/out of scope. E1.31 remains an isolated P4 test/observation foundation and must not silently become a production pixel/lighting input.

---

## Validation

`protocol/showduino_validation.h` contains shared validation helpers. New command surfaces must validate ranges and reject malformed/unsupported requests rather than returning false success.

P4 pixel commands are further validated by the P4 pixel engine, including segment bounds/parameter ranges and emergency gating.

---

## Planned later

- persistent production `AUDIO` and `PIXEL` cue schemas;
- named pixel-segment persistence and logical binding;
- Studio reusable FX preset persistence;
- end-to-end logical device-ID routing;
- generic completion-driven state/fault handling for every specialist Node;
- stronger node framing / future structured binary protocol;
- C3 Pixel Node protocol (this tree; hardware test required);
- MOSFET Node protocols as that milestone begins.

DMX is deliberately excluded from this roadmap until explicitly unparked.

---

## Shared headers / inclusion

Single source of truth is repo `protocol/`. Do not fork protocol headers into each firmware tree.

Important pixel header:

```cpp
#include "../../../protocol/showduino_pixel_fx.h"
```

Paths vary with sketch depth; keep the repository `protocol/` copy canonical.

---

## Related files

- `protocol/README.md`
- `protocol/showduino_pixel_fx.h`
- `docs/state-synchronisation.md`
- `docs/architecture.md`
- `docs/audio-node.md`
- `docs/audio-pixel-engine.md`
- `docs/studio-pixel-authoring.md`
