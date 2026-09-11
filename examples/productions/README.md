# First Audio Test

Canonical **Audio Node Hello World** production. Upload this `.shdo` file through the current Showduino persist path:

```text
Studio / upload → Comms S3 → P4 shdoCompile → SD /showduino/productions/first-audio-test/
```

Do not flash firmware to try this file. Copy the WAV onto the Audio Node SD card first.

## Upload this file

```text
examples/productions/first-audio-test.shdo
```

That is the standalone uploadable production. Do not rename the extension.

## Copy this exact file to the Audio Node SD card before testing

```text
/showduino/audio/test.wav
```

The production uses the relative name `test.wav`. The Audio Node resolves that under `/showduino/audio/`. WAV only (16-bit PCM).

## Values you can edit by hand

| Field | Where | Current value |
| --- | --- | --- |
| Production name | `project.name` | `First Audio Test` |
| Production ID | `project.id` | `first-audio-test` |
| Show length | `project.duration` and the `show-end` clip `startMs` | `30000` ms |
| Audio Node logical ID | `devices[0].binding.nodeId` | `AUDIO-01` |
| Play time | play clip `startMs` | `3000` ms |
| Stop time | play clip `startMs` + `durationMs` | `3000 + 22000 = 25000` ms |
| Filename | play clip `params.file`, `action.value`, and `action.params.file` | `test.wav` |
| Volume | play clip `params.volume` and `action.params.volume` | `80` |

Keep `project.id` lowercase with only `a-z`, `0-9`, `_`, `-`.

If you change the filename, change every copy of it in this file and put the same relative path on the Audio Node SD card under `/showduino/audio/`.

If you change the Audio Node ID, keep `devices[0].binding.route` as `audio-node`. Current P4 firmware still routes every `AUDIO:NODE:*` command to the single commissioned Audio Node; `AUDIO-01` is the SHDO logical identity, not a MAC address.

Leave `loop` `false`. Do not add pixel, lamp, relay, GPIO, DMX, or emergency clips.

## What the compiled P4 timeline does

```text
3000 ms   AUDIO:NODE:VOLUME:80
3000 ms   AUDIO:NODE:PLAY:test.wav
25000 ms  AUDIO:NODE:STOP
30000 ms  INTERNAL:LOG:show-end:complete
```

An explicit STOP is required. `SHOW:FINISHED` does not stop the Audio Node.

The 30-second end cue is a `log` clip. P4 persist compile accepts it. Studio V4 RAM deploy (`/api/studio-timeline`) currently rejects non-audio/non-pixel clips, so use the persist upload path for this file.
