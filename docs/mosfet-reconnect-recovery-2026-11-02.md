# MOSFET reconnect and steady recovery candidate

The owner reports that the actual load drops to zero when Director reconnects, not merely its displayed percentage. Director is portable over ESP-NOW; Comms and P4 remain running when its screen is switched off.

Confirmed behavior requested by the owner:

- Hold a steady percentage through a normal control outage and reconnect.
- Pause timed pulses/fades only when the node loses its P4 control link; hold the reached percentage while paused and resume remaining timing when P4 control returns. Turning off only Director must not pause a P4-controlled show.
- Across MOSFET node power loss, boot off and wait for P4 control before restoring saved steady percentages. Cancel timed effects rather than replay a saved timer checkpoint.
- Preserve Emergency/fault/explicit-off handling and no automatic resume after Emergency clear. Keep the existing MOSFET LED system untouched.

## Source findings and candidate changes

The existing node explicitly zeroed all channels on authority loss and on first/reacquired ownership grant. Actual old node code reproduces a held-level assertion failure after its existing eight-second P4 keepalive timeout. The candidate replaces those two zeroing paths with hold/pause and retain/restore/resume. Repeated healthy grants remain keepalives and do not reset outputs. The precise radio/event sequence of the owner's physical Director-only reconnect has not been captured; bench validation must confirm that this node-side path addresses the observed drop rather than an explicit upstream stop or hardware reset.

Timed effects pause at the existing loss-detection instant, not the unknowable instant a radio link disappears. Short effects may finish before the eight-second timeout. Pulse remaining duration and fade elapsed time are retained in RAM; a paused fade continues its original curve, not a newly rounded fade. Explicit channel commands replace that channel's pending recovery. All-off, Emergency and fault discard all pending recovery.

`MosfetRecoveryStore` uses a separate `sdmosresume` Preferences namespace and one six-byte versioned/checksummed record for four steady levels. Values outside 0–100, wrong versions, malformed records and failed storage initialization are not restored. Changed steady commands, completed fades and safety/off actions save the four-channel state; identical values do not rewrite flash. Starting a pulse or an unfinished fade saves zero for that channel, so timed effects do not resume after node power loss. Failed writes are reported and retried no more than once per second in the running node. Boot restoration uses the last successfully committed record, not an unsaved physical instant; storage/power-failure behavior still needs hardware acceptance.

Outputs initialize to zero on node boot. Saved steady levels are applied only when an ownership grant actually moves the node into P4-controlled state. A new local command while waiting replaces its saved channel; Emergency/fault/all-off cancels restoration. The candidate firmware identifies as `0.1.4-rc1`.

Output changes, ownership transitions, pause/restore snapshots and storage commits are serialized with one recursive FreeRTOS mutex because radio callbacks and loop service run on different tasks. This prevents a delayed old save from overwriting a later Emergency/off record or a concurrent owner tick from replacing an Emergency state. Failed lock creation leaves boot outputs off. Host tests deliberately overlap an old commit and a later Emergency on two threads and verify cold recovery stays off. Synchronous flash operations and lock contention can add command latency; physical timing must still be measured.

P4's MOSFET routing now sends Emergency stop instead of an ownership grant on discovery and periodic keepalive while `emergencyLocked` is true. This prevents a rebooting node with saved levels from being granted restoration during an active P4 Emergency. Existing clear behavior remains all-off and requires fresh output commands. This change requires review of both node and matching P4 candidate before deployment.

No MOSFET identifier/status LED source, palette, animation, pin or count changes. No new FastLED path is enabled. Shared ownership behavior for other node types is unchanged.

## Evidence and physical gate

Host tests compile actual node output/state/storage code and actual P4 MOSFET routing with GPIO/PWM/Preferences/clock/transport doubles. The old held-level regression is reproduced. Candidate cases include steady reacquisition, warm timed pause/resume, cold steady restore/wait-for-grant, cold timed cancellation, local overrides, fade completion, Emergency/fault/off invalidation, malformed storage, failed-save retry, identical-save suppression, clock rollover and P4 Emergency discovery/keepalive routing. CI also runs sanitizers, existing MOSFET tests and complete node/P4 reference builds.

These tests cannot validate physical PWM, flash durability under brownout, real ESP-NOW packet loss/order, boot timing, wiring, power-rail interactions or loaded output behavior. NVS failures can leave an older committed record; diagnostics and fault/power-cycle testing are required before accepting persistent restoration. Do not equate software tests with commissioned hardware safety.

Before upload, review exact build settings, original rollback images, the node/P4 pairing and intended connected loads. On the bench, set distinguishable percentages on all four channels, cycle only Director and verify uninterrupted actual outputs; separately interrupt node/P4 control long enough to trigger the existing timeout and verify pulse/fade hold and remaining-time resume. Cycle node power: require boot off, restore only steady channels after valid control, cancel timed channels. Assert Emergency during outage/reconnect/node reboot: require all outputs off through grants and clear. Verify fault/explicit off, repeated cycles, persistence diagnostics and unchanged indicator behavior. Rerun the 22-second full-node show and rollback on regression.

This is a separate review candidate, not a merge or hardware upload. Original bench source remains preserved. Audio Stage B is a separate draft and its acceptance is unaffected by these MOSFET changes.
