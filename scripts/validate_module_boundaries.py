#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Validate Sharp Runtime's physical module ownership and dependency graph.

The checker intentionally reads source metadata directly and does not require a
configured build directory. It verifies:

* every physical module directory is registered exactly once;
* public include spellings are unique;
* project-local includes resolve to an owned header;
* cross-module includes have a declared dependency with correct visibility;
* declared dependencies are used (or have a documented allow-list entry);
* the module dependency graph is acyclic.

Usage:
    scripts/validate_module_boundaries.py [--root PATH]

Documented link-only exceptions may be placed in
``cmake/SharpRuntimeModuleDependencyAllowlist.json``. Each entry must contain
``module``, ``dependency``, ``visibility`` (``PUBLIC`` or ``PRIVATE``), and a
non-empty ``reason``.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import json
from pathlib import Path
import re
import shlex
import sys
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parent.parent
MODULE_LIST_PATH = Path("cmake/SharpRuntimeModules.cmake")
ALLOWLIST_PATH = Path("cmake/SharpRuntimeModuleDependencyAllowlist.json")
PROJECT_INCLUDE_PREFIXES = ("System/", "SharpRuntime/")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inl"}
MODULE_SINGLE_VALUE_FIELDS = {"NAME", "TARGET", "TYPE", "SETUP"}
MODULE_MULTI_VALUE_FIELDS = {
    "DEPENDENCIES",
    "PUBLIC_DEPENDENCIES",
    "PRIVATE_DEPENDENCIES",
    "TEST_DEPENDENCIES",
}
MODULE_FIELDS = MODULE_SINGLE_VALUE_FIELDS | MODULE_MULTI_VALUE_FIELDS
INCLUDE_RE = re.compile(
    r'^\s*#\s*include\s*[<"]([^>"]+)[>"]',
    re.MULTILINE,
)


@dataclass(frozen=True)
class Module:
    directory: str
    name: str
    target: str
    module_type: str
    public_dependencies: frozenset[str]
    private_dependencies: frozenset[str]
    legacy_dependencies: frozenset[str]
    test_dependencies: frozenset[str]

    @property
    def all_dependencies(self) -> frozenset[str]:
        return (
            self.public_dependencies
            | self.private_dependencies
            | self.legacy_dependencies
        )

    @property
    def uses_explicit_visibility(self) -> bool:
        return bool(self.public_dependencies or self.private_dependencies)


@dataclass(frozen=True)
class AllowedDependency:
    module: str
    dependency: str
    visibility: str
    reason: str


@dataclass
class ValidationReport:
    problems: list[str] = field(default_factory=list)
    module_count: int = 0
    dependency_edge_count: int = 0

    @property
    def ok(self) -> bool:
        return not self.problems


def _extract_call_bodies(text: str, function_name: str) -> list[str]:
    """Return balanced parenthesized bodies for calls to *function_name*."""
    pattern = re.compile(rf"\b{re.escape(function_name)}\s*\(")
    bodies: list[str] = []
    for match in pattern.finditer(text):
        start = match.end()
        depth = 1
        quote: str | None = None
        escaped = False
        for index in range(start, len(text)):
            char = text[index]
            if quote is not None:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == quote:
                    quote = None
                continue
            if char in {'"', "'"}:
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    bodies.append(text[start:index])
                    break
        else:
            raise ValueError(f"unterminated {function_name}() call")
    return bodies


def _cmake_tokens(body: str) -> list[str]:
    lexer = shlex.shlex(body, posix=True)
    lexer.whitespace_split = True
    lexer.commenters = "#"
    return list(lexer)


