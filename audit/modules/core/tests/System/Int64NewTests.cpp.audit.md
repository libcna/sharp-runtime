# Audit: `modules/core/tests/System/Int64NewTests.cpp`

## Metadata

- Audit status: AUDITED (134 lines, 56 tests, full read).
- Validation: Core.Base `Int64NewTests.*` passed 56/56 on 2026-07-25.

## Assessment

This is a strong focused regression suite.  It asserts the dangerous signed
minimum paths for `Abs`, `DivRem`, `CopySign`, and magnitude helpers; it also
covers the 128-bit product, bit counts, rotation, logarithm, and normal clamp
paths.  The depth explains why these historical C++ overflow hazards are now
explicitly handled by `Int64.hpp`.

## Finding references

- **SR-AUD-021:** no assertion requires unknown `ToString("Q")` to raise
  `System::FormatException`; only malformed width is checked.
- **SR-AUD-022:** all Clamp tests supply ordered bounds, so the `min > max`
  path that passes an invalid range to `std::clamp` is untested.

## Required post-audit verification

Add an `EXPECT_THROW(Int64::Clamp(5, 10, 0), System::ArgumentException)`
regression after repairing the source.  Add `B` coverage for `MinValue` and an
unknown-format exception check; maintain the present MinValue UBSan-sensitive
coverage rather than replacing it with small-value tests.

## Final assessment

High-quality boundary coverage for the signed implementation, with two missing
public-input assertions corresponding to the confirmed shared findings.
