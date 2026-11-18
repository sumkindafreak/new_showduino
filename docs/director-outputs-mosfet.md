# Director Outputs page — MOSFET first implementation

The home OUTPUTS tile now opens a MOSFET desk for the first online node identified by P4. Four channels show the node-reported Actual level alongside a separate Set level. Changing a slider sends no command; APPLY sends one requested level (0–100%). OFF and ALL OFF use the existing safe-off commands. IDENTIFY blinks the node status indicator.

All requests use Director → Comms → P4 → MOSFET. Level controls are locked during a running or paused show, an emergency, node fault/offline status, or loss of the P4 link. OFF/ALL OFF remain available during show and emergency when the link and target ID are known. No output is switched on automatically when the page opens or firmware starts.

P4 0.6.9 forwards four channel levels in its existing MOSFET detail report after receiving valid node STATUS data. The Director accepts the old report too, displaying `--` when channel feedback is unavailable. Actual levels are never filled from requested values. The page distinguishes a sent request from a received status report; it does not claim physical-load operation from a command send.

## Installation

Flash the updated Director sketch and P4 0.6.10. MOSFET Node and Comms firmware do not need changes for these commands. The local Arduino `libraries/lv_conf.h` has been updated to enable LV_USE_SLIDER; the repository sketch copy includes the same setting. When building on another machine, keep those two configuration copies synchronized as described in lv_conf.h.

## Bench check

1. Open OUTPUTS with the MOSFET node online. Confirm its actual ID appears and four Actual levels populate.
2. Select a small test level on one channel. Confirm editing does not switch the output. Press APPLY and verify Actual updates from node feedback and the connected load follows.
3. Test individual OFF, ALL OFF, IDENTIFY and REFRESH.
4. Start or pause a production and confirm level controls lock, while OFF stays available. Repeat during emergency.
5. Disconnect the node or P4 link. Confirm level controls lock and stale Actual levels disappear.
6. Return through BACK and the persistent dock; verify no controls collide with the dock.

## P4 Local I/O and I2C Bus expansion

The header now selects MOSFET, P4 I/O or I2C BUS. Flash the expanded Director and **P4 0.6.10** together. Comms and the MOSFET Node need no changes.

P4 I/O displays line 1 / GPIO46 and line 2 / GPIO47 from P4 reports. INPUT lines show debounced ACTIVE/INACTIVE state and have no activation control. OUTPUT lines have explicit ACTIVE and OFF controls; ALL OFF is scoped to the local I/O lines. Link loss hides stale state. Shows, pauses and emergencies lock activation and configuration at both Director and P4 desk-command entry points; OFF remains available when linked.

CONFIGURE edits mode (DISABLED/INPUT/OUTPUT), active polarity, input pull and debounce. SAVE sends a single validated configuration command and leaves the line inactive. The P4 reports SAVED or RAM_ONLY_SD_ERROR; configuration is not reported as persistent when the SD write fails. Both pads enter high impedance during early boot and configured outputs start inactive. GPIO48 remains reserved. These are 3.3 V logic lines and require appropriate external interfaces for loads.

I2C BUS shows the P4 bus on SDA GPIO7 / SCL GPIO8, with one detected/configured device per page and NEXT DEVICE navigation. REFRESH requests that page; SCAN BUS performs an explicit idle-only scan. Select the chip and a compatible role, then SAVE ASSIGNMENT. NONE removes that device's role entry. Supported presets are SX1509/MCP23017 digital inputs, outputs or mixed I/O; PCA9685 PWM or servo outputs; and TCA9548A multiplexer. Preset assignment configures identity/role only; individual expander channels and servo/PWM control are not implemented by this page.

Assignments are written atomically to `/showduino/config/plugin-bus.json`, preserving other entries and validated with the existing role-file parser. The internal ES8311 audio address 0x18 is protected. Assignment editing currently supports root-bus addresses only; devices behind multiplexers are displayed but cannot be edited here. An invalid existing role file must be repaired before the editor can save. Scan and assignment editing are locked during shows, pauses and emergencies. Feedback distinguishes successful saves from errors and does not infer connected-load behavior.

Additional bench checks: configure a line as INPUT and verify both states; configure OUTPUT and verify boot inactive, ACTIVE, OFF, STOP ALL and emergency behavior. Check polarity/pull/debounce persistence after reboot. Attach a supported I2C device, scan, assign a compatible preset, verify persistence and verify that internal audio remains protected. Repeat save with unavailable SD and confirm the error is visible. Physical load and touchscreen checks remain required; automated UI/backend checks use mocked hardware.

Scope: one MOSFET node with four channels, two P4 digital lines, and root-bus preset assignment. Multi-MOSFET selection, channel naming, relay controls, and per-channel plugin operation remain future work.
