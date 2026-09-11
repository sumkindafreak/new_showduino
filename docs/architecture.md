# Showduino Architecture

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

This document describes the **current Showduino v1 architecture**. Historical C3/SUE, onboard-C6, CYD/Mega and relay-era code may remain in the repository, but it must not be mistaken for the current product path.

## Naming

| Term | Meaning |
|------|---------|
| **Show Engine** | Authoritative runtime and safety role |
| **Stage Controller** | Physical ESP32-P4 product running the Show Engine |
| **Communications Engine** | Dedicated ESP32-S3 transport/WebUI front door |
| **Director** | ESP32-S3 touchscreen operator desk |
| **Node** | Specialist ESP32 device that performs a commanded physical/media function |
| ~~Stage Engine~~ | Retired role name; retained only in legacy folder/sketch names |

The current P4 firmware path remains `firmware/stage-engine-p4/` for compatibility.

---

## Non-negotiable ownership

### Show Engine — ESP32-P4

The Show Engine is the single source of truth for:

- show/runtime state;
- timeline and cue scheduling;
- emergency/safety policy;
- P4 SD production/configuration storage;
- authoritative node coordination and result state;
- P4 Web API/state origin;
- P4-local system/safety audio;
- P4-local pixel engines and other local outputs as implemented.

It does **not** own the ESP-NOW radio fabric and does not depend on a browser or Director to keep a running show alive.

### Communications Engine — dedicated ESP32-S3

The Communications Engine owns:

- ESP-NOW transport for Director and specialist Nodes;
- UART transport to the P4;
- transport-side peer discovery/address resolution;
- the Showduino Wi-Fi SoftAP;
- the static Showduino Studio/WebUI hosted from generated PROGMEM;
- HTTP/API proxying between browser clients and the authoritative P4 API.

It must **not** run timelines, decide show state, invent output completion, or become the source of truth for a production.

### Director — ESP32-S3 touchscreen

The Director is an operator client. It issues requests and displays authoritative state returned by the P4. It is not the browser WebUI host and is not required once a show is running.

### Specialist Nodes

Nodes perform specialist work locally and report what actually happened. Application-level addressing is intended to use logical Showduino device IDs; MAC addresses are transport details.

---

## Canonical live paths

### Director

```text
Director ESP32-S3
    → ESP-NOW
Dedicated ESP32-S3 Communications Engine
    → UART 115200 8N1
ESP32-P4 Show Engine
```

### Specialist Nodes

```text
Specialist ESP32 Node
    ↔ ESP-NOW
Dedicated ESP32-S3 Communications Engine
    ↔ UART
ESP32-P4 Show Engine
```

### Browser / Showduino Studio

```text
Phone / Tablet / Laptop
    → Wi-Fi SoftAP on Communications S3
    → Showduino Studio static frontend from S3 PROGMEM
    → S3 API proxy / UART tunnel
    → P4 authoritative Web API
```

The source for the browser UI is `web/showduino-studio/`. The generated S3 bundle is `firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h` and must be regenerated after frontend changes.

A running show must not depend on the Director, browser, Wi-Fi client association, or internet.

### Cross-component synchronisation

Every feature is checked against Director, Comms, P4, WebUI/Studio, protocol, and applicable nodes. See `docs/constitution.md` Article XIV, `docs/network-gateway.md`, and `docs/v1-feature-matrix.md`.

### Optional P4 Ethernet

P4 Ethernet remains optional management/show-network infrastructure. The E1.31 receiver is an isolated **observation/test foundation only**. DMX/E1.31 production control is parked and out of scope until explicitly revisited.

---

## Current active product stack

```text
firmware/director-esp32-8048s050/      Director
firmware/s3-comms-controller/          Communications Engine + S3-hosted Studio
firmware/stage-engine-p4/              Show Engine / Stage Controller
firmware/audio-node-esp32-a1s/         first specialist Audio Node
```

The onboard ESP32-C6 on the Waveshare P4 module is **unused/reserved hardware**. It is not the Communications Engine.

The previous ESP32-C3 SuperMini/SUE Communications Engine is legacy/superseded.

---

## Current specialist-node roadmap

