# Showduino Final Hardware Architecture

This document describes the intended final Showduino hardware stack.

The goal is to make Showduino modular: one main touchscreen Director, one reliable Stage Engine, and expandable wireless nodes for relays, audio, NeoPixels/FastLED, sensors, and prop puzzles.

## 1. Core Showduino System

```text
Phone / tablet / laptop
        |
        | WiFi WebUI / GoreFX Studio
        v
ESP32-S3 Showduino Director
5 inch 800x480 touchscreen + LVGL Showduino OS
        |
        | UART command link, 115200 baud
        v
ESP32-P4 Stage Engine
Real-time show executor and safety controller
        |
        | UART bridge link
        v
ESP32-C6 / ESP32-C3 ESP-NOW Bridge
        |
        | ESP-NOW wireless network
        v
Relay nodes / audio nodes / pixel nodes / prop nodes
```

## 2. Main Director Hardware

### Recommended role

The Director is the human-facing controller.

It runs:

- LVGL 9 Showduino OS touchscreen interface
- Local show control screens
- Diagnostics screens
- Settings screens
- WebUI / GoreFX Studio server
- SD project storage
- UART command link to the Stage Engine

### Target board

```text
ESP32-S3 with 5 inch 800x480 capacitive touchscreen
Recommended: ESP32-S3 display board with PSRAM and SD card
```

### Minimum useful spec

```text
MCU: ESP32-S3
RAM: PSRAM strongly recommended
Flash: 16MB preferred
Display: 800x480 RGB or SPI display
Touch: capacitive preferred, resistive acceptable
Storage: microSD
WiFi: AP + STA
USB: USB-C preferred
```

### Director connections

```text
UART TX -> Stage Engine RX
UART RX <- Stage Engine TX
GND shared with Stage Engine
Optional SD card
Optional backlight control
Optional buzzer / status LED
```

### Director software

```text
firmware/director-s3/ShowduinoDirectorS3
```

This is where the LVGL Showduino OS belongs.

## 3. Stage Engine Hardware

### Recommended role

The Stage Engine is the reliable executor.

It handles:

- Show timeline execution
- Safety state
- Emergency stop
- Command validation
- DMX output
- Pixel/audio command routing
- Local sensors if required
- UART routing to ESP-NOW bridge

The Director can crash or reboot without the Stage Engine losing control of dangerous outputs.

### Target board

```text
ESP32-P4 board
Preferred: ESP32-P4 board with paired ESP32-C6 radio module where available
```

### Stage Engine connections

```text
UART 1 <-> Director ESP32-S3
UART 2 <-> ESP-NOW Bridge ESP32-C3/C6/S3
DMX output through RS485 transceiver
Optional SD card for deployed runtime assets
Optional local IO expansion
Optional I2S audio output
Optional local pixel outputs
```

### Stage Engine software

```text
firmware/stage-engine-p4/ShowduinoStageEngineP4
```

## 4. ESP-NOW Bridge

### Recommended role

The bridge connects the Stage Engine to wireless nodes.

It receives simple text commands from the Stage Engine and sends compact ESP-NOW packets to nodes.

### Target boards

```text
ESP32-C6 preferred for newer radio support
ESP32-C3 acceptable
ESP32-S3 acceptable
Standard ESP32 acceptable for early testing
```

### Bridge responsibilities

- Pair/register nodes
- Route relay commands
- Route audio commands
- Route FastLED/NeoPixel commands
- Forward node ACK/status messages back to Stage Engine
- Keep wireless logic separate from the P4 executor

### Example commands from Stage Engine

```text
BRIDGE:HELLO
NODE:RELAY:01:RELAY:1:ON
NODE:AUDIO:01:PLAY:014
NODE:PIXEL:01:FX:HELLFIRE
NODE:PIXEL:01:BRIGHTNESS:180
```

## 5. Relay Nodes

### Recommended role

Relay nodes live near props, reducing long relay wiring runs.

Each relay node receives wireless commands and controls local relay outputs.

### Recommended hardware options

```text
Option A: ESP32 + 4 relay module
Option B: ESP32 + 8 relay module
Option C: ESP32-C3 Super Mini + external transistor/MOSFET relay driver board
Option D: Custom Showduino relay node PCB
```

### Suggested relay node baseline

```text
MCU: ESP32 or ESP32-C3
Relay count: 4 or 8
Power input: 5V logic, optional 12V relay/output rail
Wireless: ESP-NOW
Status LED: required
Pair/config button: recommended
Emergency safe state: required
```

### Relay node behaviour

- Boot with relays OFF
- Announce READY over ESP-NOW
- Accept relay ON/OFF/PULSE commands
- Report ACK after every command
- Enter safe state on emergency stop
- Remain safe until emergency clear

### Example relay node commands

```text
RELAY:1:ON
RELAY:1:OFF
RELAY:1:PULSE:1000
RELAY:ALL:OFF
EMERGENCY:STOP
EMERGENCY:CLEAR
STATUS:REQUEST
```

## 6. ESP32-C3 Audio Nodes

### Recommended role

Audio nodes allow separate props or zones to play sound locally without running long audio cables.

### Recommended hardware options

```text
Option A: ESP32-C3 + DFPlayer Mini / DFPlayer Pro
Option B: ESP32-C3 + MAX98357A I2S amplifier
Option C: ESP32-S3 + SD card + I2S DAC for higher quality audio
```

### Best early-build option

```text
ESP32-C3 + DFPlayer Mini or DFPlayer Pro
```

This is simple, cheap, and reliable for scare attraction props.

