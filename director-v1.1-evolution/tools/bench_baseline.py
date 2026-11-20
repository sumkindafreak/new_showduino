#!/usr/bin/env python3
"""Inspect an existing Showduino Director UART log without changing firmware.

Python 3.9+; uses only the standard library.
Observations are *not* substitute measurements for safety, FPS or response time.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Iterable


# These match messages emitted by the existing JC8048W550C Director source.
EVENT_PATTERNS = {
    "director_boot_banner": re.compile(
        r"Showduino Portable Director 5in.*JC8048W550C starting", re.I
    ),
    "espnow_started": re.compile(r"ESP-NOW:\s*begin ok", re.I),
    "espnow_failed": re.compile(r"ESP-NOW:\s*begin failed", re.I),
    "sd_ready": re.compile(r"^\s*Storage ready\s*$", re.I),
    "sd_unavailable": re.compile(r"SD CARD NOT AVAILABLE", re.I),
    "sd_recovery_mode": re.compile(r"RECOVERY MODE ACTIVE", re.I),
    "lvgl_dual_partial_buffer": re.compile(
        r"LVGL:\s*dual PSRAM partial buffers", re.I
    ),
    "lvgl_single_partial_buffer": re.compile(
        r"LVGL:\s*single partial buffer", re.I
    ),
    "lvgl_initialisation_failure": re.compile(
        r"LVGL port init failed", re.I
    ),
    "touch_initialisation_failure": re.compile(r"Touch:\s*init failed", re.I),
    "ui_initialised": re.compile(r"UI:\s*begin done", re.I),
    "link_retry": re.compile(r"\[Link\]\s*retry:", re.I),
    "link_lost": re.compile(r"Comms lost|no heartbeat reply", re.I),
    "link_restored": re.compile(r"Comms online", re.I),
    "touch_press_log": re.compile(r"\[Touch\]\s*PRESS", re.I),
    "scroll_log": re.compile(r"\[Scroll\]\s*", re.I),
}

FAILURE_PATTERNS = {
    "guru_meditation": re.compile(r"Guru Meditation Error", re.I),
    "interrupt_watchdog": re.compile(r"Interrupt wdt timeout", re.I),
    "task_watchdog": re.compile(r"Task watchdog got triggered", re.I),
    "brownout": re.compile(r"Brownout detector was triggered", re.I),
    "abort": re.compile(r"abort\(\) was called", re.I),
}

BOOT_PSRAM_PATTERN = re.compile(r"PSRAM:\s*(\d+)\s*bytes free", re.I)


def analyse_lines(lines: Iterable[str]) -> dict[str, Any]:
    """Return evidence actually present in a UART transcript.

    Counts indicate matching log lines, not independently proven device events.
    """
    events: Counter[str] = Counter()
    failures: Counter[str] = Counter()
    psram_samples: list[int] = []
    line_count = 0

    for line in lines:
        line_count += 1
        for key, pattern in EVENT_PATTERNS.items():
            if pattern.search(line):
                events[key] += 1

        for key, pattern in FAILURE_PATTERNS.items():
            if pattern.search(line):
                failures[key] += 1

        psram = BOOT_PSRAM_PATTERN.search(line)
        if psram:
            psram_samples.append(int(psram.group(1)))

    return {
        "schema": "showduino.director.v1_1.serial_observation.v1",
        "lines_read": line_count,
        "board_banner_observed": events["director_boot_banner"] > 0,
        "events": dict(sorted(events.items())),
        "failure_signatures": dict(sorted(failures.items())),
        "boot_psram_free_bytes_samples": psram_samples,
        "measured_fps": None,
        "measured_touch_latency_ms": None,
        "measured_runtime_heap_min_bytes": None,
        "physical_emergency_test_passed": None,
        "endurance_test_passed": None,
        "limitations": [
            "Serial text is not a physical hardware safety test.",
            "A missing failure signature does not prove stability.",
            "Boot PSRAM-free lines are not runtime heap minima.",
            "No FPS, touch latency, reconnect timing or 24-hour endurance "
            "result can be inferred from these existing log messages.",
        ],
    }


def to_markdown(report: dict[str, Any]) -> str:
    """Create a readable, appropriately qualified observation report."""
    rows = [
        "# Showduino Director — Serial observation",
        "",
        "Read-only evidence from an existing UART capture. "
        "This is **not** a hardware acceptance certificate.",
        "",
        f"- Lines inspected: {report['lines_read']}",
        "- JC8048W550C boot banner observed: "
        + ("yes" if report["board_banner_observed"] else "no"),
        "- Boot PSRAM-free samples (bytes): "
        + (
            ", ".join(str(v) for v in report["boot_psram_free_bytes_samples"])
            if report["boot_psram_free_bytes_samples"]
            else "not reported"
        ),
        "",
        "## Recognised events",
        "",
    ]

    events = report["events"]
    if events:
        rows.extend(f"- {key}: {value}" for key, value in events.items())
    else:
        rows.append("- No recognised Director events in this capture.")

    rows.extend(["", "## Possible failure signatures", ""])
    failures = report["failure_signatures"]
    if failures:
        rows.extend(f"- {key}: {value}" for key, value in failures.items())
    else:
        rows.append("- None observed in the supplied text (not proof of none).")

    rows.extend(
        [
            "",
            "## Not measured",
            "",
            "- FPS and frame-time distribution",
            "- Touch response latency",
            "- Runtime heap low-water mark / fragmentation",
            "- Verified emergency behaviour and no-resume policy",
            "- 24-hour uninterrupted hardware stability",
            "",
            "Do not approve a Director V1.1 firmware upgrade from "
            "this log observation alone.",
            "",
        ]
    )
    return "\n".join(rows)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Read-only Showduino Director UART log observation"
    )
    parser.add_argument(
        "log", help="Saved 115200-baud Director UART capture path, or '-' for stdin"
    )
    parser.add_argument(
        "--format",
        choices=("json", "markdown"),
        default="markdown",
        help="Output format (default: markdown)",
    )
    parser.add_argument(
        "--output", type=Path, help="Write report to this file, otherwise stdout"
    )
    args = parser.parse_args(argv)

    try:
        if args.log == "-":
            report = analyse_lines(sys.stdin)
        else:
            with Path(args.log).open("r", encoding="utf-8", errors="replace") as f:
                report = analyse_lines(f)
        if args.format == "json":
            output = json.dumps(report, indent=2, sort_keys=True) + "\n"
        else:
            output = to_markdown(report)

        if args.output:
            args.output.write_text(output, encoding="utf-8")
        else:
            sys.stdout.write(output)
        return 0
    except OSError as exc:
        parser.exit(2, f"Cannot read or write requested file: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
