# Source Repository Audit

This document tracks every previous Showduino-related repository and what should be reused, referenced, rewritten, or archived for Showduino v1.

## Summary

The aim is not to blindly copy everything. The aim is to rescue the strongest working ideas, clean them up, and place them into a stable product structure.

## Repositories

### 1. SHOWDUINO---SUE

Repository:

https://github.com/sumkindafreak/SHOWDUINO---SUE

Primary value:

- Best modular firmware structure so far
- ESP32-S3 / SUE node work
- FastLED control
- RTC support
- SD card support
- Relay outputs
- PCM5102A I2S audio
- ESP-NOW communication

Recommended v1 use:

- Use as the foundation for `firmware/sue-esp32s3-node/`
- Reuse modular file layout style
- Reuse `config.h` style pin mapping
- Reuse ESP-NOW command concept
- Review audio code before merging

Risk / cleanup:

- Confirm hardware target and exact board variant
- Confirm pins against final SUE hardware
- Keep SUE separate from main CYD controller firmware
- Avoid mixing SUE single-node logic with Mega executor logic

Status:

- Keep and migrate carefully

---

### 2. new_showduino

Repository:

https://github.com/sumkindafreak/new_showduino

Primary value:

- Empty / clean starting point
- Being used as Showduino v1 bootstrap repo

Recommended v1 use:

- Rename this repository to `showduino_v1` in GitHub settings
- Use as the master repository

Status:

- Active v1 bootstrap repo

---

### 3. Showduino-controller

Repository:

https://github.com/sumkindafreak/Showduino-controller

Primary value:

- CYD / ESP32 touchscreen controller work
- TFT display
- XPT2046 touch
- SD card
- WiFi
- WebServer
- ArduinoJson
- Mega serial link
- UI theme work

Recommended v1 use:

- Use as the foundation for `firmware/controller-cyd/`
- Extract pin map
- Extract touch handling
- Extract SD handling
- Extract UI navigation concepts
- Extract Mega serial protocol usage

Risk / cleanup:

- Remove hard-coded WiFi credentials from committed source
- Convert large monolithic sketch into modules
- Confirm UART pins for Mega connection
- Confirm SD and touch pin conflicts
- Confirm board package settings in docs

Status:

- Strong candidate for touchscreen controller base

---

### 4. showduino-complete-code

Repository:

https://github.com/sumkindafreak/showduino-complete-code

Primary value:

- Useful command handlers
- Relay command logic
- DMX fixture command ideas
- MP3 player command logic
- ESP/Mega communication experiments

Recommended v1 use:

- Mine for protocol ideas
- Rebuild cleanly inside `firmware/executor-mega/`
- Use command names as reference, not necessarily as final code

Risk / cleanup:

- Some files appear partial or fragment-based
- Needs compile verification before reuse
- Should not become the direct v1 base without cleanup

Status:

- Reference and salvage repo

---

### 5. gorefx-shadow-control

Repository:

https://github.com/sumkindafreak/gorefx-shadow-control

Primary value:

- Best modern web dashboard foundation
- Vite
- React
- TypeScript
- Tailwind CSS
- shadcn/ui
- Likely best candidate for GoreFX / Showduino browser UI

Recommended v1 use:

- Use as foundation for `web/gorefx-dashboard/`
- Keep dashboard separate from embedded firmware
- Connect to controller via HTTP/WebSocket/MQTT-style bridge later
- Preserve timeline editor and live control concepts if present

Risk / cleanup:

- Lovable-generated structure may contain generic files
- Needs proper product README
- Needs environment cleanup
- Needs API layer designed around Showduino serial command protocol

Status:

- Strong web dashboard base

---

### 6. showduino-fx-control

Repository:

https://github.com/sumkindafreak/showduino-fx-control

Primary value:

- FX control ideas
- Possible command UI concepts
- May contain earlier dashboard/control logic

Recommended v1 use:

- Reference for live control and effect naming
- Merge only concepts that survive the v1 architecture

Risk / cleanup:

- May overlap with GoreFX dashboard
- Avoid duplicate dashboard systems

Status:

- Reference repo

---

### 7. showduino-scare-control-system

Repository:

https://github.com/sumkindafreak/showduino-scare-control-system

Primary value:

- Scare attraction focused control system ideas
- May contain useful naming, architecture, or UI concepts

Recommended v1 use:

- Reference for attraction-specific language and workflows
- Review for show/scene/timeline ideas

Risk / cleanup:

- Avoid creating another competing app structure
- Pull concepts, not clutter

Status:

- Reference repo

---

### 8. arduino_show_controller

Repository:

https://github.com/sumkindafreak/arduino_show_controller

Primary value:

- Earlier Arduino show controller work
- Likely contains foundational relay/trigger/show logic

Recommended v1 use:

- Reference for Mega executor behaviour
- Check for proven working routines
- Migrate working code only after compile review

Risk / cleanup:

- Older design decisions may conflict with current Showduino plan
- Needs modern protocol alignment

Status:

- Legacy reference / salvage repo

---

## v1 Decision

The clean architecture should be:

- `new_showduino` renamed to `showduino_v1`
- `SHOWDUINO---SUE` becomes the SUE/add-on node source
- `Showduino-controller` becomes the CYD controller source
- `showduino-complete-code` and `arduino_show_controller` feed the Mega executor
- `gorefx-shadow-control` becomes the web dashboard source
- All other repos become references unless specific files prove useful

## Migration Rule

Before code is migrated into v1, it must answer these questions:

1. What hardware does it target?
2. Does it compile?
3. What pins does it use?
4. What commands does it send or receive?
5. Does it duplicate another module?
6. Is it safe for live attraction use?
7. Is it documented well enough for future Tobe to understand?

If the answer is unclear, it goes into `reference/` or stays in the old repo until cleaned.
