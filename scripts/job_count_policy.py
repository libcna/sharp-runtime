#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Resolve the repository-wide compilation-job limit.

Precedence is an explicit caller value, then SHARP_RUNTIME_BUILD_JOBS, then the
safe repository default. Values are decimal integers in 1..2. Invalid or
excessive values are rejected rather than silently clamped.
"""

from __future__ import annotations

import argparse
from collections.abc import Mapping
import os
import re
import sys


ENVIRONMENT_VARIABLE = "SHARP_RUNTIME_BUILD_JOBS"
DEFAULT_JOBS = 2
MAXIMUM_JOBS = 2
_POSITIVE_DECIMAL_RE = re.compile(r"^[1-9][0-9]*$")


class JobPolicyError(ValueError):
    """A requested compilation-job value violates repository policy."""


def resolve_jobs(
    explicit: int | str | None = None,
    environment: Mapping[str, str] | None = None,
) -> int:
    """Return the bounded job count using argument > environment > default."""
    source = "--jobs"
    value: int | str | None = explicit
    if value is None:
        source = ENVIRONMENT_VARIABLE
        values = os.environ if environment is None else environment
        value = values.get(ENVIRONMENT_VARIABLE)
    if value is None:
        return DEFAULT_JOBS

    if isinstance(value, bool):
        raise JobPolicyError(f"{source} must be a decimal integer in 1..{MAXIMUM_JOBS}")
    if isinstance(value, int):
        jobs = value
    elif isinstance(value, str) and _POSITIVE_DECIMAL_RE.fullmatch(value):
        jobs = int(value)
    else:
        raise JobPolicyError(
            f"{source} must be a decimal integer in 1..{MAXIMUM_JOBS} (got {value!r})"
        )

    if jobs < 1 or jobs > MAXIMUM_JOBS:
        raise JobPolicyError(
            f"{source} must be in 1..{MAXIMUM_JOBS} (got {jobs}); "
            "the repository aggregate compilation ceiling is two jobs"
        )
    return jobs


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Resolve the repository build-job policy.")
    parser.add_argument(
        "--jobs",
        default=None,
        help=f"explicit job count; overrides ${ENVIRONMENT_VARIABLE}",
    )
    args = parser.parse_args(argv)
    try:
        jobs = resolve_jobs(args.jobs)
    except JobPolicyError as problem:
        print(f"FAIL: {problem}", file=sys.stderr)
        return 2
    print(jobs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
