#!/usr/bin/env python3
"""Inventories every modules/*/include/**/*.hpp and modules/*/src/**/*.cpp
file: SPDX header presence, line count, and a conservative namespace/type
ownership inference, cross-referenced against plan.sqlite3's task table.  Exact,
reasoned representation exemptions are stale-checked; every remaining mismatch
or scan/read failure is fatal. Prints a summary and writes full detail to CSV.

Usage: scripts/source_header_inventory.py [--csv PATH] [--db PATH]
"""

import argparse
import csv
import json
import os
from pathlib import Path
import re
import sqlite3
import stat
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DB = os.path.join(REPO_ROOT, "plan.sqlite3")
DEFAULT_CSV = os.path.join(REPO_ROOT, "build-tmp", "source_header_inventory.csv")
DEFAULT_EXEMPTIONS = os.path.join(REPO_ROOT, "scripts", "source_header_inventory_exemptions.json")

SPDX_RE = re.compile(r"SPDX-License-Identifier:\s*MIT")
NAMESPACE_RE = re.compile(r"namespace\s+((?:System|SharpRuntime)(?:::\w+)*)\s*\{")
TYPE_RE = re.compile(
    r"^\s*(?:template\s*<[^>]*>\s*)?"
    r"(?:class|struct|enum(?:\s+class)?)\s+(\w+)",
    re.MULTILINE,
)
TYPE_DECLARATION_RE = re.compile(
    r"^\s*(?:template\s*<[^>]*>\s*)?"
    r"(?:class|struct|enum(?:\s+class)?)\s+(\w+)\b([^;{]*)([;{])",
    re.MULTILINE,
)
ALIAS_RE = re.compile(
    r"^\s*(?:template\s*<[^>]*>\s*)?using\s+(\w+)\s*=",
    re.MULTILINE,
)


def raise_walk_error(error):
    """Make os.walk fail closed instead of silently omitting an unreadable subtree."""
    raise error


def header_namespace(path):
    """Derive the owning namespace from include/System/... rather than the first forward declaration."""
    parts = Path(path).parts
    if "include" not in parts:
        return ""
    relative = parts[parts.index("include") + 1 :]
    if len(relative) < 2 or relative[0] != "System":
        return ""
    return "::".join(relative[:-1])


def collect_files(modules_path=None):
    modules_path = modules_path or os.path.join(REPO_ROOT, "modules")
    with os.scandir(modules_path) as modules:
        for module in modules:
            if not module.is_dir():
                continue
            for base in ("include", "src"):
                base_path = os.path.join(module.path, base)
                try:
                    mode = os.stat(base_path).st_mode
                except FileNotFoundError:
                    # Header-only and source-only modules are both valid repository
                    # shapes.  Missing one of the two conventional roots is not an
                    # incomplete scan; errors from an existing root still fail closed.
                    continue
                if not stat.S_ISDIR(mode):
                    raise NotADirectoryError(base_path)
                for root, _, files in os.walk(
                        base_path, onerror=raise_walk_error):
                    for f in files:
                        if f.endswith((".hpp", ".cpp")):
                            yield os.path.join(root, f)


