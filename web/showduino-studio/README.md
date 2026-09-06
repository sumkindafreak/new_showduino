# Showduino Studio WebUI

Browser control desk for Showduino Studio.

## Source vs runtime

| Copy | Location |
|------|----------|
| **Source (version control)** | `web/showduino-studio/` |
| **Runtime (P4 SD card)** | `D:\showduino\webui\` → ESP32 `/showduino/webui/` |

Deploy to the inserted P4 SD card (does not format the card):

```powershell
powershell -File tools/deploy-webui-to-sd.ps1
```

## Architecture

**Current product path (this hardware generation):**

```text
Browser  →  P4-hosted WebUI / API
Director → ESP-NOW → ESP32-S3 Comms Controller → UART → ESP32-P4
                                                   │
                                                   ├─ local system/safety services
                                                   └─ routes specialist node commands back through S3
```

The P4 serves static files from `/showduino/webui/` and exposes JSON APIs. The production frontend is not embedded in firmware; a tiny HTML fallback is shown only if the P4 SD origin is unreachable.

The browser and Director are operator surfaces. The P4 Show Engine remains authoritative for show state, safety, cue execution and node state. The S3 Communications Engine transports messages and does not make show decisions.

## Audio model shown by the WebUI

Showduino now treats audio as two separate roles:

```text
SYSTEM AUDIO
P4 local audio
├─ boot / ready
├─ operator notifications
├─ warnings / faults
└─ emergency/system audio

SHOW AUDIO
Specialist Audio Node
├─ music
├─ ambience
├─ dialogue
├─ scare SFX
└─ production playback
```

The first specialist Audio Node baseline is the Ai-Thinker ESP32-Audio-Kit / ESP32-A1S with ES8388 codec and local microSD.

The WebUI must never imply that PCM audio is streamed over ESP-NOW. The P4 sends intent and asset references; the Audio Node plays assets locally and reports lifecycle/state back to the P4.

Until the runtime routing is implemented, the WebUI must label Audio Node behaviour as pending/unavailable rather than manufacturing successful playback state.

## Pixel model shown by the WebUI

Emergency pixels and show pixels are different systems:

```text
P4 GPIO24
└─ Emergency Pixels
   └─ safety only; never an editable show-output lane

Show Pixel Engine
├─ P4 local show-pixel outputs
└─ Remote Pixel Nodes over ESP-NOW
```

Local P4 show-pixel outputs and remote Pixel Nodes share the same conceptual engine:

```text
output
└─ segments
   ├─ range/start/count
   ├─ effect
   ├─ RGBW colour
   ├─ brightness
   ├─ speed/direction
   └─ duration
```

Multiple segments on one physical strip may run independent effects simultaneously. Effects are rendered locally by the output device; the P4 sends intent rather than pixel frames.

The WebUI must preserve the emergency/show separation and must not expose the GPIO24 emergency strip as an ordinary production fixture.

## Specialist node state rule

Audio and Pixel Nodes follow the same lifecycle philosophy:

```text
request
→ accepted | rejected
→ started
→ completed | failed
→ authoritative P4 state
```

A command being forwarded is not proof that the physical action completed. WebUI controls and indicators must show P4-confirmed state.

On `EMERGENCY:STOP`, show Audio and Pixel Nodes stop/blackout, clear active work and lock show commands. `EMERGENCY:CLEAR` returns them to safe idle; previous playback/effects do not auto-resume.

## Connect

Current generation: flash **P4**, **S3 Comms Controller**, **Director**. Wire UART (S3 GPIO17→P4 GPIO4, S3 GPIO18←P4 GPIO5). Copy the S3 boot MAC into Director `SHOWDUINO_COMMS_MAC_*`.

There is **no external DS3231** on the current P4-module stack. Time follows the P4 internal RTC.

### Previous generation (legacy C3 / SUE)

The external ESP32-C3 SuperMini hosted SoftAP `Showduino-Studio` and proxied `/api/*` over UART. That path remains documented for the legacy C3 firmware only:

```text
Browser → Wi-Fi → ESP32-C3 SUE (HTTP radio, WebSocket :81)
                    └─ UART tunnel → ESP32-P4
```

## REST API

Current WebUI code consumes P4 JSON APIs including system, logs, devices, capabilities and time data. Audio/Pixel node state endpoints or fields must be added as the corresponding P4 runtime state becomes authoritative; the frontend must use safe pending/unavailable fallbacks until then.

## Regenerate embedded assets

```powershell
powershell -File tools/embed-web-studio-assets.ps1
```
