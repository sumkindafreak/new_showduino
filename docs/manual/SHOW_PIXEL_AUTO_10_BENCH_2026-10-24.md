# Show pixel lines: automatic ten-pixel minimum (candidate)

This change is proposed for the non-DMX bench-test build. It has **not** been physically accepted.

## Intended behaviour

- P4 Show Pixel Line (GPIO23): enabled with 10 pixels by default; existing SD settings with 0–9 pixels or disabled output are repaired at boot. Larger saved counts are retained.
- Audio Node GPIO22: auto-initialised to 10 on first boot or when the stored count is less than 10 or invalid; larger valid stored counts are retained.
- C3 Pixel Node GPIO2 and C3 Emergency Node GPIO2: the shared pixel engine auto-initialises to 10, with the same persisted-count rule.
- The separate P4 Emergency signage strip (GPIO24, 100 pixels), S3 Lamp Jewel (7 pixels), node identifier LEDs and other dedicated indicators are not changed.
- Normal idle output stays black/off. An Emergency must override every **show** pixel line to white; clearing it returns to black, without resuming an interrupted effect.
- LEDs are not certified emergency lighting. Output white requires an operational node, adequate power and receipt of Emergency state. Wireless-link failure must be tested rather than assumed safe.
- The count setting can still be increased for longer strips; a minimum of 10 is intended for these Show Pixel Lines, not for every independent LED indicator.

## Acceptance checklist

1. Boot each show pixel line with no persisted pixel count: log reports COUNT=10 and READY without PIXEL:COUNT or PIXEL:INIT commands.
2. Boot with a saved zero and then with saved five: both recover to 10.
3. Boot with a valid saved larger count (e.g. 24): 24 remains unchanged.
4. Confirm P4 persisted /showduino/config/pixels.json is repaired to enabled=true, pixelCount>=10.
5. Run each show's pixel cues, assert Emergency and verify all ten LEDs on each line show white. Clear Emergency: LEDs remain black until new commands.
6. Run the fixed 22-second all-node scenario and repeat with Emergency interruption.

**Review note:** No physical test, cross-target build or firmware flash is claimed for this documentation.
