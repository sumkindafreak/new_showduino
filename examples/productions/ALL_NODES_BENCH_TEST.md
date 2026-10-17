# All Nodes Bench Test — finish-line acceptance

Use `all-nodes-bench-test.shdo` as the fixed non-DMX acceptance production.

## Preconditions

- P4 GPIO23 show line: **10 pixels**.
- C3 Pixel Node `LED-01`: **10 pixels**.
- Audio Node GPIO22 line: **10 pixels**.
- Emergency Node `ESTOP-01`: **10 pixels**.
- Lamp Node: permanent **7-pixel Jewel** controlled by Lamp FX; it is not a generic pixel line.
- Audio Node contains `/showduino/audio/system-test.wav`.
- P4 ambience commissioning asset, when testing Director ambience controls: `/showduino/audio/ambience.wav`.
- MOSFET OUT1 must use only a benign low-voltage test load for this acceptance run.
- DMX is not part of this test.

## Expected timeline

| Time | Expected action |
| ---: | --- |
| 1.0 s | P4 GPIO23: 10 pixels blue BREATHE |
| 3.0 s | P4 segment stops/black |
| 4.0 s | C3 Pixel Node: 10 pixels red FLICKER |
| 6.0 s | C3 segment stops/black |
| 7.0 s | Audio Node GPIO22: 10 pixels FIRE |
| 9.0 s | Audio Node pixel segment stops/black |
| 10.0 s | Audio Node plays `system-test.wav` at volume 50 |
| 14.0 s | Audio Node STOP |
| 15.0 s | Lamp Jewel IGNITE |
| 16.0 s | Lamp Jewel UNSTABLE |
| 17.0 s | Lamp Jewel FLARE |
| 18.0 s | Lamp Jewel EXTINGUISH |
| 19.0 s | MOSFET-01 OUT1 pulse at 25% for 500 ms then OFF |
| 22.0 s | COMPLETE log cue |

## Director ambience check

Before or after the production run, open **NODES → AUDIO NODE → AMBIENCE**:

1. STATUS must report the P4 ambience state.
2. PLAY must play `/showduino/audio/ambience.wav`.
3. LOOP must loop it.
4. VOL - / VOL + must change P4 ambience volume.
5. STOP must stop it.
6. During Emergency, PLAY/LOOP/volume are blocked and STOP remains safe.

## Emergency pass

Run the same production again and assert the Emergency Node during an active cue.

Confirm:

- P4 global Emergency latches.
- timeline is interrupted/paused;
- attraction Audio Node playback stops;
- P4 PCM5102A ambience stops;
- Lamp enters its Emergency behaviour and local emergency audio;
- P4 show pixels, C3 Pixel Node, Audio Node pixels and Emergency Node pixels all become bright white;
- clearing Emergency from the Director does not auto-resume the show or audio.

If a check fails, fix only that path and repeat this same production from the beginning.
