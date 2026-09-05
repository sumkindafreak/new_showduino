# Production format host tests

These tests exercise the P4 production manifest and timeline parser without the
Arduino or ESP32 SDK. They cover valid documents, missing required fields,
unsupported versions, path traversal, malformed JSON, duplicate cue IDs,
descending cue times, logical target preservation, empty timelines, and
unsupported physical cue types. Boundary cases cover excessive JSON nesting,
unsigned-time overflow, scheduler-command overflow, and the 512-cue ceiling.

The storage suite uses a temporary host filesystem to verify initialization,
discovery, successful loading, invalid production IDs, missing manifests,
malformed manifests, missing timelines, malformed timelines, duplicate cue IDs,
descending times, 4 KiB manifest / 128 KiB timeline ceilings, loaded-metadata
preservation after failure, and unload.

The runtime suite uses small host stubs for Arduino timing and allocation. It
also verifies timeline staging/abort, start, pause, explicit resume, stop,
finish, unload, emergency interruption, and the rule that clearing an emergency
does not automatically resume playback. It also verifies that unload remains
blocked and safe stop remains available while emergency owns the runtime.

Run on Windows:

```powershell
.\run_tests.ps1
```

Run on Linux/macOS:

```bash
./run_tests.sh
```