def _parse_module_directories(path: Path) -> tuple[list[str], list[str]]:
    problems: list[str] = []
    if not path.is_file():
        return [], [f"module list not found: {path}"]
    try:
        bodies = _extract_call_bodies(path.read_text(encoding="utf-8"), "set")
    except (OSError, ValueError) as error:
        return [], [f"cannot parse module list {path}: {error}"]
    for body in bodies:
        tokens = _cmake_tokens(body)
        if tokens and tokens[0] == "sharp_runtime_module_directories":
            directories = tokens[1:]
            duplicates = sorted(
                directory
                for directory in set(directories)
                if directories.count(directory) > 1
            )
            for directory in duplicates:
                problems.append(
                    f"module directory '{directory}' is listed more than once in {path}"
                )
            return directories, problems
    return [], [f"sharp_runtime_module_directories is not defined in {path}"]


def _field_values(tokens: list[str], field_name: str) -> list[str]:
    if field_name not in tokens:
        return []
    index = tokens.index(field_name) + 1
    values: list[str] = []
    while index < len(tokens) and tokens[index] not in MODULE_FIELDS:
        values.append(tokens[index])
        index += 1
    return values


def _single_field(tokens: list[str], field_name: str) -> str:
    values = _field_values(tokens, field_name)
    return values[0] if values else ""


def _parse_module(directory: str, cmake_path: Path) -> tuple[Module | None, list[str]]:
    problems: list[str] = []
    if not cmake_path.is_file():
        return None, [f"registered module '{directory}' has no {cmake_path}"]
    try:
        bodies = _extract_call_bodies(
            cmake_path.read_text(encoding="utf-8"),
            "sharp_runtime_register_module",
        )
    except (OSError, ValueError) as error:
        return None, [f"cannot parse {cmake_path}: {error}"]
    if len(bodies) != 1:
        return None, [
            f"module '{directory}' must contain exactly one "
            f"sharp_runtime_register_module() call, found {len(bodies)}"
        ]
    tokens = _cmake_tokens(bodies[0])
    name = _single_field(tokens, "NAME")
    target = _single_field(tokens, "TARGET")
    module_type = _single_field(tokens, "TYPE")
    for field_name, value in (
        ("NAME", name),
        ("TARGET", target),
        ("TYPE", module_type),
    ):
        if not value:
            problems.append(f"module '{directory}' has no {field_name}")
    if module_type and module_type not in {"STATIC", "INTERFACE"}:
        problems.append(
            f"module '{directory}' has unsupported TYPE '{module_type}'"
        )
    legacy = frozenset(_field_values(tokens, "DEPENDENCIES"))
    public = frozenset(_field_values(tokens, "PUBLIC_DEPENDENCIES"))
    private = frozenset(_field_values(tokens, "PRIVATE_DEPENDENCIES"))
    if legacy and (public or private):
        problems.append(
            f"module '{directory}' mixes legacy DEPENDENCIES with explicit "
            "PUBLIC_DEPENDENCIES/PRIVATE_DEPENDENCIES"
        )
    if module_type == "INTERFACE" and private:
        problems.append(
            f"INTERFACE module '{directory}' cannot have PRIVATE_DEPENDENCIES"
        )
    if problems:
        return None, problems
    return (
        Module(
            directory=directory,
            name=name,
            target=target,
            module_type=module_type,
            public_dependencies=public,
            private_dependencies=private,
            legacy_dependencies=legacy,
            test_dependencies=frozenset(
                _field_values(tokens, "TEST_DEPENDENCIES")
            ),
        ),
        [],
    )


