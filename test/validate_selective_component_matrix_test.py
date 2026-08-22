#!/usr/bin/env python3
"""Focused tests for scripts/validate_selective_component_matrix.py."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "validate_selective_component_matrix.py"
SPEC = importlib.util.spec_from_file_location("validate_selective_component_matrix", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class SelectiveMatrixValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.local = self.root / "local.sh"
        self.workflow = self.root / "components.yml"
        self.fixtures = self.root / "fixtures"
        self.fixtures.mkdir()
        (self.fixtures / "a.cpp").touch()
        (self.fixtures / "b.cpp").touch()

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write_local(self, entries: list[tuple[str, str]]) -> None:
        body = "\n".join(f'    "{component}:{fixture}"' for component, fixture in entries)
        self.local.write_text(f"MATRIX=(\n{body}\n)\n", encoding="utf-8")

    def write_workflow(self, entries: list[tuple[str, str]]) -> None:
        body = "\n".join(
            f"          - component: {component}\n            fixture: {fixture}"
            for component, fixture in entries
        )
        self.workflow.write_text(
            "jobs:\n  selective:\n    strategy:\n      matrix:\n        include:\n"
            f"{body}\n    steps:\n      - run: true\n  full:\n    steps: []\n",
            encoding="utf-8",
        )

    def test_equal_matrices_pass(self) -> None:
        entries = [("Core.Base", "a.cpp"), ("Collections.Blocking", "b.cpp")]
        self.write_local(entries)
        self.write_workflow(entries)
        self.assertEqual(MODULE.validate(self.local, self.workflow, self.fixtures), [])

    def test_missing_workflow_entry_fails(self) -> None:
        self.write_local([("Core.Base", "a.cpp"), ("Collections.Blocking", "b.cpp")])
        self.write_workflow([("Core.Base", "a.cpp")])
        problems = MODULE.validate(self.local, self.workflow, self.fixtures)
        self.assertTrue(any("workflow matrix is missing" in problem for problem in problems))

    def test_order_drift_fails(self) -> None:
        self.write_local([("Core.Base", "a.cpp"), ("Collections.Blocking", "b.cpp")])
        self.write_workflow([("Collections.Blocking", "b.cpp"), ("Core.Base", "a.cpp")])
        problems = MODULE.validate(self.local, self.workflow, self.fixtures)
        self.assertTrue(any("different order" in problem for problem in problems))

    def test_missing_fixture_fails(self) -> None:
        entries = [("Core.Base", "missing.cpp")]
        self.write_local(entries)
        self.write_workflow(entries)
        problems = MODULE.validate(self.local, self.workflow, self.fixtures)
        self.assertTrue(any("fixture does not exist" in problem for problem in problems))


if __name__ == "__main__":
    unittest.main()
