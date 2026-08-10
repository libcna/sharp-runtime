# Audit: `modules/core/tests/System/DateTimeOffsetTests.cpp`

## Metadata

- Audit status: AUDITED (281 lines, 34 tests, full read).
- Related integration coverage: `tests/integration/Task40Tests.cpp` contains
  a historical DateTimeOffset slice and will receive its own full-file audit.
- Validation: the dedicated suite passed as part of the 127-test focused run.

## Assessment

The suite covers static values, offset range checks, UTC-range checks,
arithmetic overflow regressions, conversions, Unix time, equality, and normal
parsing.  The integration slice additionally checks `+15:00` returns false.
Neither suite exercises invalid calendar time fields, invalid offset minutes,
trailing parse garbage, or `TryParse` output preservation after failure.

## Finding references

- **SR-AUD-006:** both DateTimeOffset component constructors need invalid
  hour/minute/second/millisecond assertions.
- **SR-AUD-007:** missing negative assertions let `+02:75` and malformed
  time text return successful parse results.

## Required post-audit assertions

Add `EXPECT_THROW` cases for invalid time components and `EXPECT_FALSE` / 
`EXPECT_THROW` pairs for invalid offset minutes and malformed input.  Preserve
a pre-set output value around each failed `TryParse` check.

## Final assessment

The focused suite is healthy but lacks the negative input assertions needed to
guard the two confirmed public contract defects.

## Correction — #1880 (2026-08-01)

"Preserve a pre-set output" above is the historical wrong premise. Current
.NET publishes MinValue on false. The add-only #1880 matrix now proves the
sentinel is replaced for offset grammar/range, inherited DateTime, UTC-range,
empty and suffix failures, with min/max/ordinary success and exact Parse
FormatException controls.
