# Showduino Studio — Asset System Specification

**Phase:** 0 (Planning)
**Status:** Blueprint

---

## 1. Purpose

Organise everything a Production references that is not the cue list itself: media, lighting definitions, DMX scenes, GPIO actions, variables, animations, and node resources.

---

## 2. Asset kinds (v1)

| Kind | Examples | Notes |
|------|----------|-------|
| `audio` | wav, mp3 refs | Logical player assignment at cue time |
| `image` | bmp, png for thumbs / UI overlays | Director UI assets remain separate from show media |
| `video` | Extension point — kind reserved | See Roadmap |
| `lighting` | Scenes, groups, looks | Logical fixtures |
| `dmx` | Scene snapshots | Capability-gated |
| `gpio` | Named output actions | Bound to logical devices |
| `variable` | Show variables | Typed; cue-settable |
| `animation` | Pixel / FX definitions | Capability-gated |
| `node` | Node resource packs | Referenced by logical node id |
| `other` | Misc blobs | Must still validate references |

---

## 3. Organisation

```text
assets/
  <kind>/
    <asset-id>/
      asset.json      # metadata
      payload...      # binary or child files
```

**`asset.json` minimum:**

| Field | Required |
|-------|----------|
| `id` | Yes |
| `kind` | Yes |
| `name` | Yes |
| `version` | Recommended |
| `tags` / `categories` | Optional |
| `hash` | Yes on package (integrity) |
| `refs` | Outbound references to other asset ids |

---

## 4. Catalogue features

| Feature | Behaviour |
|---------|-----------|
| Search | Name, id, tag, kind, unused flag |
| Categories | User folders / tags — not a second identity |
| Preview | Kind-appropriate (waveform, image, scene summary) |
| Unused assets | In package but not referenced by any cue/trigger/device binding |
| Broken references | Cue/action points at missing asset id |

Unused = warning (default). Broken = error (blocking).

---

## 5. Import rules

- Import copies into workspace package (no silent external absolute paths in deployed package).
- External link mode (authoring only) allowed in workspace; **deploy flattens** or fails validation.
- Hash recorded at import and rechecked at validate/deploy.

---

## 6. Relationship to Director UI assets

`/showduino/ui/...` theme assets on Director SD are **not** Production assets. Studio may manage them later as a separate "Director skin" tool; they are out of Production package v1 scope except optional thumbnail export (`thumbnail.bmp`).

---

## 7. Variables

Variables are first-class assets:

| Field | Notes |
|-------|-------|
| `id`, `name` | |
| `type` | bool, int, string (v1) |
| `default` | |
| `persist` | show-local vs session (Stage policy later) |

Cues may set variables; triggers may condition on them. Circular variable dependency graphs = validation error.

---

## Related

- [Cue System](cue-system.md)
- [Validation](validation.md)
- [Production Format](production-format.md)