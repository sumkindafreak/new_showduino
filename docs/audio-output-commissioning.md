# Audio output commissioning (Audio Node 0.4.8)

## Useful volume and spaced Emergency announcements

Volume now describes signal amplitude, rounded to the ES8388's half-dB steps.
The old mapping made 50% approximately -48 dB; the new mapping gives:

| Setting | New DAC gain |
| --- | --- |
| 0% | Explicit DAC mute |
| 10% | -20 dB |
| 25% | -12 dB |
| 50% | -6 dB |
| 75% | -2.5 dB |
| 100% | 0 dB (unity, no boost) |

Saved percentages and cue commands stay unchanged. Fades, ducking and output
switching use the new curve. Existing settings will be louder: begin testing
at a low Director volume with headphones off your ears, then increase gradually.
The fixed 22-second show still requests 50%. The reported silent headphone jack
still requires physical verification; software changes alone do not prove a fix.

Emergency audio starts immediately, then queues eight seconds of zero PCM after
each complete announcement before repeating. Output buffering therefore does
not shorten the audible gap. The task remains responsive and Emergency stays
active during the gap. Repeated Emergency messages do not restart the cycle.
Director clear stops playback and cancels pending repeats without resuming the
show. Normal show loops retain their immediate repeats.

During the gap, AUDIO:STATUS still reports the Emergency loop active; POS stays
at the end of the WAV and the SD BYTES counter does not increase. This is
expected: zero samples are being sent without reading the file again.

Host regression tests execute the actual codec gain/mute functions and playback
service: sample rates, mono/stereo, silence length, partial writes, normal loops
and stopping during the gap. See the local reconciliation report for target-build
evidence and physical checks still outstanding.

## Output selection

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
