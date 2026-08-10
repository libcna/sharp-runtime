# Audit: `modules/core/tests/System/DoubleTests.cpp`

## Metadata

- Audit status: AUDITED (409 lines, 84 tests, full read).
- Validation: the containing `DoubleTests.*:DoubleTests2.*` filter passed
  164/164 in `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

This suite has broad basic classification, comparison, special-token parsing,
and simple format coverage. Its preserved regression tests correctly reject
C++-specific abbreviated `inf` and `nan(payload)` spellings. The remaining
coverage treats the C++ parser/formatter subset as the contract, however, and
tests only normal floating values for generic-math helpers.

## Finding references

- **SR-AUD-021:** unknown type and malformed/oversized precision do not have
  an exact `System::FormatException` assertion.
- **SR-AUD-029:** no negative or greater-than-15 rounding precision requires
  `ArgumentOutOfRangeException`.
- **SR-AUD-030:** `IsPow2` lacks `Double::Epsilon`, another one-bit subnormal,
  and a multi-bit subnormal negative control.
- **SR-AUD-031:** `ILogB` lacks zero, NaN, infinities, and subnormal vectors.
- **SR-AUD-033:** valid default whitespace/grouping/overflow parse inputs and
  exact `N`/`E` text are absent. `ToString_FormatE` only checks non-emptiness,
  so `E+00` passes where .NET requires its standard exponent layout.

## Required post-audit verification

Add the exact exception, special-value, subnormal, parsing, and text-layout
vectors in the owning header report. Preserve the existing canonical-token
regressions: accepted default whitespace must not re-admit abbreviated C++
special spellings.

## Final assessment

Good coverage of prior special-token repair and ordinary values, but missing
edge assertions leave the shared floating conversion defects invisible.
