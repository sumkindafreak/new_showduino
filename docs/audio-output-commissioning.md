# Audio output commissioning (Audio Node 0.4.7)

`Emergency playback START` confirms the WAV opened and playback initialized. It does not confirm an audible analogue output. The default saved mode is SPEAKER; using the headphone jack requires HEADPHONE (or AUTO for live headphone detection).

Firmware 0.4.7 adds Settings â†’ Audio output â†’ Headphone jack â†’ SAVE OUTPUT in the Audio Node WebUI. Stop playback and clear emergency before changing it. Idle output commissioning is available even while P4 owns the node. Saving writes `/showduino/audio/config.json`; a failed SD save is reported and does not replace the in-memory configuration. The selected output is reused for normal and emergency playback after reboot.

Codec initialization now explicitly enables the ES8388 internal DAC clock (0x2B=0x80), sets its VREF/control and DAC-only mixer routes, then resets the clock state machine. Enabled analogue outputs use unity gain; DAC attenuation controls the volume. Required initialization and output-volume writes report I2C failures instead of silently succeeding.

Reference: [Espressif ES8388 driver](https://github.com/espressif/esp-adf/blob/release/v2.x/components/audio_hal/driver/es8388/es8388.c), initialization and start sequences. This is a software correction; audible playback on the user's board still needs bench verification.

## Headphone bench check

1. Flash the updated local Audio Node sketch. Confirm firmware 0.4.7 in the boot banner.
2. Clear emergency and stop playback. Open `http://192.168.5.1`, choose SETTINGS, select Headphone jack and SAVE OUTPUT. Confirm the save message.
3. Trigger emergency again. Serial should include `Playback output=HEADPHONE volume=...` and `Emergency playback START`. Listen through the headphone jack.
4. Clear emergency, test an ordinary supported WAV, then reboot and confirm HEADPHONE remains configured.
5. If silent, retain the boot banner and playback output/volume lines. A volume of zero, I2C write error, I2S error, or incorrect physical output needs its own diagnosis.

AUTO mode now follows GPIO39 continuously with a 150 ms stable-state debounce. Plugging in selects HEADPHONE; unplugging selects SPEAKER. Playback, current volume/fades and mute are preserved, including during emergency. Failed codec writes leave the amplifier off and retry at most once per second. Explicit SPEAKER, HEADPHONE and LINE modes remain fixed. Select Automatic headphone detection and SAVE OUTPUT to enable this behavior; existing saved choices are preserved.
