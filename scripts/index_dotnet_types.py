#!/usr/bin/env python3
"""Index public .NET types into an explicitly selected SQLite database.

No checkout or output path is assumed. Existing output is preserved unless the
caller passes ``--replace``; replacement is built in the destination directory
and installed atomically, so a failed scan never destroys the previous index.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import sqlite3
import sys
import tempfile


NS_RE = re.compile(r"^\s*namespace\s+([\w.]+)", re.MULTILINE)
TYPE_RE = re.compile(
    r"^\s*(?:public\s+)"
    r"(?:(?:static|sealed|abstract|partial|readonly|ref|unsafe|new|override|virtual|extern)\s+)*"
    r"(class|struct|enum|interface|delegate|record)\s+(\w+)",
    re.MULTILINE,
)


def collect_files(source_dirs: list[Path]):
    def fail_walk(error: OSError) -> None:
        raise error

    for source_dir in source_dirs:
        for root, directories, files in os.walk(source_dir, onerror=fail_walk):
            directories.sort()
            for filename in sorted(files):
                if filename.endswith(".cs"):
                    yield source_dir, Path(root) / filename


def parse_file(path: Path, source_dir: Path) -> list[tuple[str, str, str, str]]:
    # A partial scan must never replace a previously valid index. Propagate read failures so the
    # sibling temporary database is discarded and the selected output remains untouched.
    content = path.read_text(encoding="utf-8", errors="replace")

    namespaces = NS_RE.findall(content)
    types = TYPE_RE.findall(content)
    if not types:
        return []

    relative_path = Path(source_dir.name) / path.relative_to(source_dir)
    primary_namespace = namespaces[0] if namespaces else ""
    return [
        (relative_path.as_posix(), primary_namespace, name, kind)
        for kind, name in types
    ]


def normalize_inputs(source_dirs: list[Path], output: Path, replace: bool) -> tuple[list[Path], Path]:
    normalized_sources: list[Path] = []
    for source_dir in source_dirs:
        candidate = source_dir.expanduser().resolve()
        if not candidate.is_dir():
            raise ValueError(f"source directory does not exist or is not a directory: {candidate}")
        if candidate not in normalized_sources:
            normalized_sources.append(candidate)

    # Do not call resolve() here: resolving an existing symlink could silently retarget the
    # explicitly selected output to another checkout. abspath normalizes spelling without
    # following the final path.
    output = Path(os.path.abspath(output.expanduser()))
    if not output.parent.is_dir():
        raise ValueError(f"output parent directory does not exist: {output.parent}")
    output_exists = os.path.lexists(output)
    if output_exists and output.is_symlink():
        raise ValueError(f"output may not be a symbolic link: {output}")
    if output_exists and not output.is_file():
        raise ValueError(f"output exists and is not a regular file: {output}")
    if output_exists and not replace:
        raise FileExistsError(f"output already exists; pass --replace to replace it: {output}")
    return normalized_sources, output


def build_index(source_dirs: list[Path], output: Path, *, replace: bool = False) -> tuple[int, int]:
    source_dirs, output = normalize_inputs(source_dirs, output, replace)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{output.name}.", suffix=".tmp", dir=output.parent
    )
    os.close(descriptor)
    temporary = Path(temporary_name)

    total_files = 0
    inserted = 0
    connection: sqlite3.Connection | None = None
    try:
        connection = sqlite3.connect(temporary)
        cursor = connection.cursor()
        cursor.execute(
            """
            CREATE TABLE types (
                id        INTEGER PRIMARY KEY,
                file      TEXT NOT NULL,
                namespace TEXT NOT NULL,
                type_name TEXT NOT NULL,
                type_kind TEXT NOT NULL
            )
            """
        )
        cursor.execute("CREATE INDEX idx_ns ON types(namespace)")
        cursor.execute("CREATE INDEX idx_name ON types(type_name)")

        for source_dir, path in collect_files(source_dirs):
            total_files += 1
            rows = parse_file(path, source_dir)
            if rows:
                cursor.executemany(
                    "INSERT INTO types(file, namespace, type_name, type_kind) VALUES (?,?,?,?)",
                    rows,
                )
                inserted += len(rows)
        connection.commit()
        connection.close()
        connection = None
        if replace:
            os.replace(temporary, output)
        else:
            # link() is an atomic no-clobber install on the same filesystem. Unlike a final
            # exists()+replace() pair it cannot overwrite a file or dangling symlink created by
            # another process after normalize_inputs() returned.
            try:
                os.link(temporary, output)
            except FileExistsError as error:
                raise FileExistsError(
                    f"output appeared during generation and was preserved: {output}"
                ) from error
            temporary.unlink()
    except Exception:
        if connection is not None:
            connection.close()
        temporary.unlink(missing_ok=True)
        raise
    return total_files, inserted


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        dest="sources",
        type=Path,
        action="append",
        required=True,
        help="Root containing .cs files; repeat for multiple runtime source roots",
    )
    parser.add_argument("--output", type=Path, required=True, help="SQLite file to create")
    parser.add_argument(
        "--replace",
        action="store_true",
        help="Atomically replace an existing regular output file after a successful scan",
    )
    args = parser.parse_args(argv)

    try:
        total, inserted = build_index(args.sources, args.output, replace=args.replace)
    except (OSError, sqlite3.Error, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"Done. {total} files scanned, {inserted} type entries inserted.")
    print(f"DB: {Path(os.path.abspath(args.output.expanduser()))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
