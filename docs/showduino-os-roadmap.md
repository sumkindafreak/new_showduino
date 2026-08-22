# Showduino OS Roadmap

**Status:** OS 2.0 Complete - Architecture Frozen  
**Mode:** Product development (not platform redesign)

The platform is established. New work is judged by one question:

> Does this fit the platform, or does it try to bend the platform?

See `os2/Foundation.h` (Constitution) and `docs/adr/`.

Product identity and the "should we build this?" filter: [Product Vision](showduino-product-vision.md).

---

## Phase A â€” Core Applications

Applications every operator expects. Each should feel finished before moving on.

| App | Status | Notes |
|-----|--------|--------|
| Dashboard | Done | Mission Control â€” services + events only |
| Library | Done (v1) | Production browser â€” Load via CommandService |
| Lighting | Planned | LightingService + commands |
| Audio | Planned | AudioService + commands |
| Devices | Planned | DeviceService inventory |
| Network | Planned | NetworkService fabric view |
| Diagnostics | Planned | Logs / health â€” no Stage coupling in UI |
| Safety | Planned | Emergency / clear / checks via Commands |
| Settings | Planned | SettingsService preferences |

---

## Phase B â€” Production Workflow

Complete operator journey. If this feels effortless, the OS is succeeding.

```text
Power On
  â†’ Session Restore
  â†’ Dashboard
  â†’ Library
  â†’ Load Production
  â†’ Safety Check
  â†’ Stage Ready
  â†’ Run
  â†’ Monitor
  â†’ Shutdown
```

---

## Phase C â€” Production Ecosystem

Differentiation on the frozen platform:

- Production templates  
- Cue editor  
- Asset browser  
- Timeline view  
- Production validation  
- Capability checks  
- Dependency analysis  
- Packaging  
- Import / export  

---

## Phase D â€” Multiple Clients

Same platform contracts; different presentation.

| Client | Purpose |
|--------|---------|
| Director 5" | Portable operator console |
| Director XL 10â€“15" | Fixed control desk |
| Web Studio | Browser-based operator surface |
| Simulator | Development, testing, training |

None own business logic. That validates the architecture.

---

## Phase E â€” Plugin Ecosystem

Apps that depend only on Services Â· Commands Â· Events Â· Theme Â· Shell can register:

```cpp
AppRegistry::registerApp(new MyApp());
```

No shell modifications. Optional modules per industry or venue.

---

## Operator task metrics (Law 10)

Track outcomes, not lines of code.

| Task | Target |
|------|--------|
| Load a production | â‰¤ 3 interactions |
| Start a show | â‰¤ 2 interactions |
| Emergency stop | 1 interaction |
| Find a production | â‰¤ 5 seconds |
| Identify a node fault | â‰¤ 10 seconds |

---

## What we do not do from here

- New architectural layers without an ADR and constitution amendment  
- Silent Compatibility v1 semantic changes  
- Apps that own state or talk to transport  
- Features that bend the platform instead of fitting it

---

## Showduino Studio (authoring product)

Phase 0 planning blueprint (documentation only):

`docs/studio/`

Studio creates Productions. Director operates. Stage executes. See `docs/studio/README.md`.
