# Showduino Studio — Production Format Specification

**Phase:** 1 (Authoring Contract)
**Status:** SHDO v2 active; runtime projection transitional
**Compatibility:** Must project to OS Production Manifest v1 (`os2/models/Production.h`)

---

## 1. Purpose

Define how a **Production** is authored, exchanged, validated and projected for deployment without treating a file path as the product (ADR 0004).

The core OS object is still a Production. `.shdo` is the portable authoring/interchange representation of that object.

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

Studio extends this with authoring metadata that does not need to appear on the Director Manifest until projected.

---

## 3. SHDO v2 — canonical portable authoring representation

Studio saves and exchanges Productions as `.shdo` JSON documents using:

```text
schema = showduino-production-v2
package.format = showduino-production
package.version = 2
```

Top-level structure:

```text
project
architecture
compatibility
devices
tracks
clips
markers
scenes
assets
safety
globalSettings
config
package
metadata
```

The canonical JSON Schema lives in the Showduino Studio authoring repository:

```text
showduino_studio/schema/showduino-production-v2.schema.json
```

### Authoring model

Studio continues to author around:

```text
Production → Scene → Cue → Action → Target
```

On `.shdo` export, actions are normalised into timeline `tracks` and `clips`. Scene/cue entries reference those clip IDs. On import, Studio reconstructs the scene/cue/action model.

This gives the file both:

- a human/AI-friendly production structure; and
- a normalized timeline representation suitable for validation, editing and future compilation.

### Architecture declaration

Every SHDO v2 file declares the current Showduino path:

```text
Director ESP32-S3
  → ESP-NOW
Communications ESP32-S3
  → UART
ESP32-P4 Show Engine
  → logical-device-id
Specialist Nodes / local engines
```

Normative values:

```text
runtimeAuthority = esp32-p4-show-engine
operator         = esp32-s3-director
transport        = esp32-s3-comms-controller
nodeAddressing   = logical-device-id
```

### Safety declaration

SHDO can describe the expected safety contract but cannot weaken it.

The P4 firmware remains safety authority. Production files must preserve:

```text
policy                  = firmware-authoritative
emergency.stopTimeline  = true
emergency.pixelOverride = all-white
requiresManualClear     = true
autoResume              = false
productionCannotDisable = true
```

A malformed production attempting to weaken these values must fail validation/import rather than changing runtime safety behaviour.

---

## 4. Required Studio metadata

| Field | Required | Notes |
|-------|----------|-------|
| `id` | Yes | Stable; immutable after first publish preferred |
| `name` | Yes | |
| `description` | Recommended | |
| `version` | Yes | Creative/content version |
| `author` | Recommended | Person or org |
| `createdAt` | Yes | ISO-8601 |
| `updatedAt` | Yes | ISO-8601 |
| `tags` | Optional | Search / venue / show type |
| `capabilities` | Yes when deployed | Declared + verified at validate |
| `dependencies` | Optional | Required Stage version / device roles / linked productions |
| `readiness` | Derived | From Validation Engine — never free-hand authority |
| `schemaVersion` | Yes | SHDO/package schema major |

SHDO v2 stores these principally under `project`, `compatibility`, `devices`, `assets` and `metadata`.

---

## 5. Logical devices

`devices[]` is the production's logical device registry.

Timeline actions should target logical device IDs rather than physical P4 pins or raw node addresses whenever a binding exists.

A device can exist before it has a physical binding so productions can be authored away from the final venue. Validation/deployment is responsible for warning or blocking when required logical devices are not bound.

Conceptually:

```text
Production action
  → logical device id
  → binding / route
  → P4
  → specialist node or local engine
```

This preserves the rule that physical routing is not baked throughout creative timeline data.

---

## 6. Runtime/deployment projection

`.shdo` is the authoring/interchange file. It is **not** permission for the Director or browser to become runtime authority.

Deployment compiles/projects the Production into the subset understood by the target P4/Director firmware.

Transitional runtime layout may still resemble:

```text
<production-id>/
  production.json
  show.json
  thumbnail.bmp
  cues/
    cues.json
  timeline/
    timeline.json
  assets/
    audio/
    images/
    video/
    lighting/
    gpio/
    animations/
    other/
  devices/
    devices.json
  triggers/
    triggers.json
  variables/
    variables.json
  meta/
    validation-report.json
    deploy-history.json
```

**Normative rule:** this runtime folder is a generated deployment projection, not a second equal authoring format. Studio's in-memory Production and its SHDO v2 round-trip are the authoring source.

`show.json` / `timeline.json` remain transitional bridges until the P4 Stage Engine owns persistent production storage directly.

---

## 7. Versioning

| Layer | What changes |
|-------|--------------|
| SHDO `package.version` / `compatibility.shdoMajor` | Breaking authoring document changes |
| Production `project.version` | Creative content revision |
| Manifest API | OS `ProductionManifest` fields |
| Runtime protocol | Command/capability compatibility |
| Stage firmware | Capability / engine support |

Deploy rejects when:

- SHDO major unsupported by target tooling;
- required Stage capabilities are missing;
- required logical devices cannot be resolved;
- referenced clips/tracks/assets are invalid;
- safety contract is malformed or weakened;
- Manifest projection would break Manifest v1 consumers.

---

## 8. Readiness

Readiness is computed, never manually set as the sole gate:

| Level | Meaning |
|-------|---------|
| Ready | No blocking validation errors |
| Warning | Deployable with operator-visible warnings |
| Invalid | Blocking errors — deploy forbidden |

Maps to Director Manifest `StatusLevel` labels (Ready / Warning / Invalid).

---

## 9. Relationship to older formats

| Format | Status vs Studio |
|--------|------------------|
| SHDO v2 (`showduino-production-v2`) | **Current portable authoring/interchange format** |
| `showduino.production.package` v1 | Legacy Studio import/migration only |
| Raw Studio Production JSON | Legacy/local import path |
| Director `show.json` + `timeline.json` | Transitional generated runtime projection |
| `docs/show-file-format.md` `steps[]` | Legacy import/reference only |
| Older scene-only `.shdo` concepts | Legacy; must migrate/normalise before use |

Studio does not maintain several equal native formats. Everything normalises into one Production model and exports as SHDO v2.

---

## Related

- [Cue System](cue-system.md)
- [Asset System](asset-system.md)
- [Deployment](deployment.md)
- `os2/models/Production.h`
- ADR 0004
