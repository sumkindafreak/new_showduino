# Audio Node 0.4.5 receive-task crash fix

The bench log repeatedly reports `Stack canary watchpoint triggered (wifi)` and software resets on firmware 0.4.4. ESP-NOW previously dispatched commands directly on the Wi-Fi task. Audio asset browsing allocated a 3,072-byte inventory array in that command handler, then recursively scanned SD; playback and pixel writes also ran on the same callback.

The receive callback now validates and queues fixed-size packets without blocking. The main loop dispatches commands, registers peers and reports the link. Ordinary commands use a 16-entry FIFO; safety commands use a separate 8-entry FIFO with priority and preserved STOP/CLEAR order. A safety queue overflow latches emergency, and a processed safety transition discards previously queued ordinary commands. Each main-loop pass handles a bounded number of packets. Command inventory uses one shared static buffer because handlers now execute serially on the main loop.

## Bench validation

1. Flash Audio Node 0.4.5 using the existing ESP32-A1S configuration. Confirm the boot banner reports 0.4.5.
2. With the SD card inserted and the node online, repeatedly open SELECT ASSET and use NEXT PAGE. Check names arrive without a reboot or Wi-Fi stack-canary error.
3. Select a known supported WAV, press PLAY, then test STOP and LOOP.
4. Trigger and clear emergency during idle and playback. Check audio and pixels respond without rebooting, and ordinary playback stays stopped after the transition until a fresh play command.
5. Keep the serial log if playback reports FILE_NOT_FOUND, NO_STORAGE or a decoder error: those are separate from the receive-task stack crash.

OTA work remains paused. This fix changes only Audio Node firmware and does not require a protocol change on P4 or Director.
