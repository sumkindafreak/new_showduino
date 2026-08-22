# Showduino Studio — Validation Specification

**Phase:** 0 (Planning)
**Status:** Blueprint

---

## 1. Purpose

Before deployment, Studio must prove a Production is coherent, referentially intact, and compatible with the target Stage fabric — without executing the show.

Validation produces a **ValidationReport**. Deploy reads that report; it does not invent a second opinion.

---

## 2. Severity model

| Severity | Meaning | Deploy |
|----------|---------|--------|
| **Info** | Guidance | Allowed |
| **Warning** | Risk / incomplete polish | Allowed (operator-visible) |
| **Error** | Blocking failure | Forbidden |
| **Fatal** | Package unreadable / schema broken | Forbidden |

Readiness mapping:

- Any Fatal/Error → Invalid
- Only Warning/Info → Warning or Ready (Ready if zero warnings, or policy "warnings allowed still Ready" — default: zero errors and zero warnings = Ready; warnings present = Warning)

---

## 3. Check catalogue (v1)

### Identity & structure

- Missing required metadata (`id`, `name`, `version`, `schemaVersion`)
- Duplicate Production ids in workspace batch deploy
- Duplicate cue ids
- Duplicate asset ids
- Schema version unsupported

### References

- Missing assets referenced by cues / triggers / devices
- Broken device bindings (logical id required but unbound)
- Broken trigger targets
- Circular cue dependencies
- Circular variable references

### Timing

- Negative times / delays
- Relative anchors missing
- Overlaps that exceed Stage limits (if known)
- Empty timeline while Production claims duration/cues

### Triggers

- Trigger with no target
- Safety-critical trigger misconfigured (e.g. e-stop clear without authority path) → Error
- Network/ESP-NOW trigger without capability declared → Error or Warning by policy

### Capabilities & fabric

- Declared capability unused (Warning)
- Action requires capability not declared (Error)
- Required logical device role offline / absent on target inventory (Error if deploy target known; Warning if offline validate)
- Version compatibility: Stage / protocol / package schema

### Protocol / export limits

- Compiled command exceeds existing transport length limits
- Cue count exceeds Stage max (documented Stage RAM cue cap)

### Hygiene

- Unused assets (Warning)
- Missing thumbnail (Info)
- Author/tags empty (Info)

---

## 4. ValidationReport shape (logical)

| Field | Purpose |
|-------|---------|
| `productionId` | |
| `producedAt` | |
| `studioVersion` | Tool version |
| `target` | Optional fabric fingerprint |
| `issues[]` | code, severity, path, message, hint |
| `summary` | counts by severity |
| `blocking` | boolean |

Issue `code` values are stable strings (e.g. `CUE_DUP_ID`, `ASSET_MISSING`) for UI and CI.

---

## 5. When validation runs

| Moment | Required |
|--------|----------|
| Manual "Validate" | Yes |
| Before Deploy | Yes (fresh report) |
| On open (background) | Recommended |
| On CI / headless | Supported by same engine |

Deploy must refuse stale reports older than a defined TTL or if content hash changed since report.

---

## 6. Offline vs connected validation

| Mode | Fabric checks |
|------|---------------|
| Offline | Structure, refs, declared capabilities only |
| Connected | Plus live inventory / online nodes / Stage version |

Connected mode still does not execute the timeline.

---

## Related

- [Deployment](deployment.md)
- [Device Assignment](architecture.md) (logical devices)
- [Cue System](cue-system.md)