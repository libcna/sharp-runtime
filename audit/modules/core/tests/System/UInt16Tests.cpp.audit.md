# Audit: `modules/core/tests/System/UInt16Tests.cpp`

## Metadata

- Audit status: AUDITED (128 lines, 26 tests, full read).
- Validation: `UInt16Test.*` passed 26/26 on 2026-07-25.

## Assessment

The suite guards unsigned parse range handling, including the important `-0`
case, and tests normal formatter, bit, and rotation paths. It tests malformed
precision but does not distinguish an unknown format type from valid decimal
output; Clamp also has only ordered-bound vectors.

## Finding references

- **SR-AUD-021:** no unknown format exception is asserted.
- **SR-AUD-022:** no `min > max` assertion exists.
- **SR-AUD-023:** no `B`/`b` format vector exists.

## Required post-audit verification

Add exact `FormatException` and `ArgumentException` cases, then binary output
coverage for 5, zero, padded width, and `MaxValue`.

## Final assessment

Effective parse regressions but insufficient format and invalid-range
diagnostics for the observed public gaps.
