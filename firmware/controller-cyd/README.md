# Showduino v1 CYD Controller Firmware

This folder will contain the ESP32 CYD touchscreen controller firmware.

## Purpose

The CYD controller is the local front panel for Showduino.

It should allow a user to:

- Boot the system
- See status
- Trigger relays manually
- Run diagnostics
- Browse SD card shows
- Start/stop shows
- Access settings
- Trigger emergency stop
- Communicate with the Mega executor

## Source Reference

Primary source repo:

```text
sumkindafreak/Showduino-controller
```

Useful known features from source:

- TFT_eSPI display
- XPT2046_Bitbang touch
- SD card
- WiFi
- WebServer
- ArduinoJson
- ESPmDNS
- Adafruit_NeoPixel
- Mega serial link
- UI theme colours

## Initial v1 Scope

The first controller build should only do this:

1. Boot screen
2. Draw main menu
3. Send `HEARTBEAT`
4. Read `STATUS:ALIVE`
5. Button: Relay 1 ON
6. Button: Relay 1 OFF
7. Button: Relay 1 PULSE 1000ms
8. Button: Emergency Stop
9. Button: Emergency Clear
10. Serial debug log

Do not add the full dashboard, SD show browser, or advanced UI until this baseline works.

## Planned Modules

```text
controller-cyd.ino
config.h
display_ui.h / display_ui.cpp
touch_input.h / touch_input.cpp
serial_bridge.h / serial_bridge.cpp
status_panel.h / status_panel.cpp
settings.h / settings.cpp
sd_manager.h / sd_manager.cpp
```

## Hardware Pin Draft

See:

```text
docs/hardware-pinout.md
```

## Compile Target

Recommended Arduino IDE settings will be documented after the first baseline sketch is created and tested.

## Safety Requirement

Emergency stop must always be available from the main screen once the UI is beyond the first test stage.
