# Automatic Wi-Fi time sync

When the Comms controller connects to a Wi-Fi network and receives an IP address, it starts background SNTP using three internet time servers. A successful SNTP reply enables forwarding UTC epoch time to the P4 through the existing TIME:SET command. P4 remains authoritative and publishes the clock to Director.

No internet-time reply means no automatic clock overwrite. SNTP retries in the background; access-point-only operation does not start it. Disconnecting stops SNTP while the P4 clock continues ticking. Reconnecting requests fresh time. While connected and verified, Comms forwards the current clock every minute so a rebooted P4 recovers without waiting for another hourly SNTP update. Show timing continues using its existing monotonic timers.

Upload Comms firmware 0.5.5 and connect it to an internet-capable Wi-Fi network. Serial should show `[TIME] Internet time received`, then the Director should display the P4 clock. Disconnect Wi-Fi and check the clock continues; reboot P4 while Wi-Fi remains connected and check time returns within a minute. The existing system clock display remains UTC.
