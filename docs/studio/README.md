# Showduino Studio — Phase 0 Planning

**Status:** Planning complete (blueprint)
**Mode:** Documentation only — no implementation in this phase
**Depends on:** Showduino OS 2.0 Complete (Architecture Frozen)

This folder is the product blueprint for **Showduino Studio**, the production authoring environment.

```text
Studio creates.
Director operates.
Stage executes.
```

Those responsibilities must never overlap.

---

## Document map

| Document | Purpose |
|----------|---------|
| [Product Vision](product-vision.md) | What Studio is, who it serves, decision filter |
| [Architecture](architecture.md) | Layers, boundaries, module map |
| [Production Format](production-format.md) | How productions are stored and versioned |
| [Cue System](cue-system.md) | What a cue is and how it behaves |
| [Asset System](asset-system.md) | Media, lighting, GPIO, variables, references |
| [Validation](validation.md) | Pre-deploy checks, severity, blocking rules |
| [Deployment](deployment.md) | Packaging, transfer, verify, rollback |
| [Runtime Integration](runtime-integration.md) | Studio ↔ Director ↔ Stage contracts |
| [Roadmap](roadmap.md) | Incremental build order + extension points |

---

## Role reminder (inherited from OS 2.0)

| Role | Owns | Must not own |
|------|------|--------------|
| **Studio** | Authoring, validation, packaging, deployment intent | Live show state, cue clock, hardware execution |
| **Director** | Operator console, requests, confirmed-state display | Authoring authority, Stage execution |
| **Stage Runtime / Show Engine** | SoT runtime, cue execution, safety | Operator UI chrome, Studio editing model |
| **Communications Engine** | Transport | Show decisions, package authorship |

See: `docs/constitution.md`, `os2/Foundation.h`, ADRs `0001`–`0005`.

---

## Operator workflow (target)

```text
Create Production
        ↓
Import Assets
        ↓
Create Cue List
        ↓
Assign Lighting
        ↓
Assign Audio
        ↓
Assign Outputs
        ↓
Configure Triggers
        ↓
Validate
        ↓
Deploy
        ↓
Run from Director
```