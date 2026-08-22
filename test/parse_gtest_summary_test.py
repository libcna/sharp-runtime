#!/usr/bin/env python3
"""Focused tests for the component gate's GoogleTest summary accounting."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "parse_gtest_summary.py"
RUNNER = SCRIPT.parent / "run_component_tests.sh"
SPEC = importlib.util.spec_from_file_location("parse_gtest_summary", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ParseGoogleTestSummaryTests(unittest.TestCase):
    def test_all_passing_summary(self) -> None:
        text = """\
[==========] Running 2 tests from 1 test suite.
[==========] 2 tests from 1 test suite ran. (1 ms total)
[  PASSED  ] 2 tests.
"""
        self.assertEqual(MODULE.parse_summary(text), MODULE.Summary(2, 2, 0, 0))

    def test_skipped_and_failed_are_not_counted_as_passed(self) -> None:
        text = """\
[==========] 5 tests from 2 test suites ran. (1 ms total)
[  PASSED  ] 2 tests.
[  SKIPPED ] 2 tests, listed below:
[  SKIPPED ] Suite.SkippedOne
[  SKIPPED ] Suite.SkippedTwo
[  FAILED  ] 1 test, listed below:
[  FAILED  ] Suite.Failed
"""
        self.assertEqual(MODULE.parse_summary(text), MODULE.Summary(5, 2, 1, 2))

    def test_per_test_result_lines_do_not_replace_aggregate_counts(self) -> None:
        text = """\
[==========] 3 tests from 1 test suite ran. (1 ms total)
[  PASSED  ] 2 tests.
[  SKIPPED ] 1 test, listed below:
[  SKIPPED ] Suite.Skipped
"""
        self.assertEqual(MODULE.parse_summary(text), MODULE.Summary(3, 2, 0, 1))

    def test_missing_summary_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "run summary is missing"):
            MODULE.parse_summary("[ RUN      ] Suite.Crashed\n")

    def test_inconsistent_totals_are_rejected(self) -> None:
        text = """\
[==========] 3 tests from 1 test suite ran. (1 ms total)
[  PASSED  ] 2 tests.
"""
        with self.assertRaisesRegex(ValueError, "inconsistent GoogleTest summary"):
            MODULE.parse_summary(text)

    def test_component_gate_rejects_an_exit_zero_skipped_test(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fake = Path(directory) / "SharpRuntimeTests_Fake"
            fake.write_text(
                "#!/usr/bin/env bash\n"
                "echo '[==========] 1 test from 1 test suite ran. (0 ms total)'\n"
                "echo '[  PASSED  ] 0 tests.'\n"
                "echo '[  SKIPPED ] 1 test, listed below:'\n"
                "echo '[  SKIPPED ] Fake.EnvironmentUnavailable'\n",
                encoding="utf-8",
            )
            fake.chmod(0o755)
            result = subprocess.run(
                [str(RUNNER), directory],
                cwd=SCRIPT.parents[1],
                text=True,
                capture_output=True,
                check=False,
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("zero-failure/zero-skip", result.stderr)
        self.assertIn("skipped=1", result.stderr)


if __name__ == "__main__":
    unittest.main()
