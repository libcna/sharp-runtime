# Audit: `modules/core/include/System/Single.hpp`

## Metadata

- Audit status: AUDITED (633 lines, header-only implementation, full read).
- Validation: `SingleTest.*` passed 102/102 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-single-audit-probe.cpp`, compiled with
  `c++ -std=c++20 -I modules/core/include -I include` on 2026-07-25.

## Assessment

The wrapper has sound ordinary classification, zero/NaN ordering, `Clamp`,
bit-increment/decrement, canonical special-token parsing, and hash behavior.
It deliberately gives the public `Single` name to C++ `float`, so the
generic-math and formatting methods must still preserve the documented .NET
contracts. Six public edge families do not: decimal rounding validates no
digit range, power-of-two classification excludes valid subnormals, `ILogB`
maps NaN to the C library sentinel, Pi-scaled trigonometry is a naive
multiplication, parsing/formatting exposes a much narrower C++ grammar, and
`IsPositive` rejects a positive-sign NaN.

## SR-AUD-029 — medium — `Round(float, digits)` accepts invalid precision and can return a spurious value or NaN

`Round(float, intcs)` is `noexcept`, computes `pow(10.0f, digits)`, and never
validates the supported precision.  The direct probe observed:

```
round_neg1=0
round_7=1.234500051
round_100=-nan
```

for `Round(1.2345f, -1)`, `Round(1.2345f, 7)`, and
`Round(1.2345f, 100)`, respectively.  `MathF.Round(float, int)` only permits
0 through 6 and throws `ArgumentOutOfRangeException` outside that range:
<https://learn.microsoft.com/en-us/dotnet/api/system.mathf.round?view=netstandard-2.1>.
The current `noexcept` signature also precludes the required exception.

### Required post-audit verification

Validate `digits` before evaluating a scale, throw
`System::ArgumentOutOfRangeException`, and remove `noexcept`.  Add exact
negative and 7-digit exception tests, the valid 0/6 boundaries, and a large
invalid value proving that no NaN is returned.

## SR-AUD-030 — medium — `IsPow2` rejects every valid subnormal power of two

`IsPow2` requires all trailing-significand bits to be zero.  That is correct
for normal floats, but every subnormal power of two carries one such bit.  The
probe shows `IsPow2(Single::Epsilon)` as false:

```
is_pow2_epsilon=0
```

The authoritative `Single` implementation separates subnormals and accepts a
one-bit trailing significand (`PopCount(...) == 1`):
<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Single.cs.html>.
This breaks the generic `IBinaryNumber` contract for `Epsilon`,
`2 * Epsilon`, and the remaining subnormal powers of two.

### Required post-audit verification

Decode exponent and significand separately; for exponent zero accept exactly
one significand bit, while retaining the zero/negative/NaN/infinity rejection.
Add `Epsilon`, `2 * Epsilon`, and a two-bit subnormal negative control.

## SR-AUD-031 — medium — `ILogB(NaN)` leaks the C library sentinel instead of the .NET result

The wrapper forwards directly to `std::ilogb`.  On this supported toolchain,
the C++ NaN sentinel is `INT_MIN`, so the probe returns:

```
ilogb_zero=-2147483648
ilogb_nan=-2147483648
ilogb_inf=2147483647
```

The .NET API reserves `Int32.MinValue` for zero and returns
`Int32.MaxValue` for NaN and either infinity.  Its reference implementation
explicitly performs that classification before computing a finite exponent:
<https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/MathF.cs.html>.
The current behavior therefore makes zero and NaN indistinguishable to callers.

### Required post-audit verification

Classify zero and non-finite values before calling or replacing `ilogb`, then
test zero, positive/negative infinity, NaN, the smallest subnormal, and a
normal exponent.

## SR-AUD-032 — medium — Pi-scaled trigonometric APIs lose exact turn-boundary results

`SinPi`, `CosPi`, `TanPi`, and `SinCosPi` directly evaluate the ordinary C++
trigonometric function after multiplying by the rounded `float` `Pi`.  This
loses the exact zeros and signs promised by a dedicated Pi-scaled operation.
The probe produced:

```
sinpi_1=-8.742277657e-08
cospi_half=-4.371138829e-08
tanpi_1=8.742277657e-08
sincospi_1=-8.742277657e-08,-1
```

for integer and half-integer turns.  The .NET `Single` implementation reduces
the fractional turn and specifically returns `x * 0.0f` at integral inputs and
`0.0f` for the cosine half-turn case:
<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Single.cs.html>.
That also preserves the signed-zero result for negative integral turns, which
the naive multiplication cannot provide.

### Required post-audit verification

Use a Pi-scaled reduction algorithm (or an equivalently specified helper) for
all four methods.  Add bit-sign-aware zero assertions for `SinPi(1)`,
`SinPi(-1)`, `TanPi(1)`, `CosPi(0.5)`, and both fields of `SinCosPi(1)`.

## SR-AUD-033 — medium — public `Single` parsing and standard formatting implement a C++ subset rather than .NET defaults

`tryParseCore` sends ordinary input to `from_chars` and rejects all non-finite
results; the formatting overload emits `N` through plain `fixed` output and
uses stream exponent formatting.  Consequently it rejects valid default
`Single.Parse` inputs and returns incompatible standard-format text:

```
parse_" 1.5 "=0,0
parse_"1,234.5"=0,0
parse_"1e999"=0,0
parse_"-1e999"=0,0
parse_" NaN "=0,0
format_N2=1234.50
format_E2=1.25E+00
```

The default .NET parse overload uses `NumberStyles.Float |
NumberStyles.AllowThousands` and returns signed infinity rather than throwing
on a value outside the finite float range; these contracts are visible in the
reference `Single` source:
<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Single.cs.html>.
The documented `N` standard numeric format includes group separators, and
scientific standard formats use the .NET exponent layout:
<https://learn.microsoft.com/en-us/dotnet/standard/base-types/standard-numeric-format-strings>.
`ToString("Q")` also falls back to the general format and malformed precision
(`"Fz"`) leaks `std::stoi`; those diagnostic failures extend SR-AUD-021.

### Required post-audit verification

Implement the selected invariant/default .NET grammar explicitly (including
outer whitespace, group separators, canonical special tokens, and overflow to
infinity), or narrow the public API and documentation before release.  Route
format validation through `System::FormatException`, add grouping and
three-digit-exponent format tests, and keep a separate exact unknown-format
assertion under SR-AUD-021.

## SR-AUD-034 — medium — `IsPositive` incorrectly excludes positive-sign NaN

`IsPositive` requires both a clear sign bit and `!isnan(value)`. The probe
therefore observes:

```
is_positive_nan=0
```

for the positive-sign `Single::NaN`. .NET defines the generic-math predicate
from the raw signed bit representation (`SingleToInt32Bits(value) >= 0`), so a
positive-sign NaN is positive while a negative-sign NaN is negative:
<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Single.cs.html>.
`Double::IsPositive` in this repository already follows that sign-bit rule,
confirming that this is not a deliberate project-wide NaN policy.

### Required post-audit verification

Remove the NaN exclusion and test both the canonical positive-sign NaN and a
sign-flipped NaN with `IsPositive`/`IsNegative`. Retain the existing `+0` and
ordinary finite assertions.

## Other missing assertions and diagnostics

- The 102-test suite does not probe subnormal power-of-two inputs, any special
  `ILogB` value, Pi-scaled functions, invalid rounding precision, parse
  whitespace/grouping/overflow, or the `N`/`E` output layout.
- It also does not distinguish the positive and negative NaN sign bit for
  `IsPositive`/`IsNegative`.
- `Round` has no diagnostic path for a bad `digits` argument because it is
  declared `noexcept`; the report records the required exception type.
- The parser has no error detail beyond a boolean or generic
  `FormatException`, so valid-but-rejected input is difficult to distinguish
  from invalid syntax at a call site.

## Final assessment

The ordinary float paths are well covered, but missing edge assertions conceal
six contract deviations in generic math, special-value classification,
Pi-scaled numerical functions, and text conversion.  No implementation was
modified during this audit.

### Remediated (SR-AUD-021 float slice) — ticket #1849 (2026-07-30)

`Single::ToString(float, const std::string&)` (Single.hpp:602) now guards the
precision parse: the `std::stoi(format.substr(1))` is wrapped in
`try/catch (const std::exception&)` that throws
`System::FormatException("Format specifier was invalid.")`, so `ToString(1.0f,
"Fx")` no longer leaks `std::invalid_argument` and an oversized precision no
longer leaks `std::out_of_range`. The former silent `return ToString(value);`
fallback for an unrecognised specifier is replaced by the same
`FormatException`, so `C`/`P`/`D`/`X`/`B`/`Q`/etc. are rejected loudly rather than
returning a silently wrong value — matching the integer wrappers (#1847) and
.NET's `Format_BadFormatSpecifier`. `F/E/G/R/N` (and lowercase) stay valid; the
overload is not `noexcept` so no exception-spec changed. +6 tests. This closes the
CCF-006 float slice of SR-AUD-021. The `N`-branch's missing group separators and
the `E` two-vs-three exponent-digit divergence are value-fidelity gaps tracked
under CCF-007, not this leak. `docs/NumericWrapperBoundaryPlan.md` §15.5.
