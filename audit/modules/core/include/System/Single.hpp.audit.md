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

**Remediated (#1862, 2026-07-31, CCF-007 item CCF7-4).** Approved by the batch
instruction in the exact words of `docs/RemainingApprovalDecisions.md` §A.10
(option A: drop `noexcept`, throw). `Single::Round(float,intcs)` and
`Double::Round(double,intcs)` dropped `noexcept` and now reject
`digits < 0 || digits > 6` (float) / `> 15` (double) **before** the `std::pow`,
throwing `ArgumentOutOfRangeException("digits", …)` with .NET's own resource
strings verbatim — `ArgumentOutOfRange_RoundingDigits_MathF`
("Rounding digits must be between 0 and 6, inclusive.") and
`ArgumentOutOfRange_RoundingDigits` ("Rounding digits must be between 0 and 15,
inclusive."). Because .NET's guard is `(uint)digits > max`, a negative count
throws too, and the port matches. Measured before/after over 16 rounding cases:
`build-probe/1854_{prefix,postfix}_plain.log` (`Round(1.2345f,99)` NaN → throw;
`Round(1.2345f,-1)` `0` → throw; `Round(1.2345,16)` silently-ignored → throw).
+13 tests in `SingleTests.cpp`/`DoubleTests2.cpp`, including `static_assert`s
pinning that the two-argument overload is no longer `noexcept` and the
one-argument overload still is. No parameter list, return type, layout, vtable
or mangled name changed (an Itanium mangled name does not encode `noexcept`), so
there is no exported-symbol break. **SR-AUD-029 → remediated.**

**Premise correction — the audit text is right about the defect and silent about
its cause.** The audit recorded that `Round(float,digits)` "computes
`pow(10.0f, digits)`" without saying *why* .NET does not: .NET's
`Single.Round(x,d)` **forwards to `MathF.Round(x,d,ToEven)`**, and this port's
`Single::Round` does not — it re-implements the scale/round/divide inline. The
port's own `MathF::Round(float,intcs,MidpointRounding)` already carried both the
correct guard and .NET's large-magnitude short-circuit. Two consequences,
measured 2026-07-31 (`build-probe/1854_prefix_plain.log` cases A30–A35) and
**deliberately left unrepaired because they are outside #1862's approval**:
`Single::Round(3.0e38f,6)` returns `inf` and `Double::Round(1e300,15)` returns
`inf`, where `MathF::Round`/`Math::Round` return the value unchanged; and
`Math::Round(x,digits,mode)` throws the message
"digits must be between 0 and 15, inclusive.", missing .NET's leading
"Rounding ". Filed as inactive tickets **#1927** and **#1928**; no new
`SR-AUD-*` identifier was issued and audit numbering stays frozen at 364.

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

**Remediated (#1860, 2026-07-30, CCF-007):** `Single::IsPow2`/`Double::IsPow2`
now branch on a zero biased exponent (subnormal) and accept
`std::popcount(trailingSignificand) == 1`, retaining the existing
zero/negative/NaN/infinity rejection and the normal-value `trailingSignificand
== 0` rule. Tests added for `Epsilon`, `2·Epsilon`, `4·Epsilon`, a two-bit
subnormal (false), a negative subnormal (false), and a normal power of two.

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

**Remediated (#1859, 2026-07-30, CCF-007):** `Single::ILogB`, `Double::ILogB`,
and `Math::ILogB` now return `INT_MIN` for zero and `INT_MAX` for NaN and both
infinities before the finite `std::ilogb`, matching the already-fixed
`MathF::ILogB` and .NET `Math.ILogB`; `noexcept` unchanged. Tests added for
zero, ±infinity, NaN, the smallest subnormal (`-149`/`-1074`), and a normal
exponent, on all three types.

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

**Remediated (#1861, 2026-07-30, CCF-007):** `Single::SinPi`, `CosPi`, `TanPi`,
and `SinCosPi` (and their `Double` counterparts) were rewritten from
`std::sin(x*Pi)` etc. to the .NET integral/fractional-turn reduction, ported
verbatim from `sinpif`/`cospif`/`tanpif` (amd/aocl-libm-ose) including the
interval kernels `SinForIntervalPiBy4`/`CosForIntervalPiBy4`/
`TanForIntervalPiBy4`. Integer turns now yield a sign-carried zero, half turns
exact `±1`/`0`/`±Infinity`, non-finite inputs `NaN`; ordinary values stay within
libm ULPs. `noexcept`/`[[nodiscard]]`/signatures/layout unchanged (header-only).
20 add-only tests; UBSan + ASan + `float-cast-overflow` clean on
`build-probe/1861_pitrig_probe.cpp`. **Premise correction:** the finding's
requested assertion `TanPi(1)` was expected `+0` in the CCF-007 plan §12, but the
reference returns `sign * (odd ? -0.0 : +0.0)`, so `TanPi(+1)==-0` and
`TanPi(-1)==+0`; the tests assert the measured .NET values.

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

### SR-AUD-033 fully remediated — ticket #1863 (2026-07-31), and CCF-007 complete

Approved by the batch instruction in the exact words of
`docs/RemainingApprovalDecisions.md` §D.5, option D(i). The last open half of this
finding — the text `Single::ToString(value, format)` and
`Double::ToString(value, format)` **emit** — now matches .NET, through one shared
private header `System/detail/FloatTextFormat.hpp` so the two types cannot drift
apart: `E`/`e` carry a sign and at least three exponent digits (`"1.25E+000"`,
was `"1.25E+00"`), `N`/`n` insert invariant-culture group separators
(`"1,234.50"`, was `"1234.50"` — identical to `F`), and `G` becomes the shortest
round-trippable text while `G<n>` uses `std::to_chars`' general form. The
`to_chars` default/`R` fast path is unchanged and a test pins that it did not
move — that was the approval's one non-negotiable constraint.

**Two premises corrected by measurement** (`build-probe/1863_{prefix,postfix}_plain.log`,
46 cases), preserved additively in `docs/FloatingValueFidelityPlan.md` §18:

1. **`G17` and `G9` already round-tripped.** The plan recorded them as
   "`setprecision` (not guaranteed shortest/round-trip)". Measured,
   `setprecision(17)` with `defaultfloat` gives exactly 17 significant digits,
   which *is* `double`'s round-trip width; a round-trip sweep over ten doubles
   and eight floats passed **before** the change, and their emitted text is
   unchanged after it. What was missing was the guarantee, not the behaviour.
2. **`G` with no precision was not the `to_chars` fast path.** §D.3 of the
   decision packet lists it alongside `R` and the no-format overload as "already
   shortest-round-trip … must be preserved". Measured, it fell to
   `oss << value` — **six** significant digits, `"0.333333"` for one third. So
   making it shortest is a change, and one §D.5 explicitly approves.

`Half` inherits the whole change by delegation with no edit of its own, pinned by
a test. No public signature, `noexcept` specification, object layout or ABI
change; both bodies are header-inline. +11 permanent tests; ASan and UBSan clean
over all 46 probe cases with answers identical to the plain build. The migration
cost §D.4 named is real and has no compiler diagnostic: any golden file, snapshot
test or serialized text that captured `"1234.50"`, `"1.25E+00"` or a six-digit
`G` changes.

**`SR-AUD-033 → remediated`** (whitespace #1864, parse tail #1865, format slice
#1863). **CCF-007 is complete**: SR-AUD-029, 030, 031, 032 and 033 are all
`remediated`.

### Post-audit remediation — #1927 (2026-08-01)

With the exact approval in `docs/TextSubsetCompatibilityDecision.md` §6.5 item
(1), `Single::Round(float,intcs)` now delegates to
`MathF::Round(x,digits,MidpointRounding::ToEven)`. The former local
`std::pow`/`nearbyint` copy returned infinity for large finite values such as
`Single::Round(3e38f,6)`; the delegate returns the input unchanged at and above
the `1e8f` round limit. The permanent boundary/special-value matrix covers both
signs, zero, subnormal, normal, finite-limit-adjacent and infinite values over
all seven legal digit counts. The signature and `noexcept` state are unchanged.
