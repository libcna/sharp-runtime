#!/usr/bin/env python3
"""Safety and path-handling tests for scripts/index_dotnet_types.py."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "index_dotnet_types.py"
SPEC = importlib.util.spec_from_file_location("index_dotnet_types", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class DotnetTypeIndexerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / "runtime-source"
        self.source.mkdir()
        self.output = self.root / "index.sqlite3"

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write_type(self, name: str) -> None:
        (self.source / "Example.cs").write_text(
            f"namespace System.Example;\npublic sealed class {name} {{}}\n",
            encoding="utf-8",
        )

    def indexed_names(self) -> list[str]:
        with sqlite3.connect(self.output) as connection:
            return [row[0] for row in connection.execute("SELECT type_name FROM types ORDER BY id")]

    def test_explicit_source_and_output_create_index(self) -> None:
        self.write_type("First")
        self.assertEqual(MODULE.build_index([self.source], self.output), (1, 1))
        self.assertEqual(self.indexed_names(), ["First"])

    def test_existing_output_is_preserved_without_replace(self) -> None:
        self.output.write_bytes(b"do-not-destroy")
        self.write_type("First")
        with self.assertRaises(FileExistsError):
            MODULE.build_index([self.source], self.output)
        self.assertEqual(self.output.read_bytes(), b"do-not-destroy")

    def test_replace_is_atomic_and_uses_new_content(self) -> None:
        self.write_type("First")
        MODULE.build_index([self.source], self.output)
        self.write_type("Second")
        MODULE.build_index([self.source], self.output, replace=True)
        self.assertEqual(self.indexed_names(), ["Second"])
        self.assertEqual(list(self.root.glob(".index.sqlite3.*.tmp")), [])

    def test_missing_source_fails_without_creating_output(self) -> None:
        with self.assertRaises(ValueError):
            MODULE.build_index([self.root / "missing"], self.output)
        self.assertFalse(self.output.exists())

    def test_missing_output_parent_fails_safely(self) -> None:
        self.write_type("First")
        with self.assertRaises(ValueError):
            MODULE.build_index([self.source], self.root / "missing" / "index.sqlite3")
        self.assertFalse((self.root / "missing").exists())

    def test_dangling_and_regular_output_symlinks_are_never_replaced(self) -> None:
        self.write_type("First")
        target = self.root / "target.sqlite3"
        self.output.symlink_to(target)
        with self.assertRaisesRegex(ValueError, "symbolic link"):
            MODULE.build_index([self.source], self.output)
        self.assertTrue(self.output.is_symlink())
        self.assertFalse(target.exists())

        self.output.unlink()
        target.write_bytes(b"preserve-target")
        self.output.symlink_to(target)
        with self.assertRaisesRegex(ValueError, "symbolic link"):
            MODULE.build_index([self.source], self.output, replace=True)
        self.assertTrue(self.output.is_symlink())
        self.assertEqual(target.read_bytes(), b"preserve-target")

    def test_late_creator_wins_and_is_not_clobbered_without_replace(self) -> None:
        self.write_type("First")
        real_link = MODULE.os.link

        def create_competing_output(source: Path, destination: Path) -> None:
            Path(destination).write_bytes(b"created-concurrently")
            real_link(source, destination)

        with mock.patch.object(MODULE.os, "link", side_effect=create_competing_output):
            with self.assertRaisesRegex(FileExistsError, "appeared during generation"):
                MODULE.build_index([self.source], self.output)

        self.assertEqual(self.output.read_bytes(), b"created-concurrently")
        self.assertEqual(list(self.root.glob(".index.sqlite3.*.tmp")), [])

    def test_source_read_failure_preserves_existing_replace_target(self) -> None:
        self.output.write_bytes(b"valid-old-index")
        self.write_type("First")
        with mock.patch.object(MODULE.Path, "read_text", side_effect=OSError("read failed")):
            with self.assertRaisesRegex(OSError, "read failed"):
                MODULE.build_index([self.source], self.output, replace=True)
        self.assertEqual(self.output.read_bytes(), b"valid-old-index")


if __name__ == "__main__":
    unittest.main()
