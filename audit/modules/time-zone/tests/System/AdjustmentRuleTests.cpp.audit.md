# Audit: `modules/time-zone/tests/System/AdjustmentRuleTests.cpp`

## Metadata

- AUDITED: 205-line AdjustmentRule fixture, 19 tests, fully read.
- Validation: `AdjustmentRuleTests.*` passed 19/19 on 2026-07-27.

## Coverage observed

The fixture validates field retention, normal equality, and selected hash
properties for valid ascending adjustment periods.  It does not execute the
managed constructor validation boundary.

## Missing assertions and diagnostics

- Add reversed/equal start/end dates, non-date date components, daylight and
  base-offset range/minute constraints, invalid transitions, and no-daylight
  transition semantics.
- Do not require distinct hash codes for distinct values; the current
  `EXPECT_NE` label says “LikelyDiffHash” but remains a non-contractual
  collision assumption.

## Final assessment

The omitted reversed-date path permits SR-AUD-226.  No source or test was
changed.
