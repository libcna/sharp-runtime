# Audit: `modules/core/tests/System/Int32NewTests.cpp`

## Metadata

- AUDITED: 41-line direct Int32 supplemental fixture, fully read.
- Validation: `Int32NewTests.*` passed 9/9 on 2026-07-27.
- Reference basis: audited `Int32.hpp` and local current-.NET `Int32.cs` for
  comparison, hashing, and signed magnitude tie behavior.

## Assessment

The fixture has focused value on the `MinValue` magnitude special cases, which
avoid the non-representable positive magnitude.  It also checks normal
three-way comparison, equality, and the current .NET value-as-hash result.
The reviewed behavior agrees with the managed source for these cases.  No new
implementation defect is demonstrated.

## Other missing assertions and diagnostics

- `GetHashCode_SameValue_SameHash` compares the same expression with itself,
  so it cannot detect any deterministic wrong hash result; the separate `-7`
  expectation supplies the only concrete oracle.
- Compare/equality coverage omits `MinValue`, `MaxValue`, zero/negative
  boundaries, and the native adaptation's missing boxed-object/null mismatch
  diagnostics.
- Magnitude coverage omits equal nonzero opposite-sign ties (where
  MaxMagnitude selects positive and MinMagnitude negative), ordinary unequal
  magnitudes, and zero.  It also does not test the shared invalid-interval
  Clamp finding (SR-AUD-022), unknown-format diagnostic (SR-AUD-021), parsing,
  formatting, divisions, shifts, or bit operations described as covered by
  another fixture.

## Final assessment

The supplemental fixture validates a meaningful MinValue edge case but is not
a complete Int32 behavioral contract.  No new finding and no source or test
change.
