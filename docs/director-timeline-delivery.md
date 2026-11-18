# Director timeline delivery

Director 0.9.12 waits for each ESP-NOW send callback before sending the next
command. Previously, sending while a frame was pending returned false
immediately. With UART fallback disabled, the next frame was discarded,
but the caller still counted it and logged a UART send. Production uploads
could lose cues and control frames such as TL:BEGIN and TL:END.

Transport success now requires a successful callback within 250 ms. Busy,
timeout, immediate send errors and oversized commands report failure. The
callback confirms gateway radio delivery; P4's end acknowledgement and cue
count still confirm the complete timeline.

The uploader checks LOAD, BEGIN, every cue and END, rejects skipped/empty
timelines, and records the production ID after P4 confirms the cue count.
RUN uploads again when the selected ID is unverified or different. Losing
the Stage link invalidates verification. Current and OS2 production controls
report upload failures. Emergency and offline links block RUN.

Only Director needs flashing for this fix. Keep the existing SD bench package
and WAV. Load Audio + Dual Pixel Bench, look for 177 verified cues, then RUN.
The melody must come from the Audio Node and the first colours appear around
three seconds. P4 system sounds alone do not prove node cue delivery.

Validation: ESP32-S3 compiler checks passed for both repository and local
Director sketches. A regression test executes the actual send/run bodies with
simulated radio callbacks: 177 consecutive commands, busy sends, failed
callbacks, timeout, immediate rejection, size limits, uptime wrap and preventing
RUN after a failed/different-production upload. Full binary linking and bench
playback remain unverified. Serial capture is still needed to establish whether
this defect caused the reported bench failure.
