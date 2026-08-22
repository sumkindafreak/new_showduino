# Showduino Studio — Production Format Specification

**Phase:** 0 (Planning)
**Status:** Blueprint contract
**Compatibility:** Must project to OS Production Manifest v1 (`os2/models/Production.h`)

---

## 1. Purpose

Define how a Production is stored for authoring, packaging, and deployment — without treating a file path as the product (ADR 0004).

---

## 2. Core object

A **Production** is the unit of creative work and deployment.

Operator-facing summary (already frozen as Manifest v1):

| Field | Role |
|-------|------|
| `id` | Stable identifier |
| `name` | Operator title |
| `description` | Short summary |
| `version` | Authoring/content version string |
| `author` | Author information |
| `durationSeconds` | Nominal duration |
| `hasThumbnail` | Catalogue affordance |
| `capabilities` | audio / lighting / effects + counts |
| `entryShow` | Runtime entry id (usually = `id`) |
| `lastEdited` | Display stamp |
| `readiness` | Ready / Warning / Invalid (derived) |

Studio **extends** this with authoring metadata that does not need to appear on the Director Manifest until projected.

---

## 3. Required Studio metadata (authoring)

| Field | Required | Notes |
|-------|----------|-------|
| `id` | Yes | Stable; immutable after first publish preferred |
| `name` | Yes | |
| `description` | Recommended | |
| `version` | Yes | SemVer-like string; Studio increments on meaningful publish |
| `author` | Recommended | Person or org |
| `createdAt` | Yes | ISO-8601 in package |
| `modifiedAt` | Yes | ISO-8601 |
| `tags` | Optional | Search / venue / show type |
| `capabilities` | Yes | Declared + verified at validate |
| `dependencies` | Optional | Other productions, min Stage firmware, required device roles |
| `readiness` | Derived | From Validation Engine — not free-hand author fiction |
| `schemaVersion` | Yes | Package schema major |

---

## 4. Package layout (Studio package v1)

Logical layout (maps to today's Director transitional SD package and future Stage store):

```text
<production-id>/
  production.json          # authoring + manifest projection source
  show.json                # transitional runtime entry (compat with Director today)
  thumbnail.bmp            # optional
  cues/
    cues.json              # cue list (authoritative in Studio model)
  timeline/
    timeline.json          # timed schedule derived or co-edited with cues
  assets/
    audio/
    images/
    video/
    lighting/
    dmx/
    gpio/
    animations/
    other/
  devices/
    devices.json           # logical device requirements + bindings
  triggers/
    triggers.json
  variables/
    variables.json
  meta/
    validation-report.json # last report (optional cache)
    deploy-history.json    # local copy of deploy records
```

**Normative rule:** `production.json` is the Studio source of truth for authoring metadata. `show.json` remains the transitional bridge to current Director `ShowManager` until Stage owns project storage. Both must stay consistent on export (same `id`, `name`, `version`, duration, capabilities flags).

---

## 5. Versioning

| Layer | What changes |
|-------|--------------|
| `schemaVersion` | Package document shapes (breaking → major) |
| Production `version` | Creative content revision |
| Manifest API | OS `ProductionManifest` fields (Compatibility.h) |
| Stage firmware | Capability / command support |

Deploy rejects when:

- Studio package `schemaVersion` unsupported by target
- Required Stage capabilities missing
- Manifest projection would break Manifest v1 consumers

---

## 6. Author information, tags, capabilities, dependencies

**Author:** free text + optional contact; not a security identity in v1.

**Tags:** lowercase slugs; Studio search only until catalogue sync defines shared taxonomy.

**Capabilities:** boolean/count flags aligned with Manifest (`audio`, `lighting`, `effects`, scene/track counts). Validation may raise declared capability that assets/cues do not support (warning) or required fabric capability missing (error).

**Dependencies:**

- `requiresDevices[]` — logical roles (e.g. `relay.board`, `audio.engine`)
- `requiresMinStage` — firmware / protocol version labels already used in product docs
- `requiresProductions[]` — optional linked packages (advanced; warn if missing)

---

## 7. Readiness

Readiness is **computed**, never manually set as the sole gate:

| Level | Meaning |
|-------|---------|
| Ready | No blocking validation errors |
| Warning | Deployable with operator-visible warnings |
| Invalid | Blocking errors — deploy forbidden |

Maps to Director Manifest `StatusLevel` labels (Ready / Warning / Invalid).

---

## 8. Relationship to legacy formats

| Format | Status vs Studio |
|--------|------------------|
| Director `show.json` + `timeline.json` | Transitional runtime package — Studio must export compatible subset |
| `docs/show-file-format.md` steps[] | Legacy — import path only |
| `.shdo` scene files | Legacy aspirational — import path only |

Studio does not maintain three equal native formats. One Studio package; importers normalise into it.

---

## Related

- [Cue System](cue-system.md)
- [Asset System](asset-system.md)
- [Deployment](deployment.md)
- `os2/models/Production.h`
- ADR 0004