# Hardware acceptance — persistent emergency latch (P4)

Not yet performed. Mark each result PASS/FAIL with date, firmware version and serial log excerpt. Do not mark the feature hardware-validated until every item passes.

Preconditions: P4 flashed from `fix/emergency-persistent-latch`; GPIO25 wired NO-to-GND (INPUT_PULLUP); emergency.wav on SD; Director connected.

1. Normal boot, latch clear: no `[ESTOP] Restoring` line; status `CLEAR`.
2. Press GPIO25 → latch ACTIVE; Director shows Emergency; pixels white.
3. Release the button. Power-cycle the P4. Expect `Stored latch ACTIVE`, Emergency restored, no show playback, Director shows Emergency.
4. Hold the button during power-up. Expect Emergency restored and no clear. Release. Still ACTIVE.
5. Director clear request while the button is held: rejected `BUTTON_ACTIVE`, still ACTIVE.
6. Director clear request, button released, confirm: `CLEAR_OK`, outputs return to normal idle. Power-cycle: stays CLEAR, no Emergency.
7. Attempt RUN after an emergency without clear: rejected `EMERGENCY_ACTIVE`.
8. Wireless emergency assertion during boot: latch holds after restart.
9. Remove or corrupt the NVS namespace (if you can do so safely) and reboot: expect `storage unreadable or corrupt — fail-safe LATCHED`.
10. Confirm no node resumes an unsafe output after P4 power loss while nodes stay powered (document node-by-node result).
11. Director disconnected during an emergency, then reconnected: Director receives ACTIVE.
12. Record whether the Director shows an unknown `PERSIST_FAILED` reply sensibly (only testable if storage can be forced to fail).
