# Audit: `modules/core/tests/System/MathFTests.cpp`

## Metadata

- Audit status: AUDITED (176 lines, 26 tests, full read).
- Validation: `MathTests.*:MathFTest.*` passed 174/174 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

The compact float suite correctly preserves recent base-log and NaN/signed-zero
Min/Max repairs.  It covers only valid Clamp and normal default rounding,
however; no test probes invalid public input or the C++ floating-point
environment.  The sibling double suite contains precisely the latter
regression guard.

## Finding references

- **SR-AUD-022:** Clamp lacks an inverted-bound `ArgumentException` assertion.
- **SR-AUD-036:** Round lacks an invalid `MidpointRounding` exception
  assertion.
- **SR-AUD-040:** no `fesetround` test verifies that default and explicit
  `ToEven` rounding remain ties-to-even under ambient upward/downward modes.

## Required post-audit verification

Add the Math-compatible invalid Clamp and enum vectors.  Use a restoring RAII
rounding-mode guard, as `MathTests.cpp` does, and assert both `Round(2.5f)` and
`Round(2.5f, ToEven)` return `2.0f` with upward and downward modes selected.

## Final assessment

The 26 ordinary tests are useful smoke coverage but do not exercise the three
confirmed MathF boundary defects.  No implementation was modified during this
audit.
