# Future P4↔C6 SDIO Transport

**Documentation only. Not implemented.**

**Current application path (this generation):** Director → ESP-NOW → dedicated ESP32-S3 Comms Controller → UART → P4.

The Waveshare onboard ESP32-C6 is **UNUSED BY SHOWDUINO / RESERVED HARDWARE**. This note records a possible future use of the factory P4↔C6 SDIO nets. It is not current architecture and must not be treated as a reason to wait for the C6 at boot.

UART remains the Showduino command path until a future SDIO implementation is compiled and physically proven. Keep UART as the fallback.

## What exists today

The Waveshare ESP32-P4-Module routes P4↔C6 **SDIO internally** (no jumpers). Pin map: [`docs/final-hardware-architecture.md`](final-hardware-architecture.md).

Current Showduino commands use **ESP-NOW into the dedicated S3 Comms Controller, UART into P4**. The onboard C6 is not in that path.

## Possible future zero-wire command path

```text
Director
    ↓ ESP-NOW
Onboard C6 (existing ESP-NOW bridge logic)
    ↓ custom SDIO application transport
ESP32-P4 Stage Engine
```

That would require:

- ESP-IDF C6 `sdio_slave` alongside the existing ESP-NOW application
- P4 SDMMC **Slot 1** host (Slot 0 stays on GPIO39–45 for the SD card)
- Bidirectional application protocol for the same colon-text commands
- Reconnect / re-enumeration after C6 reset
- Non-blocking failure: P4 must not wait forever; GPIO25 emergency stays local
- Coexistence testing: C6 ESP-NOW + SDIO slave
- Hardware validation on the Waveshare module

Do **not** confuse this with ESP-Hosted.

## ESP-Hosted (separate future, not Director transport)

[ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu) can give the P4 Wi-Fi/BLE through the same internal SDIO bus ([sdio.md](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/sdio.md), [P4+C6 setup](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md)).

As investigated:

- Official features are STA, SoftAP, BLE HCI — not ESP-NOW ([features.md](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/features.md))
- [EHM-19](https://github.com/espressif/esp-hosted-mcu/issues/19) remains open (ESP-NOW over Hosted is roadmap, not shipping)
- Hosted **slave** firmware would **replace** the Showduino C6 ESP-NOW bridge

**ESP-Hosted is not the current Showduino Director transport.** Do not install it for this hardware generation.

P4 pins GPIO6, GPIO14–19, and GPIO54 stay reserved so this bus remains available later.
