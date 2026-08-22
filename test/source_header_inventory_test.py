#!/usr/bin/env python3
"""Regression tests for source/header planning cross-references."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "source_header_inventory.py"
SPEC = importlib.util.spec_from_file_location("source_header_inventory", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class SourceHeaderInventoryCrossReferenceTests(unittest.TestCase):
    def test_rows_are_annotated_in_both_directions(self) -> None:
        rows = [
            {
                "path": "modules/example/include/System/Example/Present.hpp",
                "lines": 10,
                "has_spdx": True,
                "namespace": "System::Example",
                "types": "Present;Helper",
            },
            {
                "path": "modules/example/include/System/Example/Helper.hpp",
                "lines": 5,
                "has_spdx": True,
                "namespace": "System::Example",
                "types": "Helper",
            },
            {
                "path": "modules/example/src/System/Example/Present.cpp",
                "lines": 8,
                "has_spdx": True,
                "namespace": "System::Example",
                "types": "ImplementationDetail",
            },
        ]
        tasks = {
            ("System.Example", "Present"): "ported",
            ("System.Example", "Missing"): "ported",
        }

        source_only, plan_only = MODULE.cross_reference(rows, tasks)

        self.assertEqual(rows[0]["task_matches"], "System.Example.Present:ported")
        self.assertEqual(rows[0]["unmatched_types"], "")
        self.assertEqual(source_only, [("System.Example", "Helper")])
        self.assertEqual(plan_only, [("System.Example", "Missing")])

    def test_cpp_only_declaration_does_not_satisfy_header_inventory(self) -> None:
        rows = [
            {
                "path": "modules/example/src/System/Example/Hidden.cpp",
                "lines": 8,
                "has_spdx": True,
                "namespace": "System::Example",
                "types": "Hidden",
            }
        ]
        tasks = {("System.Example", "Hidden"): "ported"}
        _, plan_only = MODULE.cross_reference(rows, tasks)
        self.assertEqual(plan_only, [("System.Example", "Hidden")])

    def test_path_namespace_beats_an_earlier_forward_declaration(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            header = (Path(directory) / "modules/example/include/System/Timers/Timer.hpp")
            header.parent.mkdir(parents=True)
            header.write_text(
                "namespace System::Threading {\nclass Timer;\n}\n"
                "namespace System::Timers {\nclass Timer {};\n}\n",
                encoding="utf-8",
            )
            row = MODULE.inspect_file(str(header))
        self.assertEqual(row["namespace"], "System::Timers")
        self.assertEqual(row["types"], "Timer")
        self.assertEqual(row["owned_types"], "Timer")

    def test_foreign_forward_declaration_does_not_satisfy_a_ported_task(self) -> None:
        rows = [{
            "path": "modules/example/include/System/Example/Container.hpp",
            "lines": 5,
            "has_spdx": True,
            "namespace": "System::Example",
            "types": "Missing;Container",
            "owned_types": "Container",
        }]
        source_only, plan_only = MODULE.cross_reference(
            rows, {
                ("System.Example", "Container"): "ignored",
                ("System.Example", "Missing"): "ported",
            }
        )
        self.assertEqual(source_only, [])
        self.assertEqual(plan_only, [("System.Example", "Missing")])

    def test_delegate_alias_satisfies_a_ported_task(self) -> None:
        rows = [{
            "path": "modules/example/include/System/Example/Callback.hpp",
            "lines": 5,
            "has_spdx": True,
            "namespace": "System::Example",
            "types": "Callback",
        }]
        source_only, plan_only = MODULE.cross_reference(
            rows, {("System.Example", "Callback"): "ported"}
        )
        self.assertEqual(source_only, [])
        self.assertEqual(plan_only, [])

    def test_exact_exemptions_are_applied_and_stale_entries_fail(self) -> None:
        source = [("System", "CppHelper")]
        plan = [("System.Linq", "Enumerable")]
        exemptions = {
            "source_without_task": {"System.CppHelper": "C++ representation"},
            "ported_without_header": {"System.Linq.Enumerable": "namespace functions"},
        }
        self.assertEqual(MODULE.apply_exemptions(source, plan, exemptions), ([], [], []))
        exemptions["source_without_task"]["System.Stale"] = "stale"
        _, _, problems = MODULE.apply_exemptions(source, plan, exemptions)
        self.assertEqual(problems, ["stale source_without_task exemption: System.Stale"])

    def test_missing_database_is_not_silently_treated_as_an_empty_plan(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(FileNotFoundError):
                MODULE.load_tasks(str(Path(directory) / "missing.sqlite3"))

    def test_unreadable_header_is_not_silently_omitted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "Missing.hpp"
            with self.assertRaises(FileNotFoundError):
                MODULE.inspect_file(str(missing))

    def test_directory_walk_error_is_not_silently_omitted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            module = Path(directory) / "example"
            (module / "include").mkdir(parents=True)

            def failing_walk(_path, *, onerror):
                onerror(PermissionError("denied subtree"))
                return iter(())

            with mock.patch.object(MODULE.os, "walk", side_effect=failing_walk):
                with self.assertRaisesRegex(PermissionError, "denied subtree"):
                    list(MODULE.collect_files(directory))

    def test_optional_source_or_include_root_may_be_absent(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            include = Path(directory) / "header-only" / "include"
            include.mkdir(parents=True)
            header = include / "Only.hpp"
            header.write_text("// SPDX-License-Identifier: MIT\n", encoding="utf-8")

            self.assertEqual(list(MODULE.collect_files(directory)), [str(header)])


if __name__ == "__main__":
    unittest.main()
