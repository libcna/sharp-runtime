#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Negative and positive fixtures for validate_module_boundaries.py."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import textwrap
import unittest


REPO_ROOT = Path(__file__).resolve().parent.parent
VALIDATOR_PATH = REPO_ROOT / "scripts/validate_module_boundaries.py"
SPEC = importlib.util.spec_from_file_location("module_boundary_validator", VALIDATOR_PATH)
assert SPEC is not None and SPEC.loader is not None
VALIDATOR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = VALIDATOR
SPEC.loader.exec_module(VALIDATOR)


class FixtureRepository:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.module_directories: list[str] = []

    def add_module(
        self,
        directory: str,
        name: str,
        *,
        public_dependencies: tuple[str, ...] = (),
        private_dependencies: tuple[str, ...] = (),
        legacy_dependencies: tuple[str, ...] = (),
    ) -> None:
        self.module_directories.append(directory)
        module_root = self.root / "modules" / directory
        (module_root / "include").mkdir(parents=True)
        (module_root / "src").mkdir()
        lines = [
            "sharp_runtime_register_module(",
            f"    NAME {name}",
            f"    TARGET fixture_{directory.replace('-', '_')}",
            "    TYPE STATIC",
        ]
        if legacy_dependencies:
            lines.append("    DEPENDENCIES " + " ".join(legacy_dependencies))
        if public_dependencies:
            lines.append(
                "    PUBLIC_DEPENDENCIES " + " ".join(public_dependencies)
            )
        if private_dependencies:
            lines.append(
                "    PRIVATE_DEPENDENCIES " + " ".join(private_dependencies)
            )
        lines.append(")")
        (module_root / "CMakeLists.txt").write_text(
            "\n".join(lines) + "\n",
            encoding="utf-8",
        )

    def write_header(self, module: str, logical_path: str, content: str = "") -> None:
        path = self.root / "modules" / module / "include" / logical_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            "#pragma once\n" + textwrap.dedent(content),
            encoding="utf-8",
        )

    def write_source(self, module: str, relative_path: str, content: str = "") -> None:
        path = self.root / "modules" / module / "src" / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(textwrap.dedent(content), encoding="utf-8")

    def finish(self) -> None:
        cmake_dir = self.root / "cmake"
        cmake_dir.mkdir(exist_ok=True)
        (cmake_dir / "SharpRuntimeModules.cmake").write_text(
            "set(sharp_runtime_module_directories\n"
            + "".join(f"    {directory}\n" for directory in self.module_directories)
            + ")\n",
            encoding="utf-8",
        )
        (cmake_dir / "SharpRuntimeModuleDependencyAllowlist.json").write_text(
            '{"dependencies": []}\n',
            encoding="utf-8",
        )


class ModuleBoundaryValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.fixture = FixtureRepository(self.root)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def validate(self):
        self.fixture.finish()
        return VALIDATOR.validate_repository(self.root)

    def test_valid_repository_passes(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module(
            "feature",
            "Feature",
            public_dependencies=("Core",),
        )
        self.fixture.write_header(
            "feature",
            "System/Feature.hpp",
            '#include "System/Object.hpp"\n',
        )
        report = self.validate()
        self.assertEqual([], report.problems)
        self.assertEqual(2, report.module_count)
        self.assertEqual(1, report.dependency_edge_count)

    def test_unregistered_physical_module_fails(self) -> None:
        self.fixture.add_module("core", "Core")
        orphan = self.root / "modules" / "orphan"
        (orphan / "include/System").mkdir(parents=True)
        (orphan / "include/System/Orphan.hpp").write_text(
            "#pragma once\n",
            encoding="utf-8",
        )
        report = self.validate()
        self.assertTrue(
            any("is not registered" in problem for problem in report.problems)
        )

    def test_duplicate_logical_header_fails(self) -> None:
        self.fixture.add_module("first", "First")
        self.fixture.write_header("first", "System/Duplicate.hpp")
        self.fixture.add_module("second", "Second")
        self.fixture.write_header("second", "System/Duplicate.hpp")
        report = self.validate()
        self.assertTrue(
            any("duplicate public include path" in problem
                for problem in report.problems)
        )

    def test_undeclared_cross_module_include_fails(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module("feature", "Feature")
        self.fixture.write_header(
            "feature",
            "System/Feature.hpp",
            '#include "System/Object.hpp"\n',
        )
        report = self.validate()
        self.assertTrue(
            any("without a public dependency" in problem
                for problem in report.problems)
        )

    def test_stale_dependency_fails(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module(
            "feature",
            "Feature",
            public_dependencies=("Core",),
        )
        self.fixture.write_header("feature", "System/Feature.hpp")
        report = self.validate()
        self.assertTrue(
            any("declares stale dependency" in problem
                for problem in report.problems)
        )

    def test_dependency_cycle_fails(self) -> None:
        self.fixture.add_module(
            "first",
            "First",
            public_dependencies=("Second",),
        )
        self.fixture.add_module(
            "second",
            "Second",
            public_dependencies=("First",),
        )
        self.fixture.write_header(
            "first",
            "System/First.hpp",
            '#include "System/Second.hpp"\n',
        )
        self.fixture.write_header(
            "second",
            "System/Second.hpp",
            '#include "System/First.hpp"\n',
        )
        report = self.validate()
        self.assertTrue(
            any("cyclic module dependency" in problem
                for problem in report.problems)
        )

    def test_public_header_cannot_use_private_dependency(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module(
            "feature",
            "Feature",
            private_dependencies=("Core",),
        )
        self.fixture.write_header(
            "feature",
            "System/Feature.hpp",
            '#include "System/Object.hpp"\n',
        )
        report = self.validate()
        self.assertTrue(
            any("declares 'Core' private" in problem
                for problem in report.problems)
        )


if __name__ == "__main__":
    unittest.main()
