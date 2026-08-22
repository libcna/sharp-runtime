#!/usr/bin/env python3
"""Focused tests for the offline Unicode generated-table gate."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "gen_unicode_tables.py"
SPEC = importlib.util.spec_from_file_location("gen_unicode_tables", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
TABLE_DIR = ROOT / "modules" / "core" / "include" / "System" / "Globalization" / "detail"


class UnicodeTableIntegrityTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.out_dir = Path(self.temp.name)
        for filename in MODULE.COMMITTED_INTEGRITY:
            shutil.copy2(TABLE_DIR / filename, self.out_dir / filename)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_committed_snapshot_passes_digest_declaration_and_trie_checks(self) -> None:
        self.assertEqual(MODULE.validate_committed_integrity(self.out_dir), [])

    def test_one_changed_table_byte_is_rejected(self) -> None:
        path = self.out_dir / "UnicodeCategoryTable.hpp"
        text = path.read_text(encoding="utf-8")
        path.write_text(text.replace("0x00", "0x01", 1), encoding="utf-8")

        problems = MODULE.validate_committed_integrity(self.out_dir)

        self.assertTrue(any("SHA-256" in problem for problem in problems))

    def test_truncated_array_is_rejected_even_without_the_external_source(self) -> None:
        path = self.out_dir / "UnicodeNumericTable.hpp"
        text = path.read_text(encoding="utf-8")
        path.write_text(
            text[: text.index("inline constexpr double kNumericValues")], encoding="utf-8"
        )

        problems = MODULE.validate_committed_integrity(self.out_dir)

        self.assertTrue(any("NumericValues" in problem for problem in problems))
        self.assertTrue(any("SHA-256" in problem for problem in problems))

    def test_verify_succeeds_offline_and_does_not_download(self) -> None:
        missing_source = self.out_dir / "no-runtime-checkout" / "CharUnicodeInfoData.cs"
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--verify",
                "--source",
                str(missing_source),
                "--out-dir",
                str(self.out_dir),
            ],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SHA-256 verified", result.stdout)
        self.assertIn("no download attempted", result.stdout)
        self.assertFalse(missing_source.parent.exists())


if __name__ == "__main__":
    unittest.main()
