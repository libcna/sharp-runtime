# Audit: `modules/core/include/System/IComparable.hpp`

## Metadata

- Audit status: AUDITED (39-line public template interface, fully read).
- Supporting validation: `IComparableTests2.*` passed 3/3 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The generic comparison contract correctly states sign-only ordering and uses a
const-reference operand with a virtual destructor.  It does not prescribe a
subtraction-based implementation; concrete types remain responsible for
avoiding overflow and for preserving ordering laws.

## Other missing assertions and diagnostics

- The representative test implementation returns `v - other.v`; extreme
  `int` values would overflow before the sign is observed.  Tests use only
  small values, so this fixture does not document a safe comparison idiom.
- No test checks antisymmetry, transitivity, base-pointer dispatch, or a
  comparison result whose magnitude differs from `-1`, `0`, and `1`.

## Final assessment

The interface contract itself is sound.  The subtraction note is a test
fixture boundary that should be strengthened in a future test pass, not a
confirmed production implementation defect; no source or test was modified.
