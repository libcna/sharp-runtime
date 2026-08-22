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
        self.allowlist = '{"dependencies": []}\n'

    def add_module(
        self,
        directory: str,
        name: str,
        *,
        public_dependencies: tuple[str, ...] = (),
        private_dependencies: tuple[str, ...] = (),
        legacy_dependencies: tuple[str, ...] = (),
        test_dependencies: tuple[str, ...] = (),
        module_type: str = "STATIC",
        target: str | None = None,
    ) -> None:
        self.module_directories.append(directory)
        module_root = self.root / "modules" / directory
        (module_root / "include").mkdir(parents=True)
        (module_root / "src").mkdir()
        lines = [
            "sharp_runtime_register_module(",
            f"    NAME {name}",
            f"    TARGET {target or 'fixture_' + directory.replace('-', '_')}",
            f"    TYPE {module_type}",
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
        if test_dependencies:
            lines.append("    TEST_DEPENDENCIES " + " ".join(test_dependencies))
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

    def write_test(self, module: str, relative_path: str, content: str = "") -> None:
        path = self.root / "modules" / module / "tests" / relative_path
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
            self.allowlist,
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

    def test_build_generated_header_resolves_without_an_owning_module(self) -> None:
        # SharpRuntime/Version.hpp is rendered by cmake/Version.cmake into the build tree and
        # published on SharpRuntime::Headers, so no module directory owns it and no module
        # declares an edge for it. Both the implementation scan and the test scan must accept it.
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.write_source(
            "core",
            "System/Object.cpp",
            '#include "SharpRuntime/Version.hpp"\n',
        )
        self.fixture.write_test(
            "core",
            "SharpRuntime/VersionTests.cpp",
            '#include "SharpRuntime/Version.hpp"\n',
        )
        report = self.validate()
        self.assertEqual([], report.problems)

    def test_unknown_sharp_runtime_header_still_fails(self) -> None:
        # The exception is one named path, not the SharpRuntime/ prefix: a typo or a header that
        # was never generated must still be reported, or the generated-header allowance would
        # silently disable ownership checking for everything under that prefix.
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.write_source(
            "core",
            "System/Object.cpp",
            '#include "SharpRuntime/Versions.hpp"\n',
        )
        report = self.validate()
        self.assertTrue(
            any("SharpRuntime/Versions.hpp" in problem for problem in report.problems),
            report.problems,
        )

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

    def test_private_include_requires_private_or_public_dependency(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module("feature", "Feature")
        self.fixture.write_header("feature", "System/Feature.hpp")
        self.fixture.write_source(
            "feature",
            "System/Feature.cpp",
            '#include "System/Object.hpp"\n',
        )
        report = self.validate()
        self.assertTrue(
            any("privately includes 'Core' without a dependency" in problem
                for problem in report.problems),
            report.problems,
        )

    def test_implementation_only_include_cannot_silently_widen_visibility(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module(
            "feature",
            "Feature",
            public_dependencies=("Core",),
        )
        self.fixture.write_header("feature", "System/Feature.hpp")
        self.fixture.write_source(
            "feature",
            "System/Feature.cpp",
            '#include "System/Object.hpp"\n',
        )
        report = self.validate()
        self.assertTrue(
            any("only in implementation sources but declares it public" in problem
                for problem in report.problems),
            report.problems,
        )

    def test_test_include_requires_test_dependency(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module("feature", "Feature")
        self.fixture.write_header("feature", "System/Feature.hpp")
        self.fixture.write_test(
            "feature",
            "System/FeatureTests.cpp",
            '#include "System/Object.hpp"\n',
        )
        report = self.validate()
        self.assertTrue(
            any("without TEST_DEPENDENCIES" in problem for problem in report.problems),
            report.problems,
        )

    def test_stale_test_dependency_fails(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module(
            "feature",
            "Feature",
            test_dependencies=("Core",),
        )
        self.fixture.write_header("feature", "System/Feature.hpp")
        report = self.validate()
        self.assertTrue(
            any("declares stale test dependency 'Core'" in problem
                for problem in report.problems),
            report.problems,
        )

    def test_unknown_production_and_test_dependencies_fail(self) -> None:
        self.fixture.add_module(
            "feature",
            "Feature",
            private_dependencies=("MissingProduction",),
            test_dependencies=("MissingTest",),
        )
        self.fixture.write_header("feature", "System/Feature.hpp")
        report = self.validate()
        self.assertTrue(
            any("unknown dependency 'MissingProduction'" in problem
                for problem in report.problems),
            report.problems,
        )
        self.assertTrue(
            any("unknown test dependency 'MissingTest'" in problem
                for problem in report.problems),
            report.problems,
        )

    def test_mixed_dependency_syntax_and_interface_private_dependency_fail(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module(
            "mixed",
            "Mixed",
            public_dependencies=("Core",),
            legacy_dependencies=("Core",),
        )
        self.fixture.write_header("mixed", "System/Mixed.hpp")
        self.fixture.add_module(
            "interface",
            "Interface",
            private_dependencies=("Core",),
            module_type="INTERFACE",
        )
        self.fixture.write_header("interface", "System/Interface.hpp")
        report = self.validate()
        self.assertTrue(
            any("mixes legacy DEPENDENCIES" in problem for problem in report.problems),
            report.problems,
        )
        self.assertTrue(
            any("INTERFACE module 'interface' cannot have PRIVATE_DEPENDENCIES" in problem
                for problem in report.problems),
            report.problems,
        )

    def test_duplicate_component_name_and_target_fail(self) -> None:
        self.fixture.add_module("first", "Duplicate", target="fixture_duplicate")
        self.fixture.write_header("first", "System/First.hpp")
        self.fixture.add_module("second", "Duplicate", target="fixture_duplicate")
        self.fixture.write_header("second", "System/Second.hpp")
        report = self.validate()
        self.assertTrue(
            any("component name 'Duplicate' is owned by both" in problem
                for problem in report.problems),
            report.problems,
        )
        self.assertTrue(
            any("target 'fixture_duplicate' is owned by both" in problem
                for problem in report.problems),
            report.problems,
        )

    def test_allowlist_must_reference_a_declared_edge_with_matching_visibility(self) -> None:
        self.fixture.add_module("core", "Core")
        self.fixture.write_header("core", "System/Object.hpp")
        self.fixture.add_module(
            "feature",
            "Feature",
            private_dependencies=("Core",),
        )
        self.fixture.write_header("feature", "System/Feature.hpp")
        self.fixture.write_source(
            "feature",
            "System/Feature.cpp",
            '#include "System/Object.hpp"\n',
        )
        self.fixture.allowlist = textwrap.dedent(
            """\
            {"dependencies": [
              {"module": "Feature", "dependency": "Core", "visibility": "PUBLIC", "reason": "fixture"},
              {"module": "Feature", "dependency": "Missing", "visibility": "PRIVATE", "reason": "fixture"}
            ]}
            """
        )
        report = self.validate()
        self.assertTrue(
            any("claims PUBLIC visibility but declaration is private" in problem
                for problem in report.problems),
            report.problems,
        )
        self.assertTrue(
            any("Feature -> Missing is not declared" in problem
                for problem in report.problems),
            report.problems,
        )


if __name__ == "__main__":
    unittest.main()
