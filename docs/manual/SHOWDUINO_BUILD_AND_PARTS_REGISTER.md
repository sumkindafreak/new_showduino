# Showduino — Hardware, Software & Replacement Parts Register

**Documentation review:** 20 October 2026 · firmware source `f4c9aeaabdcf939417f9891fb01d98e1e5d8aec2` · [review scope and evidence](SHOWDUINO_DOCUMENTATION_REVIEW.md). Source review does not establish physical acceptance.

**Working inventory · 16 October 2026**  
**Product:** 1.0.0-rc.1 · **Status:** working documentation; hardware acceptance remains separate  
**Primary sources:** [hardware pinout](../hardware-pinout.md), [repository status](../repository-status.md), each active firmware `README.md` and board `BoardConfig.h`.

This register is for the *physical Showduino build*, rather than an idealised shopping list. It separates what the firmware describes from what has been personally checked on the installed unit. **Do not order a replacement based on a generic ESP32 product photograph alone.** Match markings, voltage, PCB revision, memory, connectors and dimensions.

The current physical main Showduino enclosure contains **three central boards**: the tilting Director touchscreen ESP32-S3, the dedicated Communications ESP32-S3, and the Waveshare ESP32-P4 Show Engine in its base. The separate dark enclosure used for the Audio Node contains the ESP32-A1S; it is **not** a second P4 enclosure.

## 1. Main control enclosure — confirmed architecture

| Item / searchable model | Role and physical installation | Key electrical / software identity | Source and confidence |
| --- | --- | --- | --- |
| Waveshare **ESP32-P4-Module-DEV-KIT** | Authoritative Show Engine, installed in base of main unit | P4 on-board microSD, ES8311 system audio, GPIO23 Show Pixel, GPIO24 Emergency/signage, GPIO25 Emergency button, I²C GPIO7/8, PCM5102A GPIO20/21/22 | [P4 firmware](../../firmware/stage-engine-p4/README.md), [pinout](../hardware-pinout.md). Model documented; installed revision/serial still needs photo |
| **ESP32-S3 800×480 Director touchscreen** (JC8048W550-series / ST7262 RGB + GT911 touch) | Adjustable touchscreen on top of main enclosure | LVGL 9, RGB display, GT911 capacitive touch, ESP-NOW to Comms. Match *actual rear PCB marking* before ordering; generic ESP32-8048S050 panels are **not** automatically interchangeable | [Director README](../../firmware/director-esp32-8048s050/README.md). Photograph connector orientation/rear silk |
| Dedicated **ESP32-S3 Dev Module**, N8R8-class build | Comms inside main base, separate from Director | Comms GPIO17 TX → P4 GPIO4 RX; GPIO18 RX ← P4 GPIO5 TX; common GND; 115200 8N1; onboard status WS2812 pin varies by PCB revision (GPIO48 on DevKitC v1.0, GPIO38 on v1.1) | [Comms README](../../firmware/s3-comms-controller/README.md). Confirm flash/PSRAM and physical PCB revision before swap |
| **PCM5102A** external DAC breakout | P4 dedicated ambient sound output, not system/safety sound | WS/LRCK GPIO20, BCLK GPIO21, data-out GPIO22 | [pinout](../hardware-pinout.md). Photograph specific breakout |
| **Momentary emergency button**, normally open contact to ground per current firmware | Local main-unit Emergency assertion | P4 GPIO25, INPUT_PULLUP, pressed LOW, 30 ms debounce, latches in software; eight-second continuous hold adds Locate | [pinout](../hardware-pinout.md), [manual gaps](SHOWDUINO_MANUAL_GAPS.md). Photograph contact wiring; validate installed behaviour |
| **WS2812 pixels**, two distinct lines | Local theatrical effects vs dedicated signage | GPIO23: show line, max 1024 / 16 segment slots. GPIO24: designated signage, up to 100 pixels in 10-pixel groups | [pinout](../hardware-pinout.md). Counts/settings are installation-specific |
| External **microSD card** | Production media/config/storage | Inspect mounted directory tree, record card capacity, filesystem and spare/backup | [P4 storage](../p4-sd-storage.md) |
| P4 **ES8311** + external audio path | System/Emergency audio | Onboard ES8311 codec is I²C 0x18; GPIO10 is its WS/LRCK clock, **not** a status LED | [pinout](../hardware-pinout.md) |
| 5 V power / logic-level buffer / protection components | Installation dependent | Pixel ground must share PSU ground; 330–470 Ω DATA series resistor, robust 5 V logic buffer for long/noisy strips, properly fused supply and power injection | [pinout](../hardware-pinout.md). **Quantities and installed ratings not yet inventoried** |

