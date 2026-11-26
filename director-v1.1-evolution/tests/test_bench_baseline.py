#!/usr/bin/env python3
"""Offline tests for the Director V1.1 serial-log observer.

Run from the repository root:
    python -m unittest discover -s director-v1.1-evolution/tests -v
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from bench_baseline import analyse_lines, to_markdown


class BenchBaselineTests(unittest.TestCase):
    def test_existing_boot_markers_are_detected(self) -> None:
        lines = [
            "Showduino Portable Director 5in - JC8048W550C starting...\n",
            "PSRAM: 5678901 bytes free\n",
            "ESP-NOW: begin ok\n",
            "Storage ready\n",
            "LVGL: dual PSRAM partial buffers (landscape).\n",
            "UI: begin done\n",
        ]
        report = analyse_lines(lines)
        self.assertEqual(report["lines_read"], len(lines))
        self.assertTrue(report["board_banner_observed"])
        self.assertEqual(report["boot_psram_free_bytes_samples"], [5678901])
        self.assertEqual(report["events"]["ui_initialised"], 1)
        self.assertEqual(report["events"]["lvgl_dual_partial_buffer"], 1)
        self.assertEqual(report["failure_signatures"], {})

    def test_recovery_and_failure_signatures(self) -> None:
        report = analyse_lines(
            [
                "SD CARD NOT AVAILABLE\n",
                "RECOVERY MODE ACTIVE\n",
                "ESP-NOW: begin failed\n",
                "[Link] retry: ESP-NOW re-init failed\n",
                "Task watchdog got triggered\n",
                "Brownout detector was triggered\n",
            ]
        )
        self.assertEqual(report["events"]["sd_unavailable"], 1)
        self.assertEqual(report["events"]["sd_recovery_mode"], 1)
        self.assertEqual(report["events"]["link_retry"], 1)
        self.assertEqual(report["failure_signatures"]["task_watchdog"], 1)
        self.assertEqual(report["failure_signatures"]["brownout"], 1)
        self.assertFalse(report["board_banner_observed"])

    def test_missing_evidence_never_becomes_claimed_pass(self) -> None:
        report = analyse_lines([])
        self.assertIsNone(report["measured_fps"])
        self.assertIsNone(report["physical_emergency_test_passed"])
        self.assertIsNone(report["endurance_test_passed"])
        markdown = to_markdown(report)
        self.assertIn("not proof of none", markdown)
        self.assertIn("not a hardware acceptance certificate", markdown)


if __name__ == "__main__":
    unittest.main()
