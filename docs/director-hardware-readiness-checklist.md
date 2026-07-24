# Showduino Director — Hardware Readiness Checklist (Phase 6)

Use this checklist on the physical 800x480 Director touchscreen hardware (JC8048W550C + GT911).

## 1) Boot and initial health

- [ ] Splash/startup sequence renders correctly with no clipped UI.
- [ ] Status bar appears and updates time/link state.
- [ ] Permanent dock is visible (`Desktop | Live | Shows | More | E-STOP`).
- [ ] E-STOP button is visible and tappable immediately after boot.
- [ ] Initial Stage link state is shown honestly (connected/searching/disconnected).
- [ ] Initial P4 audio status is shown honestly (reported/offline/not reported).
- [ ] Initial SD/package state is shown honestly (mounted/unavailable/recovery).
- [ ] No-show state is clear and actionable (no blank cards).

## 2) Navigation and routing consistency

- [ ] Dock routes load correct screens: Desktop, Live, Shows, More.
- [ ] More launcher routes: Nodes, Audio, Logs, Settings, Diagnostics, Maintenance.
- [ ] Show Details back returns to Shows (without unloading selected show).
- [ ] Node Details back returns to Nodes.
- [ ] Diagnostics back returns to More.
- [ ] Maintenance back returns to Settings.
- [ ] About back returns to Settings.
- [ ] Logs back returns to More.
- [ ] Repeated route switching does not freeze, flicker, or load stale content.
- [ ] Long-duration screen switching (several minutes) remains stable.

## 3) Touch ergonomics and interaction safety

- [ ] Primary buttons are reliably tappable with gloves/fingertips.
- [ ] E-STOP touch target is large and reliable under stress.
- [ ] Adjacent dangerous controls are visually separated.
- [ ] Scroll gestures do not trigger adjacent buttons unexpectedly.
- [ ] Scroll regions end above the dock; dock remains accessible.
- [ ] Confirmation modal buttons are not too close for accidental activation.
- [ ] No clipped controls in modals or overlays at 800x480.

## 4) Emergency behavior validation

- [ ] E-STOP press sends one intentional request (no rapid flood on hold).
- [ ] Emergency overlay appears above all pages/modals.
- [ ] Non-emergency controls are blocked while emergency overlay is active.
- [ ] Pending destructive confirmation dialogs are cancelled on emergency activation.
- [ ] Overlay shows honest fields for source/reason when unavailable.
- [ ] `Clear requested — awaiting Stage confirmation` appears when applicable.
- [ ] Overlay cannot be dismissed while emergency remains active.
- [ ] Clearing emergency does not auto-resume show playback.
- [ ] Clearing emergency does not auto-resume normal audio.

## 5) Show workflow checks

- [ ] Scan packages from Shows.
- [ ] Open Show Details from selected package.
- [ ] Load show and confirm status transitions.
- [ ] Open Live from loaded show.
- [ ] Start, pause, resume, and stop commands reflect Stage confirmations.
- [ ] Completion overlay appears only on confirmed finished state.
- [ ] Emergency during running show transitions to safe emergency UI path.

## 6) Nodes, diagnostics, and maintenance checks

- [ ] Nodes page shows grouped/ordered states without duplicate cards.
- [ ] Node discovery pending/success/fail states are honest.
- [ ] Diagnostics telemetry updates without requiring page rebuild.
- [ ] Maintenance actions are blocked during active show/emergency as expected.
- [ ] Destructive maintenance actions require confirmation.

## 7) Logs and long-run behavior

- [ ] Logs filter changes are responsive and accurate.
- [ ] Pause/resume behavior is clear; unseen counter increments while paused.
- [ ] Latest button returns to live tail behavior.
- [ ] Clear logs uses confirmation and does not execute twice.
- [ ] No unbounded UI object growth during extended logging sessions.

## 8) Stability and resource checks

- [ ] Device remains responsive during 30+ minute idle soak.
- [ ] Device remains responsive during 30+ minute active-show soak.
- [ ] No obvious heap/PSRAM collapse under repeated navigation.
- [ ] No duplicate callbacks (single tap = single action).
- [ ] No null-screen loads or stale pointer crashes observed.

## 9) Release sign-off notes

- Build command used:
  - `arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi firmware/director-esp32-8048s050/ShowduinoDirector8048S050`
- Record firmware size, RAM usage, board revision, and test firmware SHA.
- Attach photos/videos for emergency overlay and route regression checks.
