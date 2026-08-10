# Audit: `modules/core/tests/System/HalfNewTests.cpp`

## Metadata

- Audit status: AUDITED (335 lines, 87 tests, full read).
- Validation: `HalfNewTests.*` passed 87/87 on 2026-07-25.

## Assessment

This is a high-quality focused suite: known bits, subnormal magnitude,
NaN/zero equality distinctions, compare ordering, hash normalization,
arithmetic, parsing, special formatting, and span output all have observable
assertions. The accompanying exhaustive probe complements its representative
conversion vectors without duplicating a 65,536-case unit-test loop.

## Required post-audit verification

If `FromDouble` is changed to direct binary64 conversion, add exact tie cases
near half boundaries and retain the present signed-zero, subnormal, and NaN
tests.

## Final assessment

Thorough targeted coverage with no confirmed finding owned by this file.