Development order after the current P4 pixel work is:

1. **Audio Node** — implemented firmware; hardware commissioning required.
2. **C3 Lamp Node** — second specialist; replaces the retired Relay Node product role (Director fabric slot).
3. **C3 Pixel Node** — firmware implemented; remote GPIO23-equivalent Show Pixel Line. Hardware test required.
4. **MOSFET Node** — digital/PWM specialist (not a Relay revival).

`firmware/relay-node-esp32/` remains historical/experimental source only. It must not be presented as a production Node.

DMX remains parked/out of scope.

---

## P4 local pixel architecture

The P4 owns exactly two local NeoPixel roles in this generation:

```text
GPIO23  Main Show Pixel Line
GPIO24  Emergency/signage Pixel Line
```

Additional theatrical pixel lines belong on specialist Pixel Nodes rather than consuming more P4 GPIOs.

### GPIO23 — Main Show Pixel Line

GPIO23 is the single local theatrical pixel line. Its engine is implemented and requires hardware commissioning.

The core authoring/runtime unit is a **segment**. A segment is a contiguous region of the physical strip with its own FX state. Multiple segments on one line can run different non-blocking effects simultaneously.

Example:

```text
pixels 0-7    LIGHTNING
pixels 8-10   SOLID BLUE
pixels 11-29  FIRE
pixels 30-49  PULSE RED
pixels 50-79  CANDLE / warm flicker
```

The shared FX vocabulary lives in `protocol/showduino_pixel_fx.h` so the P4 GPIO23 line and C3 Pixel Nodes use the same Showduino effect names and parameters.

Current FX vocabulary contains 25 entries:

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

Typical segment parameters are range, primary/secondary colour, brightness, speed, intensity, randomness, reverse/direction and duration.

Direct `PIXEL:*` commands are currently a commissioning/runtime-control surface. Persistent production format v1 still accepts TEST/LOG cues only; production `PIXEL` cue parsing, named segment persistence and logical target binding remain separate future integration work.

### GPIO24 — Emergency/signage line

GPIO24 is **safety-owned**, not an ordinary Studio production lane.

The line supports up to 100 pixels arranged as fixed 10-pixel emergency-sign groups. With 100 pixels that is up to 10 signs.

Normal state for each group:

```text
[first pixel GREEN] [remaining 9 pixels OFF]
```

So a 30-pixel/three-sign line appears logically as:

```text
G.........G.........G.........
```

Emergency state:

```text
WWWWWWWWWWWWWWWWWWWWWWWWWWWWWW...
```

All pixels are prepared in the frame buffer first and transmitted as one frame so every sign changes together rather than visibly cascading sign-by-sign.

---

## Global emergency pixel law

**Emergency = ALL PIXELS BRIGHT WHITE.**

This is a Showduino-wide safety rule, not a show effect.

When the emergency latch becomes active:

- GPIO24 emergency/signage pixels become bright white;
- GPIO23 main Show Pixel Line becomes bright white;
- C3 Pixel Nodes become bright white on the entire configured line;
- Lantern pixels must become bright white where applicable;
- every future pixel-capable Showduino output must implement the same override.

No active segment, FX, colour, production cue, Studio control or node-local animation may override emergency white.

On emergency clear:

- GPIO24 returns to its normal green-locator signage pattern;
- GPIO23 returns to a safe non-running state rather than automatically resuming the interrupted effects;
- remote pixel-capable Nodes must likewise remain safe until explicitly commanded again.

Emergency clear does not automatically resume the show.

---

## Pixel wiring standard

Each P4 pixel data output uses a **470 Ω series data resistor**.

```text
P4 GPIO23 → 5 V-capable logic buffer → 470 Ω → Main Pixel DIN
P4 GPIO24 → 5 V-capable logic buffer → 470 Ω → Emergency Pixel DIN
```

For short bench tests a direct 3.3 V GPIO → 470 Ω → DIN connection may work, but the production wiring standard uses a 5 V-capable logic buffer such as a 74AHCT125/74HCT125-class device. The resistor is for signal integrity; it is not a logic-level converter.