def _load_allowlist(root: Path) -> tuple[list[AllowedDependency], list[str]]:
    path = root / ALLOWLIST_PATH
    if not path.exists():
        return [], []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [], [f"cannot parse dependency allow-list {path}: {error}"]
    raw_entries = data.get("dependencies") if isinstance(data, dict) else None
    if not isinstance(raw_entries, list):
        return [], [
            f"dependency allow-list {path} must contain a 'dependencies' array"
        ]
    entries: list[AllowedDependency] = []
    problems: list[str] = []
    for index, raw in enumerate(raw_entries):
        if not isinstance(raw, dict):
            problems.append(f"allow-list entry {index} must be an object")
            continue
        module = raw.get("module", "")
        dependency = raw.get("dependency", "")
        visibility = raw.get("visibility", "")
        reason = raw.get("reason", "")
        if not all(isinstance(value, str) and value.strip() for value in (
            module,
            dependency,
            visibility,
            reason,
        )):
            problems.append(
                f"allow-list entry {index} requires non-empty string fields "
                "module, dependency, visibility, and reason"
            )
            continue
        visibility = visibility.upper()
        if visibility not in {"PUBLIC", "PRIVATE"}:
            problems.append(
                f"allow-list entry {index} has invalid visibility '{visibility}'"
            )
            continue
        entries.append(
            AllowedDependency(module, dependency, visibility, reason.strip())
        )
    duplicate_keys = {
        (entry.module, entry.dependency)
        for entry in entries
        if sum(
            other.module == entry.module and other.dependency == entry.dependency
            for other in entries
        ) > 1
    }
    for module, dependency in sorted(duplicate_keys):
        problems.append(
            f"dependency allow-list repeats edge {module} -> {dependency}"
        )
    return entries, problems


def _iter_source_files(path: Path) -> Iterable[Path]:
    if not path.is_dir():
        return ()
    return (
        candidate
        for candidate in path.rglob("*")
        if candidate.is_file() and candidate.suffix in SOURCE_SUFFIXES
    )


