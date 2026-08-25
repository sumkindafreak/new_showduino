# Showduino P4/C6 Hosted-Link Qualification

This is the **first no-risk hardware qualification** for the Waveshare ESP32-P4-Module-DEV-KIT onboard ESP32-C6.

It runs on the **ESP32-P4** and deliberately leaves the factory ESP32-C6 firmware untouched.

## What it proves

The test verifies:

```text
ESP32-P4
   -> internal SDIO
onboard ESP32-C6 factory ESP-Hosted firmware
   -> Wi-Fi radio
```

A PASS means:

- the P4 can initialize the onboard C6;
- the internal SDIO transport comes up;
- Wi-Fi RPC calls reach the C6;
- the P4 can read the C6 STA MAC;
- the C6 can perform a real Wi-Fi scan and return results.

This does **not** yet prove Director ESP-NOW. Espressif's normal ESP-Hosted Wi-Fi API does not currently expose ESP-NOW to the P4 host, so Showduino needs a deliberate C6-side ESP-NOW extension/custom service for the next gate.

## Important: do not flash the C6 for this test

Waveshare ships the onboard C6 with factory firmware for the hosted SDIO link. Keep that firmware intact for this qualification.

## Hardware

No extra modules and no jumper wires are required.

The module routes the P4/C6 link internally using the standard P4/C6 SDIO layout:

| Function | P4 GPIO |
|---|---:|
| C6 SDIO D0 | 14 |
| C6 SDIO D1 | 15 |
| C6 SDIO D2 | 16 |
| C6 SDIO D3 | 17 |
| C6 SDIO CLK | 18 |
| C6 SDIO CMD | 19 |
| C6 CHIP_PU / reset | 54 |

These pins are **reserved for the onboard C6** in Showduino and must not be assigned to other Stage Controller features.

## Toolchain

Use ESP-IDF 5.4 or 5.5. Waveshare currently recommends ESP-IDF for reliable ESP32-P4 peripheral work.

The project pins the managed components to the IDF 5.x families used by Waveshare's P4 Wi-Fi example:

- `espressif/esp_wifi_remote` `0.14.*`
- `espressif/esp_hosted` `1.4.*`

## Build

From an ESP-IDF terminal:

```text
cd firmware/p4-c6-espnow-bridge/hosted-link-qualification
idf.py set-target esp32p4
idf.py build
```

If the board's P4 revision requires a specific revision profile, use the matching ESP-IDF/Waveshare revision settings rather than forcing an incompatible image.

## Flash and monitor

```text
idf.py -p COMx flash monitor
```

Replace `COMx` with the Stage Controller P4 serial port.

## Expected successful result

The important lines are similar to:

```text
PASS: initialize ESP-Hosted host
PASS: connect P4 to onboard C6
ESP-Hosted event: SDIO transport UP
PASS: internal P4 <-> C6 SDIO transport is UP
[C6] STA MAC: XX:XX:XX:XX:XX:XX
--- Onboard C6 Wi-Fi scan ---
...
SHOWDUINO ONBOARD C6 QUALIFICATION: PASS
```

The exact AP names and counts will differ.

## Failure rule

If this test fails, **do not flash custom C6 firmware**. Capture the full P4 serial log first. The failure should be resolved while the factory recovery path is still intact.

## Next gate

After this passes on the physical Stage Controller:

1. preserve/recovery-test the factory C6 image;
2. implement Showduino ESP-NOW on the C6 side;
3. carry Director command packets over ESP-NOW into the C6;
4. forward them to the P4 over the internal SDIO transport;
5. return P4 replies/status back through C6 -> ESP-NOW -> Director;
6. only then retire the external C3 compatibility path.
