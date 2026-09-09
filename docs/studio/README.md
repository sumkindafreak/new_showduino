# Showduino Studio

**Status:** Two surfaces share the Studio name. Do not treat the planning blueprint as live firmware.

```text
Studio creates (target).
Director operates.
P4 Show Engine executes.
Communications S3 transports and hosts the browser console.
```

Those responsibilities must never overlap.

---

## What is actually in this repository

| Surface | Location | Status |
|---------|----------|--------|
| **S3-hosted system console / commissioning WebUI** | `web/showduino-studio/` embedded as `WebAssets.generated.h` | **ACTIVE** — hosted by the Communications S3 SoftAP |
| **SHDO v2 authoring contract** | [`production-format.md`](production-format.md) | **ACTIVE CONTRACT** — portable `.shdo` interchange spec |
| **Studio product blueprint** | this folder (vision, cue/asset/validation docs) | **PLANNING** — target product, not a claim that full authoring is finished |
| **P4 persistent productions** | `docs/production-storage.md` + P4 `ProductionStore` | **ACTIVE RUNTIME** — format v1 `manifest.json` + `timeline.json`, **TEST/LOG cues only** |
| **Studio RAM timeline deploy** | `/api/studio-timeline` on S3 → P4 | **ACTIVE COMMISSIONING PATH** — PIXEL and AUDIO:NODE cues into P4 RAM; does not persist a `.shdo` |

The hosted WebUI is **not** the Director and **not** the Show Engine. A loaded show continues if the browser disconnects.

### Honest workflow vs target workflow

**Target (not fully implemented):**

```text
CREATE SHOW → configure devices → cues/actions → audio/pixels/outputs
→ timeline → save/export .shdo → upload to Showduino → P4 validates/stores
→ P4 runs independently
```

**What works today:**

```text
Author / commission from the S3 console
  → PIXEL:* and AUDIO:NODE:* live commands
  → optional RAM timeline via /api/studio-timeline (PIXEL + AUDIO:NODE only)
  → persistent SD productions remain format v1 TEST/LOG
  → P4 runs the loaded RAM or SD timeline without the browser
```

SHDO v2 is the authoring interchange to build toward. The P4 parser does **not** ingest `.shdo` JSON yet. Do not maintain a second incompatible native format: Studio must project to the P4 runtime subset it actually supports.

---

## Document map

| Document | Purpose | Honesty |
|----------|---------|---------|
| [Product Vision](product-vision.md) | What Studio is for | Target product |
| [Architecture](architecture.md) | Layers and boundaries | Target + current split |
| [Production Format](production-format.md) | SHDO v2 contract | Current authoring spec |
| [Cue System](cue-system.md) | Cue/action model | Planning |
| [Asset System](asset-system.md) | Media and references | Planning |
| [Validation](validation.md) | Pre-deploy checks | Planning |
| [Deployment](deployment.md) | Package/transfer/rollback | Planning; RAM timeline is the only live deploy endpoint |
| [Runtime Integration](runtime-integration.md) | Studio ↔ Director ↔ Stage | Planning |
| [Roadmap](roadmap.md) | Build order | Planning |

Live operator/commissioning behaviour of the S3 WebUI: [`web/showduino-studio/README.md`](../../web/showduino-studio/README.md).

Persistent P4 store: [`docs/production-storage.md`](../production-storage.md).

Legacy CYD/Mega `steps[]` JSON: [`docs/show-file-format.md`](../show-file-format.md) (**LEGACY**).

---

## Role reminder

| Role | Owns | Must not own |
|------|------|--------------|
| **Studio / WebUI** | Authoring intent, commissioning UI, packaging/deploy requests | Live show clock, safety policy, hardware execution |
| **Director** | Operator console, requests, confirmed-state display | Authoring authority, Stage execution |
| **P4 Show Engine** | Runtime SoT, cue execution, safety, SD store | Operator UI chrome, Studio editing model |
| **Communications Engine** | Transport + static WebUI host + API proxy | Show decisions, package authorship |

See: `docs/constitution.md`.

DMX is **parked** and must not be expanded in Studio during this phase.