Pixel power is supplied from a suitable external 5 V supply with P4/pixel grounds common. A substantial pixel line should also have approximately 1000 µF bulk capacitance across 5 V and GND near the start of the line. Power injection/current sizing must be based on the installed pixel count and worst-case emergency-white load.

---

## Showduino Studio and segmented authoring

Segments are the preferred Studio authoring primitive.

The browser UI should present users with named lighting areas/segments rather than forcing them to manipulate individual LED addresses for every cue. A future production can therefore refer to logical targets such as `doorway`, `altar`, or `corridor_left`, while the P4 resolves those names to physical pixel ranges.

The current Outputs page provides GPIO23 **and** C3 Pixel Node commissioning/editor surfaces using the shared FX vocabulary. Studio V4 production authoring now uses the same pixel-output picker for P4 GPIO23 and discovered/saved Pixel Nodes. Persistent named segment libraries and reusable FX presets remain planned schema work.

See [`studio-pixel-authoring.md`](studio-pixel-authoring.md).

---

## Audio architecture

P4 onboard ES8311 audio is **Showduino system/safety audio only**. Attraction/programme audio belongs to the ESP32-A1S Audio Node.

```text
P4 ES8311
    → boot / accepted / error / emergency / system sounds

Audio Node
    → ambience / music / dialogue / scare SFX / programme audio
```

The P4 does not use its system speaker as fallback programme audio.

---

## Safety/runtime behaviour

Emergency stop overrides entertainment commands. When the P4 accepts an emergency activation it must keep authoritative emergency state latched, interrupt/pause show execution according to runtime policy, command relevant nodes toward their safe state, force all pixel-capable outputs white, and keep status reporting available.

A lost Director, browser or Wi-Fi client does not itself stop a running show.

Command acceptance and physical completion remain separate lifecycle concepts. The P4 must not claim that a remote Node completed an action merely because the command was transmitted.

---

## Storage

The P4 SD card is the persistent Show Engine store under `/showduino/`. Versioned production discovery and transactional TEST/LOG timeline loading are implemented. Broader asset/project authoring and physical production cue types are still being extended.

The S3 PROGMEM WebUI is static frontend code, not authoritative show storage.

---

## Current maturity

### Implemented / active foundation

- Director → ESP-NOW → dedicated S3 Comms → UART → P4.
- S3 SoftAP + PROGMEM Showduino Studio frontend + P4 API proxy.
- P4 authoritative runtime/emergency state.
- RAM timeline with load/order/start/pause/resume/stop/emergency interruption.
- P4 SD production discovery and transactional TEST/LOG loading.
- P4 onboard ES8311 system/safety audio.
- specialist Audio Node firmware and P4 routing/lifecycle tracking; hardware commissioning still required.
- GPIO24 grouped emergency/signage pixel behavior.
- GPIO23 segmented non-blocking Show Pixel Engine with shared 25-FX vocabulary; hardware commissioning still required.
- C3 Pixel Node firmware, P4/Comms/Director/Studio integration, SHDO `pixel-node` compile; hardware commissioning still required.
- S3-hosted Studio source includes GPIO23 and Pixel Node commissioning/editor surfaces.

### Still to complete

- browser WebUI bundle regeneration after source changes before the updated UI is present in an S3 firmware image;
- persistent production PIXEL/AUDIO cue types and timeline dispatch polish;
- named logical pixel-segment persistence and binding;
- completion-driven state/fault handling for all future Nodes;
- Audio Node, Lamp Node, Pixel Node and P4 pixel physical bench commissioning;
- MOSFET Node milestone.

### Explicitly parked

- DMX production work.
- E1.31 production mapping/control.
- onboard P4 C6 application use.

---

## Related documents

- [`constitution.md`](constitution.md)
- [`final-hardware-architecture.md`](final-hardware-architecture.md)
- [`hardware-pinout.md`](hardware-pinout.md)
- [`audio-pixel-engine.md`](audio-pixel-engine.md)
- [`studio-pixel-authoring.md`](studio-pixel-authoring.md)
- [`production-storage.md`](production-storage.md)
- [`repository-status.md`](repository-status.md)
- [`command-protocol.md`](command-protocol.md)