def _display(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def _find_cycle(graph: dict[str, set[str]]) -> list[str] | None:
    state: dict[str, int] = {}
    stack: list[str] = []

    def visit(node: str) -> list[str] | None:
        state[node] = 1
        stack.append(node)
        for dependency in sorted(graph[node]):
            if dependency not in graph:
                continue
            if state.get(dependency, 0) == 0:
                cycle = visit(dependency)
                if cycle:
                    return cycle
            elif state.get(dependency) == 1:
                start = stack.index(dependency)
                return stack[start:] + [dependency]
        stack.pop()
        state[node] = 2
        return None

    for node in sorted(graph):
        if state.get(node, 0) == 0:
            cycle = visit(node)
            if cycle:
                return cycle
    return None


def validate_repository(root: Path) -> ValidationReport:
    root = root.resolve()
    report = ValidationReport()
    module_root = root / "modules"
    listed_directories, problems = _parse_module_directories(root / MODULE_LIST_PATH)
    report.problems.extend(problems)

    physical_directories = (
        sorted(path.name for path in module_root.iterdir() if path.is_dir())
        if module_root.is_dir()
        else []
    )
    listed_set = set(listed_directories)
    physical_set = set(physical_directories)
    for directory in sorted(physical_set - listed_set):
        report.problems.append(
            f"physical module directory 'modules/{directory}' is not registered"
        )
    for directory in sorted(listed_set - physical_set):
        report.problems.append(
            f"registered module directory 'modules/{directory}' does not exist"
        )

    modules: dict[str, Module] = {}
    names: dict[str, str] = {}
    targets: dict[str, str] = {}
    for directory in listed_directories:
        module, module_problems = _parse_module(
            directory,
            module_root / directory / "CMakeLists.txt",
        )
        report.problems.extend(module_problems)
        if module is None:
            continue
        if module.name in names:
            report.problems.append(
                f"component name '{module.name}' is owned by both "
                f"'{names[module.name]}' and '{directory}'"
            )
        else:
            names[module.name] = directory
        if module.target in targets:
            report.problems.append(
                f"target '{module.target}' is owned by both "
                f"'{targets[module.target]}' and '{directory}'"
            )
        else:
            targets[module.target] = directory
        modules[directory] = module

    report.module_count = len(modules)
    allowlist, allowlist_problems = _load_allowlist(root)
    report.problems.extend(allowlist_problems)
    allowed_by_edge = {
        (entry.module, entry.dependency): entry for entry in allowlist
    }

    graph: dict[str, set[str]] = {module.name: set() for module in modules.values()}
    for module in modules.values():
        for dependency in sorted(module.all_dependencies):
            graph[module.name].add(dependency)
            if dependency not in names:
                report.problems.append(
                    f"module '{module.name}' declares unknown dependency '{dependency}'"
                )
        for dependency in sorted(module.test_dependencies):
            if dependency not in names:
                report.problems.append(
                    f"module '{module.name}' declares unknown test dependency "
                    f"'{dependency}'"
                )
    report.dependency_edge_count = sum(len(edges) for edges in graph.values())
    cycle = _find_cycle(graph)
    if cycle:
        report.problems.append(
            "cyclic module dependency: " + " -> ".join(cycle)
        )

    header_owners: dict[str, str] = {}
    header_paths: dict[str, Path] = {}
    for module in modules.values():
        include_root = module_root / module.directory / "include"
        if not include_root.is_dir():
            report.problems.append(
                f"module '{module.name}' has no include directory: {include_root}"
            )
            continue
        for header in sorted(_iter_source_files(include_root)):
            if header.suffix not in HEADER_SUFFIXES:
                continue
            logical_path = header.relative_to(include_root).as_posix()
            if logical_path in header_owners:
                report.problems.append(
                    f"duplicate public include path '{logical_path}' owned by "
                    f"'{header_owners[logical_path]}' and '{module.name}'"
                )
            else:
                header_owners[logical_path] = module.name
                header_paths[logical_path] = header

    public_edges: dict[str, set[str]] = {
        module.name: set() for module in modules.values()
    }
    private_edges: dict[str, set[str]] = {
        module.name: set() for module in modules.values()
    }
    edge_locations: dict[tuple[str, str, str], list[str]] = {}
    for module in modules.values():
        module_path = module_root / module.directory
        for section, edge_map in (
            ("include", public_edges),
            ("src", private_edges),
        ):
            section_path = module_path / section
            for source in sorted(_iter_source_files(section_path)):
                try:
                    content = source.read_text(encoding="utf-8", errors="replace")
                except OSError as error:
                    report.problems.append(
                        f"cannot read {_display(source, root)}: {error}"
                    )
                    continue
                for include in INCLUDE_RE.findall(content):
                    owner = header_owners.get(include)
                    if owner is not None:
                        if owner != module.name:
                            edge_map[module.name].add(owner)
                            edge_locations.setdefault(
                                (module.name, owner, section),
                                [],
                            ).append(_display(source, root))
                        continue
                    if include.startswith(PROJECT_INCLUDE_PREFIXES):
                        report.problems.append(
                            f"{_display(source, root)} includes unresolved "
                            f"project header '{include}'"
                        )

    for module in modules.values():
        declared_public = (
            module.public_dependencies
            if module.uses_explicit_visibility
            else module.legacy_dependencies
        )
        declared_private = module.private_dependencies
        actual_public = public_edges[module.name]
        actual_private = private_edges[module.name] - actual_public

        for dependency in sorted(actual_public):
            if dependency not in declared_public:
                locations = edge_locations.get(
                    (module.name, dependency, "include"), []
                )
                suffix = f" ({locations[0]})" if locations else ""
                report.problems.append(
                    f"module '{module.name}' publicly includes '{dependency}' "
                    f"without a public dependency{suffix}"
                )
        for dependency in sorted(actual_private):
            if dependency not in declared_public and dependency not in declared_private:
                locations = edge_locations.get(
                    (module.name, dependency, "src"), []
                )
                suffix = f" ({locations[0]})" if locations else ""
                report.problems.append(
                    f"module '{module.name}' privately includes '{dependency}' "
                    f"without a dependency{suffix}"
                )
            elif (
                module.uses_explicit_visibility
                and dependency in declared_public
                and (module.name, dependency) not in allowed_by_edge
            ):
                report.problems.append(
                    f"module '{module.name}' uses '{dependency}' only in "
                    "implementation sources but declares it public"
                )

        actual_dependencies = actual_public | actual_private
        for dependency in sorted(module.all_dependencies - actual_dependencies):
            allowed = allowed_by_edge.get((module.name, dependency))
            if allowed is None:
                report.problems.append(
                    f"module '{module.name}' declares stale dependency '{dependency}'"
                )

        for dependency in sorted(module.private_dependencies & actual_public):
            report.problems.append(
                f"module '{module.name}' declares '{dependency}' private but "
                "uses it from a public header"
            )

    for allowed in allowlist:
        module = next(
            (candidate for candidate in modules.values()
             if candidate.name == allowed.module),
            None,
        )
        if module is None:
            report.problems.append(
                f"allow-list references unknown module '{allowed.module}'"
            )
            continue
        if allowed.dependency not in module.all_dependencies:
            report.problems.append(
                f"allow-list edge {allowed.module} -> {allowed.dependency} "
                "is not declared"
            )
            continue
        if (
            allowed.visibility == "PUBLIC"
            and allowed.dependency not in (
                module.public_dependencies | module.legacy_dependencies
            )
        ):
            report.problems.append(
                f"allow-list edge {allowed.module} -> {allowed.dependency} "
                "claims PUBLIC visibility but declaration is private"
            )
        if (
            allowed.visibility == "PRIVATE"
            and allowed.dependency not in module.private_dependencies
        ):
            report.problems.append(
                f"allow-list edge {allowed.module} -> {allowed.dependency} "
                "claims PRIVATE visibility but declaration is not private"
            )

    public_graph = {
        module.name: set(
            module.public_dependencies
            if module.uses_explicit_visibility
            else module.legacy_dependencies
        )
        for module in modules.values()
    }

    def public_closure(component: str) -> set[str]:
        closure: set[str] = set()
        pending = list(public_graph.get(component, ()))
        while pending:
            dependency = pending.pop()
            if dependency in closure:
                continue
            closure.add(dependency)
            pending.extend(public_graph.get(dependency, ()))
        return closure

    for module in modules.values():
        allowed_test_components = {module.name} | public_closure(module.name)
        for dependency in module.test_dependencies:
            allowed_test_components.add(dependency)
            allowed_test_components.update(public_closure(dependency))
        used_test_dependencies: set[str] = set()
        test_root = module_root / module.directory / "tests"
        for source in sorted(_iter_source_files(test_root)):
            try:
                content = source.read_text(encoding="utf-8", errors="replace")
            except OSError as error:
                report.problems.append(
                    f"cannot read {_display(source, root)}: {error}"
                )
                continue
            for include in INCLUDE_RE.findall(content):
                owner = header_owners.get(include)
                if owner is None:
                    if include.startswith(PROJECT_INCLUDE_PREFIXES):
                        report.problems.append(
                            f"{_display(source, root)} includes unresolved "
                            f"project header '{include}'"
                        )
                    continue
                if owner not in allowed_test_components:
                    report.problems.append(
                        f"{_display(source, root)} includes test-only component "
                        f"'{owner}' without TEST_DEPENDENCIES"
                    )
                for dependency in module.test_dependencies:
                    if owner == dependency or owner in public_closure(dependency):
                        used_test_dependencies.add(dependency)
        for dependency in sorted(
            module.test_dependencies - used_test_dependencies
        ):
            report.problems.append(
                f"module '{module.name}' declares stale test dependency "
                f"'{dependency}'"
            )

    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=REPO_ROOT,
        help="Sharp Runtime repository root",
    )
    args = parser.parse_args(argv)
    report = validate_repository(args.root)
    if report.ok:
        print(
            "OK: module boundaries valid "
            f"({report.module_count} physical modules, "
            f"{report.dependency_edge_count} dependency edges)"
        )
        return 0
    print(
        f"FAIL: {len(report.problems)} module-boundary problem(s) found:",
        file=sys.stderr,
    )
    for problem in report.problems:
        print(f"  - {problem}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
