# Showduino Stage Engine — ESP32-P4

This is the first native ESP-IDF implementation of the Showduino Stage Engine for the **Waveshare ESP32-P4-Module-DEV-KIT**.

The P4 is the authoritative show runtime. The Director is an operator interface; the Stage Engine owns timing, safety and execution.

## Current implementation

- Native ESP-IDF project targeting `esp32p4`
- Runtime state machine:
  - `BOOTING`
  - `READY`
  - `RUNNING`
  - `STOPPED`
  - `EMERGENCY`
  - `ERROR`
- Command router
- Latched emergency state
- Safe-output handling
- Command counters and timestamps
- Ten-second status logging
- USB/serial console command testing
- GPIO23 proof output

## Supported commands

```text
HELLO
STATUS:REQUEST
SHOW:START
SHOW:STOP
LED:ON
LED:OFF
LED:TOGGLE
EMERGENCY:STOP
EMERGENCY:CLEAR
```

`EMERGENCY:STOP` immediately forces registered outputs to their safe state and blocks ordinary commands until `EMERGENCY:CLEAR` is received.

## Proof output

The first proof output is configured as **GPIO23**, which is exposed on the Waveshare 40-pin header.

For a physical LED test, connect:

```text
GPIO23 -> suitable resistor -> LED anode
LED cathode -> GND
```

Do not connect an LED directly without a resistor.

The proof pin is defined in:

```text
main/showduino_gpio_service.c
```

## Build

Use an ESP-IDF release that supports ESP32-P4.

```bash
cd stage-engine/esp32-p4
idf.py set-target esp32p4
idf.py build
idf.py flash monitor
```

Enter commands into the monitor, for example:

```text
LED:TOGGLE
STATUS:REQUEST
EMERGENCY:STOP
LED:ON
EMERGENCY:CLEAR
LED:ON
```

The `LED:ON` command issued during the emergency state should be rejected.

## Source layout

```text
main/
├── main.c
├── showduino_runtime.c
├── showduino_runtime.h
├── showduino_command_router.c
├── showduino_command_router.h
├── showduino_gpio_service.c
└── showduino_gpio_service.h
```

## Next implementation steps

1. Keep the Arduino P4 UART path as the product transport (`firmware/stage-engine-p4/`).
2. Optional later: a custom P4↔C6 SDIO application transport — documentation only until proven. See `docs/future-p4-c6-sdio-transport.md`. Do not install ESP-Hosted as the Director path.
3. Feed received Showduino protocol packets into `showduino_command_router_handle()`.
4. Send ACK and status responses back through the C6.
5. Add Ethernet service startup.
6. Add USB Host service startup.
7. Replace the proof GPIO with modular relay, DMX, audio and lighting services.

The command router is deliberately transport-independent. UART (current), SDIO (future), Ethernet, USB, simulator and local console inputs will all use the same runtime API.
