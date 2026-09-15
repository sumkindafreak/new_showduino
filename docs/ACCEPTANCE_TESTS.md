# Showduino Acceptance Tests

This file documents realistic acceptance checks for currently implemented functionality in this repository.

Test-type labels used here:
- **AUTOMATED**
- **HOST TEST**
- **BENCH TEST**
- **MANUAL UI TEST**
- **NOT YET IMPLEMENTED**

## 1) Architecture and authority

- Verify P4 remains authoritative for runtime/show/emergency state (`SHOW/API/state` from P4 paths) — **BENCH TEST**.
- Verify Comms transports/proxies but does not invent runtime authority when P4 is offline — **BENCH TEST**.
- Verify Director acts as requester/display surface and a running show survives Director loss — **BENCH TEST**.

## 2) Boot and startup

- P4 boots with/without SD card and reports degraded state appropriately — **BENCH TEST**.
- Comms S3 boots, starts ESP-NOW + SoftAP, and serves WebUI — **BENCH TEST**.
- Director boots and reconnects to Comms on expected channel behavior — **BENCH TEST**.

## 3) Runtime continuity

- Running show continues if Director disappears — **BENCH TEST**.
- Running show continues if browser disconnects — **BENCH TEST**.
- Runtime does not require internet access — **BENCH TEST**.

## 4) Communications

- Director commands traverse Director -> Comms ESP-NOW -> P4 UART and return status correctly — **BENCH TEST**.
- Comms `/api/comms` remains available even if P4 UART link is down — **BENCH TEST**.
- Missing/offline node is surfaced safely (degraded/offline), without crashing runtime — **BENCH TEST**.
- Malformed protocol input is rejected safely (no runtime crash) — **HOST TEST** + **BENCH TEST**.

## 5) Studio/WebUI

- WebUI loads from Comms S3 at AP endpoint and reflects P4 authoritative state — **MANUAL UI TEST**.
- UI reconnect does not incorrectly reset authoritative state — **MANUAL UI TEST**.
- UI polling does not destroy actively edited forms (especially network credentials flow) — **MANUAL UI TEST**.
- Mobile usability remains acceptable at realistic phone widths — **MANUAL UI TEST**.

## 6) Networking

- SoftAP behavior remains stable during optional STA join/leave workflows — **BENCH TEST**.
- Internet unavailable state is reported without implying full runtime failure — **MANUAL UI TEST** + **BENCH TEST**.
- Password/credential fields are not leaked in status JSON or logs — **HOST TEST** + **MANUAL UI TEST**.

## 7) Storage

- P4 storage directories and storage-version handling remain valid (`/showduino/...`) — **HOST TEST** + **BENCH TEST**.
- Production list/load/unload/status behavior matches current format support — **BENCH TEST**.

## 8) Show loading and timeline

- Persistent production v1 TEST/LOG cue handling remains intact — **HOST TEST** + **BENCH TEST**.
- Studio RAM timeline upload endpoint enforces envelope policy and does not auto-start unless implemented to do so — **HOST TEST** + **BENCH TEST**.

## 9) Outputs and nodes

- P4 GPIO23 segmented show-pixel controls operate as expected — **BENCH TEST**.
- P4 GPIO24 designated-signage behavior matches configured normal/emergency patterns — **BENCH TEST**.
- Audio/Lamp/Pixel node command and status paths preserve current role boundaries — **BENCH TEST**.

## 10) Audio (if applicable)

- P4 local ES8311 path remains system/safety audio only — **BENCH TEST**.
- Audio Node remains program-attraction audio path where configured — **BENCH TEST**.

## 11) Pixels (if applicable)

- Emergency overrides normal pixel FX/output behavior per documented rule — **BENCH TEST**.
- Clearing emergency does not auto-resume interrupted FX — **BENCH TEST**.

## 12) Emergency/safety

- Physical emergency input latches on trigger and remains latched until valid clear path — **BENCH TEST**.
- Emergency overrides normal show behavior immediately and safely — **BENCH TEST**.
- Emergency clear cannot bypass required physical/logical safety conditions — **BENCH TEST**.

## 13) Offline and failure handling

- Loss of Director or browser does not halt active runtime — **BENCH TEST**.
- Comms restart window is handled as degraded transport, not fabricated show-state reset — **BENCH TEST**.

## 14) OTA (if applicable)

- Comms self-OTA preconditions and gating (maintenance/show/emergency/integrity) work as documented — **BENCH TEST**.
- Non-Comms component OTA requests return unavailable/not implemented behavior as expected — **HOST TEST** + **BENCH TEST**.

## 15) Regression and host automation

Run existing host-side checks where relevant:

- `tools/protocol-tests/run_tests.sh` — **HOST TEST**
- `tools/production-tests/run_tests.sh` — **HOST TEST**
- `tools/storage-tests/run_tests.sh` — **HOST TEST**
- `tools/plugin-bus-tests/run_tests.sh` — **HOST TEST**
- `tools/e131-tests/run_tests.sh` — **HOST TEST**

Additional PowerShell runners in `tools/*/run_tests.ps1` are available for Windows host validation.

## 16) Not yet implemented / out of scope checks

- Full production OTA across all components — **NOT YET IMPLEMENTED**.
- Broad production `AUDIO`/`PIXEL` persistent cue support where explicitly documented as future work — **NOT YET IMPLEMENTED**.
