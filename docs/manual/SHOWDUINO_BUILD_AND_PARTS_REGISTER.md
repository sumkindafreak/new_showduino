# Showduino V1 — Hardware, Software and Replacement Parts Register

**Revision 19 November 2026 · Source tree `41c7c71` · Product `1.0.0-rc.1`**

Use this for the **actual physical installation** and any future replacement, not as a shopping list. Every reported board, GPIO and source library must be checked against the **flashed** build and photographed hardware. A documented model is **not** proof of the installed PCB revision.

## 1. Core system (three boards)

| Role | Source-described board | Function and connections | Evidence/status |
| --- | --- | --- | --- |
| **P4 Show Engine** | **Waveshare ESP32-P4-Module-DEV-KIT** | Authoritative state, SDMMC microSD, ES8311 system sound, GPIO23 show pixels, GPIO24 signage, GPIO25 Emergency; I²C 7/8; PCM5102A 20/21/22 | Source in `firmware/stage-engine-p4/ShowduinoStageEngineP4/BoardConfig.h`; actual PCB revision/power/acceptance requires record |
| **Communications S3** | Dedicated **ESP32-S3 Dev Module**, confirm memory, USB and RGB LED PCB variant | ESP-NOW + UART + SoftAP/WebUI; S3 GPIO17 TX→P4 GPIO4 RX, S3 GPIO18 RX←P4 GPIO5 TX, common GND, **115200 8N1** | `firmware/s3-comms-controller/ShowduinoS3CommsController/BoardConfig.h` |
| **Director** | **JC8048W550C** ESP32-S3 RGB touchscreen, 800×480, **GT911** | LVGL operator desk; ESP-NOW to Comms; LCD backlight GPIO2; touch SDA19/SCL20/INT18/RST38; two fitted ambient pixels GPIO17 | `firmware/director-esp32-8048s050/ShowduinoDirector8048S050/BoardConfig.h`; check actual rear PCB silk |

**Critical replacement notes:** The P4's onboard C6 is reserved/unused, not the Comms board. Similar ESP32-S3 5-inch screens can have incompatible RGB/touch wiring. The Director's XPT2046 fallback conflicts with GT911 hardware (e.g. RST/CS); use the **GT911** production configuration. Do not enable boot USB CDC over Director GT911 GPIO19/20.

The Comms BoardConfig describes GPIO48 for one DevKitC RGB variant and notes GPIO38 on another. **Inspect your fitted S3** rather than assuming the built-in indicator pin.

## 2. P4 resource map

| Resource | Source assignment | Physical register required |
| --- | --- | --- |
| Serial bridge | P4 RX GPIO4 / TX GPIO5 | Actual cable, grounds, logic levels, strain relief |
| Main Emergency | GPIO25, input pull-up; button to GND (LOW asserted), debounce 30 ms; 8 s hold **Locate** | Type/contact arrangement, installation wiring, response test |
| Show Pixel Line | GPIO23 WS2812 | Actual type, 5 V supply, LED count/segments, fusing |
| Emergency/designated signage | GPIO24 WS2812 | Actual LED count/grouping, emergency current rating |
| SDMMC microSD | CLK43, CMD44, D0=39 D1=40 D2=41 D3=42, power GPIO45 | Card make, size, FAT32, verified backup |
| I²C/plugin + ES8311 | SDA GPIO7 / SCL GPIO8, 3.3 V | External addresses, pull-ups, devices, cable length |
| PCM5102A ambience | WS GPIO20, BCLK GPIO21, DOUT GPIO22 | Exact DAC breakout, power, mute and output |
| ES8311 system sound | P4 onboard codec; associated I²S/amp control | Correct connector, speaker impedance, approved system audio files |
| Onboard C6 | Reserved | Do not wire as active Comms replacement |

See [Hardware Pinout](../hardware-pinout.md), [P4 storage](../p4-sd-storage.md), [audio architecture](../audio-pixel-engine.md).

## 3. Replaceable specialist nodes

