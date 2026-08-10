# Audit: `modules/core/tests/System/MathTests.cpp`

## Metadata

- Audit status: AUDITED (681 lines, 148 tests, full read).
- Validation: together with `MathFTests.cpp`, `MathTests.*:MathFTest.*` passed
  174/174 in `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

This is a strong ordinary and edge-case suite: it covers NaN/signed-zero
Min/Max, Clamp ordering, integer division traps, large rounding precision, and
the double ambient-rounding regression.  Its double-base Log test nevertheless
covers only a normal base, and it contains no `ILogB` or invalid-mode vectors.
That leaves three confirmed public defects green.

## Finding references

- **SR-AUD-031:** no zero, subnormal, infinity, or NaN `Math::ILogB` assertion
  exists, so the native NaN sentinel leak is unobserved.
- **SR-AUD-036:** named modes and digit range are tested, but no invalid
  `MidpointRounding` value is required to throw `ArgumentException`.
- **SR-AUD-039:** `Log_WithBase` validates only `(8, 2)` and omits the base
  one/zero/positive-infinity paths that .NET specifies and the MathF suite
  already tests.

## Required post-audit verification

Add exact `ILogB` sentinels for zero and NaN/non-finite values, invalid enum
assertions for both Round families, and the base special-value matrix already
used for MathF.  Keep the ambient-mode guard around its existing test so the
process mode is restored after all new assertions.

## Final assessment

The 148 tests catch several high-value regressions, but asymmetry with the
much smaller MathF suite leaves the remaining Math-specific public gaps
undetected.
