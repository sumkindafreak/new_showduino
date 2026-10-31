# Director emergency clearance recovery

## Source and scope

Main inspected: d5d8f2e7b629113ce64dc609f739544faa2a3502. Source identities: Director 0.9.14-director, Comms S3 0.5.4, P4 0.6.8, platform 1.0.0-rc.1. These are source labels, not verified versions flashed on the user's boards.

Preservation commit bf113e4 imports the user's complete 24 October reconciled Director folder. Its sketch matches the later uploaded ShowduinoDirector8048S050(1).ino; Cue Library callbacks, state, and scene navigation are retained. Review the subsequent safety change independently. Do not replace this sketch with main's older sketch.

## Failure and behavior

The emergency screen disabled CLEAR before checking delivery, and had no response deadline. Comms forwards Director commands to P4 over UART and forwards P4 responses to Director once over ESP-NOW; it provides no application delivery guarantee. A lost response or a full queue can therefore strand the old UI. The newer Director transport also used a callback-owned busy flag, which a missing callback could leave set.

Director now tracks request -> P4 CLEAR_REQUEST -> operator CONFIRM -> P4 result. Request and result waits last 4 seconds; operator confirmation expires locally after 10 seconds (P4's existing request window is 12 seconds). Failed queue submission immediately restores retry. Missing response, confirmation, or result restores retry with a visible message and best-effort cancellation. Late/duplicate acknowledgments outside a request are ignored. Confirmation is sent only following a current acknowledged request and explicit operator action. No automatic confirmation retransmission, latch release, or show resume is introduced. ABORT sends STOP without requesting clearance.

Emergency commands bypass a stale software send-busy gate and rely on ESP-NOW queue acceptance plus application responses. Normal timeline transport remains serialized. A successful Wi-Fi submission is explicitly not P4 authorization. A full SDK queue still rejects submission; the operator can retry clearance. E-STOP remains independent of the clearance state machine. No queue can guarantee delivery over a failed radio.

P4's physical input, pending request, deadline, and assertion-generation checks remain authoritative and unchanged. Its authorized clear transitions runtime to safe idle. Comms and P4 firmware are inspected but unchanged. Existing legacy wire messages lack transaction IDs; this patch does not add replay protection. P4 continues to enforce its current pending request and emergency assertion generation.

## Separate Cue Library fix

DisplayManager::showPage closes Cue Library, whose root belongs to the persistent active LVGL screen rather than a page layer. Its existing delete callback removes the timer and clears pointers. This prevents its overlay surviving page navigation. This is a separate commit; verify navigation and emergency preemption on the actual display.

## Validation and hardware acceptance

Host suites: Director (including new production clearance-state tests), emergency-button/P4 clear policy, and protocol. New coverage includes lost request/acknowledgment, late/duplicate acknowledgment, confirmation deadline, lost confirmation/result, cancellation/send-rejection recovery, rollover, and 100 repeated request-confirm/cancel cycles. These are host state-machine tests, not actual RF packet-loss tests or LVGL interaction tests.

Before release, with hazardous loads disconnected:

1. Record all three boards' boot versions. Flash only a reviewed, successfully built Director image compatible with the actual display settings.
2. Repeat Director E-STOP -> bottom CLEAR -> P4 CLEAR_REQUEST -> visible CONFIRM -> explicit confirm -> P4-authorized clear at least 100 times. Check outputs and runtime remain stopped until a separate deliberate start.
3. Disconnect Comms/P4 or drop each request, acknowledgment, confirmation, and result in turn. Within the deadline Director must offer retry while outputs remain safe. Reconnect and complete a fresh explicit workflow without reset. Never infer clearance from a timeout.
4. Hold the physical E-STOP during request and during confirm: P4 must reject. Assert another emergency between request and confirm: the old confirmation must not release the latch.
5. Exercise E-STOP while traffic is busy and during each clearance phase. Simulate missing Wi-Fi callbacks/full queues; E-STOP must remain eligible for submission and clearance must recover safely.
6. Navigate Cue Library -> Desktop/Live/Shows/Settings repeatedly, reopen it, then assert emergency while it is open. Verify no surviving overlay, timer, or hidden touch target.

Hardware acceptance remains unverified. Do not merge as a hardware-qualified release based solely on host tests.

## Firmware build attempt

Arduino CLI 1.5.1 with installed ESP32 core 3.3.12 was invoked for `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app`, using the installed user libraries. The initial restricted invocation could not initialize its default package indexes. Explicit installed package/library paths reached build preparation but did not produce a compiler result during this session; the stalled attempt was stopped. No successful full firmware compile or flash is claimed. The FQBN is a compile attempt, not confirmation of the user's actual flash/display configuration. Rebuild with the verified board settings before deployment.

The loss harness also runs the production P4 clear authorization header together with the Director flow, dropping each protocol leg and checking that a new assertion or held physical input prevents authorization. Firmware integration, radio delivery, and LVGL behavior still require the hardware checklist above.