| Module | Physical hardware and output | Ownership/source boundary |
| --- | --- | --- |
| **Audio Node** | Ai-Thinker ESP32-A1S Audio Kit V2.2 **A161/ES8388**; local SD WAV/MP3 source support; GPIO22 programmable show pixels; fixed GPIO5 status pixel | **Not** AC101 variant; P4 decides playback/Emergency, node plays; [Audio source](../../firmware/audio-node-esp32-a1s/README.md) |
| **S3 Lamp Node** | ESP32-S3, seven-pixel Jewel, local carbide effect, optional local FX audio, sensors | Active source-managed S3 lamp; [Lamp](../s3-lamp-node.md) |
| **Simplified C3 Lamp** | Proposed ESP32-C3 SuperMini, seven pixels, one button and rechargeable prop enclosure | Separate **design/implementation track**; do not mark as current tested S3 replacement; [status](../c3-lamp-node-simplified-integration.md) |
| **C3 Pixel Node** | ESP32-C3 Super Mini OLED; GPIO2 show pixels, GPIO5 SDA/GPIO6 SCL OLED | Remote pixels, source implemented; physical power/output test required; [Pixel](../c3-pixel-node.md) |
| **C3 Emergency + Pixel** | ESP32-C3 station, wireless **assert-only** Emergency, GPIO2 pixel line on same ESTOP identity | Physical button wiring and radio failure modes require acceptance; no clear command from station; [Emergency](../emergency-node.md) |
| **MOSFET Node** | ESP32_MOS_X4/303E32NMOS4 four low-voltage DC PWM/switching channels, four identifier pixels | Reported actual states != load measurements; verify pinout, power handling and reconnect; [MOSFET](../director-outputs-mosfet.md) |
| **Relay Node / legacy C3 Comms** | Old development hardware | Historical, not active V1 production parts |

**Never** infer a permissible current, voltage, mains rating or safety certification from the node category. Obtain the vendor datasheet and verify the specific load circuit.

## 4. Programmable LED versus fixed indicators

Per `protocol/showduino_pixel_defaults.h`, new **programmable pixel lines** use a minimum default of **10 LEDs** (within device limits) when stored count is missing/invalid; valid larger counts are retained. This does **not** rewrite physically fixed indicator counts:

- Director ambient: **2** on the same data line.
- Lamp flame Jewel: **7**.
- MOSFET identifier: **4**.
- Audio status indicator: **1**.
- Emergency signage: specifically configured/fitted count, separate from the show line.

Record the actual LED count, type/order, wire gauge, max total current at Emergency white, PSU capacity and voltage at far end.

## 5. Firmware and toolchain register

For **every** physical device, fill in one row and attach build/archive details:

| Field | Record |
| --- | --- |
| Module role, logical ID and MAC | |
| Vendor, exact silkscreen, PCB revision and dimensions | |
| Photos front/back and installed wiring diagram | |
| Power supply voltage, current limit, fuse and polarity | |
| Source repo/commit or local archive hash | |
| Firmware binary SHA-256 and flash date | |
| Arduino FQBN, core version, partition, flash/PSRAM selections | |
| Libraries including versions (LVGL, display, audio/pixel drivers as applicable) | |
| Node nonvolatile settings (safe non-secret export) | |
| Media filenames/SD copy location and verified restore | |
| Test steps performed, results, dated logs and witness | |
| Spare supplier/vendor part number and replacement procedure | |

Do not put Wi-Fi passwords, API credentials, keys or personal procurement records into public GitHub docs. Keep an offline encrypted/deployed-build record with the installer.

## 6. Service and safe replacement procedure

1. Put the show and independently controlled site equipment into a safe state; isolate load power per local electrical procedure.
2. Identify the exact board and connector by **its own photos and markings**, not a similar online image.
3. Back up the existing flash/config, P4 SD and node media. Confirm you can restore the known-good bench image.
4. Check electrical/pin/voltage compatibility before rewiring or plugging in.
5. Load the documented **approved** firmware for that component. Do not use an untested GitHub branch as a maintenance shortcut.
6. Power with **low-energy test loads**, confirm boot/discovery/node ID and P4 authorisation.
7. Verify normal output, loss-of-comms, Emergency assert, deliberate P4 clear and **no automatic restart** for that node.
8. Record the repair, firmware and signed retest. Only then restore authorised loads/site operation.

## 7. Source-of-truth hierarchy

**Installed flash/config + dated test evidence** wins for behaviour claimed on the bench. **Current firmware, protocol and WebUI source** defines what GitHub contains. **Unmerged PRs, plans and archived sketches** are neither installed behaviour nor final product features.

[User Manual](SHOWDUINO_USER_MANUAL.md) · [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) · [Physical Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md).
