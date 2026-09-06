# Showduino Audio and Pixel Engine

Audio and pixels are core Showduino outputs. The architecture now separates local P4 system/safety services from theatrical show outputs while keeping the P4 Show Engine authoritative for both.

## 1. Audio architecture

### A. P4 local audio — Showduino/system role

The P4 keeps a local audio path for sounds that belong to Showduino itself:

```text
boot
ready
production loaded
show armed
operator notification
link warning
fault
emergency/system acknowledgement
```

This path must remain local to the Stage Controller and must not depend on the Director, browser, Wi-Fi, ESP-NOW or a specialist Audio Node.

The repository currently contains P4-local audio hardware definitions for both the onboard ES8311/NS4150B path and the external PCM5102A path. During the migration to specialist show-audio nodes, existing P4 audio code may still use the current PCM5102A implementation. Do not remove or break working P4 emergency/system audio while the new node path is being commissioned.

Target role rule:

> P4 local audio is for Showduino/system/safety sounds. Theatrical programme audio belongs to Audio Nodes.

### B. Show/programme audio — specialist Audio Node

The first dedicated Showduino Audio Node baseline is:

```text
Ai-Thinker ESP32-Audio-Kit
└─ ESP32-A1S
   ├─ ES8388 codec
   ├─ local microSD
   ├─ speaker / line audio outputs
   └─ onboard buttons for commissioning/maintenance
```

Use the Audio Node for:

```text
music
voice / dialogue
ambience
scare SFX
stingers
timeline audio
production-specific playback
```

Audio data is not streamed over ESP-NOW. Assets live on the node's microSD; the P4 sends commands and asset references.

## 2. Audio ownership and routing

```text
Director / WebUI
      │ request
      ▼
P4 Show Engine (authority)
      │
      ├─ local system/safety audio
      │
      └─ show-audio request
             │
             ▼
       S3 Comms Controller
             │ ESP-NOW
             ▼
         Audio Node
```

The S3 Communications Engine transports node traffic only. It must not select tracks, advance shows, invent success or make safety decisions.

## 3. Audio command model

Keep P4-local and remote-node intent visibly separate.

Examples:

```text
AUDIO:LOCAL:PLAY
AUDIO:LOCAL:STOP
AUDIO:LOCAL:VOLUME:60

AUDIO:NODE:PLAY:effects/thunder.wav
AUDIO:NODE:LOOP:ambience/chamber.wav
AUDIO:NODE:STOP
AUDIO:NODE:PAUSE
AUDIO:NODE:RESUME
AUDIO:NODE:VOLUME:80
```

The shared legacy node envelope remains `ShowduinoNodePacket` with `nodeType = "AUDIO"` while protocol v1 colon-text is active.

## 4. Audio lifecycle and state

Forwarding a command is not proof of completion.

Preferred lifecycle:

```text
request
→ accepted | rejected
→ started
→ completed | failed
→ authoritative P4 state
```

Example replies:

```text
AUDIO:ACCEPTED:142
AUDIO:STARTED:142:thunder.wav
AUDIO:COMPLETED:142:thunder.wav
AUDIO:FAILED:142:FILE_NOT_FOUND
```

The P4 mirrors the confirmed node state to Director/WebUI.

Useful node states:

```text
UNKNOWN
OFFLINE
IDLE
PLAYING
PAUSED
FAULT
EMERGENCY
```

## 5. Audio Node local buttons

The common Ai-Thinker board buttons are part of commissioning/maintenance, not show authority.

Target uses:

```text
PLAY   → local test playback / stop while in maintenance
VOL+   → local master volume up
VOL-   → local master volume down
MODE   → diagnostic/test selection
REC    → diagnostics / future input test
SET    → leave disabled initially where it conflicts with SD on common revisions
```

During a running show, maintenance actions that could trigger arbitrary playback should be disabled. Emergency state overrides all local playback controls.

## 6. Audio emergency policy

On `EMERGENCY:STOP` the Audio Node must:

```text
stop show playback immediately
close/clear active playback
enter emergency-locked state
reject show-audio commands
```

`EMERGENCY:CLEAR` returns the node to safe idle. Previous show audio must not auto-resume.

P4 local emergency/system audio remains independent of the remote node so loss of the node or ESP-NOW path cannot remove the Stage Controller's local safety feedback.

## 7. Pixel architecture

Pixels are part of the show language, not decoration.

There are two normal show-pixel execution targets:

```text
P4 Local Show Pixels
Remote Pixel Nodes
```

Both should use the same conceptual Pixel Engine so productions do not need different cue formats for local and remote strips.

The P4 decides what effect should run. The output device renders it locally.

Do not stream individual pixel frames over ESP-NOW for normal effect playback.

## 8. Emergency pixel line

