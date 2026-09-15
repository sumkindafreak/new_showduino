#!/usr/bin/env python3
"""Read-only validation helper for the Showduino Cursor Development Pack."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REQUIRED_FILES = [
    ROOT / "AGENTS.md",
    ROOT / ".cursor" / "rules" / "00-showduino-core.mdc",
    ROOT / ".cursor" / "rules" / "p4-show-engine.mdc",
    ROOT / ".cursor" / "rules" / "comms-controller.mdc",
    ROOT / ".cursor" / "rules" / "director.mdc",
    ROOT / ".cursor" / "rules" / "studio-webui.mdc",
    ROOT / ".cursor" / "rules" / "nodes.mdc",
    ROOT / ".cursor" / "rules" / "protocol.mdc",
    ROOT / ".cursor" / "rules" / "safety-emergency.mdc",
    ROOT / ".cursor" / "rules" / "testing-release.mdc",
    ROOT / ".cursor" / "skills" / "showduino-preflight" / "SKILL.md",
    ROOT / ".cursor" / "skills" / "showduino-build" / "SKILL.md",
    ROOT / ".cursor" / "skills" / "showduino-protocol-change" / "SKILL.md",
    ROOT / ".cursor" / "skills" / "showduino-studio-change" / "SKILL.md",
    ROOT / ".cursor" / "skills" / "showduino-release" / "SKILL.md",
    ROOT / ".cursor" / "skills" / "showduino-bench-test" / "SKILL.md",
    ROOT / "showduino-project.json",
    ROOT / "docs" / "ACCEPTANCE_TESTS.md",
    ROOT / "docs" / "KNOWN_GOOD_BASELINE.md",
    ROOT / "docs" / "DEVELOPMENT_GUARDRAILS.md",
]

MANIFEST_REFERENCE_KEYS = {
    "sourcePath",
    "firmwareVersionSource",
    "storageFwVersionSource",
    "productVersionSource",
    "protocolVersionSource",
    "deskWireVersionSource",
    "shdoPackageVersionSource",
    "source",
    "embeddedBundlePath",
    "embedTool",
    "sharedPath",
    "canonicalSource",
    "generatedBundle",
    "sourceRefs",
}


def collect_references(value):
    refs = []
    if isinstance(value, dict):
        for k, v in value.items():
            if k in MANIFEST_REFERENCE_KEYS:
                if isinstance(v, str):
                    refs.append(v)
                elif isinstance(v, list):
                    refs.extend(x for x in v if isinstance(x, str))
            refs.extend(collect_references(v))
    elif isinstance(value, list):
        for item in value:
            refs.extend(collect_references(item))
    return refs


def path_exists(rel: str) -> bool:
    if rel.startswith("http://") or rel.startswith("https://"):
        return True
    return (ROOT / rel).exists()


def main() -> int:
    errors = []

    for req in REQUIRED_FILES:
        if not req.exists():
            errors.append(f"Missing required file: {req.relative_to(ROOT)}")

    manifest_path = ROOT / "showduino-project.json"
    manifest = None
    if manifest_path.exists():
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            errors.append(f"Invalid JSON in showduino-project.json: {exc}")

    if manifest is not None:
        refs = collect_references(manifest)
        for ref in sorted(set(refs)):
            if not path_exists(ref):
                errors.append(f"Manifest reference does not exist: {ref}")

    if errors:
        print("VALIDATION: FAIL")
        for e in errors:
            print(f"- {e}")
        return 1

    print("VALIDATION: PASS")
    print("- required files present")
    print("- showduino-project.json parsed")
    print("- manifest references exist")
    return 0


if __name__ == "__main__":
    sys.exit(main())
