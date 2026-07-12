# Director to Stage Engine — First Test

This test uses the current Showduino Director command format and the native ESP32-P4 Stage Engine command router.

## Commands under test

```text
HELLO
STATUS:REQUEST
LED:ON
LED:OFF
LED:TOGGLE
EMERGENCY:STOP
EMERGENCY:CLEAR
```

## Director preparation

1. Open:

```text
firmware/director-esp32-8048s050/ShowduinoDirector8048S050/
```

2. Install:

- Arduino_GFX_Library
- LVGL 9
- TAMC_GT911
- XPT2046_Touchscreen only when using a resistive panel

3. Replace the six `SHOWDUINO_P4_C6_MAC_*` values in `BoardConfig.h` with the station MAC printed by the C6 firmware.

4. Upload the Director and open Serial Monitor at 115200 baud.

5. Confirm:

```text
ESP-NOW: Director MAC = ...
ESP-NOW: P4/C6 bridge peer = ...
```

6. Tap `P4 LED TOGGLE` on the desktop or live-control screen.

## P4 preparation

Build and flash:

```bash
cd stage-engine/esp32-p4
idf.py set-target esp32p4
idf.py build
idf.py flash monitor
```

Before wireless integration, verify the command router directly through the P4 USB console:

```text
HELLO
STATUS:REQUEST
LED:TOGGLE
EMERGENCY:STOP
LED:ON
EMERGENCY:CLEAR
LED:ON
```

The proof output is currently GPIO23. Use an LED with a suitable resistor unless the final board LED pin is confirmed.

## Important transport status

The Director side is now synchronized with the uploaded working touchscreen code. The P4 runtime and command router are present.

The production C6-to-P4 path is the board's SDIO connection. The repository's older UART bridge sketch is retained only as an early bench experiment and is not the final built-in-C6 transport. Do not assume GPIO4/GPIO5 are the internal C6-to-P4 connection.

The remaining end-to-end task is to install the supported C6/SDIO firmware and feed received `ShowduinoEspNowPacket` commands into the existing P4 command router.

## Expected Director serial output

```text
ESP-NOW: queued seq=... cmd=LED:TOGGLE result=0
ESP-NOW: radio delivery = delivered
```

A radio delivery result only confirms delivery to the C6 radio. Final Stage Engine ACK support will confirm that the P4 executed the command.
