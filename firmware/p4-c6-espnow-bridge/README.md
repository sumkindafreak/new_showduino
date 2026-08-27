# Showduino P4-C6 ESP-NOW Bridge

```text
Status: UNUSED BY SHOWDUINO / RESERVED HARDWARE
Role: Historical onboard ESP32-C6 Communications Engine
```

This firmware is **not** the current Communications Engine.

Current path:

```text
Director ESP32-S3 --ESP-NOW--> dedicated ESP32-S3 Comms Controller --UART--> P4
```

See `firmware/s3-comms-controller/`.

The Waveshare onboard ESP32-C6 remains on the P4 module. Showduino must not require C6 firmware, ESP-NOW, SDIO, ESP-Hosted, or WebUI on the C6. Do not erase or flash the onboard C6 for this product path. Its reserved P4 nets (GPIO6, GPIO14–19, GPIO54) stay reserved.

This folder is retained as historical source for the previous C6 UART/ESP-NOW bridge.

Related: [`docs/repository-status.md`](../../docs/repository-status.md), [`docs/final-hardware-architecture.md`](../../docs/final-hardware-architecture.md).
