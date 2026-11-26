"""Regression tests for the canonical Studio fetch path (no network required)."""

import io
import subprocess
import unittest
import urllib.error
from unittest.mock import patch, MagicMock

import embed_webui as builder

COMMIT = "a" * 40
TAG = "b" * 40


class FetchTests(unittest.TestCase):
    def test_branch_resolution_does_not_call_rest_api(self):
        result = subprocess.CompletedProcess([], 0, f"{COMMIT}\trefs/heads/main\n", "")
        with patch.object(builder, "AUTHORING_REF", "main"), \
             patch.object(builder, "AUTHORING_COMMIT_OVERRIDE", ""), \
             patch.object(builder.subprocess, "run", return_value=result) as git, \
             patch.object(builder, "fetch_bytes") as http:
            self.assertEqual(builder.resolve_authoring_commit(), COMMIT)
            self.assertIn("ls-remote", git.call_args.args[0])
            http.assert_not_called()

    def test_annotated_tag_resolves_to_peeled_commit(self):
        result = subprocess.CompletedProcess([], 0, f"{TAG}\trefs/tags/v1\n{COMMIT}\trefs/tags/v1^{{}}\n", "")
        with patch.object(builder, "AUTHORING_REF", "v1"), \
             patch.object(builder, "AUTHORING_COMMIT_OVERRIDE", ""), \
             patch.object(builder.subprocess, "run", return_value=result):
            self.assertEqual(builder.resolve_authoring_commit(), COMMIT)

    def test_override_bypasses_network(self):
        with patch.object(builder, "AUTHORING_COMMIT_OVERRIDE", COMMIT), \
             patch.object(builder.subprocess, "run") as git:
            self.assertEqual(builder.resolve_authoring_commit(), COMMIT)
            git.assert_not_called()

    def test_git_failure_retries_and_fails_closed(self):
        failure = subprocess.CalledProcessError(128, ["git"])
        with patch.object(builder, "AUTHORING_REF", "main"), \
             patch.object(builder, "AUTHORING_COMMIT_OVERRIDE", ""), \
             patch.object(builder.subprocess, "run", side_effect=failure) as git, \
             patch.object(builder.time, "sleep") as sleep:
            with self.assertRaisesRegex(RuntimeError, "three attempts"):
                builder.resolve_authoring_commit()
            self.assertEqual(git.call_count, 3)
            self.assertEqual(sleep.call_count, 2)

    def test_transient_http_failure_retries(self):
        failure = urllib.error.HTTPError("https://example.com", 503, "Unavailable", {}, None)
        response = MagicMock()
        response.__enter__.return_value = io.BytesIO(b"asset")
        with patch.object(builder.urllib.request, "urlopen", side_effect=[failure, response]) as http, \
             patch.object(builder.time, "sleep") as sleep:
            self.assertEqual(builder.fetch_bytes("https://example.com"), b"asset")
            self.assertEqual(http.call_count, 2)
            sleep.assert_called_once_with(1)

    def test_missing_asset_fails_without_retry(self):
        failure = urllib.error.HTTPError("https://example.com", 404, "Not found", {}, None)
        with patch.object(builder.urllib.request, "urlopen", side_effect=failure) as http, \
             patch.object(builder.time, "sleep") as sleep:
            with self.assertRaises(urllib.error.HTTPError):
                builder.fetch_bytes("https://example.com")
            http.assert_called_once()
            sleep.assert_not_called()

    def test_long_rate_limit_delay_fails_without_retry(self):
        failure = urllib.error.HTTPError("https://example.com", 429, "Rate limited", {"Retry-After": "120"}, None)
        with patch.object(builder.urllib.request, "urlopen", side_effect=failure) as http, \
             patch.object(builder.time, "sleep") as sleep:
            with self.assertRaises(urllib.error.HTTPError):
                builder.fetch_bytes("https://example.com")
            http.assert_called_once()
            sleep.assert_not_called()


if __name__ == "__main__":
    unittest.main()
