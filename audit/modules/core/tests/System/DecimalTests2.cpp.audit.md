# Audit: `modules/core/tests/System/DecimalTests2.cpp`

## Metadata

- Audit status: AUDITED (301 lines, 43 tests, full read).
- Validation: `DecimalTests.*:DecimalTests2.*` passed 143/143 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

The focused supplemental suite covers raw construction for a nonzero negative
value, named rounding modes, and normal `CopySign` cases.  It omits exactly
the invalid and zero-representation boundaries where this implementation
diverges: reversed Clamp bounds, an invalid `MidpointRounding` value, and
negative zero's raw sign.

## Finding references

- **SR-AUD-022:** only ordered Decimal Clamp intervals are tested; no
  `min > max` vector demands the required `ArgumentException`.
- **SR-AUD-036:** every named `MidpointRounding` path is covered, but an
  out-of-enum cast is not required to throw `ArgumentException`.
- **SR-AUD-038:** `CtorFromBits_Negative` and `CopySign` use nonzero values;
  `GetBits_Zero` checks only positive zero.  Consequently the suite cannot
  detect erasure of negative zero's sign bit.

## Required post-audit verification

Add explicit exception assertions for an inverted Clamp range and an invalid
rounding enum.  Construct `Decimal(0, 0, 0, true, scale)`, call `CopySign` on
zero, and check bit 31 via `GetBits` for both values.

## Final assessment

The suite protects the recent ordinary Decimal functionality but needs public
invalid-input and raw-representation assertions before it can prevent the
confirmed regressions.
