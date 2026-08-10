# Audit: `modules/core/tests/System/DoubleTests2.cpp`

## Metadata

- Audit status: AUDITED (176 lines, 59 tests, full read).
- Validation: `DoubleTests.*:DoubleTests2.*` passed 164/164 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

This companion suite covers a useful breadth of ordinary transcendental and
IEEE helpers. Its Pi-scaled checks demonstrate that normal quarter/half-turn
approximations work, but they compare a paired API to the same flawed scalar
implementation and use tolerances where the reference defines exact zeros.

## Finding references

- **SR-AUD-029:** `Round_WithDigits` covers only `digits == 2`; it misses both
  Double's 0/15 limits and the required invalid-precision exception.
- **SR-AUD-031:** `ILogB_PowerOfTwo` covers only a normal finite value, leaving
  zero/NaN/infinity sentinel behavior unobserved.
- **SR-AUD-032:** `SinPi_Half`, `CosPi_Zero`, and `TanPi_Quarter` do not cover
  the exact-zero boundaries; `SinCosPi_MatchesIndividual` compares two methods
  that share the same naive multiplication rather than an independent expected
  result.

## Required post-audit verification

Add exact, sign-aware assertions for `SinPi(1)`, `SinPi(-1)`, `TanPi(1)`,
`CosPi(0.5)`, and both `SinCosPi(1)` fields. Add the Double rounding-boundary
and special `ILogB` cases specified in the header report.

## Final assessment

Useful normal-path numerical coverage, but its self-comparisons and tolerance
checks cannot expose the shared Pi-reduction and special-value defects.
