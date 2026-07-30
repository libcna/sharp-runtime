# Audit: `modules/core/include/System/Math.hpp`

## Metadata

- Audit status: AUDITED (594 lines, declarations and inline implementation
  fully read).
- Validation: `MathTests.*:MathFTest.*` passed 174/174 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-math-audit-probe.cpp`, compiled with
  `c++ -std=c++20 -I modules/core/include -I include` against
  `build/libsharp_runtime_core.a` on 2026-07-25.

## Assessment

The header contains substantial and generally careful public numeric behavior:
the integer/float Clamp overloads validate their bounds, `DivRem` protects
native undefined division cases, and double `Round(ToEven)` intentionally
avoids ambient rounding-mode state.  Two exposed helpers still forward an
unqualified native result or silently reinterpret an invalid public enum.

## Finding references

- **SR-AUD-031:** `ILogB` is a direct `std::ilogb` forwarding.  The probe
  reports `math_ilogb_nan=-2147483648`, conflating NaN with zero's sentinel;
  .NET returns `Int32.MaxValue` for NaN and infinities while retaining
  `Int32.MinValue` for zero:
  <https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Math.cs.html>.
  **Remediated (#1859, 2026-07-30, CCF-007):** `Math::ILogB` (with
  `Single::ILogB` and `Double::ILogB`) now returns `INT_MIN` for zero and
  `INT_MAX` for NaN and both infinities before the finite `std::ilogb`.
- **SR-AUD-036:** both public `Round` overloads that accept
  `MidpointRounding` use their switch default as `ToEven`.  The probe obtains
  `math_round_invalid=2` for `Round(1.9, static_cast<MidpointRounding>(99))`.
  .NET throws `ArgumentException` for an invalid enum value:
  <https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Math.cs.html>.
  **Remediated (#1855, 2026-07-30):** `Round(double, MidpointRounding)`'s
  `switch` default now throws `System::ArgumentException(paramName "mode")`
  instead of `roundToEvenImpl`, matching
  `ThrowHelper.ThrowArgumentException_InvalidEnumValue`. The
  `Round(double, int, MidpointRounding)` overload validates the mode through
  the funnel for magnitudes `< 1e16` (matching .NET's own Math.cs, which does
  not validate the mode for larger magnitudes). **Closes CCF-008.**

## Required post-audit verification

Classify zero and non-finite values before calling `std::ilogb`, preserving
the documented `Int32.MinValue`/`Int32.MaxValue` split.  Validate
`MidpointRounding` before every `Round` calculation and test the invalid cast
for both the integer-digits and no-digits overload families.

## Other missing assertions and diagnostics

- The 148 Math tests have no `ILogB` vector at all, including zero, subnormal,
  NaN, and either infinity.
- Named rounding modes are covered, but an invalid public enum is not required
  to throw.

## Final assessment

Most established Math hardening is present, but direct native sentinel leakage
and missing enum validation remain observable public defects.  No
implementation was modified during this audit.
