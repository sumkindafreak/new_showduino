# Showduino Studio — Architecture

**Phase:** 0 (Planning)
**Status:** Blueprint — no implementation

---

## 1. Purpose

Define the modular architecture of Showduino Studio so implementation can proceed incrementally without redesigning OS 2.0 or overlapping Director / Stage responsibilities.

---

## 2. System context

```text
                 ┌──────────────────────────────┐
                 │      Showduino Studio        │
                 │  Author · Validate · Deploy  │
                 └─────────────┬────────────────┘
                               │
              package + deploy intent + optional monitor
                               │
         ┌─────────────────────┼─────────────────────┐
         ▼                     ▼                     ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ Local workspace │  │    Director     │  │ Stage Runtime   │
│ (author store)  │  │ operator client │  │ (Show Engine)   │
└─────────────────┘  └────────┬────────┘  └────────▲────────┘
                              │                     │
                              └──────────┬──────────┘
                                         │
                              Communications Engine
                                   (transport)
```

Studio has three external faces:

1. **Workspace** — local (or networked) authoring store owned by Studio.
2. **Deploy target** — Stage project store (target SoT) via approved transfer path; Director SD is transitional.
3. **Runtime observe** — optional read of confirmed Stage/Director state for verification after deploy (display only).

---

## 3. Layered architecture (Studio internal)

```text
┌─────────────────────────────────────────────────────┐
│ Presentation                                        │
│  Desktop / Web UI — editors, browsers, inspectors   │
├─────────────────────────────────────────────────────┤
│ Application modules                                 │
│  Production Manager · Asset Manager · Cue Editor    │
│  Timeline · Triggers · Device Assignment · Validate │
│  Deploy · History                                   │
├─────────────────────────────────────────────────────┤
│ Domain model                                        │
│  Production · Cue · Asset · DeviceRef · Trigger     │
│  ValidationReport · DeploymentPackage               │
├─────────────────────────────────────────────────────┤
│ Platform adapters                                   │
│  Workspace FS · Package codec · Deploy transport    │
│  Runtime status client (read-only / request-only)   │
└─────────────────────────────────────────────────────┘
```

Rules:

- Presentation never writes Stage SoT directly.
- Domain model is versioned (see Production Format).
- Adapters absorb hardware and transport change (Law 7).
- No module owns live show clock or physical outputs.

---

## 4. Module map

| Module | Responsibility | Does not |
|--------|----------------|----------|
| **Production Manager** | CRUD productions, metadata, versions, tags, readiness | Run shows |
| **Asset Manager** | Import, catalogue, preview, reference integrity | Stream live media to fixtures |
| **Cue System** | Cue objects, properties, dependencies | Dispatch at show time |
| **Timeline** | Edit experience over cue list + time | Own playback position SoT |
| **Trigger System** | Author trigger definitions bound to cues/productions | Arm safety overrides on Stage |
| **Device Assignment** | Map cue targets to logical devices / zones | Store pin numbers as product API |
| **Validation Engine** | Produce ValidationReport before deploy | Auto-heal Stage state |
| **Deployment** | Package, transfer, verify, rollback records | Claim execution success |
| **Runtime Integration** | Handoff + observe confirmed state / logs | Duplicate ShowService |

---

## 5. Data ownership

| Data | Owner while authoring | Owner after deploy | Owner while running |
|------|----------------------|--------------------|---------------------|
| Production draft | Studio workspace | Stage project store (target) | Stage (loaded Production) |
| Manifest catalogue view | Derived | AssetService / Library mapping | Director mirrors catalogue |
| Cue clock / show state | N/A | N/A | Stage ShowRuntime |
| Operator actions | N/A | N/A | Director → Commands → Stage |
| Deploy history | Studio + Stage audit | Stage + Studio copy | Append-only |

**Law 1:** one truth per fact. Studio drafts are not runtime truth until Stage accepts a verified deployment and a load succeeds.

---

## 6. Alignment with OS 2.0 stack

Frozen OS stack:

```text
Shell → Apps → Event Bus → Commands → Services → Communication → Stage Runtime
```

Studio sits **beside** Director as another client family:

- Authoring UI is not an OS Shell app on the Director (unless a future thin "deploy status" app is added).
- When Studio issues load/deploy-related intent toward Stage, it must use the same **Command** vocabulary semantics as Director (`LoadProduction`, etc.) — not invent a second control plane.
- Studio must not reimplement ShowService business logic; it may display published runtime fields for verify-after-deploy.

---

## 7. Logical devices (architecture mandate)

Cues and assets reference **logical device IDs** and **roles/zones**, for example:

- `lighting.engine.main`
- `audio.engine.a`
- `relay.board.stage-left`
- `fx.fog.1`
- `node.3`
- `zone.a`

Pin maps, DMX universes, and node MACs live in **fabric / device inventory** owned by Stage + Communications configuration — not in cue bodies as authoring primitives.

Studio Device Assignment binds production requirements → available fabric capabilities at validate/deploy time.

---

## 8. Extension strategy (architecture level)

Extension points are **capability plugins** behind stable domain interfaces:

| Extension | Hooks into |
|-----------|------------|
| Video / MIDI / OSC / Art-Net / sACN | Asset kinds + cue action kinds + device capability tags |
| Simulation | Adapter that fakes Stage responses for offline validate/preview |
| Cloud backup | Workspace adapter (sync drafts); never becomes SoT for live shows |
| Multi-user editing | Workspace concurrency layer; merge on Production document |
| Remote collaboration | Same as multi-user + presence; no shared live clock ownership |

New kinds require schema version bumps when they break readers (see Production Format). Do not add parallel package formats.

---

## 9. Non-goals

- Embedding Stage Runtime inside Studio
- Making Studio the primary emergency desk
- Replacing Communications Engine
- Speculative wire APIs beyond existing Stage/Director command families (new fields go through versioned package + ADR when needed)

---

## Related

- [Product Vision](product-vision.md)
- [Production Format](production-format.md)
- `docs/adr/0001-platform-boundary.md`
- `docs/architecture.md`