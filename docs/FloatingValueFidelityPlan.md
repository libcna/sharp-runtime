<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Floating value-fidelity family — CCF-007 plan

*Authored 2026-07-30 by the batch on branch
`feature/remediation-batch-decimal-ccf007`, immediately after the CCF-005 Decimal
slice landed (#1855 SR-AUD-036/CCF-008, #1856 SR-AUD-038, #1857 SR-AUD-035
compatible portion; #1858 opened blocked). It is the durable, evidence-based plan
for **CCF-007 — "the binary float wrappers delegate public edge semantics to
unsuitable native primitives"** (`audit/AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-007).
Five findings, all `confirmed`, on `System::Single`/`System::Double` (and
`System::Math`/`System::Half` where the same code recurs): SR-AUD-029
(`Round(x,digits)` precision validation), SR-AUD-030 (`IsPow2` subnormals),
SR-AUD-031 (`ILogB` non-finite sentinels), SR-AUD-032 (Pi-scaled trig turn
boundaries), SR-AUD-033 (parse/format .NET grammar).*

*Current-state verification was performed 2026-07-30 by three parallel read-only
agents over the production sources (`Single.hpp`, `Double.hpp`, `Half.hpp`,
`Math.hpp`, `MathF.hpp`), the per-file audit reports, and the current .NET
reference (`/rv/tmp/runtime/src/libraries/…`: `Single.cs`, `Double.cs`, `Half.cs`,
`Math.cs`, `MathF.cs`, `Number.Formatting.cs`, `Number.Formatting.Common.cs`,
`Number.Parsing.cs`). **All five findings still reproduce; none is remediated.***

**This document creates no `SR-AUD-*` identifier** (audit numbering is frozen at
364). It maps the five findings to files/lines, verifies premises against measured
current behaviour, groups the work into bounded dependency-ordered tickets, and
draws the implementation-vs-approval boundary. **No CCF-007 finding is marked
remediated by this document.**

---

## 1. Complete finding inventory

| Finding | Sev | One-line defect | Surfaces (measured) |
|---|---|---|---|
| **SR-AUD-029** | medium | `Round(x,digits)` never validates `digits` and is `noexcept`, so it silently returns a spurious value or NaN instead of throwing | `Single::Round(float,intcs)`, `Double::Round(double,intcs)` |
| **SR-AUD-030** | medium | `IsPow2` applies the normal "trailing significand == 0" rule to subnormals, rejecting every subnormal power of two (`Epsilon`, `2·Epsilon`, …) | `Single::IsPow2`, `Double::IsPow2` |
| **SR-AUD-031** | medium | `ILogB` forwards straight to `std::ilogb`, mapping NaN to `INT_MIN` — the same sentinel .NET reserves for zero — and using the platform sentinels for ±Inf | `Single::ILogB`, `Double::ILogB`, `Math::ILogB` (**`MathF::ILogB` already fixed**) |
| **SR-AUD-032** | medium | Pi-scaled trig multiplies a rounded `Pi` then calls the ordinary C++ function, losing exact turn-boundary zeros/±1/±Inf and their signs | `Single`/`Double` `SinPi`,`CosPi`,`TanPi`,`SinCosPi` |
| **SR-AUD-033** | medium | Parse rejects .NET-default whitespace/thousands/overflow inputs; `ToString` emits a C++ subset for `N` (no grouping) and `E` (2-digit exponent), and `G`/`G9`/`G17` are not shortest-round-trip | `Single`/`Double` `Parse`/`TryParse`/`tryParseCore`, `ToString(value,format)` |

The CCF-007 umbrella prose (`AUDIT_CROSS_CUTTING_FINDINGS.md:253-268`) requires
that any repair be coordinated across Single/Double **but retain the distinct 0–6
(float) and 0–15 (double) rounding limits**.

---

## 2. Complete file / public-surface inventory (measured 2026-07-30)

Owning component: **`Core.Base`** (target `sharp_runtime_core_base`), surfaced
through the `Core` INTERFACE umbrella. All three headers are header-only
(methods defined inline in-class).

| Method | Single.hpp | Double.hpp | Half.hpp | Math.hpp | noexcept? |
|---|---|---|---|---|---|
| `Round(x)` | :232 | :279 | — | (Math funnel) | yes |
| `Round(x,digits)` | :236 | :282 | — (Half has none) | — | **yes** (blocks throw) |
| `Round(x,MidpointRounding)` / `(x,digits,mode)` | **absent** | **absent** | absent | Math/MathF have them | — |
| `IsPow2` | :119 | :162 | absent | — | Single yes / Double neither |
| `ILogB` | :464 | :521 | absent | :467 | Single/Double yes; Math no |
| `SinPi`/`CosPi`/`TanPi` | :358-376 | :405-423 | absent | — | yes |
| `SinCosPi` | :396 | :444 | absent | — | yes |
| `Asin/Acos/Atan/Atan2 Pi` | :358-376 | :405-423 | absent | — | yes (simple ratios; **not** in SR-AUD-032) |
| `ToString(value)` | :592 | :670 | :313 | — | no |
| `ToString(value,format)` | :602 | :685 | :324 (→Single) | — | no (throws `FormatException`) |
| `ToString(format,provider)` / `TryFormat` | **absent** | **absent** | present (:333/:296) | — | — |
| `Parse`/`TryParse`/`tryParseCore` | :518-586 | :589-659 | :270 (→Single) | — | Single `TryParse` noexcept; **Double `TryParse` not** |

**Premise corrections vs. audit/plan wording (preserve the audit narrative; these
are appended):**

1. **Single/Double expose NO `MidpointRounding` `Round` overload and NO
   `TryFormat`/`ToString(format,provider)`.** Only `Half` has `TryFormat` and the
   provider overloads (and `Half` has no `Round`/`IsPow2`/`ILogB`/Pi-trig at all).
   So SR-AUD-036/CCF-008 (#1855) correctly never touched Single/Double, and any
   CCF-007 `TryFormat` fidelity work is Half-only.
2. **SR-AUD-031 also lives in `Math::ILogB` (`Math.hpp:467`), still a raw
   `std::ilogb` forward; `MathF::ILogB` was already given the explicit
   NaN/Inf/zero classification** (visible in `MathF.hpp`), so the ILogB fix must
   cover Single + Double + Math and match the existing MathF shape.
3. **The audit's SR-AUD-032 "SinCosPi" is real** — `SinCosPi` exists on both types
   (`Single.hpp:396`, `Double.hpp:444`) returning a `{SinPi,CosPi}` pair via the
   same naive multiply.
4. **`Round(x)`/`Round(x,digits)` use `std::nearbyint`**, which honours the ambient
   FP rounding mode. That is the *SR-AUD-040 class* of defect (the one fixed for
   `MathF::Round(ToEven)` by ticket #1723) and is **not** a CCF-007 finding — do
   not fold it in; note it as adjacent.
5. **`Double::TryParse` is not `noexcept` though its `tryParseCore` is** — a free,
   ABI-neutral tightening, but outside CCF-007's findings; note only.

---

## 3. Current-behaviour matrix (measured) vs. 4. actual .NET behaviour (reference)

| Finding | Measured current behaviour | Actual .NET (file:line) |
|---|---|---|
| **029** | `Round(1.2345f,-1)=0`, `Round(1.2345f,7)=1.2345…`, `Round(1.2345f,100)=NaN`; `noexcept`, never throws | `Single.Round(x,d)`→`MathF.Round` throws `ArgumentOutOfRangeException("digits","Rounding digits must be between 0 and 6, inclusive.")` for `(uint)d>6` (`MathF.cs:430`); double: 0–15, `Math.cs:1407`, msg "…between 0 and 15, inclusive." (`Strings.resx:1928-1933`) |
| **030** | `IsPow2(Epsilon)=false` (subnormal has a trailing-significand bit) | `Single.cs:546-572`: for `biasedExponent==MinBiasedExponent` (subnormal) returns `PopCount(trailingSignificand)==1` ⇒ `IsPow2(Epsilon)=true`; normal ⇒ `trailingSignificand==0`; `(int)bits<=0` rejects zero/negatives; `MaxBiasedExponent` (NaN/Inf) ⇒ false |
| **031** | `ilogb_zero=INT_MIN`, `ilogb_nan=INT_MIN`, `ilogb_inf=INT_MAX` (zero and NaN indistinguishable) | `Math.cs:891-915`: zero→`int.MinValue`; NaN **and** ±Inf→`int.MaxValue`; subnormal→computed exponent; normal→`x.Exponent` |
| **032** | `sinpi_1=-8.7e-08`, `cospi_half=-4.4e-08`, `tanpi_1=8.7e-08` (nonzero; wrong sign of zero) | `Double.cs:2092-2172`/`Single.cs`: split `integral=(long)ax`, `fractional=ax-integral`; `SinPi(k)=x*0.0` (**exact signed zero**), `SinPi(k+0.5)=±1`, `CosPi(k+0.5)=0.0` exact, `TanPi(k)=sign*±0.0`, `TanPi(k+0.5)=±Inf`; quarter-turn kernels `SinForIntervalPiBy4`/`CosForIntervalPiBy4` |
| **033 format** | `N2 → "1234.50"` (no grouping), `E2 → "1.25E+00"` (2-digit exp), `G/G9/G17` via `ostringstream setprecision` (not shortest-round-trip) | `N`: culture group sizes + 2 decimals (`Number.Formatting.Common.cs:884-909`, group insert :745-813); `E`: sign + **≥3 exponent digits** e.g. `1.25E+000` (`FormatExponent(...,3,true)` :911-955); `G`/`R`/parameterless: **shortest round-trippable**; `G17`(double)/`G9`(float)/`G5`(Half) = round-trip widths (`Double.cs:2344`, `Single.cs:2260`, `Half.cs:2374`) |
| **033 parse** | `" 1.5 "`, `"1,234.5"`, `"1e999"` all fail; only exact `NaN`/`Infinity` tokens; overflow→failure | default `NumberStyles.Float\|AllowThousands` (`Double.cs:388`): trims outer whitespace, accepts thousands; magnitude overflow → **±Infinity, no throw** (`Number.Parsing.cs:1482-1503`); NaN/±Infinity tokens (`:1343-1424`) |

---

## 5. Shared root causes

1. **"Native primitive == public contract" fallacy.** Every finding is the same
   mistake: a C++ standard-library primitive (`std::pow`+`nearbyint`, a
   trailing-mask bit test, `std::ilogb`, `std::sin(x*Pi)`, `from_chars`/`ostream`)
   is exposed *as* the .NET method without the type-specific guard/classification/
   reduction/grammar .NET wraps around it. The repair is per-method .NET-shaped
   logic, not a different primitive.
2. **Duplication across Single/Double (and Math/Half).** Each defect recurs
   verbatim at two-to-four sites with only the width changed. Repairs must land the
   two/three/four sites **together** or the family reopens at the untouched width.
3. **`noexcept` painted onto methods with a validating .NET contract** (SR-AUD-029)
   — the same trap as SR-AUD-043b/#1854: the qualifier, not the logic, is the
   blocker.

---

## 6. Formatting backend inventory

- **Shortest round-trip:** `std::to_chars` (used by `ToString(value)` and the
  `R`/`r` specifier). This is the only current path that is .NET-faithful for the
  default/`R`/`G` shortest forms.
- **Fixed/scientific/general with precision:** `std::ostringstream` +
  `std::fixed`/`std::scientific`/`std::setprecision`/`std::uppercase`, imbued with
  `std::locale::classic()`. This backend cannot, without post-processing, (a) emit
  a 3-digit exponent (libstdc++ emits 2), (b) insert group separators for `N`, or
  (c) guarantee shortest-round-trip `G`. A .NET-faithful `E`/`N`/`G` needs either a
  custom digit formatter or post-processing of the stream output.
- **Precision parse:** guarded `std::stoi` (→`FormatException` on malformed,
  already hardened by #1849/CCF-006).
- **Parse backend:** `SharpRuntime::FromCharsFloat` (portable `std::from_chars`
  wrapper) + explicit `NaN`/`Infinity` token handling + a whole-string-consumed +
  `std::isfinite` acceptance gate.

---

## 7. Culture / provider dependencies

sharp-runtime is **invariant-culture only** (no `IFormatProvider`/`NumberStyles`
surface on Single/Double; `Half`'s provider overloads ignore the provider — a
documented deviation). CCF-007 therefore targets **invariant-culture .NET
semantics only**: `.` decimal separator, `,` group separator, group size 3,
`NaN`/`Infinity`/`-Infinity` symbols, `+`/`-` signs. Any group-separator work
shares the exact decision surface as the Decimal `,`-as-group-separator question
(#1858): adopting `,` grouping in float **parse** is a value-affecting change and
must be gated the same way.

---

## 8. Round-trip guarantees

- **Preserve:** `ToString(value)`, `"R"`, and `"G"`-with-no-precision must remain
  shortest-round-trippable (they already are via `to_chars`). Any `G`/`E`/`N`
  rework must not regress the `to_chars` default path.
- **Target:** `"G9"`(float)/`"G17"`(double) must round-trip exactly (they are the
  `MaxRoundTripDigits` widths). Parse∘Format round-trips for finite values must
  hold across the reworked format specifiers.

---

## 9. Compatibility / approval matrix

| Ticket | Finding | Class | Autonomous? | Why |
|---|---|---|---|---|
| **CCF7-1** | SR-AUD-031 | compatible value fix; no signature/`noexcept`/layout change; matches the existing `MathF::ILogB` shape | **yes** | zero→INT_MIN, NaN/±Inf→INT_MAX, else `std::ilogb`; add-only tests |
| **CCF7-2** | SR-AUD-030 | compatible value fix; subnormal branch only; no signature/layout change | **yes** | `biasedExp==0 ⇒ popcount(sig)==1`; add-only tests |
| **CCF7-3** | SR-AUD-032 | compatible value fix (larger, algorithmic port); no signature change | **yes** (medium) | integral/fractional reduction with exact turn-boundary literals + signed zeros |
| **CCF7-4** | SR-AUD-029 | **exception-spec change** (`noexcept`→throwing) OR clamp | **design-first / approval** | parity requires dropping `noexcept` to throw `ArgumentOutOfRangeException`; parallels #1854 |
| **CCF7-5** | SR-AUD-033 format | **observable output change** toward parity (`E` 3-digit, `N` grouping, `G`/`G9`/`G17` round-trip) | **design-first** (observable) | changes existing `ToString` text; needs a format-backend decision + explicit before/after |
| **CCF7-6** | SR-AUD-033 parse | **mixed**: whitespace = pure widening; thousands = value-adjacent (shares #1858's comma decision); overflow→±Infinity = semantic (throw→value) | **split**: whitespace autonomous; thousands + overflow-to-infinity approval | landing whitespace is safe; the other two change accepted values/behaviour |

---

## 10. Dependency order

All five findings are **independent** (no ordering constraint between them). The
recommended landing order is by risk/value, cheapest-and-safest first:

```
1. CCF7-1  SR-AUD-031 ILogB           (Single+Double+Math)  compatible, small
2. CCF7-2  SR-AUD-030 IsPow2          (Single+Double)       compatible, small
3. CCF7-3  SR-AUD-032 Pi-trig         (Single+Double)       compatible, medium
4. CCF7-5  SR-AUD-033 format          (Single+Double)       observable → design-first
5. CCF7-6  SR-AUD-033 parse           (Single+Double)       split; whitespace now, rest blocked
6. CCF7-4  SR-AUD-029 Round validation(Single+Double)       approval-blocked (noexcept)
```

CCF7-1 and CCF7-2 are the "ready" pair (this batch lands them as #1859/#1860).
CCF7-3 is ready but larger and left for a follow-up. CCF7-4/5/6 carry the
approval/observability weight.

---

## 11. Bounded implementation tickets

| Ticket | Scope | Status |
|---|---|---|
| **#1859** (CCF7-1) | `Single::ILogB`, `Double::ILogB`, `Math::ILogB`: classify zero→`INT_MIN`, NaN/±Inf→`INT_MAX` before `std::ilogb`; keep `noexcept`. Add-only tests (zero, ±Inf, NaN, smallest subnormal, normal). | ready → **landed this batch** |
| **#1860** (CCF7-2) | `Single::IsPow2`, `Double::IsPow2`: add the subnormal branch (`biasedExp==0 ⇒ popcount(trailingSignificand)==1`); keep the zero/negative/NaN/Inf rejection and normal-value rule. Add-only tests (`Epsilon`, `2·Epsilon`, a 2-bit subnormal control, a normal pow2, a non-pow2). | ready → **landed this batch** |
| **#1861** (CCF7-3) | `Single`/`Double` `SinPi`,`CosPi`,`TanPi`,`SinCosPi`: port .NET's integral/fractional reduction; exact signed-zero at integer turns, `±1` at half turns (Sin), exact `0` at half turns (Cos), `±Inf` at half turns (Tan). Bit/sign-aware tests. | ready (medium), next batch |
| **#1862** (CCF7-4) | `Single`/`Double` `Round(x,digits)`: reject `digits` outside 0–6 / 0–15 with `ArgumentOutOfRangeException("digits", …)`. **Requires dropping `noexcept`** (option A, full parity) or clamping (option B). | **needs_user** (approval) |
| **#1863** (CCF7-5) | `Single`/`Double` `ToString(value,format)`: `"E"/"e"` → sign + ≥3 exponent digits; `"N"/"n"` → group-separated + 2 decimals; `"G"/"G9"/"G17"/"R"` → shortest-round-trip / round-trip widths. Observable output change; design-first. | **needs_user** (observable output) |
| **#1864** (CCF7-6) | `Single`/`Double` parse: **compatible** — skip leading/trailing whitespace. | **done** — whitespace slice landed (trim a `std::string_view` in `tryParseCore`; interior/empty still fail; +8 tests) |
| **#1865** (CCF7-6 tail) | `Single`/`Double` parse: **blocked** — accept `,` thousands (shares #1858's comma decision) and return `±Infinity` on magnitude overflow instead of failing (throw→value). Split from #1864, mirroring #1857→#1858. | **needs_user** (approval) |

---

## 12. Permanent test vectors (add-only — no existing assertion weakened)

| Ticket | Must add |
|---|---|
| #1859 | `ILogB(±0)==INT_MIN`; `ILogB(+Inf)==ILogB(-Inf)==INT_MAX`; `ILogB(NaN)==INT_MAX`; `ILogB(smallest subnormal)` == the IEEE exponent; `ILogB(8)==3` still holds — for Single, Double, and Math |
| #1860 | `IsPow2(Epsilon)==true`, `IsPow2(2·Epsilon)==true`, a two-bit subnormal `==false`, a normal pow2 `==true`, `1.5f/1.5 ==false`, `0/-x/NaN/Inf ==false` — Single + Double |
| #1861 | `std::signbit`-aware: `SinPi(1)==+0`, `SinPi(-1)==-0`, `SinPi(0.5)==1`, `CosPi(0.5)==0`, `TanPi(1)==-0` (see correction below), `TanPi(-1)==+0`, `TanPi(0.5)==+Inf`, both `SinCosPi(1)` fields — Single + Double |

> **Correction (ticket #1861, 2026-07-30).** The row above and the ticket
> acceptance text originally read `TanPi(1)==+0`. That was a design-from-summary
> error: the actual .NET `Single.TanPi`/`Double.TanPi` return
> `sign * (int.IsOddInteger(integral) ? -0.0 : +0.0)` at integer turns
> (`Single.cs:2125`, `Double.cs:2209`). For `x==+1` the integral part `1` is odd
> and `sign==+1`, so the result is **`-0.0`**, not `+0.0`; `TanPi(-1)` is `+0.0`.
> The implemented tests assert the measured .NET values (`TanPi(+1)==-0`,
> `TanPi(-1)==+0`), verified with a runtime-operand probe
> (`build-probe/1861_pitrig_probe.cpp`). Historical text is preserved; this note
> records the actual reference behaviour per the repository's premise-correction
> practice.
| #1862 | `Round(x,-1)`/`Round(x,7f)`/`Round(x,16d)` throw `ArgumentOutOfRangeException("digits")` with the exact per-type message; `Round(x,0)`/`Round(x,6f)`/`Round(x,15d)` valid |
| #1863 | exact `E2=="1.25E+000"`, `N2=="1,234.50"`, `G9`/`G17` round-trip; `R` unchanged |
| #1864 | `Parse(" 1.5 ")==1.5`; (blocked) `Parse("1,234.5")==1234.5`, `Parse("1e999")==+Inf` |

---

## 13. Differential / reference testing strategy

Each vector's expected value is taken from the cited .NET reference source
(§3/§4), not from memory. Where a boundary is sign-sensitive (Pi-trig zeros,
`-Infinity`), assert with `std::signbit`/raw bits, never `==` on a zero. For the
round-trip formats, assert `Parse(ToString(x,"G17"))==x` for a spread of finite
doubles (and `G9` for floats) rather than pinning brittle digit strings where the
shortest form is what matters.

---

## 14. Sanitizer relevance

None of the five is a memory-safety defect; they are value/exception-contract
defects. The only arithmetic added is: integer classification (ILogB), a
`popcount` on an unsigned significand (IsPow2), and IEEE reductions (Pi-trig) — no
signed overflow, no OOB, no float-cast-to-integer UB. **A quick ASan+UBSan run of
the touched Core.Base suites is sufficient** (as done for the Decimal slice);
`float-cast-overflow` is not implicated (no float→int narrowing is introduced).
The Pi-trig ticket (#1861) should additionally run under UBSan because it casts
`(long)ax` — guarded by the `ax < 2^52`/`2^63` thresholds, but worth confirming.

---

## 15. Performance strategy

ILogB/IsPow2 add one-to-three branches on the cold non-finite/subnormal paths;
the normal-value fast path is unchanged. Pi-trig replaces one multiply+libm call
with a reduction plus a quarter-interval kernel — comparable to .NET, and the libm
call is avoided at exact boundaries. Format/parse rework must keep the `to_chars`
fast path for the default/`R`/`G` forms and only add work on the `E`/`N`/`G<n>`
branches.

---

## 16. Explicit exclusions

- **`Half`** has none of `Round`/`IsPow2`/`ILogB`/Pi-trig (deliberate MathF-surface
  omission), so it is out of scope for CCF7-1..4; its `ToString`/`TryFormat`
  delegate to `Single`, so it inherits any CCF7-5 format fix automatically.
- **The `std::nearbyint` ambient-rounding-mode issue** in `Round(x)`/`Round(x,digits)`
  (SR-AUD-040 class, not a CCF-007 finding) — noted, not scoped here.
- **`Double::TryParse` missing `noexcept`** — a free tightening, not a CCF-007
  finding; note only.
- **`NumberStyles`/`IFormatProvider` culture surface** — permanent out-of-scope
  deviation; CCF-007 is invariant-culture only.
- **Generic-math `CreateChecked`/etc.** — out of scope project-wide.

---

## 17. Completion criteria

CCF-007 closes when SR-AUD-029, 030, 031, 032, 033 are all `remediated`, each with
a fix matching the cited .NET reference, add-only tests, and
`scripts/local_ci_check.sh build` green with no test-count regression and Doxygen
inside its ceiling. The approval-gated pieces (CCF7-4 `noexcept`; CCF7-5 observable
format output; CCF7-6 thousands + overflow→Infinity) each need an explicit user
decision recorded on their blocked tickets before landing; the compatible pieces
(CCF7-1/2/3 and CCF7-6 whitespace) do not.

---

## 18. Implementation status

**CCF7-1 — SR-AUD-031 — DONE (#1859, 2026-07-30).** See #1859.

**CCF7-2 — SR-AUD-030 — DONE (#1860, 2026-07-30).** See #1860.

**CCF7-3 — SR-AUD-032 — DONE (#1861, 2026-07-30).** `Single`/`Double`
`SinPi`/`CosPi`/`TanPi`/`SinCosPi` were rewritten from the naive
`std::sin(x*Pi)` forms to the .NET integral/fractional-turn reduction (based on
`sinpi(f)`/`cospi(f)`/`tanpi(f)` from amd/aocl-libm-ose, ported verbatim
including the interval kernels `SinForIntervalPiBy4`/`CosForIntervalPiBy4`/
`TanForIntervalPiBy4`; the Double kernels keep the `xTail` parameter of the
reference and are always called with `0.0`). Integer turns now return a
sign-carried zero, half turns return exact `±1`/`0`/`±Infinity`, non-finite
inputs return `NaN`, and ordinary fractional values stay within a small ULP
tolerance of libm. `noexcept`, `[[nodiscard]]`, public signatures, and object
layout are unchanged (both types are header-only). 24 add-only tests (10 Single
+ 14 Double, of which 4 pre-existed); UBSan + ASan + `float-cast-overflow` clean
on `build-probe/1861_pitrig_probe.cpp`. **Premise corrected:** the §12 vector
`TanPi(1)==+0` was wrong; the reference (and this implementation) return `-0.0`
— see the Correction note in §12.

**CCF7-6 (whitespace slice) — SR-AUD-033 parse — DONE (#1864, 2026-07-30).**
`Single`/`Double` `tryParseCore` now trims leading/trailing ASCII whitespace
(space, tab, LF, VT, FF, CR) via a non-allocating `std::string_view` before the
NaN/Infinity token checks and `FromCharsFloat`, so `Parse(" 1.5 ")`, `" NaN "`,
`"\t-Infinity\t"` succeed while interior whitespace and an empty/all-whitespace
string still fail. `equalsIgnoreCaseAscii` was widened to take `std::string_view`;
`noexcept`/signatures/layout unchanged. +8 add-only tests (4 Single + 4 Double).
The approval-gated tail (accept `,` thousands + overflow→`±Infinity`) was split to
**needs_user #1865** (mirroring #1857→#1858); SR-AUD-033 stays `confirmed`
(partial) until both #1863 format and #1865 parse-tail land.

**CCF7-4 — SR-AUD-029 — DONE (#1862, 2026-07-31).** Approved by the batch
instruction in the exact words of `docs/RemainingApprovalDecisions.md` §A.10,
option **(A)**: `Single::Round(float,intcs)` and `Double::Round(double,intcs)`
dropped `noexcept` and reject `digits` outside `[0,6]` / `[0,15]` **before** the
`std::pow`, throwing `ArgumentOutOfRangeException("digits", …)` with .NET's
resource strings verbatim (`ArgumentOutOfRange_RoundingDigits_MathF` and
`ArgumentOutOfRange_RoundingDigits`). Option (B) (clamp) was not taken; #1854 was
decided identically in the same batch, so §19.3's "decide together" requirement
is satisfied. Measured (`build-probe/1854_{prefix,postfix}_plain.log`):
`Round(1.2345f,99)` NaN → throw, `Round(1.2345f,7)` a spurious value → throw,
`Round(1.2345f,-1)` `0` → throw, `Round(1.2345,16)`/`(…,99)` silently ignored →
throw, `Round(…,INTCS_MIN)`/`(…,INTCS_MAX)` → throw. Valid `0`/`2`/`6` (float)
and `0`/`2`/`15` (double) and both one-argument overloads are **byte-identical**.
+13 tests, including `static_assert`s pinning that the two-argument overload is
no longer `noexcept` and the one-argument overload still is. No parameter list,
return type, layout, vtable or mangled name change; no exported-symbol break.
`SR-AUD-029 → remediated`.

**Two premises corrected by measurement, and deliberately left unrepaired.**

1. **`Single::Round`/`Double::Round` do not delegate the way .NET's do.** .NET's
   `Single.Round(x,d)` is `=> MathF.Round(x, d)` and `Double.Round(x,d)` is
   `=> Math.Round(x, d)` (`Single.cs:683`, `Double.cs:688`); this port
   re-implements the scale/round/divide inline in each type, so it does **not**
   inherit `MathF::Round`'s `|x| >= 1e8` / `Math::Round`'s `|x| >= 1e16`
   short-circuit. Measured 2026-07-31: `Single::Round(3.0e38f, 6)` returns
   **`inf`** where `MathF::Round(3.0e38f, 6)` returns `3.0e38f`, and
   `Double::Round(1e300, 15)` returns **`inf`** where `Math::Round(1e300, 15)`
   returns `1e300` (probe cases A30–A33). This is a *value* change on
   currently-valid input, so it is **outside #1862's approval**, which covers
   invalid-argument rejection only. Filed as inactive ticket **#1927**.
2. **`Math::Round`'s message is not .NET's.** `Math.hpp`'s
   `Round(double,intcs,MidpointRounding)` throws
   `"digits must be between 0 and 15, inclusive."`, missing .NET's leading
   `"Rounding "` (probe case A35); `MathF.hpp`'s float counterpart is already
   correct (A34). #1862 gave `Double::Round` .NET's exact string, which makes the
   two spellings differ inside this repository until `Math::Round` is corrected.
   Changing a pinned message on a different type is outside this approval. Filed
   as inactive ticket **#1928**.

Neither carries an `SR-AUD-*` identifier; audit numbering stays frozen at **364**.

CCF7-5/#1863 (format output) remains approval-blocked, per §11.

**CCF-007 family status:** SR-AUD-030, SR-AUD-031, SR-AUD-032 remediated; the
compatible parse-whitespace slice of SR-AUD-033 landing in #1864;
SR-AUD-029 (#1862), the SR-AUD-033 format slice (#1863), and the SR-AUD-033
thousands+overflow parse tail (#1865) all remain approval-gated. The family closes
only when all five findings are `remediated` (§17).

*Revised 2026-07-31:* **SR-AUD-029 is now `remediated` (#1862).** The family's
remaining findings are the SR-AUD-033 format slice (#1863) and its parse tail
(#1865), both carried by the approved Groups B and D of this batch.

---

## 19. Approval decision records (refined 2026-07-30, ticket-planning pass)

These are the durable, reference-exact decision records for the three CCF-007
approval-gated tickets, plus the reconciliation with the sibling noexcept finding
#1854. **No implementation is authorised by this section**; each requires the
explicit per-action user decision named below.

### 19.1 CCF7-4 / #1862 — `Round(x,digits)` precision validation (SR-AUD-029) — **APPROVED AND DONE (2026-07-31, option A); see §18**

**Current state (measured):** `Single::Round(float x, intcs digits) noexcept`
(`Single.hpp:246`) and `Double::Round(double x, intcs digits) noexcept`
(`Double.hpp:291`) — both `[[nodiscard]] static`, header-only inline, computing
`pow(10,digits)` with **no** range check. Probe: `Round(1.2345f,-1)=0`,
`Round(1.2345f,7)=1.234500051`, `Round(1.2345f,100)=NaN`.

**.NET reference (exact):** `Single.Round(x,d)`→`MathF.Round(x,d)`→
`MathF.Round(x,d,ToEven)`; `Double.Round(v,d)`→`Math.Round(v,d,ToEven)`. The
3-arg funnel validates first: `if ((uint)digits > maxRoundingDigits) Throw…` with
`maxRoundingDigits = 6` (MathF) / `15` (Math) (`MathF.cs:389/415`,
`Math.cs:1366/1407`). Because the guard casts to `uint`, **negative** digits also
throw. The exception is `ArgumentOutOfRangeException`, `paramName = "digits"`,
messages (`Strings.resx:1928/1931`, `ThrowHelper.cs:246/252`):
- float: `"Rounding digits must be between 0 and 6, inclusive."`
- double: `"Rounding digits must be between 0 and 15, inclusive."`
`ArgumentOutOfRangeException.HResult` is `COR_E_ARGUMENTOUTOFRANGE` (0x80131502)
in .NET; sharp-runtime's `ArgumentOutOfRangeException(paramName, message)` ctor
(`ArgumentOutOfRangeException.hpp:57`) is the vehicle (it does not currently model
that HResult — record but do not block on it).

**Options:**
- **(A) Full parity — drop `noexcept`, throw.** Reject `digits<0 || digits>6/15`
  with `ArgumentOutOfRangeException("digits", <per-type message>)` **before** the
  `pow`. Distinct 0–6 (float) / 0–15 (double) limits retained.
- **(B) Compatible — keep `noexcept`, clamp.** Clamp `digits` to `[0,6]`/`[0,15]`
  and round at the limit. No exception; changes the *result* for out-of-range
  digits (spurious value/NaN → clamped value) but not the exception spec.

**Why (A) is approval-gated:** removing `noexcept` from a public method is an
exception-spec change outside the compatible-narrowing envelope. **ABI:** for a
non-template static member function the mangled symbol does **not** encode
`noexcept`, and both types are header-only, so there is **no exported-symbol/ABI
break**; the impact is *source-level* — `noexcept(Single::Round(x,d))` flips
`true→false`, and any `&Round` taken as a `noexcept`-typed function pointer stops
compiling. No object-layout or parameter-list change under either option.

**SR-AUD-040 boundary (do not conflate):** `Round(x)`/`Round(x,digits)` use
`std::nearbyint`, which honours the *ambient FP rounding mode* — a separate defect
class (the one fixed for `MathF::Round(ToEven)` by #1723), **not** SR-AUD-029 and
**not** in CCF-007. It should be its own finding/ticket family; #1862 must not
absorb it.

**Test matrix (add-only, when approved):** `Round(x,-1)`, `Round(x,7)` (float) /
`Round(x,16)` (double), `Round(x,100)` → option (A): throw
`ArgumentOutOfRangeException` with `paramName=="digits"` and the exact per-type
message; option (B): equal the clamped-at-limit value. `Round(x,0)`,
`Round(x,6)` (float) / `Round(x,15)` (double) remain valid and unchanged.

### 19.2 CCF7-5 / #1863 — `ToString(value,format)` output (SR-AUD-033 format)

**Current vs .NET (exact before/after), invariant culture:**

| Spec | Current sharp-runtime | .NET target |
|---|---|---|
| `E2` of `1.25` | `"1.25E+00"` (libstdc++ 2-digit exp) | `"1.25E+000"` (sign + ≥3 exp digits) |
| `N2` of `1234.5` | `"1234.50"` (no grouping) | `"1,234.50"` (group size 3 + 2 decimals) |
| `G9`(float)/`G17`(double) | `ostringstream setprecision` (not guaranteed shortest/round-trip) | exact round-trip width |
| default / `R` / `G`(no prec) | `to_chars` shortest round-trip ✓ | shortest round-trip ✓ (already matches — must not regress) |

**Backend decision (required):** the current `E`/`N`/`G<n>` path is
`std::ostringstream` + `std::fixed`/`std::scientific`/`setprecision`, which
**cannot** emit a 3-digit exponent (libstdc++ emits 2), insert `N` group
separators, or guarantee shortest-`G` without post-processing. Landing parity
requires either (i) post-processing the stream output (pad the exponent to ≥3
digits; insert `,` every 3 integer digits) or (ii) a custom digit formatter. The
`to_chars` fast path for default/`R`/shortest-`G` **must be preserved**.

**Why approval-gated:** this changes existing **observable `ToString` text**.
**Compatibility impact:** any serialized text, golden files, or downstream string
comparisons that captured the current `"1.25E+00"`/`"1234.50"` forms would change
(`E` gains an exponent digit; `N` gains separators). **ABI:** none (inline body,
no signature change). `Half` inherits the fix automatically (its `ToString`
delegates to `Single`).

**Test matrix (add-only, when approved):** exact `E2=="1.25E+000"`,
`N2=="1,234.50"`; `Parse(ToString(x,"G17"))==x` over a spread of finite doubles
and `G9` for floats; `R`/default text unchanged (regression guard on the
`to_chars` path).

### 19.3 #1854 (SR-AUD-043b) reconciliation with #1862

**#1854 is NOT a duplicate of, nor a prerequisite of, #1862.** They are
**independent findings on different types** — #1854 covers `ReadOnlyMemory<T>`
constructors and `HashCode::AddBytes` (negative-length rejection); #1862 covers
`Single`/`Double` `Round(x,digits)`. Neither's code touches the other's.

They **do** share one decision shape: *"an invalid-argument check is blocked by a
`noexcept`/`constexpr` qualifier — drop it to throw
`ArgumentOutOfRangeException` (full parity, option A) or keep it and clamp/degrade
(compatible, option B)."* Both are gated for the **same** reason (exception-spec
change), and both are header-only with **no ABI-symbol break** (only the
`noexcept` source-trait changes). They should be **decided together** so the
project adopts one convention rather than splitting A/B across siblings.

**Contrast — why #1855 (CCF-008) landed autonomously:** the already-remediated
`Decimal`/`Math`/`MathF` `Round(…,mode)` overloads were **not** `noexcept`
(verified 2026-07-30: `MathF.hpp:229`, `Math.hpp`, `Decimal.hpp:545` carry no
`noexcept`), so validating an invalid `MidpointRounding` by throwing needed no
exception-spec change and was inside the autonomous envelope. The distinguishing
factor for #1854 and #1862 is precisely the `noexcept` qualifier that must be
**removed**. No new ticket is created by this reconciliation; #1854 stays
`needs_user`, #1862 stays `needs_user`, cross-referenced here and in
`docs/ConversionBoundaryFamilyPlan.md`.

### 19.4 #1858 / #1865 comma decision (shared)

The Decimal parser comma tail (**#1858**, `docs/DecimalBoundaryFamilyPlan.md`
§3/§7/§12) and the float parser thousands tail (**#1865**, §19-adjacent, plan
§9/§11) both hinge on the same choice: adopt invariant-culture `,`-as-group
semantics or keep the current behaviour as an accepted deviation. **They differ
in blast radius:** for Decimal, `Parse("1,5")` today returns `1.5m` and would
become `15m` — a *silent value change* of an already-accepted input; for
`Single`/`Double`, `,` is today *rejected* (`Parse("1,234.5")` fails), so adoption
is a rejected→accepted widening with **no** existing accepted value changing. Both
must be resolved consistently; neither is authorised here.
