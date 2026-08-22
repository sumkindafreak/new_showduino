# Showduino Studio — Future Roadmap

**Phase:** 0 (Planning)
**Status:** Blueprint

---

## 1. Implementation waves (after Phase 0)

| Wave | Deliver | Exit criteria |
|------|---------|---------------|
| **S0** | This documentation set accepted | Team aligned on boundaries |
| **S1** | Workspace + Production Manager + package read/write | Create/open/save Production on disk |
| **S2** | Asset Manager + reference tracker | Import + unused/broken detection |
| **S3** | Cue list + Timeline editor | Edit cues; export Stage-compatible timeline |
| **S4** | Device Assignment + Triggers (authoring) | Logical bindings; trigger JSON |
| **S5** | Validation Engine | Report gates deploy |
| **S6** | Deployment to Director SD path | Verify + history |
| **S7** | Runtime observe (read-only mirror) | ShowRuntime display in Studio |
| **S8** | Stage-owned project store deploy | Director SD no longer SoT |

No wave may violate: Studio creates / Director operates / Stage executes.

---

## 2. Extension points (document only — do not implement in Phase 0)

| Extension | Strategy |
|-----------|----------|
| **Video** | New asset kind + cue action kind; capability flag; Stage plugin later |
| **MIDI** | Trigger source + optional cue action; device capability `midi` |
| **OSC** | Trigger/action adapter; never bypass Command semantics for show control |
| **Art-Net / sACN** | Lighting/DMX backend under logical lighting devices |
| **Remote collaboration** | Workspace sync + presence; CRDT/merge on Production document |
| **Cloud backup** | Encrypted draft backup adapter; live shows remain local-authority (Constitution) |
| **Simulation** | Offline Stage fake for preview/validate; watermark UI "Simulated" |
| **Multi-user editing** | Lock or merge per section (cues/assets); audit trail |

Each extension: add capability tag → schema minor/major as needed → validation rules → Stage support gate. Prefer extending kinds over new package formats.

---

## 3. Alignment with OS roadmap phases

| OS roadmap | Studio relationship |
|------------|---------------------|
| Phase C Production Ecosystem | Cue editor, assets, validation, packaging — **Studio waves S1–S6** |
| Phase D Multiple clients | Director + future web operator desk; Studio remains authoring client |
| Release name "Studio" | Full ecosystem milestone — includes this product mature |

Existing SoftAP `web/showduino-studio` operator desk should be tracked separately as **Web Operator** to avoid name collision with this authoring product.

---

## 4. Success criteria (Phase 0 exit)

- Clear documented blueprint for create → validate → deploy → run on Director
- Modular, versioned, extensible without architectural redesign
- Responsibilities explicit enough for incremental implementation
- OS 2.0 guarantees preserved (no second SoT, no Studio execution engine)

**Phase 0 is complete when this `docs/studio/` set is accepted.**

---

## Related

- [README](README.md)
- `docs/showduino-os-roadmap.md`
- `docs/showduino-product-vision.md`