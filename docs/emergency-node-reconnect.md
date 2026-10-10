# Emergency Node reconnection

C3 Emergency Node firmware 0.3.2 sends discovery/heartbeat and emergency
assert frames to broadcast while its Comms receive link is stale. Previously,
the learned Comms address remained the destination indefinitely. ESP-NOW can
accept a queued unicast even when it cannot deliver it, so the immediate-error
broadcast fallback did not cover that failure.

Fresh Comms replies restore unicast. The existing channel-follow scan remains
active during link loss. No reboot, latch clear, or automatic rearm is added.
The OLED now says SEARCHING / LINK LOST while disconnected; STATUS radio=0
reflects a stale link rather than merely a previously learned peer address.

Install this firmware on the C3 Emergency Node by USB. P4, Comms and Director
do not need updates for this change. It is a recovery improvement; identifying
the original dropout still requires node and gateway logs.

Validation: ESP32-C3 compiler checks passed for the changed transport,
diagnostics and OLED files. The routing regression test covers startup, fresh
and stale peers, rediscovery and uptime wrap. Hardware reconnection remains
to be verified: interrupt Comms, observe SEARCHING / LINK LOST, restore Comms,
and check LINK OK plus P4/Director online state. Repeat with a latched node;
reconnection must preserve the latch and must never clear P4 emergency.
