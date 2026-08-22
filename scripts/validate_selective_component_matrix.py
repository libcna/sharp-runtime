#!/usr/bin/env python3
"""Keep the local and GitHub selective-component matrices identical."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LOCAL = REPO_ROOT / "scripts" / "check_selective_components.sh"
DEFAULT_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "components.yml"


def parse_local_matrix(path: Path) -> tuple[list[tuple[str, str]], list[str]]:
    entries: list[tuple[str, str]] = []
    problems: list[str] = []
    in_matrix = False
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line.strip() == "MATRIX=(":
            in_matrix = True
            continue
        if in_matrix and line.strip() == ")":
            break
        if not in_matrix:
            continue
        match = re.fullmatch(r'\s*"([^":]+):([^"/]+\.cpp)"\s*', line)
        if match is None:
            problems.append(f"{path}:{line_number}: malformed local matrix entry")
            continue
        entries.append((match.group(1), match.group(2)))
    if not in_matrix:
        problems.append(f"{path}: MATRIX=( block not found")
    return entries, problems


def parse_workflow_matrix(path: Path) -> tuple[list[tuple[str, str]], list[str]]:
    entries: list[tuple[str, str]] = []
    problems: list[str] = []
    pending_component: tuple[str, int] | None = None
    in_selective = False
    in_include = False
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        if line.startswith("  selective:"):
            in_selective = True
            continue
        if in_selective and line.startswith("  ") and not line.startswith("    ") and stripped:
            break
        if not in_selective:
            continue
        if stripped == "include:":
            in_include = True
            continue
        if in_include and stripped == "steps:":
            break
        if not in_include:
            continue
        component = re.fullmatch(r"- component:\s*(\S.*)", stripped)
        if component:
            if pending_component is not None:
                problems.append(
                    f"{path}:{pending_component[1]}: component lacks a following fixture"
                )
            pending_component = (component.group(1).strip(), line_number)
            continue
        fixture = re.fullmatch(r"fixture:\s*(\S.*)", stripped)
        if fixture:
            if pending_component is None:
                problems.append(f"{path}:{line_number}: fixture has no preceding component")
            else:
                entries.append((pending_component[0], fixture.group(1).strip()))
                pending_component = None
    if pending_component is not None:
        problems.append(f"{path}:{pending_component[1]}: component lacks a following fixture")
    if not in_include:
        problems.append(f"{path}: selective matrix include block not found")
    return entries, problems


def duplicate_entries(entries: list[tuple[str, str]]) -> list[tuple[str, str]]:
    seen: set[tuple[str, str]] = set()
    duplicates: list[tuple[str, str]] = []
    for entry in entries:
        if entry in seen and entry not in duplicates:
            duplicates.append(entry)
        seen.add(entry)
    return duplicates


def validate(local_path: Path, workflow_path: Path, fixture_root: Path) -> list[str]:
    local, problems = parse_local_matrix(local_path)
    workflow, workflow_problems = parse_workflow_matrix(workflow_path)
    problems.extend(workflow_problems)

    for label, entries in (("local", local), ("workflow", workflow)):
        duplicates = duplicate_entries(entries)
        if duplicates:
            problems.append(f"{label} matrix contains duplicate entries: {duplicates}")

    local_set = set(local)
    workflow_set = set(workflow)
    missing_from_workflow = sorted(local_set - workflow_set)
    missing_from_local = sorted(workflow_set - local_set)
    if missing_from_workflow:
        problems.append(f"workflow matrix is missing local entries: {missing_from_workflow}")
    if missing_from_local:
        problems.append(f"local matrix is missing workflow entries: {missing_from_local}")
    if local_set == workflow_set and local != workflow:
        problems.append("local and workflow matrices contain the same entries in different order")

    for component, fixture in local:
        fixture_path = fixture_root / fixture
        if not fixture_path.is_file():
            problems.append(f"{component}: fixture does not exist: {fixture_path}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--local", type=Path, default=DEFAULT_LOCAL)
    parser.add_argument("--workflow", type=Path, default=DEFAULT_WORKFLOW)
    parser.add_argument("--fixture-root", type=Path, default=REPO_ROOT / "test" / "consumer")
    args = parser.parse_args()
    problems = validate(args.local.resolve(), args.workflow.resolve(), args.fixture_root.resolve())
    if problems:
        print(f"FAIL: {len(problems)} selective-matrix consistency problem(s):", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1
    local, _ = parse_local_matrix(args.local.resolve())
    print(f"OK: local and GitHub selective matrices agree ({len(local)} components)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
