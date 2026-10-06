# Director Outputs page — MOSFET first implementation

The home OUTPUTS tile now opens a MOSFET desk for the first online node identified by P4. Four channels show the node-reported Actual level alongside a separate Set level. Changing a slider sends no command; APPLY sends one requested level (0–100%). OFF and ALL OFF use the existing safe-off commands. IDENTIFY blinks the node status indicator.

All requests use Director → Comms → P4 → MOSFET. Level controls are locked during a running or paused show, an emergency, node fault/offline status, or loss of the P4 link. OFF/ALL OFF remain available during show and emergency when the link and target ID are known. No output is switched on automatically when the page opens or firmware starts.

P4 0.6.9 forwards four channel levels in its existing MOSFET detail report after receiving valid node STATUS data. The Director accepts the old report too, displaying `--` when channel feedback is unavailable. Actual levels are never filled from requested values. The page distinguishes a sent request from a received status report; it does not claim physical-load operation from a command send.

## Installation

Flash the updated Director sketch and P4 0.6.9. MOSFET Node and Comms firmware do not need changes for these commands. The local Arduino `libraries/lv_conf.h` has been updated to enable LV_USE_SLIDER; the repository sketch copy includes the same setting. When building on another machine, keep those two configuration copies synchronized as described in lv_conf.h.

## Bench check

1. Open OUTPUTS with the MOSFET node online. Confirm its actual ID appears and four Actual levels populate.
2. Select a small test level on one channel. Confirm editing does not switch the output. Press APPLY and verify Actual updates from node feedback and the connected load follows.
3. Test individual OFF, ALL OFF, IDENTIFY and REFRESH.
4. Start or pause a production and confirm level controls lock, while OFF stays available. Repeat during emergency.
5. Disconnect the node or P4 link. Confirm level controls lock and stale Actual levels disappear.
6. Return through BACK and the persistent dock; verify no controls collide with the dock.

Scope: one MOSFET node with four channels. Multi-node selection, channel naming, relay controls and other output families remain future work. Physical load and touchscreen checks are required; automated UI checks use mocked hardware.
