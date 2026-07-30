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
