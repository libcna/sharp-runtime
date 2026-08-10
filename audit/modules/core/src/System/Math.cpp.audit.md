# Audit: `modules/core/src/System/Math.cpp`

## Metadata

- Audit status: AUDITED (277 lines, full read).
- Validation: `MathTests.*:MathFTest.*` passed 174/174 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-math-audit-probe.cpp`, compiled and run
  against `build/libsharp_runtime_core.a` on 2026-07-25.

## Assessment

This implementation correctly applies the recently hardened NaN/signed-zero
Min/Max policy and protects integral `DivRem` traps.  Its double logarithm with
a base is still a naive `log(a) / log(base)` forwarding, despite .NET's
specified preconditions for NaN, base one, zero, and positive infinity.

## Finding references

- **SR-AUD-039:** `Math::Log(5.0, 1.0)` returns positive infinity and
  `Math::Log(5.0, 0.0)` returns negative zero.  Both must return NaN.  .NET
  explicitly checks base one and, when the argument is not one, base zero or
  positive infinity before performing the quotient:
  <https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Math.cs.html>.
  The local `MathF::Log` already implements this policy, so the mismatch is
  within the paired public APIs as well as against .NET.

## Required post-audit verification

Mirror the existing `MathF::Log(float, float)` special-value policy in the
double overload, preserving NaN payload propagation where the local API can
do so.  Test base one, zero, positive infinity, argument one, and NaN on both
positions.

## Other missing assertions and diagnostics

- `Log_WithBase` covers only `Log(8, 2)`; it misses every special base case
  already covered in `MathFTests.cpp`.
- The focused suite's green result does not exercise the implementation's
  direct `std::ilogb` wrapper declared in the paired header.

## Final assessment

The ordinary double routines are broadly covered, but one naive native formula
leaves incompatible special-value results that the sibling float API already
avoids.  No implementation was modified during this audit.