def inspect_file(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        content = fh.read()

    lines = content.count("\n") + 1
    has_spdx = bool(SPDX_RE.search(content[:400]))
    ns_match = NAMESPACE_RE.search(content)
    namespace = header_namespace(path) or (ns_match.group(1) if ns_match else "")
    # Preserve order while de-duplicating forward declaration + definition pairs.  Keep
    # ownership evidence separate: a forward declaration in an unrelated header proves only
    # reachability, not that the corresponding ported type has an implementation.
    type_names = list(dict.fromkeys(TYPE_RE.findall(content) + ALIAS_RE.findall(content)))
    owned_type_names = list(dict.fromkeys(
        [name for name, _, terminator in TYPE_DECLARATION_RE.findall(content)
         if terminator == "{"] + ALIAS_RE.findall(content)
    ))

    return {
        "path": os.path.relpath(path, REPO_ROOT),
        "lines": lines,
        "has_spdx": has_spdx,
        "namespace": namespace,
        "types": ";".join(type_names),
        "owned_types": ";".join(owned_type_names),
    }


def load_tasks(db_path):
    if not os.path.exists(db_path):
        raise FileNotFoundError(f"planning database does not exist: {db_path}")
    conn = sqlite3.connect(db_path)
    try:
        rows = conn.execute(
            "SELECT namespace, name, status FROM task"
        ).fetchall()
    finally:
        conn.close()
    return {(namespace, name): status for namespace, name, status in rows}


def load_exemptions(path):
    with open(path, encoding="utf-8") as source:
        payload = json.load(source)
    if payload.get("schema_version") != 1:
        raise ValueError(f"{path}: unsupported schema_version")
    result = {}
    for category in ("source_without_task", "ported_without_header"):
        entries = payload.get(category)
        if not isinstance(entries, dict):
            raise ValueError(f"{path}: {category} must be an object")
        if any(not key or not isinstance(reason, str) or not reason.strip()
               for key, reason in entries.items()):
            raise ValueError(f"{path}: {category} contains a blank key or reason")
        result[category] = entries
    return result


def qualified_key(key):
    namespace, name = key
    return f"{namespace}.{name}" if namespace else name


def cross_reference(rows, tasks):
    """Annotate rows and return conservative source/plan ownership mismatches.

    A task is satisfied by a definition or alias in a header under its path-derived namespace.
    A mere forward declaration is deliberately not ownership evidence.
    The reverse assertion is deliberately narrower: only a header whose basename names one of its
    declarations claims a task row. Nested/private helpers and grouped/detail headers therefore do
    not become hundreds of false positives. Permanent representation differences are handled by
    an exact, stale-checked exemption file in ``apply_exemptions``.
    """
    declared_task_keys = set()
    source_without_task = set()
    for row in rows:
        matches = []
        namespace = row["namespace"].replace("::", ".")
        declared_names = list(filter(None, row["types"].split(";")))
        owned_names = list(filter(None, row.get("owned_types", row["types"]).split(";")))
        for type_name in declared_names:
            key = (namespace, type_name)
            status = tasks.get(key)
            if status is not None:
                matches.append(f"{namespace}.{type_name}:{status}")
                if row["path"].endswith(".hpp") and type_name in owned_names:
                    declared_task_keys.add(key)

        primary = Path(row["path"]).stem
        primary_key = (namespace, primary)
        if row["path"].endswith(".hpp") and primary_key in tasks:
            # A same-named public header is itself conservative ownership evidence, including
            # forwarding headers and C++ namespace-based representations of static classes.
            declared_task_keys.add(primary_key)
            primary_match = f"{qualified_key(primary_key)}:{tasks[primary_key]}"
            if primary_match not in matches:
                matches.append(primary_match)
        unmatched = []
        if (row["path"].endswith(".hpp") and namespace.startswith("System") and
                "detail" not in namespace.split(".") and primary in declared_names and
                primary_key not in tasks):
            unmatched.append(qualified_key(primary_key))
            source_without_task.add(primary_key)
        row["task_matches"] = ";".join(matches)
        row["unmatched_types"] = ";".join(unmatched)

    ported_keys = {key for key, status in tasks.items() if status == "ported"}
    return sorted(source_without_task), sorted(ported_keys - declared_task_keys)


def apply_exemptions(source_without_task, ported_without_header, exemptions):
    source = set(source_without_task)
    plan = set(ported_without_header)
    source_exemptions = set(exemptions["source_without_task"])
    plan_exemptions = set(exemptions["ported_without_header"])
    source_names = {qualified_key(key) for key in source}
    plan_names = {qualified_key(key) for key in plan}
    problems = []
    for stale in sorted(source_exemptions - source_names):
        problems.append(f"stale source_without_task exemption: {stale}")
    for stale in sorted(plan_exemptions - plan_names):
        problems.append(f"stale ported_without_header exemption: {stale}")
    source = sorted(key for key in source if qualified_key(key) not in source_exemptions)
    plan = sorted(key for key in plan if qualified_key(key) not in plan_exemptions)
    return source, plan, problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", default=DEFAULT_CSV, help="Output CSV path")
    parser.add_argument("--db", default=DEFAULT_DB, help="plan.sqlite3 path")
    parser.add_argument("--exemptions", default=DEFAULT_EXEMPTIONS,
                        help="Exact, reasoned representation exemptions")
    args = parser.parse_args()

    try:
        rows = []
        missing_spdx = []
        for path in sorted(collect_files()):
            info = inspect_file(path)
            rows.append(info)
            if not info["has_spdx"]:
                missing_spdx.append(info["path"])

        tasks = load_tasks(args.db)
        exemptions = load_exemptions(args.exemptions)
    except (OSError, sqlite3.Error, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    unmatched_declarations, ported_without_header = cross_reference(rows, tasks)
    unmatched_declarations, ported_without_header, exemption_problems = apply_exemptions(
        unmatched_declarations, ported_without_header, exemptions
    )

    try:
        Path(args.csv).parent.mkdir(parents=True, exist_ok=True)
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            writer = csv.DictWriter(
                fh,
                fieldnames=[
                    "path",
                    "lines",
                    "has_spdx",
                    "namespace",
                    "types",
                    "owned_types",
                    "task_matches",
                    "unmatched_types",
                ],
            )
            writer.writeheader()
            writer.writerows(rows)
    except OSError as error:
        print(f"FAIL: cannot write inventory CSV: {error}", file=sys.stderr)
        return 1

    total_lines = sum(r["lines"] for r in rows)
    hpp_count = sum(1 for r in rows if r["path"].endswith(".hpp"))
    cpp_count = sum(1 for r in rows if r["path"].endswith(".cpp"))

    print(f"Files scanned:     {len(rows)} ({hpp_count} .hpp, {cpp_count} .cpp)")
    print(f"Total lines:       {total_lines}")
    print(f"Missing SPDX:      {len(missing_spdx)}")
    for p in missing_spdx:
        print(f"  - {p}")
    ported_count = sum(status == "ported" for status in tasks.values())
    print(f"Tasks marked ported (plan.sqlite3): {ported_count}")
    print(f"Header declarations without a matching task row: {len(unmatched_declarations)}")
    for key in unmatched_declarations:
        print(f"  source-only: {qualified_key(key)}")
    print(f"Ported task rows without a detected header declaration: {len(ported_without_header)}")
    for namespace, name in ported_without_header:
        qualified = f"{namespace}.{name}" if namespace else name
        print(f"  plan-only:   {qualified}")
    print(f"Stale/invalid cross-reference exemptions: {len(exemption_problems)}")
    for problem in exemption_problems:
        print(f"  exemption:  {problem}")
    print("Cross-reference mismatches are a strict failure after exact exemptions.")
    print(f"CSV written to:    {os.path.abspath(args.csv)}")

    return 1 if (missing_spdx or unmatched_declarations or ported_without_header or
                 exemption_problems) else 0


if __name__ == "__main__":
    sys.exit(main())
