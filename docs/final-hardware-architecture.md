# Showduino Final Hardware Architecture

This document describes the intended Showduino hardware stack and how physical hardware maps to architectural roles.

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

## 1. Product topology

### Operator path

```text
Director ESP32-S3 (5" touch)
        |
        | ESP-NOW
        v
Onboard ESP32-C6
Communications Engine
        |
        | integrated P4/C6 transport
        | target: module SDIO path
        v
ESP32-P4 Show Engine
Stage Controller
```

### Node path

```text
Showduino Node
        |
        | ESP-NOW
        v
Onboard ESP32-C6
        |
        v
ESP32-P4 Show Engine
```

### Browser / phone

```text
Phone / tablet / laptop
        |
        | Wi-Fi
        v
Onboard ESP32-C6
        |
        v
P4 Show Engine Web UI / API / WebSocket
```

The Director does not host the primary Web UI.

---

## 2. Director hardware — ESP32-S3

### Role

Human-facing control surface: command requests and status display.

### Active software

```text
firmware/director-esp32-8048s050/
```

### Normal product connection

```text
ESP-NOW → onboard C6 Communications Engine
```

USB remains flash/diagnostic only.

---

## 3. Stage Controller hardware — Waveshare ESP32-P4-Module-DEV-KIT

The Stage Controller is deliberately lean. Functions already fitted to the board are used before adding external modules.

### Core board resources

```text
ESP32-P4NRW32        Show Engine processor
ESP32-C6             Communications Engine hardware target
32 MB PSRAM          P4 package
16 MB NOR flash      board/module storage
microSD / SDMMC      projects, WebUI, logs, media
100M Ethernet        wired networking
USB                  service / host / device capability
P4 RTC + VBAT        system time / backup target
ES8311 codec         onboard audio codec
NS4150B amplifier    onboard speaker amplification
onboard microphone   local audio input capability
8Ω 2W speaker header local system-sound output
```

### External hardware retained

```text
PCM5102A I2S DAC     dedicated show/programme audio output
```

### External modules removed from the baseline

```text
Separate C3/SUE communications MCU    REMOVED
Separate DS3231 RTC                   REMOVED
Separate controller just for UI audio REMOVED
```

This does not prevent specialist Showduino nodes from existing elsewhere in an installation. It only removes redundant boards from the Stage Controller itself.

---

## 4. Communications Engine — onboard ESP32-C6

### Role

Wireless transport only:

- ESP-NOW to Director
- ESP-NOW to nodes
- Wi-Fi AP/STA for browser clients
- Bluetooth capability where later required
- P4 transport/link health

### Hardware relationship

The Waveshare ESP32-P4 module integrates its C6 radio with the P4 using SDIO for Wi-Fi/Bluetooth expansion. A C6 UART header is exposed for flashing/debugging.

### Firmware relationship

```text
firmware/p4-c6-espnow-bridge/       TARGET / BRING-UP
firmware/c3-supermini-espnow-bridge/ COMPATIBILITY / ROLLBACK
```

The external C3 code remains in-tree only until onboard C6 parity is proven.

### C6 flashing safety

Waveshare ships factory C6 firmware. Preserve a recovery path before replacing it.

Documented board procedure:

```text
C6_IO9 LOW during power/reset → C6 download mode
P4 also placed into download mode as required
Flash via C6_U0RXD / C6_U0TXD
```

`C6_IO9` must not be confused with P4 GPIO9.

---

## 5. RTC / system time

### Target

Use the **ESP32-P4 integrated RTC domain** and the board's rechargeable RTC battery connection.

No DS3231 is required in the final Stage Controller.

### Maturity rule

The presence of the RTC/VBAT hardware does not itself prove power-loss time retention in the Showduino firmware. RTC backup behaviour must be bench-qualified before being labelled confirmed.

The Show Engine owns system time and publishes it to Director/WebUI clients.

---

## 6. Audio architecture

### A. Showduino/system audio — onboard

```text
ESP32-P4
  → ES8311 codec
  → NS4150B amplifier
  → 8Ω 2W speaker
```

Purpose:

- boot sound
- ready sound
- production loaded
- show armed
- link / warning / error sounds
- emergency acknowledgement
- restart / shutdown sound

Official board resources:

```text
I2C SDA     GPIO7
I2C SCL     GPIO8
I2S DSDIN  GPIO9
I2S LRCK   GPIO10
I2S ASDOUT GPIO11
I2S SCLK   GPIO12
I2S MCLK   GPIO13
PA enable  GPIO53
```

### B. Show/programme audio — external PCM5102A

Purpose:

- music
- dialogue
- ambience
- timeline SFX
- emergency programme audio where required by the show runtime

Current wiring:

```text
WS/LRCK  GPIO20
BCLK     GPIO21
DOUT     GPIO22
```

### I2S constraint

The P4 has one I2S peripheral. The two audio **roles** are intentionally separate, but v1 firmware must arbitrate that hardware resource. System sounds must not be assumed to overlap an active show-audio stream until a supported implementation has been proven.

---

## 7. Emergency and local pixel hardware

```text
GPIO25  momentary emergency push button to GND
GPIO24  emergency NeoPixel data
```

Emergency behaviour:

- Pressed LOW after debounce → latch emergency once.
- Holding the button does not retrigger.
- Release does not clear.
- Second press does not toggle/clear.
- `EMERGENCY:CLEAR` is rejected while the physical button remains LOW.
- Clearing never auto-resumes a show.

---

## 8. Stage Controller storage

Onboard microSD is the canonical runtime/project media location.

Current P4 mapping:

```text
CLK    GPIO43
CMD    GPIO44
D0     GPIO39
D1     GPIO40
D2     GPIO41
D3     GPIO42
POWER  GPIO45 active LOW
```

Runtime paths include:

```text
/showduino/webui/
/showduino/shows/
/showduino/audio/
/showduino/logs/
/showduino/system/
```

---

## 9. Relay and specialist nodes

Relay control remains node-based when the output is remote from the Stage Controller.

### Active relay example

```text
firmware/relay-node-esp32/
```

Rules:

- Boot safe / OFF
- Absolute ON/OFF/timed pulse
- Safe state on emergency
- Report actual completion separately from command acceptance
- Logical Showduino device IDs at the application layer

Future specialist nodes may include pixel, audio-zone, sensor, motor and environmental devices.

---

## 10. Power architecture

General engineering rules remain:

```text
3.3V — ESP logic
5V   — logic modules / some pixels / peripheral boards
12V  — props, lamps, solenoids, motors and amplifiers as required
```

- Shared signal ground where required
- Never feed 5V directly into ESP GPIO
- Fuse high-current groups
- Inject pixel power correctly
- Emergency design must make dangerous energy safe
- Outputs boot safe

---

## 11. Minimum current hardware proof

```text
1x ESP32-S3 Director
1x Waveshare ESP32-P4-Module-DEV-KIT
   ├── onboard ESP32-C6
   ├── onboard RTC/VBAT hardware
   └── onboard ES8311 system audio
1x PCM5102A show-audio DAC
1x emergency button on GPIO25
1x emergency NeoPixel line on GPIO24
optional relay/node hardware required by the test scenario
```

The onboard C6 migration and RTC/audio firmware integration must be proven before this is called a fully qualified hardware release.

---

## 12. Product family naming

```text
Showduino Director
Showduino Stage Controller   (runs Show Engine)
Showduino Communications Engine   (onboard C6 role)
Showduino Relay Node
Showduino Audio Node
Showduino Pixel Node
Showduino Prop Node
```

Avoid new materials that call the P4 software role “Stage Engine” or imply SUE must be a separate physical board.