The existing P4 emergency strip remains independent from normal show pixel assignments.

```text
DATA GPIO24
```

It belongs to the P4 emergency subsystem only.

Normal Showduino Studio timelines and WebUI fixture editors must never expose this line as an ordinary editable show output.

## 9. Shared Pixel Engine model

A physical output contains one or more independently rendered segments.

Example:

```text
Output 1
├─ pixels 0-7   → LIGHTNING
├─ pixels 8-10  → SOLID BLUE
├─ pixels 11-29 → CANDLE
└─ pixels 30-59 → RED PULSE
```

All segments may run simultaneously on one strip.

Core data model:

```text
output
segment id/name
start
count
effect
RGBW colour
brightness
speed
direction
duration
```

Use an RGBW-capable internal colour model even when the installed strip is RGB-only.

## 10. Pixel effect vocabulary

Initial effects:

```text
OFF
SOLID
FADE
PULSE
FLICKER
CANDLE
FIRE
STROBE
LIGHTNING
CHASE
SPARKLE
BREATH
COLOR_CYCLE
BLACKOUT
```

Effects run non-blocking and independently per segment.

## 11. Pixel brightness and power model

Brightness is hierarchical:

```text
node master
× output brightness
× segment brightness
```

Remote Pixel Nodes should drive strip data through a suitable 5 V logic-level buffer/level shifter where required. Pixel power comes from a correctly sized external supply with common ground, fusing/power injection as appropriate, and firmware current/brightness limiting.

## 12. Pixel routing

Remote path:

```text
P4
→ ROUTE:PIXEL:...
→ S3 Comms Controller
→ ShowduinoNodePacket (nodeType = "PIXEL")
→ Pixel Node
```

Local P4 show pixels execute the same intent directly in the P4's local Pixel Engine without a radio hop.

## 13. Pixel lifecycle and emergency behaviour

Remote Pixel Nodes follow the same completion philosophy as Audio Nodes:

```text
request
→ accepted | rejected
→ started
→ completed | failed
→ authoritative P4 state
```

On `EMERGENCY:STOP` normal show pixels:

```text
blackout
clear active effects
lock show-pixel commands
```

`EMERGENCY:CLEAR` returns them to safe idle/black. Effects do not auto-resume.

The P4 emergency GPIO24 line performs its own local emergency behaviour independently.

## 14. Show-file direction

Example future Audio cue:

```json
{
  "time_ms": 0,
  "type": "AUDIO",
  "target": "audio01",
  "action": "play",
  "file": "ambience/chamber.wav",
  "volume": 85
}
```

Example future Pixel cue:

```json
{
  "time_ms": 4000,
  "type": "PIXEL",
  "target": "pixel01",
  "output": 1,
  "segment": "window",
  "effect": "LIGHTNING",
  "color": [220, 235, 255, 0],
  "brightness": 200,
  "speed": 40,
  "duration_ms": 3000
}
```

Logical device-ID routing is still a future protocol milestone; examples here describe the target production model, not a claim that device IDs are already carried on the v1 wire format.

## 15. WebUI / Director requirements

The operator surfaces must present these as distinct roles:

```text
Audio
├─ System Audio — P4 local
└─ Show Audio — Audio Node(s)

Pixels
├─ Emergency Pixels — P4 local, safety-only
├─ P4 Local Show Pixels
└─ Remote Pixel Node(s)
```

The UI must display confirmed P4 state rather than assuming that a forwarded command succeeded.

Until runtime support is live, controls/status must say pending, unavailable or unsupported honestly.

## 16. Implementation order

Audio:

1. Audio Node local board diagnostics.
2. ES8388 + microSD + WAV playback.
3. Local buttons/volume/maintenance mode.
4. ESP-NOW node packet receive/reply.
5. S3 `ROUTE:AUDIO` forwarding.
6. P4 authoritative Audio Node state.
7. Director/WebUI live Audio Node state and controls.
8. Timeline / `.shdo` first-class audio cues.

Pixels:

1. Shared non-blocking segment/effect renderer.
2. Multiple simultaneous segments on one physical strip.
3. P4 local show-pixel target.
4. Remote ESP32 Pixel Node target.
5. S3 `ROUTE:PIXEL` forwarding.
6. P4 authoritative pixel state.
7. Director/WebUI segment/effect controls.
8. Timeline / `.shdo` first-class pixel cues.

## 17. Locked role split

```text
P4 Show Engine
├─ local system/safety audio
├─ local emergency pixels (GPIO24)
├─ optional local show-pixel outputs using shared Pixel Engine
└─ authoritative routing/state for specialist nodes

Specialist Nodes
├─ Audio Node → theatrical/show audio
└─ Pixel Node → distributed theatrical/show pixels
```
