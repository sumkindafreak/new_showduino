# Showduino Hardware Baseline — 25 August 2026

This document records the current physical Stage Controller baseline after the enclosure/hardware simplification.

## Decision

The Stage Controller is centred on the **Waveshare ESP32-P4-Module-DEV-KIT** and uses the capabilities already present on that board wherever practical.

The final product should not carry separate modules for functions already available on the Stage Controller board.

## Physical Stage Controller baseline

```text
Waveshare ESP32-P4-Module-DEV-KIT
├── ESP32-P4                 Show Engine / authority
├── onboard ESP32-C6         Communications Engine target
├── ESP32-P4 RTC domain      system clock / RTC target
├── RTC rechargeable battery header
├── ES8311 + NS4150B         local Showduino/system audio
├── onboard microphone
├── onboard microSD          projects, WebUI, runtime assets
├── 100M Ethernet
├── USB
├── GPIO24                   emergency NeoPixel line
├── GPIO25                   momentary emergency trigger
└── external PCM5102A        dedicated show/programme audio
```

## Removed from the Stage Controller

The new baseline does **not** require:

- A separate ESP32-C3/SUE communications board.
- A separate DS3231 RTC module.
- A separate controller solely to provide system/boot audio.

The **PCM5102A remains** because it has a different job: it is the dedicated show-audio output path.

## Role ownership

### ESP32-P4 — Show Engine

Owns show state, safety policy, timeline/cues, project/runtime storage, local outputs and the Web services as they are implemented.

### Onboard ESP32-C6 — Communications Engine

Selected hardware target for Wi-Fi/Bluetooth/ESP-NOW transport. The Waveshare module integrates the C6 with the P4 using SDIO. The C6 also exposes a UART programming header.

**Migration status:** the repository still contains the previously working external C3/UART bridge. Do not delete that code until the onboard C6 implementation has parity for Director traffic, nodes, WebUI transport, replies/state and emergency handling.

### ESP32-P4 RTC

The P4 contains RTC timekeeping and the Waveshare board exposes the P4 rechargeable RTC backup-battery connection. No external DS3231 is part of the new hardware baseline.

RTC backup behaviour is a firmware integration task; documentation must not claim battery-backed operation has been qualified until it has been bench tested.

### Onboard audio — Showduino/system sounds

The board's ES8311 codec and NS4150B amplifier drive the speaker header. This path is reserved for short system sounds such as:

- boot/startup
- ready
- production loaded
- armed
- link/error notification
- emergency acknowledgement
- shutdown/restart

Official board mapping:

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

### External PCM5102A — show audio

The external PCM5102A is retained for timeline/programme audio: music, dialogue, ambience and timed effects.

Current Showduino wiring:

```text
WS/LRCK  GPIO20
BCLK     GPIO21
DOUT     GPIO22
```

The ESP32-P4 has one I2S peripheral. Therefore v1 firmware must **arbitrate the I2S resource** and must not assume the onboard codec and external PCM5102A can run two independent simultaneous streams. The safe v1 policy is that system sounds do not overlap active show audio unless a later implementation proves a supported shared-bus or equivalent solution.

## C6 flashing warning

Waveshare ships the ESP32-C6 with factory firmware. Before replacing it, retain a recovery path and identify the firmware image/version used.

Waveshare's documented flashing procedure is:

1. Pull **C6_IO9** low while powering/resetting to enter C6 download mode.
2. Put the P4 into download mode as required by the board procedure.
3. Flash through **C6_U0RXD / C6_U0TXD**.

`C6_IO9` is a C6 signal; it is **not** P4 GPIO9. P4 GPIO9 is used by the onboard ES8311 audio path.

## Resource rules

Do not allocate these P4 GPIOs to unrelated Showduino features without deliberately changing this baseline:

```text
7-13    onboard codec / I2C audio control
20-22   external PCM5102A show audio
24      emergency NeoPixel
25      emergency push button
39-45   onboard microSD / SDMMC and power
53      onboard audio amplifier enable
```

The onboard C6, Ethernet, USB, RTC crystal/backup and other board-integrated functions must also be treated as reserved board resources even where their internal nets are not exposed as ordinary Showduino connectors.

## Migration rule

**Hardware target and firmware maturity are different things.**

The P4 + onboard-C6 + internal-RTC + dual-role-audio hardware baseline is now canonical. Existing external-C3/DS3231 firmware remains in-tree only as compatibility/reference code until each replacement path is proven on real hardware.

## Primary references

- Waveshare ESP32-P4-Module-DEV-KIT documentation: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT
- Waveshare C6 flashing FAQ: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/FAQ
- Espressif ESP32-P4 VBAT/RTC notes: https://docs.espressif.com/projects/esp-iot-solution/en/latest/low_power_solution/esp32p4_vbat.html
