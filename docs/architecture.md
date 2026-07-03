# Showduino v1 Architecture

Showduino v1 has one controller and one executor.

This document locks the official v1 architecture:

```text
CYD = Director
Mega = Stage Engine
```

## Core Design

```text
GoreFX WebUI / Scene Creator
        |
        | Hosted by CYD
        v
ESP32 CYD Director
        |
        | UART serial command protocol
        v
Arduino Mega Stage Engine
        |
        | Direct hardware wiring
        v
4 Pixel Lines / Mega SD Runtime Files / RTC / Audio Hardware
```

## CYD Director

The CYD is the master controller.

It owns:

- GoreFX WebUI server
- Scene Creator
- Timeline editor
- Project storage
- Scene storage
- Show storage
- UI assets
- Themes/icons/images
- Settings
- Web/dashboard access
- Serial command bridge to Mega

The CYD SD card is used for creative/project storage.

Recommended CYD SD layout:

```text
/scenes/
  chamber_intro.shdo
  portal_test.shdo

/shows/
  chamber_full_show.shdo

/projects/
  chamber_project.json

/assets/
  icons/
  images/
  themes/

/logs/
/settings.json
```

## Mega Stage Engine

The Mega is the deterministic runtime executor.

It owns:

- 4 pixel output lines
- RTC module
- Mega SD card for runtime files
- Audio control/playback hardware
- Emergency blackout behaviour
- Local execution of deployed scenes/cues
- Status reporting back to CYD

Recommended Mega SD layout:

```text
/audio/
  001.wav
  002.wav
  heartbeat.mp3

/runtime/
  active_scene.txt

/pixels/
  fire.tbl
  portal.tbl

/logs/
```

## SD Card Roles

The two SD cards are not duplicates.

### CYD SD Card

Purpose:

```text
Creative/project storage
```

Stores:

- Scene files
- Show files
- WebUI assets
- Icons/images/themes
- Project files
- Settings
- Backups

### Mega SD Card

Purpose:

```text
Runtime/execution storage
```

Stores:

- Audio files
- Runtime compiled scene files
- Pixel lookup tables
- Runtime logs
- Any files needed by the Mega during playback

## Scene Deployment Model

The CYD creates and stores scenes.

Before playback, the CYD deploys a simple command list to the Mega.

First version:

```text
SCENE:TEST
```

Next version:

```text
SCENE:BEGIN:portal_intro
CUE:0:AUDIO:PLAY:001
CUE:0:PIXEL:1:EFFECT:PULSE
CUE:4000:PIXEL:2:EFFECT:FIRE
CUE:7000:PIXEL:4:EFFECT:STROBE
SCENE:END
SCENE:PLAY
```

The Mega then runs the scene locally for better timing.

## Weekend Build Target

Tonight/weekend test target:

```text
1. Upload Mega Stage Engine firmware.
2. Upload CYD Director test firmware.
3. Connect CYD TX/RX/GND to Mega Serial1.
4. CYD sends HEARTBEAT.
5. Mega replies STATUS:ALIVE.
6. CYD sends pixel commands.
7. Mega runs 4 pixel lines.
8. CYD sends SCENE:TEST.
9. Mega runs audio/pixel scene test.
10. CYD sends EMERGENCY:STOP.
11. Mega blackouts all pixels and stops runtime.
```

## Final v1 Design Statement

```text
CYD Director creates, stores, and controls scenes.
Mega Stage Engine executes runtime hardware.
CYD SD = projects/scenes/WebUI.
Mega SD = audio/runtime/pixel tables/logs.
```
