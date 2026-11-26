# Scouting Mission 001 — JC8048W550C Director

**Research status:** source-informed shortlist; **not** permission to change the active firmware.  
**Date:** 19 November 2026

## Actual system found in GitHub

The canonical Director source currently contains:
- ESP32-S3, 800 × 480 RGB ST7262-style panel, GT911 touch controller.
- Arduino_GFX, LVGL 9, TAMC_GT911.
- Dedicated RGB bounce-buffer configuration; two 40-line LVGL partial draw buffers, allocated from PSRAM where available.
- LVGL built-in PSRAM-backed allocation pool; backlight handling, touch calibration, ESP-NOW and node/diagnostic pages.
- An existing central motion policy that suppresses cosmetic animation on emergency.
- Existing node/diagnostics/settings card implementations and a separate OS2 card widget.
- SD-supported miniature BMP show thumbnails.

Do **not** infer '52% of the Director used' from a flash percentage. Flash occupancy does not measure graphical ability, CPU headroom, RAM safety or available feature capacity.

## Research candidates

| Candidate | Relevance | Integration risk | Decision |
| --- | --- | --- | --- |
| Espressif ESP32_Display_Panel | Board pin/timing/config comparison and library maintenance | High if used to replace working graphics stack | Reference first, no immediate migration |
| LVGL 9 built-in performance/memory instrumentation | FPS, refresh, draw and heap evidence | Low in test-only build | First diagnostic experiment |
| Existing LVGL 9 primitives | Reusable cards, labels, progress, trends and diagnostic styles | Low–moderate | Recommended first UI enhancement |
| LVGL PC simulator | Prove layouts without repeated reflashing | Moderate | Explore after baseline |
| chipguyhere RasterFireRainSnow | Isolate scene algorithms for low-resolution preview tiles | Moderate–high | Algorithms only; no SPI driver transplant |
| Shadowtrance JC8048W550C examples | Board-specific implementation reference | Unknown until reviewed per revision | Comparison only |
| JC8048W550C AVI player | Example of board-side SD media use | High resource and scheduling cost | Defer |

## Primary sources

- Espressif display panel: https://github.com/esp-arduino-libs/ESP32_Display_Panel
- LVGL: https://github.com/lvgl/lvgl
- Espressif LVGL integration: https://github.com/espressif/esp-bsp/tree/master/components/esp_lvgl_port
- LVGL desktop port: https://github.com/lvgl/lv_port_pc_vscode
- Shadowtrance board example: https://github.com/Shadowtrance/jc8048w550c
- Raster scenes: https://github.com/chipguyhere/ESP32-CYD-Resistive_display/tree/main/examples/RasterFireRainSnow
- Raster FPS test: https://github.com/chipguyhere/CYD-Raster-FPS-Stress-Test
- JC8048W550C AVI: https://github.com/MarianoItalian/AVI-Player-for-JC8048W550C

The Raster FPS project does **not** list JC8048W550C among its supported boards. It targets SPI CYD implementations rather than the Director's RGB display architecture.

## Hardware and regression traps from Showduino source

1. Preserve the known working RGB timings and pin assignments in BoardConfig.h; compare upstream mappings only in a separate experiment.
2. GPIO19/20 are GT911 I2C. Keep USB CDC On Boot disabled for the current CH340/UART0 bench baseline.
3. GPIO17 drives Director ambient NeoPixels. GPIO18 is the GT911 interrupt pin.
4. Preserve SD-before-RGB boot order; the source documents an interrupt watchdog issue when heavy SPI is attempted while RGB DMA is active.
5. Preserve the immediate boot READY → main screen hand-off. The source documents an RGB freeze caused by a fade at this boundary.
6. Do not increase draw buffers, add a framebuffer, or enable large assets before measuring internal heap, PSRAM and render latency.
7. Never let a preview effect generate physical cue commands or conceal emergency state.
8. The Director is not the S3 Communications Engine, P4 Stage Engine, or canonical Studio host.

## First actionable opportunity

Build a **read-only benchmark and fault evidence pipeline**, then make one enhanced node card behind a test toggle *after* the GitHub/bench crossover. Keep the visual preview engine for a later, measurable experiment.

## Outstanding verification

Physical Director logs, actual firmware partition map, current versions of Arduino core/LVGL/Arduino_GFX, P95 touch response, P95 render time, max UI stall, free internal heap, largest free block, ESP-NOW reconnect timing, 24-hour stability, emergency-test evidence and rollback time must be captured on the reconciled hardware build.
