# Persistent P4 production storage

Status: implemented foundation for Showduino v1 production format 1.

The ESP32-P4 Show Engine is the sole owner of the loaded production and its
timeline. The Director requests operations, and the dedicated ESP32-S3
Communications Engine only transports those requests and responses.

## SD layout

```text
/showduino/productions/
  <production-id>/
    manifest.json
    timeline.json
    assets/              # reserved for later formats; unused in this milestone
```

The repository copy used to populate an SD card is under
`sd_card/showduino/productions/`.

## Manifest format 1

Required fields:

| Field | Type | Meaning |
|---|---|---|
| `formatVersion` | unsigned integer | Must be `1` |
| `productionId` | string | Stable lowercase ID; letters, digits, `_`, `-` |
| `name` | string | Operator-facing production name |
| `timeline` | relative path | Explicit timeline file inside the production folder |

Supported optional fields are `description`, `author`, `created`, `modified`,
and unsigned integer `revision`. Unknown fields are validated as JSON and then
ignored, allowing compatible metadata additions. The manifest `productionId`
must exactly match its folder name. Absolute paths, backslashes, and `..` are
rejected.

## Timeline format 1

```json
{
  "formatVersion": 1,
  "cues": [
    {
      "id": "cue_001",
      "timeMs": 1000,
      "type": "TEST",
      "target": "diagnostic.timeline",
      "action": "LOG",
      "value": "CUE ONE",
      "parameters": { "level": "info" }
    }
  ]
}
```

Required cue fields are `id`, `timeMs`, and `type`. `target`, `action`, `value`,
and an object-valued `parameters` field are optional. Logical target strings are
preserved by the parser but are not routed to hardware in this milestone.

Only internal `TEST` and `LOG` cue types are accepted. `action`, when supplied,
must be `LOG`. Cue times must be nondecreasing, IDs must be unique, and the
timeline must contain at least one cue. Physical output types are rejected.

## Validation limits

| Limit | Value |
|---|---:|
| Manifest size | 4 KiB |
| Timeline size | 128 KiB |
| Discovered productions | 32 |
| Cues per production | 512 |
| Production ID | 47 characters |
| Cue scheduler command | 63 characters |

Parsing is strict and bounded. Malformed JSON, unsupported versions, missing
required fields, numeric overflow, path traversal, duplicate cue IDs, descending
cue times, oversized strings, and unsupported cue types fail explicitly.

## Commands and responses

Requests:

```text
PRODUCTION:LIST
PRODUCTION:LOAD:<production-id>
PRODUCTION:UNLOAD
PRODUCTION:STATUS
SHOW:START
SHOW:PAUSE
SHOW:RESUME
SHOW:STOP
```

Representative authoritative responses:

```text
PRODUCTION:LIST:BEGIN:<count>
PRODUCTION:ITEM:<id>
PRODUCTION:LIST:END
PRODUCTION:LOAD:OK:<id>
PRODUCTION:LOAD:ERROR:<reason>
PRODUCTION:UNLOAD:OK
PRODUCTION:STATUS:NONE
PRODUCTION:STATUS:LOADED:<id>
SHOW:START:OK
SHOW:START:REJECTED:NO_PRODUCTION
SHOW:PAUSE:OK
SHOW:RESUME:OK
SHOW:STOP:OK
```

Production parsing happens before the active timeline is changed. The
`TimelineEngine` builds replacement cues in a staging allocation and swaps the
new timeline into service only after the entire load succeeds. A failed load
therefore leaves the previous valid timeline available.

Loading and unloading are rejected while emergency is active. Start and resume
remain gated by the existing authoritative emergency logic. Emergency activation
pauses an active timeline, and clearing the emergency does not automatically
resume it.

## Relationship to Studio prototypes

This is the P4 runtime deployment format. The broader authoring package described
under `docs/studio/` remains a planning contract and is not authoritative at
runtime. No host-side Web UI is required to list, load, or execute an SD
production.
