# Showduino Onboard C6 Qualification — Arduino IDE

This is the normal Showduino qualification path for the Waveshare ESP32-P4-Module-DEV-KIT onboard ESP32-C6 when using **Arduino IDE**.

It runs on the **ESP32-P4** and leaves the factory ESP32-C6 firmware untouched.

## What it tests

```text
ESP32-P4
   -> internal SDIO
onboard ESP32-C6 factory hosted firmware
   -> Wi-Fi radio
```

A PASS confirms:

- Arduino-ESP32 can configure the Waveshare P4/C6 SDIO pins;
- the hosted C6 Wi-Fi interface starts;
- the P4 receives a valid C6 STA MAC address;
- the C6 completes a real Wi-Fi scan and returns results.

It does **not** yet prove Showduino ESP-NOW. That is the next migration gate.

## Required software

- Arduino IDE 2.x
- `esp32 by Espressif Systems` **3.3.x or newer**
- Showduino bench baseline currently uses **3.3.11**

Arduino-ESP32 3.3.x includes P4 ESP-Hosted / Wi-Fi Remote support and exposes:

```cpp
WiFi.setPins(clk, cmd, d0, d1, d2, d3, rst);
```

No extra Arduino libraries are required.

## Sketch

Open:

```text
firmware/p4-c6-espnow-bridge/arduino-hosted-link-qualification/ShowduinoP4C6HostedQualification/ShowduinoP4C6HostedQualification.ino
```

## Arduino IDE settings

Use the same Stage Controller settings as the current P4 firmware:

```text
Board: ESP32P4 Dev Module
Flash Size: 16MB
PSRAM: Enabled
Serial Monitor: 115200 baud
```

Use the compatible chip-revision setting for the fitted P4 if that option is shown by your installed board package.

## Important

This sketch is flashed to the **P4**.

Do **not**:

- pull `C6_IO9` low;
- put the C6 into download mode;
- connect the C6 UART programmer;
- overwrite the factory C6 firmware.

No extra modules or jumper wires are needed.

## Internal C6 pins used by the sketch

```text
P4 GPIO18 = C6 SDIO CLK
P4 GPIO19 = C6 SDIO CMD
P4 GPIO14 = C6 SDIO D0
P4 GPIO15 = C6 SDIO D1
P4 GPIO16 = C6 SDIO D2
P4 GPIO17 = C6 SDIO D3
P4 GPIO54 = C6 reset / CHIP_PU
```

The sketch calls `WiFi.setPins()` **before starting Wi-Fi**.

## Expected Serial Monitor result

A successful board should produce output similar to:

```text
Showduino P4 -> onboard C6 Arduino qualification
PASS: onboard C6 SDIO pins accepted
PASS: hosted Wi-Fi interface started
[C6] STA MAC: XX:XX:XX:XX:XX:XX
PASS: valid C6 STA MAC received over hosted link
[C6] Scan complete: ... network(s) found

SHOWDUINO ONBOARD C6 QUALIFICATION: PASS
P4 -> SDIO -> onboard C6 -> Wi-Fi radio is operational.
```

The nearby SSIDs/RSSI values will naturally differ.

## Serial controls

After PASS:

```text
S = perform another Wi-Fi scan
I = print current C6/link information
```

## Failure rule

If the sketch reports `FAIL`, do not flash custom C6 firmware. Copy the **complete Serial Monitor output** first so the failed hosted step can be diagnosed while the factory recovery path is still intact.

## ESP-IDF project in the neighbouring folder

`hosted-link-qualification/` is retained as a low-level reference/alternate diagnostic project, but it is **not required for the normal Showduino Arduino workflow**.

## Next gate after PASS

The next job is the real Showduino transport:

```text
Director / nodes
   -> ESP-NOW
onboard C6 Showduino service
   -> internal P4/C6 transport
P4 Show Engine
```

with the reverse path for P4 ACK/state replies.