### Audio node features

- ESP-NOW command receiver
- Local SD audio playback through DFPlayer or I2S
- Volume control
- Stop / pause / resume
- Optional busy pin feedback
- Optional local trigger input
- Optional status LED

### Example audio commands

```text
AUDIO:PLAY:001
AUDIO:PLAY:014
AUDIO:STOP
AUDIO:PAUSE
AUDIO:RESUME
AUDIO:VOLUME:25
AUDIO:LOOP:003
STATUS:REQUEST
```

## 7. ESP32-C3 FastLED / NeoPixel Nodes

### Recommended role

Pixel nodes control local LED strips, props, lanterns, signs, control panels, scare lighting, and scenic effects.

These should use FastLED where possible for flexible effects and animation control.

### Recommended hardware

```text
MCU: ESP32-C3, ESP32-S3, or standard ESP32
LED library: FastLED
Pixel type: WS2812B / SK6812 / compatible addressable LEDs
Power: external 5V supply sized for LED count
Data protection: 330R resistor on data line recommended
Level shifting: recommended for long runs or unreliable strips
Capacitor: 1000uF across 5V/GND at strip power input recommended
```

### Suggested C3 pixel node baseline

```text
Board: ESP32-C3 Super Mini
Pixel data pin: GPIO 4 or GPIO 5
Config button: GPIO 9 if available and safe for board boot mode
Status LED: onboard LED where available
Default LED count: 30, configurable
Wireless: ESP-NOW
```

### Pixel node behaviour

- Boot dark or low amber idle glow
- Announce READY over ESP-NOW
- Store last brightness setting
- Accept named FX commands
- Accept RGB colour and brightness commands
- Stop all effects on emergency stop
- Optional standalone fallback effect if wireless is unavailable

### Example pixel commands

```text
PIXEL:FX:HELLFIRE
PIXEL:FX:STROBE
PIXEL:FX:GHOST_FADE
PIXEL:FX:LIGHTNING
PIXEL:FX:CHASER
PIXEL:COLOR:255:40:0
PIXEL:BRIGHTNESS:180
PIXEL:CLEAR
EMERGENCY:STOP
STATUS:REQUEST
```

### Core FastLED effects to include

```text
HELLFIRE
CANDLE_FLICKER
GHOST_FADE
LIGHTNING
STROBE
POLICE_RED_BLUE
RADIOACTIVE_PULSE
CONSOLE_GLITCH
PORTAL_SPIN
CHASE
BREATHING
BLACKOUT
```

## 8. Combo Prop Nodes

Some props should use one ESP32 node for multiple jobs.

### Example combo nodes

```text
ESP32-C3 relay + audio node
ESP32-C3 audio + pixel node
ESP32-S3 puzzle + relay + audio + pixel node
ESP32 prop controller with local buttons/sensors
```

### Good use cases

- Scare box with relay, sound, and LEDs
- Puzzle panel with lights and audio feedback
- Door controller with maglock relay and status pixels
- Lantern with standalone animation and wireless trigger
- R3 terminal-style prop

## 9. R3 Terminal Node

Current known R3 Terminal hardware:

```text
MCU: ESP32
Pots: GPIO 27, 26, 25
Relays: GPIO 4, 5, 18
NeoPixels: GPIO 33, 6 pixels
Reset button: GPIO 32
Connect button: GPIO 35
DFPlayer Pro: RX 16, TX 17
Showduino Serial: RX 19, TX 23
```

Role:

- Standalone puzzle terminal
- Optional Showduino-integrated prop
- Potentiometer puzzle
- Relay latching
- Pixel feedback
- Audio feedback

## 10. Optional Legacy / Development Hardware

These are useful for testing but are not the final preferred architecture.

### CYD controller

```text
ESP32-2432S028R CYD 2.8 inch touchscreen
```

Good for early UI tests, Bank of Dad, small controllers, and low-cost nodes.

### Arduino Mega executor

```text
Arduino Mega 2560 + 8 relay board
```

Good as a proven executor during transition, but the final architecture moves execution to ESP32-P4.

## 11. Power Architecture

### Recommended rails

```text
5V rail: ESP32 boards, pixels, relay modules, DFPlayers
12V rail: props, solenoids, lamps, motors, amplifiers, 12V relays
3.3V rail: ESP32 logic only
```

### Rules

- All boards sharing signal wires must share GND.
- Never feed 5V logic into ESP32 GPIO.
- Use level shifters where required.
- Use proper fusing per output group.
- Use separate high-current pixel power injection.
- Put a 330R resistor on addressable LED data lines.
- Put a large capacitor across LED strip power input.
- Emergency stop should remove or disable dangerous power.
- Relay nodes should boot with outputs OFF.

## 12. Minimum Final Demo System

This is the smallest system that proves the final architecture.

```text
1x ESP32-S3 5 inch Director running LVGL Showduino OS
1x ESP32-P4 Stage Engine
1x ESP32-C3/C6 ESP-NOW Bridge
1x ESP32 relay node with 4 relays
1x ESP32-C3 audio node with DFPlayer
1x ESP32-C3 FastLED pixel node
1x emergency stop input
1x shared 5V/12V power setup
```

## 13. Final Product Direction

The preferred final Showduino product family should be:

```text
Showduino Director
Showduino Stage Engine
Showduino Relay Node 4
Showduino Relay Node 8
Showduino Audio Node
Showduino Pixel Node
Showduino Prop Node
Showduino Bridge
```

This gives the system a clean commercial structure while keeping each part cheap, replaceable, and expandable.
