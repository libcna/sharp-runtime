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

### SR-AUD-039 remediated — ticket #2232 (2026-08-10)

`Math::Log(double, double)` is no longer a bare `std::log(a) / std::log(newBase)`.
It now applies .NET's four guards in .NET's order — NaN in the argument, NaN in
the base, base one, and (for an argument that is not one) base zero or positive
infinity — exactly as the `MathF::Log(float, float)` sibling in this repository
already did.

Measured on the shipped library before the edit
(`build-probe/2229_probe1_before.log`, group `[039]`): **15 cases, 3 wrong** —
`Log(5, 1)` returned `+Inf`, `Log(5, 0)` returned `-0` and `Log(5, +Inf)`
returned `+0`, where .NET returns `NaN` for all three. After: **0 wrong**.

**Premise note, produced by the probe itself.** The probe's first draft expected
`Log(1, 0) == +0` and flagged both the `Math` case *and* the `MathF` control as
wrong. The control disagreeing is the discriminator: `MathF::Log` is a faithful
transcription of .NET's algorithm, so a control it fails is an error in the
expectation. .NET's `(a != 1) && ((newBase == 0) || IsPositiveInfinity(newBase))`
guard does **not** fire when `a == 1`, so the result falls through to
`Log(1)/Log(0)` — a **signed** zero. `Log(1, 0)` is `-0` and `Log(1, +Inf)` is
`+0`; both already held in the port and are preserved deliberately, and both are
now pinned as the over-rejection control. The corrected baseline is the one
quoted above.

`Math::Log(double)`, `Log2` and `Log10` are untouched — only the two-argument
overload has special cases. The signature and exception specification are
unchanged. +14 test cases in
`modules/core/tests/System/NumericSpecialValueTests.cpp`, including a suite
asserting the `Math`/`MathF` pair now agrees on every special base. Family plan:
`docs/CoreNumericSpecialValueRoundingFamilyPlan.md` §4.3.
