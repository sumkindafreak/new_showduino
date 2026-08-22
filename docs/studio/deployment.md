# Showduino Studio — Deployment Specification

**Phase:** 0 (Planning)
**Status:** Blueprint — design only

---

## 1. Purpose

Define how a validated Production becomes available for Director load and Stage execution — including package, transfer, version checks, verification, rollback, and status.

Studio owns **deployment intent and packaging**.
Stage owns **acceptance and authoritative store** (target).
Director owns **operator load/run UX**.

---

## 2. Deployment package

A **DeploymentPackage** is an immutable snapshot of a Production at a specific content version:

| Content | Notes |
|---------|-------|
| Full package tree | Per Production Format |
| `manifest.sha256` | File inventory + hashes |
| `validation-report.json` | Report that gated this deploy |
| `deploy.json` | package id, production id/version, createdAt, studioVersion, target schema |

Once built, the package bytes are not edited in place; a new version builds a new package.

---

## 3. Transfer paths (ordered preference)

| Path | Role | Notes |
|------|------|-------|
| A. Stage project store | Target SoT | Preferred long-term (Ethernet / Show Engine API when present) |
| B. Director SD packages | Transitional | Today's `/showduino/shows/packages/<id>/` |
| C. USB / removable media | Offline ferry | Same package layout; operator copies then Director/Stage import |
| D. SoftAP / tunnel file push | Convenience | Must remain transport; not a second SoT |

Communications Engine may carry bytes; it must not decide show content.

---

## 4. Version checks (before accept)

1. Package `schemaVersion` supported by target
2. Production `version` not silently overwriting without policy (see below)
3. Stage firmware / protocol compatibility labels
4. ValidationReport present, fresh, `blocking == false`

**Overwrite policy (v1):**

- Same `id` + higher `version` → upgrade path
- Same `id` + same `version` + different hash → reject (conflict)
- Same `id` + lower `version` → reject unless explicit downgrade/rollback command

---

## 5. Verification (after transfer)

Verification is mandatory before status `Deployed`:

1. Re-hash files on target vs `manifest.sha256`
2. Re-parse `production.json` / `show.json` identity fields
3. Optional: Stage ACK that package is registered
4. Optional connected check: logical devices still satisfy requirements

Failure → status `Failed`; previous version remains active if present.

---

## 6. Rollback

| Mechanism | Behaviour |
|-----------|-----------|
| Keep N prior packages | Target retains last known good |
| Rollback command | Studio/Director requests restore of prior version id |
| Verify after rollback | Same verification pipeline |

Rollback does not auto-start the show. Operator loads from Director.

---

## 7. Deployment status model

| Status | Meaning |
|--------|---------|
| `NotDeployed` | Workspace only |
| `Packaging` | Building immutable snapshot |
| `Transferring` | Bytes moving |
| `Verifying` | Hash / parse / register |
| `Deployed` | Accepted on target; available to Director catalogue |
| `Failed` | Stopped; error recorded |
| `RolledBack` | Prior version restored |

Acceptance ≠ show loaded ≠ show running.

---

## 8. History

Each attempt appends a record:

- timestamp, operator/studio identity (best-effort)
- production id/version, package hash
- target id, path used
- result, error codes
- validation report id

Stored in Studio workspace and copied to target `meta/deploy-history.json` when possible.

---

## Related

- [Validation](validation.md)
- [Runtime Integration](runtime-integration.md)
- [Production Format](production-format.md)