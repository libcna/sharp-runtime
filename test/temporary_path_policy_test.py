#!/usr/bin/env python3
"""Regression tests for the repository-local mktemp policy."""

from __future__ import annotations

from pathlib import Path
import unittest


REPOSITORY = Path(__file__).resolve().parents[1]
SCRIPTS = REPOSITORY / "scripts"
LOCAL_MKTEMP = 'TMPDIR="$REPO_ROOT/build-tmp" mktemp'
CREATE_LOCAL_TMPDIR = 'mkdir -p "$REPO_ROOT/build-tmp"'


class TemporaryPathPolicyTests(unittest.TestCase):
    def test_every_shell_mktemp_is_redirected_to_build_tmp(self) -> None:
        calls: list[tuple[Path, int, str]] = []
        for script in sorted(SCRIPTS.glob("*.sh")):
            lines = script.read_text(encoding="utf-8").splitlines()
            for line_number, line in enumerate(lines, start=1):
                stripped = line.strip()
                if stripped.startswith("#") or "mktemp" not in stripped:
                    continue
                calls.append((script, line_number, stripped))
                self.assertIn(
                    LOCAL_MKTEMP,
                    stripped,
                    f"{script.relative_to(REPOSITORY)}:{line_number} uses bare mktemp",
                )
                self.assertIn(
                    CREATE_LOCAL_TMPDIR,
                    lines[: line_number - 1],
                    f"{script.relative_to(REPOSITORY)}:{line_number} does not create build-tmp first",
                )

        self.assertGreater(len(calls), 0, "the policy test did not inspect any mktemp call")


if __name__ == "__main__":
    unittest.main()
