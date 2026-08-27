# Showduino ESP32-S3 Comms Controller

```text
Status: ACTIVE
Role: Showduino Communications Engine (dedicated ESP32-S3 Dev Module)
```

Dedicated USB-programmable ESP32-S3 communications processor.

This is **not** the 800×480 touchscreen Director (`firmware/director-esp32-8048s050/`).
It has no LVGL, no display, and no GT911. It is a separate ESP32-S3 Dev Module.

```text
Director ESP32-S3
        |
        | ESP-NOW
        v
ESP32-S3 Comms Controller   (this firmware)
        |
        | UART 115200 8N1
        v
ESP32-P4 Stage Engine
```

The P4 remains the authoritative Stage Engine. This board transports only. The P4 boots and runs locally if this S3 is missing.

## Constitution

> The Communications Engine transports.

It must **not**:

- Run the show timeline
- Own authoritative show state
- Make show-level decisions
- Host SoftAP / WebUI
- Own SD, audio, pixels, or the Plug-in Bus
- Initialise Bluetooth

## Sketch

```text
firmware/s3-comms-controller/ShowduinoS3CommsController/
```

Arduino board: **ESP32S3 Dev Module** (`esp32:esp32:esp32s3`).

USB CDC must remain enabled so this board stays directly programmable and debuggable over USB. The P4 UART uses UART1 on GPIO17/GPIO18, not UART0 and not native USB D+/D−.

### Compile (Arduino core 3.3.11)

```text
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PSRAM=disabled,PartitionScheme=default_8MB" firmware/s3-comms-controller/ShowduinoS3CommsController
```

Do **not** flash from this task's report until the UART pin choice is approved.

## UART pins

Defined only in `BoardConfig.h`:

| S3 GPIO | Function |
|---------|----------|
| **18** | UART RX (from P4 TX) |
| **17** | UART TX (to P4 RX) |

Wiring:

```text
S3 GPIO17 TX  ->  P4 GPIO4 RX
S3 GPIO18 RX  <-  P4 GPIO5 TX
S3 GND        --  P4 GND
115200 8N1, newline-framed ASCII
```

## Phase 1 behaviour

- Receive Director ESP-NOW desk packets (magic `0x5348444F`, version 1, 96-byte command)
- Validate and reject malformed packets
- Remember Director MAC
- Newline-frame commands to the P4 UART
- Forward P4 UART lines back to the Director as desk packets
- Reply `DIAG:PONG` to P4 `DIAG:PING` (local; not forwarded to Director)
- USB maintenance: `HELP`, `STATUS`, `MAC`, `PING:P4`
- `PING:P4` sends `DIAG:PING` and waits for `DIAG:PONG`

USB does **not** inject arbitrary Stage Engine commands.

Copy the boot Wi-Fi MAC into Director `SHOWDUINO_COMMS_MAC_*` in `firmware/director-esp32-8048s050/ShowduinoDirector8048S050/BoardConfig.h`.

## USB power / brownout

Cheap ESP32-S3 Dev Modules often brownout on USB when UART or the Wi-Fi radio starts (`E BOD: Brownout detector was triggered`).

Firmware mitigations (USB sag, not a substitute for 5 V):

- Brownout *reset* is disabled so a short dip does not reboot-loop
- ESP-NOW TX power is reduced (`WIFI_POWER_8_5dBm`)
- `esp_wifi_start()` is not called twice after `WiFi.mode(WIFI_STA)`

First boot: **unplug the P4 UART wires**. If GPIO17 TX is clamped by an unpowered P4, USB current spikes and the chip resets. Power the P4, then connect:

```text
S3 GPIO17 TX  ->  P4 GPIO4 RX
S3 GPIO18 RX  <-  P4 GPIO5 TX
GND
```

Prefer a powered USB hub or the board 5 V pin, not a weak laptop port.

## FUTURE / RESERVED / NOT IMPLEMENTED

This S3 is the intended future owner of:

- Bluetooth LE (mobile commissioning, pairing, local maintenance)
- Wi-Fi SoftAP / STA
- WebUI tunnel / proxy
- OTA / update mechanisms

**None of those are implemented in this firmware.** Do not initialise BLE or Wi-Fi networking here. ESP-NOW uses Wi-Fi STA mode as a radio only.

## Related

| Path | Status |
|------|--------|
| `firmware/p4-c6-espnow-bridge/` | UNUSED BY SHOWDUINO / RESERVED HARDWARE (onboard C6) |
| `firmware/c3-supermini-espnow-bridge/` | LEGACY / SUPERSEDED |
| `firmware/stage-engine-p4/` | ACTIVE Stage Engine |
| `firmware/director-esp32-8048s050/` | ACTIVE Director |