## 2. Replaceable specialist nodes

| Device and controller board | What it does | Known pins and fitted peripherals | Firmware / documentation status |
| --- | --- | --- | --- |
| **Audio Node — Ai-Thinker Audio Kit V2.2 A161 ESP32-A1S + ES8388** | Remote programme/show WAV sound, speaker/headphone/line, separate from the P4 system sound | Codec ES8388; local SD card; GPIO22 **configurable theatrical WS2812 line** (not a status-only LED); save a nonzero count and initialise before use (10 pixels for fixed bench scenario). **Board must be A161/ES8388, not AC101 variant.** | [Audio firmware](../../firmware/audio-node-esp32-a1s/README.md) · 0.4.3 in repository; full physical acceptance required |
| **Lamp Node — ESP32-S3 Dev Module** | Standalone/Showduino-managed carbide lamp practical | GPIO8 seven-pixel flame Jewel; ignition GPIO7; microphone GPIO4; light GPIO5; voltage GPIO6; optional motion GPIO15; Audio FX UART GPIO17/18 | [Lamp firmware](../../firmware/s3-lamp-node/README.md) · 0.4.3 in repository; sensor/motion validation as noted |
| **C3 Pixel Node — ESP32-C3 Super Mini OLED** | Remote WS2812 output and segmented FX | OLED SDA GPIO5/SCL GPIO6 at 0x3C; pixel DATA GPIO2; local buttons GPIO9/0; max configured 512 pixels | [Pixel firmware](../../firmware/c3-pixel-node/README.md) · 0.1.0; full output path hardware qualification required |
| **C3 Emergency Node — ESP32-C3 Super Mini OLED** | Wireless **assert-only** extra Emergency station, plus independent pixel capability | Emergency input proposed GPIO4 LOW=pressed (physical GPIO **unconfirmed**); pixels GPIO2; OLED SDA5/SCL6 | [Emergency firmware](../../firmware/c3-emergency-node/README.md) · 0.3.0; no remote clear, physical bench mandatory |
| **MOSFET Node — ESP32_MOS_X4 / 303E32NMOS4** | Four independent low-voltage DC switched/PWM outputs | Firmware mapping OUT1–4 GPIO16/17/26/27; status LED GPIO23; four local identifier pixels GPIO25. Mapping marked **UNVERIFIED** | [MOSFET firmware](../../firmware/mosfet-node-esp32/README.md) · 0.1.2. Do not infer safe load current or mains capability |
| **Relay Node** | Historical design, **not a current production node** | Do not include as a current finished module or a product replacement for MOSFET | [Repository status](../repository-status.md) |

**Not counted as built-in product hardware:** DMX/E1.31 production integration (parked), custom fabrication PCBs, uncommissioned I²C expansion cards, older C3 Lamp firmware, and the onboard P4 C6 radio (reserved/unused in the current architecture).

## 3. Libraries, environments and services to document

These are the *source-reported* components, not a guarantee every board uses the same versions.

