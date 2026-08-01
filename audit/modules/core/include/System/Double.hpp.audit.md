# Audit: `modules/core/include/System/Double.hpp`

## Metadata

- Audit status: AUDITED (716 lines, header-only implementation, full read).
- Validation: `DoubleTests.*:DoubleTests2.*` passed 164/164 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-double-audit-probe.cpp`, compiled with
  `c++ -std=c++20 -I modules/core/include -I include` on 2026-07-25.

## Assessment

Ordinary IEEE classification, NaN ordering, signed-zero selection, `Clamp`,
magnitude operations, and the corrected special-token parser are well
represented. The public double-specific edge behavior repeats audited `Single`
defects rather than introducing a divergent policy: normal-significand
assumptions omit subnormal powers, direct C-library `ilogb` leaks the NaN
sentinel, direct Pi multiplication loses exact turns, and native text
facilities do not implement the default .NET grammar or format layout.

## Finding references

- **SR-AUD-021:** `ToString(1.25, "Q")` returns `"1.25"`; `"Fz"` and an
  oversized precision leak `std::stoi` instead of `System::FormatException`.
- **SR-AUD-029:** `Round(1.234567890123456, -1)` returns `0`, a 16-digit
  request returns a value, and a 1000-digit request returns NaN. `Double`
  permits only 0 through 15; the public method is also `noexcept`, so it
  cannot raise `ArgumentOutOfRangeException`.
- **SR-AUD-030:** `IsPow2(Double::Epsilon)` returns false because the normal
  trailing-significand-zero rule is incorrectly applied to subnormals.
  **Remediated (#1860, 2026-07-30, CCF-007):** a subnormal branch
  (`popcount(trailingSignificand)==1`) was added; `IsPow2(Double::Epsilon)` is
  now true. See `Single.hpp.audit.md` §SR-AUD-030.
- **SR-AUD-031:** the direct `std::ilogb` forwarding reports
  `ilogb_nan=-2147483648`, while zero also maps to that value and infinity
  maps to `2147483647`. .NET reserves `Int32.MinValue` for zero and returns
  `Int32.MaxValue` for all non-finite values.
  **Remediated (#1859, 2026-07-30, CCF-007):** `Double::ILogB` (with
  `Single::ILogB` and `Math::ILogB`) now returns `INT_MIN` for zero and
  `INT_MAX` for NaN and both infinities before the finite `std::ilogb`. See
  `Single.hpp.audit.md` §SR-AUD-031.
- **SR-AUD-032:** direct multiplication by `Pi` produces
  `sinpi_1=1.22464679914735321e-16`,
  `cospi_half=6.12323399573676604e-17`, and
  `tanpi_1=-1.22464679914735321e-16` rather than the exact Pi-scaled boundary
  results. `SinCosPi(1)` likewise produces a nonzero sine.
  **Remediated (#1861, 2026-07-30, CCF-007):** `Double::SinPi`/`CosPi`/`TanPi`/
  `SinCosPi` (and the `Single` counterparts) were rewritten to the .NET
  integral/fractional-turn reduction (amd/aocl-libm-ose kernels, `xTail`
  parameter retained and called with `0.0`), so integer turns yield a
  sign-carried zero, half turns exact `±1`/`0`/`±Infinity`, non-finite inputs
  `NaN`, and ordinary values stay within libm ULPs. `noexcept`/signatures/layout
  unchanged. See `Single.hpp.audit.md` §SR-AUD-032, including the `TanPi(±1)`
  signed-zero premise correction.
- **SR-AUD-033:** the probe rejects valid default inputs with outer whitespace,
  thousands grouping, and finite overflow (`" 1.5 "`, `"1,234.5"`,
  `"1e999"`), and emits `N2` as `1234.50` and `E2` as `1.25E+00` rather than
  the documented .NET layouts. The default Double parser uses
  `NumberStyles.Float | NumberStyles.AllowThousands`:
  <https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Double.cs.html>.

## Required post-audit verification

Apply the precision bound appropriate to `Double` (0–15) and remove
`noexcept` from the throwing overload. Reuse the documented subnormal,
`ILogB`, Pi-boundary, parse/format, and invalid-format vectors from the
`Single` remediation, with double-precision expected values. The .NET
`Math`/`Double` implementation is the reference for non-finite `ILogB` and
dedicated Pi-scaled reduction:
<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Math.cs.html>.

## Other missing assertions and diagnostics

- The tests validate only normal `IsPow2` values and finite `ILogB(8)`.
- The Pi tests use quarter/half turns with tolerances, but omit exact integral
  and cosine half-turn zero/sign checks.
- The stream-based `E` test asserts merely nonempty output; it cannot detect
  the incompatible two-digit exponent.

## Final assessment

The basic double wrapper is functional, but the same public edge contracts
already confirmed for numeric formatting/conversion and generic floating math
need coordinated repair. No implementation was modified during this audit.

### Remediated (SR-AUD-021 float slice) — ticket #1849 (2026-07-30)

`Double::ToString(double, const std::string&)` (Double.hpp:685) receives the same
fix as `Single`: the precision `std::stoi` is wrapped in
`try/catch → System::FormatException("Format specifier was invalid.")` (so
`ToString(1.0, "Fz")` no longer leaks `std::invalid_argument` and an oversized
precision no longer leaks `std::out_of_range`), and the silent
`return ToString(value);` fallback is replaced by the same `FormatException` so
an unrecognised specifier is rejected loudly. `F/E/G/R/N` stay valid; not
`noexcept`. +6 tests. Closes the CCF-006 float slice of SR-AUD-021. The `N`
group-separator and `E` exponent-digit fidelity gaps are the separate CCF-007
review. `docs/NumericWrapperBoundaryPlan.md` §15.5.

### Post-audit remediation — #1927 (2026-08-01)

With the exact approval in `docs/TextSubsetCompatibilityDecision.md` §6.5 item
(1), `Double::Round(double,intcs)` now uses the
`Math::Round(x,digits,MidpointRounding::ToEven)` funnel. The former local
`std::pow`/`nearbyint` copy returned infinity for large finite values such as
`Double::Round(1e300,15)`; the funnel returns the input unchanged at and above
the `1e16` round limit. A stronger pre-fix probe corrected the decision
packet's incomplete premise: negative subnormal and smallest-normal values
exposed a pre-existing sign-of-zero defect in the port's `Math` funnel. The
inseparable private-helper correction is recorded in `Math.hpp.audit.md` and
the packet's appended correction. No declaration, exception specification,
layout, vtable, or mangled name changed.
