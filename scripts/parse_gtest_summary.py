#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Read the final GoogleTest summary without mistaking skipped tests for passes."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re


RUN_RE = re.compile(r"^\[==========\]\s+(\d+)\s+tests?\s+from\s+.+\s+ran\.")
RESULT_RE = re.compile(r"^\[\s*(PASSED|FAILED|SKIPPED)\s*\]\s+(\d+)\s+tests?\b")


@dataclass(frozen=True)
class Summary:
    run: int
    passed: int
    failed: int
    skipped: int


def parse_summary(text: str) -> Summary:
    run_counts: list[int] = []
    result_counts: dict[str, list[int]] = {"PASSED": [], "FAILED": [], "SKIPPED": []}

    for line in text.splitlines():
        if match := RUN_RE.match(line):
            run_counts.append(int(match.group(1)))
        if match := RESULT_RE.match(line):
            result_counts[match.group(1)].append(int(match.group(2)))

    if not run_counts:
        raise ValueError("GoogleTest run summary is missing")
    if not result_counts["PASSED"]:
        raise ValueError("GoogleTest passed summary is missing")

    run = run_counts[-1]
    passed = result_counts["PASSED"][-1]
    failed = result_counts["FAILED"][-1] if result_counts["FAILED"] else 0
    skipped = result_counts["SKIPPED"][-1] if result_counts["SKIPPED"] else 0
    if run != passed + failed + skipped:
        raise ValueError(
            "inconsistent GoogleTest summary: "
            f"{run} run != {passed} passed + {failed} failed + {skipped} skipped"
        )
    return Summary(run, passed, failed, skipped)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="GoogleTest log to parse")
    args = parser.parse_args()

    try:
        summary = parse_summary(args.log.read_text(encoding="utf-8", errors="replace"))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"{summary.run}\t{summary.passed}\t{summary.failed}\t{summary.skipped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
