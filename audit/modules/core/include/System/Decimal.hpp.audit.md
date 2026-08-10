# Audit: `modules/core/include/System/Decimal.hpp`

## Metadata

- Audit status: AUDITED (738 lines, declarations and inline implementation
  fully read).
- Validation: `DecimalTests.*:DecimalTests2.*` passed 143/143 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.  The `DecimalTests` suite
  combines the 51 tests in `DecimalTests.cpp` and the 49 tests in
  `DecimalNewTests.cpp`.
- Direct probe: `/tmp/sharp-runtimervc-decimal-audit-probe.cpp`, compiled with
  `c++ -std=c++20 -I modules/core/include -I include` against
  `build/libsharp_runtime_core.a` on 2026-07-25.

## Assessment

The public surface gives the 96-bit decimal representation a useful amount of
ordinary arithmetic, scale, conversion, and generic-math coverage.  It does
not, however, preserve all observable representation and boundary contracts:
inverted `Clamp` bounds are silently selected, `CopySign` deliberately clears
the sign of zero, and OA Currency conversion truncates where the .NET API
rounds to the nearest four-decimal currency unit.

## Finding references

- **SR-AUD-022:** `Clamp(5, 10, 0)` returns `10` rather than rejecting
  `min > max` with `ArgumentException`.  This is an independently implemented
  decimal instance of the already-confirmed primitive numeric Clamp defect.
- **SR-AUD-037:** `ToOACurrency` multiplies by 10,000 then calls `Truncate`.
  The probe returns `1234` for `0.123456789` and `-792281` for
  `-79.228162514264337593543950335`; the documented .NET results are `1235`
  and `-792282`, respectively:
  <https://learn.microsoft.com/pt-br/dotnet/api/system.decimal.tooacurrency?view=netframework-4.7.2>.
- **SR-AUD-038:** `CopySign` rejects a negative sign when the magnitude is
  zero.  Together with the raw constructor and parser in `Decimal.cpp`, this
  erases a representation that callers can observe through `GetBits`.
  .NET documents bit 31 as a sign bit and explicitly states that its decimal
  representation distinguishes positive and negative zero:
  <https://learn.microsoft.com/en-us/dotnet/api/system.decimal.getbits?view=net-10.0>.
  **Remediated (#1856, 2026-07-30):** `CopySign` now copies the sign
  unconditionally (`sign.negative_`), so `CopySign(0m, -1m)` is a negative zero
  observable via `GetBits`/`IsNegative`. Paired with the raw-ctor and parser
  fixes in `Decimal.cpp`. `−0m == 0m` and hash equality were already correct.

## Required post-audit verification

Validate `min <= max` before selecting a Decimal Clamp result.  Implement OA
Currency's documented nearest-unit rounding (including negative ties and
overflow), not truncation.  Preserve the raw sign in `Decimal(lo, mid, hi,
isNegative, scale)` and make `CopySign(0, negative)` set that sign; verify the
four words returned by `GetBits`, not just numeric equality.

## Other missing assertions and diagnostics

- No test supplies inverted Decimal Clamp bounds.
- No test covers `ToOACurrency`, half-unit rounding, negative rounding, or
  its overflow boundary.
- `CopySign` covers only nonzero magnitudes; neither its sign-zero behaviour
  nor the negative-zero raw constructor/`GetBits` round-trip is asserted.

## Final assessment

Decimal's ordinary APIs are serviceable, but a financial conversion and the
public raw representation still have observable .NET compatibility gaps.  No
implementation was modified during this audit.

### SR-AUD-037 remediated — ticket #2231 (2026-08-10)

`Decimal::ToOACurrency` no longer computes `Truncate(*this * Decimal(10000))`. It
rounds to the four decimals Currency stores through the existing
`Decimal::Round(d, 4, MidpointRounding::ToEven)` funnel and only then scales, so
the value is rounded to the **nearest** currency unit as .NET documents.

Measured on the shipped library before the edit
(`build-probe/2229_probe1_before.log`, group `[037]`): **15 cases, 5 wrong**.
The two values the .NET documentation tabulates both failed — `0.123456789`
returned `1234` (documented `1235`) and `-79.228162514264337593543950335`
returned `-792281` (documented `-792282`) — and every magnitude below `0.00005`
collapsed to `0`. After: **0 wrong**, with the exactly-representable controls
(`0`, `1`, `1.0000`, `-1.2345`, `100000000000000`) unchanged and the untouched
`FromOACurrency` round trip asserted.

**The tie rule is the one element the documented examples cannot decide**, and
this is recorded rather than glossed: neither tabulated value is a midpoint.
`ToEven` was implemented because .NET performs the scale reduction through
`DecCalc.VarCyFromDec` → `InternalRound(…, MidpointRounding.ToEven)` and because
every other rounding funnel in this port defaults to `ToEven`; `/rv` is absent in
this container to confirm the exact call. The choice is **pinned by tests**
(`1.00005` → `10000`, `1.00015` → `10002`, `-1.00005` → `-10000`; under
`AwayFromZero` those would be `10001`, `10002`, `-10001`) and the residual
question is **deferred-verification ticket #2234**, following the #2060 / #2070 /
#2130 convention.

Overflow was measured rather than assumed. Both `Decimal::MaxValue` and
`1000000000000000` **already** threw `OverflowException` before this ticket, but
with the generic messages `"Decimal overflow."` and `"Value was either too large
or too small for an Int64."`. The exception **type** was already right; only the
message moves, to .NET's `SR.Overflow_Currency` text `"Value was either too large
or too small for a Currency."`. `Decimal.hpp` gains `#include
"System/OverflowException.hpp"` — an intra-`Core.Base` include, so no module edge
changes. The signature and exception specification are unchanged. +18 test cases
in `modules/core/tests/System/NumericSpecialValueTests.cpp`. Family plan:
`docs/CoreNumericSpecialValueRoundingFamilyPlan.md` §4.2.
