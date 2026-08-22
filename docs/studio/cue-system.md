# Showduino Studio — Cue System Specification

**Phase:** 0 (Planning)
**Status:** Blueprint

---

## 1. What a cue is

A **cue** is a discrete, addressable unit of show intent scheduled on a Production timeline.

At runtime, Stage executes cues. Studio only authors and validates them.

A cue is **not**:

- a pin toggle description owned by Studio
- live show state
- an operator button label alone (GO triggers fire cues; they are not cues themselves)

---

## 2. Required properties

| Property | Description |
|----------|-------------|
| `id` | Stable unique id within the Production |
| `name` | Human label (e.g. "House Out", "Scare 1") |
| `timeMs` | Schedule time from show zero (or relative anchor — see Timing) |
| `actions[]` | One or more actions to perform when the cue fires |

Minimum viable export to today's Stage path: each scheduled action projects to a timed command string compatible with existing `SHOW:TL:C:<timeMs>:<command>` upload (Director `TimelineEngine` / Stage RAM cues). Studio may keep richer action objects internally and **compile** them at deploy.

---

## 3. Optional properties

| Property | Description |
|----------|-------------|
| `notes` | Operator / designer notes |
| `color` | Timeline colour coding |
| `tags` | Search / filter |
| `groupId` | Cue group / scene stack membership |
| `bookmark` | Named jump target |
| `delayMs` | Additional delay after schedule time before actions |
| `durationMs` | Expected span for UI / overlap hints (not always execution hold) |
| `transition` | Fade / follow / wait semantics (authoring; Stage support gated by capability) |
| `conditions[]` | Guard conditions before fire |
| `dependencies[]` | Other cue ids that must have completed / fired |
| `priority` | Conflict resolution hint when overlapping |
| `armed` | Whether cue is eligible (default true) |
| `safeLocked` | Requires safety-clear context (authoring flag; Stage enforces) |

---

## 4. Actions

Each action targets a **logical device** (or production-level service) plus an intent:

| Kind (v1 authoring set) | Example intent |
|-------------------------|----------------|
| `relay` | Set absolute ON/OFF (constitution: absolute states) |
| `audio` | Play / pause / stop / volume on logical player |
| `lighting` | Scene recall / brightness / fixture group |
| `dmx` | Scene or universe snapshot reference |
| `gpio` | Logical output action (not raw pin in editor) |
| `variable` | Set / toggle production variable |
| `wait` | Hold (only if Stage supports wait cue type) |
| `command` | Escape hatch: already-validated Stage command string |

**Compile rule:** Studio Validation + Deploy must resolve actions to Stage-accepted commands. Unknown kinds for the target firmware = blocking error.

---

## 5. Execution behaviour (runtime ownership)

Defined here only so Studio authors correctly — Stage owns execution:

1. Show loaded → cues resident on Stage.
2. Clock advances (or manual GO advances cue pointer, depending on trigger mode).
3. When schedule/condition met, Stage dispatches actions.
4. Acceptance ≠ completion (Constitution Art. VII). Director/Studio display confirmed state.

Studio must not simulate completion as truth in connected mode.

---

## 6. Dependencies, transitions, delays, conditions

**Dependencies:** soft (warn) or hard (block fire until predecessor done). Hard deps that create cycles = validation error.

**Transitions:** follow-on cues, fades — stored as cue metadata; only exported if Stage capability present.

**Delays:** `delayMs` after cue time; validation rejects negative delays.

**Conditions:** expressions over variables, device online flags, or show state enums Stage already publishes. Unresolvable condition references = error.

---

## 7. Timing model

| Mode | Meaning |
|------|---------|
| Absolute | `timeMs` from show zero |
| Relative | offset from anchor cue / bookmark |
| Manual-only | no automatic time; awaits GO / trigger |

Timeline editor may display all modes on one ruler; export flattens relatives to absolute times when Stage timeline is absolute-only (current Stage RAM cue model).

---

## 8. Validation hooks (cue-specific)

- Duplicate `id`
- Empty `actions`
- Unknown device refs
- Circular dependencies
- Negative / non-monotonic schedules where mode forbids
- Command string overflow vs desk/protocol limits (existing ≤95-char class constraints where applicable)
- Capability mismatch (e.g. DMX action without DMX capability)

---

## 9. Timeline editing experience (Studio UI contract)

Not pixel UI — behaviour contract:

| Affordance | Behaviour |
|------------|-----------|
| Move | Drag changes `timeMs` / relative offset; snap optional |
| Copy | New ids; preserve actions; shift time |
| Group | Shared `groupId`; collapse/expand in UI |
| Search | By name, id, tag, device, action kind |
| Filter | Armed, colour, group, device, warnings |
| Colour coding | Per cue or per action kind defaults |
| Bookmarks | Named time marks; jump list |
| Zoom | Time ruler zoom; does not change data |

---

## Related

- [Asset System](asset-system.md)
- [Validation](validation.md)
- [Runtime Integration](runtime-integration.md)