| Component | Relevant software / library | Where to check for exact version |
| --- | --- | --- |
| All Arduino ESP32 boards | Arduino-ESP32 3.3.x (individual firmware README files contain board-specific FQBN/flash/PSRAM settings) | Arduino Board Manager, `arduino-cli core list`, per-board README |
| Director S3 display | LVGL 9.x, Arduino_GFX_Library, TAMC_GT911, Adafruit NeoPixel | [Director firmware](../../firmware/director-esp32-8048s050/README.md), installed Libraries |
| Comms S3 | WiFi, esp_now, WebServer / UART, built-in ESP32 framework; Studio embedded build | [Comms firmware](../../firmware/s3-comms-controller/README.md) and [embedded Studio build](../../web/showduino-studio/README.md) |
| P4 Show Engine | ESP32-P4 Arduino core, SD/SDMMC, I²S, I²C, P4 pixel / emergency components | [P4 firmware](../../firmware/stage-engine-p4/README.md), [hardware pinout](../hardware-pinout.md) |
| Audio Node | WiFi, esp_now, SD, SPI, Wire, ESP_I2S, WebServer, Preferences, Adafruit NeoPixel | [Audio firmware](../../firmware/audio-node-esp32-a1s/README.md) |
| Lamp | ESP-NOW, Adafruit Audio FX Sound Board (UART), seven-LED Jewel render | [Lamp firmware](../../firmware/s3-lamp-node/README.md) |
| Pixel / Emergency C3 | shared pixel FX vocabulary and hardware-specific OLED/pixel libraries | [Pixel](../../firmware/c3-pixel-node/README.md), [Emergency](../../firmware/c3-emergency-node/README.md) |
| Showduino Studio / web | Static HTML/CSS/JS, SHDO v2 producer; community uses Supabase | [Studio README](../../web/showduino-studio/README.md), [website source](https://github.com/sumkindafreak/showduino.com) |
| HauntSync | Supabase authentication, private profiles/projects, opt-in published creator snapshot, sanitised public .shdo library | [website repository](https://github.com/sumkindafreak/showduino.com), Supabase migrations |

**Recording real version numbers:** save screenshots of each Arduino Board Manager core, Library Manager installed dependencies, selected FQBN, serial boot version, compile output and deployed firmware. A library named in source is not proof the installed board runs that library version.

## 4. Photograph every part — weekend shot list

For **each** installed board, capture: (1) front with silkscreen legible, (2) rear silkscreen and revision number, (3) fitted in enclosure, (4) every plug/terminal before disconnection, (5) wiring with connector positions, and (6) running/LED-status photo. Name files:

`hardware/<node>/<node>_front.jpg`, `hardware/<node>/<node>_back.jpg`, `hardware/<node>/<node>_wiring.jpg`.

Do not publish Wi-Fi passwords, QR codes containing credentials, MAC-based access tokens, serial/device secrets, private venue details or raw SD contents in those photos.

## 5. Fill this out for each individual physical part

Copy one row per purchased board, sensor, cable, switch, connector, PSU, pixel strip, housing print and audio item. Unknown is better than guessing.

| Field | Record |
| --- | --- |
| Showduino assembly / node ID | |
| Full product name and PCB silkscreen | |
| Board revision and MCU marking | |
| Quantity installed / spares kept | |
| Purchase source (AliExpress, eBay, Amazon etc.) and order reference (private) | |
| Paid price, date and warranty (private) | |
| Supply voltage and *documented* load rating | |
| Firmware sketch / Git commit / installed firmware version | |
| Arduino-ESP32 core, board option/FQBN and required libraries | |
| Live pin map and cable colour/function | |
| Backup configuration and SD media location | |
| Expected boot/status indication and healthy test | |
| Safe replacement / repair steps | |
| Known defect, unverified feature or commissioning result | |
| Photograph names (front, back, wiring, assembled) | |
| Public supplier/manufacturer documentation URL | |

## 6. Service procedure (non-energised setup)

1. Identify the failed function and confirm whether it is board, PSU, cable, load or software configuration.
2. Stop the show, isolate mains and low-voltage supplies and ensure independently required venue safety systems remain in operation.
3. Photograph/label connectors and record firmware version, settings, pairing ID, device address and SD files.
4. Fit a **voltage/pinout/revision-compatible** spare. Never assume two ESP32 modules share GPIO wiring.
5. Restore correct firmware, node identity, configuration, media and peer address.
6. Bring up at a safe test bench with loads isolated; confirm boot logs, Comms/P4 ownership and local functions.
7. Recommission output behaviour, Emergency assert, linked/unlinked fail-safe, recovery and **no automatic restart**.
8. Update the component register and archive fault history.

Showduino's theatrical emergency control **is not** a certified replacement for venue fire/life-safety or machinery protection.

## 7. Source-of-truth rule

- Canonical **code and wiring**: active firmware `BoardConfig.h` and checked-in source.
- Canonical **commissioned board identity**: labelled photographs and your dated bench measurements.
- Canonical **customer operating behaviour**: [User Manual](SHOWDUINO_USER_MANUAL.md) plus unresolved [Manual Gaps](SHOWDUINO_MANUAL_GAPS.md).
- The website hardware gallery is a **visual guide**, and generic vendor photos may show a different board revision.

If they disagree, flag a documentation discrepancy. Never quietly call it physically validated.
