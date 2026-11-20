# Showduino V1 — Quick Start

**Revision:** 1.0-RC documentation rewrite, 19 November 2026  
**Reviewed GitHub source:** [`main` at `41c7c71`](https://github.com/sumkindafreak/new_showduino/tree/41c7c71ac617abdf9feeedf2064e6b5a9f31cc15)  
**Audience:** owner, operator and supervised bench tester  
**Status:** Source-reviewed instructions, **NOT** physical acceptance or public-operation approval

> **Safety gate:** The owner reports a working local Emergency-clear fix and an existing 22-second show. This is not the same as two logged complete runs plus a signed Emergency interruption/clear/no-auto-resume pass. Do not attach hazardous effects or admit guests until the actual installed build has passed venue-specific acceptance. Never use a reboot as a substitute for an authorised Emergency clear.

## The rule to remember

**P4 decides. Comms transports. Director requests and displays. Nodes act.**

Showduino can run a previously loaded show locally without a browser, Director, venue Wi-Fi or Internet. An online Communications S3 does **not** prove the P4 is ready.

## 1. Before switching on

1. Use a supervised, low-risk test installation. Disconnect smoke, motion, mains-switched and other hazardous loads for first commissioning.
2. Record the exact flashed firmware/build and settings of the **P4 Show Engine**, **Comms S3**, **Director**, and each node. Back up the P4 SD card and node media; do not replace a working flash with GitHub `main` simply because the versions differ.
3. Inspect power, wiring, polarities, grounds, fusing, antenna clearance, SD card and the P4's physical Emergency button. Fix any damaged connection first.
4. Ensure the physical Emergency input and separate site-required safety systems are in good order. Showduino's theatrical Emergency function is **not** a certified life-safety or machinery emergency-stop system.

See the [Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) and [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md).

## 2. Start the core

1. Power the **P4 Show Engine** and verify it boots without an unresolved Emergency or storage error.
2. Start the **dedicated Communications ESP32-S3** and establish the UART link to the P4.
3. Start the **JC8048W550C Director** and verify its P4-linked status; then start the required specialist nodes.
4. Do not infer readiness from a bright display or connected Wi-Fi alone. Confirm P4 state, selected/loaded production, node readiness and relevant fault indicators.

**Reference wiring:** P4 GPIO4 RX ← Comms S3 GPIO17 TX; P4 GPIO5 TX → Comms S3 GPIO18 RX; common ground; **115200 8N1**. Never wire UART TX to TX. See [hardware pinout](../hardware-pinout.md).

## 3. Connect a browser (optional)

The Communications S3 hosts a local **Showduino** Wi-Fi access point. Connect using the credential configured **on that installation**, then open the gateway URL; factory/bench networks commonly use `http://192.168.4.1/`. The source contains a development password; it is **not** a suitable public-venue secret. Obtain the actual access details from the installer, not a public manual.

- **System Console:** Comms-hosted home, Productions, Live, Outputs, Devices, Network, System and Settings pages. This controls/commissions the installed system; the P4 still confirms commands.
- **Embedded `/studio/`:** a packaged authoring snapshot, potentially different from current Website Studio.
- **Website Studio:** wider authoring environment, with its own export and compatibility limits.

Internet/venue Wi-Fi are optional. The Comms **Network** page offers Scan, Connect, Disconnect, Forget and AP only. A failed external Wi-Fi connection is not the same as a failed P4 show link.

## 4. Select and run an **existing** production

For the safest first test, **reuse the known-good, already-persisted P4 SD production**, rather than recompiling the show:

1. With the P4 linked, open **Productions** on the Director or Comms System Console.
2. Select the production and request **Load**. Wait for the P4 to confirm that it has loaded; do not confuse selecting, persisting and loading.
3. On the Director, check the ready state and intended output/node routing. Keep hazardous loads isolated.
4. Request **Start/Run Show**, and observe the **Live** page and physical outputs. The P4 owns timing, cue state and final completion.
5. Record completion (or fault), actual audio/pixel/MOSFET response and logs. To repeat, deliberately load/start according to the displayed P4 state.

A sent command, browser preview or local LED animation does not prove the production executed. Refer to the [User Manual](SHOWDUINO_USER_MANUAL.md#running-and-stopping-a-production) for stop, pause, resume and faults.

## 5. For a **new** authored production

1. In Website Studio, create the production, add lanes/cues, set exact logical node IDs and media/output routes, preview and run the Project Check.
2. Export a **SHDO v2** `.shdo` document. Creative/editor compatibility is **not** the same as installed P4 compiler support.
3. Open **Comms System Console → Productions → Persist SHDO v2 to P4 SD → Persist to SD**, choose the export and read the **P4 result**.
4. A successful persist **does not auto-load or auto-start**. Separately **Load**, check readiness, then **Start** from an authorised operator surface.
5. If compilation is rejected, preserve the error and compare the export with the *installed* P4 software. Do not flash a different P4 or bypass rejection simply to make a demo work.

See [Studio Cue Library](SHOWDUINO_STUDIO_CUE_LIBRARY_GUIDE.md), [production storage](../production-storage.md) and [source matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md).

## 6. Emergency and Locate

- A valid press of the **main P4 physical button** (GPIO25 to ground, active LOW, debounced) **latches Emergency**. It does not clear when released.
- Continuing to hold for **8 seconds** additionally requests **Locate**: the Director wakes and indicates where it is. Acknowledge Locate on the Director; this **does not clear Emergency**.
- If the physical button is released, inspect and correct the cause. On the **Director Emergency** screen request **CLEAR EMERGENCY**, then review **Clear Emergency Stop?** and deliberately press **CONFIRM CLEAR**. The P4 may reject or time out the request.
- Confirm the **P4 reports Emergency clear** and the outputs are safe. **Clearing does not automatically resume** the show. A separate deliberate run/resume decision is required, subject to safety policy.
- The Comms browser **Live** page provides **PANIC** for asserting Emergency; it is not the authoritative normal clearance path.

**Do not** hold the button to clear, reset the Director/P4 as a shortcut, override the P4 state, or use ordinary pixel cues to mask Emergency white.

## 7. Before opening to guests

Use the [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) **on the actual venue hardware**. Record at least **two consecutive P4-confirmed complete runs** and a separate mid-show Emergency → safe outputs → release → Director-confirmed clear → **no automatic restart** test. A documentation PR or host CI pass is not an acceptance pass.

## Where to go next

- [Full User Manual](SHOWDUINO_USER_MANUAL.md)
- [Director Operator Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md)
- [Build and Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md)
- [Open source/bench gaps](SHOWDUINO_MANUAL_GAPS.md)
- [Source-by-source evidence](SHOWDUINO_MANUAL_SOURCE_MATRIX.md)
