<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Decimal boundary family — CCF-005 Decimal slice plan

*Authored 2026-07-30 by the batch on branch
`feature/remediation-batch-ccf005-convert-decimal`, immediately after the CCF-005
memory-safety slice landed (#1851 SR-AUD-041, #1852 SR-AUD-043a, #1853
SR-AUD-026/027; #1850 SR-AUD-047 earlier). It is the durable, evidence-based plan
for the **Decimal slice of CCF-005**, which
[`docs/ConversionBoundaryFamilyPlan.md` §17](ConversionBoundaryFamilyPlan.md)
deliberately carved out as "Decimal-specific, medium severity, larger; a separate
review." Three findings: SR-AUD-035 (parser), SR-AUD-036 (`MidpointRounding`
validation, == the whole of CCF-008), SR-AUD-038 (negative-zero erasure).*

*Current-state verification was performed 2026-07-30 by three parallel read-only
agents over the production sources (`modules/core/include/System/Decimal.hpp`,
`modules/core/src/System/Decimal.cpp`, `Math.hpp`, `MathF.hpp`), the per-file
audit reports, and the .NET reference (`/rv/tmp/runtime/src/libraries/…`,
`Decimal.cs`/`Decimal.DecCalc.cs`/`Number.Parsing.cs`/`Math.cs`/`MathF.cs`).
**All three findings still reproduce in the current tree; none is remediated.***

**This document creates no `SR-AUD-*` identifier** (audit numbering is frozen at
364). It maps the three existing findings to files/lines, verifies premises,
groups the work into bounded dependency-ordered tickets, and draws the
implementation-vs-approval boundary.

---

## 1. Decimal representation and public-surface inventory

### Internal representation (`Decimal.hpp:48-53`)

```cpp
using u128 = unsigned __int128;
static constexpr u128 MAX_MANTISSA = (u128(1) << 96) - 1;  // 79228162514264337593543950335
u128    mantissa_ = 0;   // 96-bit magnitude
uint8_t scale_    = 0;   // 0..28 (base-10 exponent)
bool    negative_ = false;
```

The sign is a **standalone `bool`**, orthogonal to the mantissa — so **negative
zero (`mantissa_==0, negative_==true`) is fully representable** in the layout;
every erasure (§4) is runtime policy, not a layout limit. `sizeof(Decimal) == 32`
(the `__int128` forces 16-byte alignment; 2 payload bytes pad to 32). This is a
deviation from .NET's 16-byte `decimal` — **out of scope here**, but any future
ABI/layout ticket must note it.

### Public constructors (`Decimal.hpp`, defs in `Decimal.cpp`)

| Ctor | Line | Throws | Note |
|---|---|---|---|
| `Decimal()` | hpp:77 | — | zero |
| `Decimal(intcs)` / `(long long)` implicit | hpp:84/91 | — | exact |
| `Decimal(long)` / `(uintcs)` / `(ulongcs)` | hpp:96/120/127 | — | exact |
| `Decimal(double)` / `(float)` explicit | hpp:105/113 | `OverflowException` (NaN/Inf/too-large) | lossy |
| `Decimal(intcs lo, intcs mid, intcs hi, bool isNegative, bytecs scale)` | hpp:141 / cpp:155 | `ArgumentOutOfRangeException` if `scale>28` | raw parts — **SR-AUD-038 site** |
| (private) `Decimal(u128, uint8_t, bool)` | hpp:69 / cpp:108 | — | internal |

### Constants (`Decimal.hpp:148-160`, defs `cpp:167-171`)

`Zero`, `One`, `MinusOne`; `MaxValue`/`MinValue` are **runtime-initialised via
`Parse("±79228162514264337593543950335")`** (no `constexpr` ctor).

### Conversions

- **From integers**: exact, never throw (`cpp:115-153`).
- **To integers** (`ToInt32` cpp:184 … `ToUInt16` cpp:266): truncate fraction,
  **throw `OverflowException`** out of range.
- **To float/double** (`ToDouble` cpp:177, `ToSingle` cpp:182): lossy, never throw.
- Explicit `operator` conversions (hpp:396-414) delegate to the `To*` methods.

### Scaling / arithmetic / sign

- `normalize()` (cpp:99-102) strips trailing zero digits and **zeroes the sign of
  a zero mantissa** (cpp:101) — SR-AUD-038-adjacent.
- `operator+/-/*//`, `Truncate` (cpp:451), `Round` (cpp:466), `operator-()`
  unary (cpp:399).
- Sign members: `getIsNegativeProperty` (hpp:180, `noexcept`), `IsNegative`
  (hpp:655, `noexcept`), `IsPositive` (hpp:662, `noexcept`), `Sign` (hpp:627,
  zero-agnostic), `CopySign` (hpp:638) — **SR-AUD-038 site**. There is **no**
  `getSignProperty`.

### Bit representation (serialization)

`GetBits(const Decimal&, intcs& lo, intcs& mid, intcs& hi, intcs& flags)`
(hpp:328 / cpp:278). `flags = (scale_ << 16) | (negative_ ? 0x80000000u : 0)` —
scale in bits 16-23, **sign in bit 31**. Valid scale 0..28 (enforced only in the
raw ctor).

### `noexcept` / `constexpr` (exception-spec / layout sensitive)

`noexcept`: `getScaleProperty`, `getIsNegativeProperty`, `GetHashCode`,
`IsNegative`, `IsPositive`. `constexpr`: only the private `MAX_MANTISSA` and
file-scope `kPow10[29]`. **No public ctor is `constexpr`.** `Parse`, `TryParse`,
arithmetic, comparisons, `ToString` are **not** `noexcept` — so adding a throw to
any of them is not an exception-spec change.

---

## 2. Finding-by-finding current-state verification (measured 2026-07-30)

| Finding | Sev | Measured current behaviour | .NET reference | Reproduces? |
|---|---|---|---|---|
| **SR-AUD-035** | medium | `TryParse` (cpp:289) rejects leading/trailing whitespace and treats `,` as a *decimal point* (so `" 1,234.5 "` and `"1,234.5"` are rejected); `Parse` (cpp:321) throws `FormatException` for the out-of-range `…950336` (overflow ≠ format); excess fractional digits past scale 28 are `continue`-**discarded** not rounded, so `Parse("0.00000000000000000000000000006")==0` | `NumberStyles.Number` (default) allows leading/trailing white + thousands + sign + decimal point; overflow → **`OverflowException`** (distinct from `FormatException`); excess precision → **banker's rounding** (`6e-29` → `1e-28`) | **yes (all three)** |
| **SR-AUD-036** | medium | `Round(…, MidpointRounding)` funnels (Decimal cpp:466; Math hpp:540; MathF hpp:227) hit a `switch default` for an out-of-range enum → silently truncate (Decimal) or ties-to-even (Math/MathF) instead of throwing | `if ((uint)mode > (uint)ToPositiveInfinity) throw new ArgumentException(SR.Argument_InvalidEnumValue…, nameof(mode))` | **yes** |
| **SR-AUD-038** | medium | Raw ctor (cpp:160 `negative_ = isNegative && mantissa_ != 0`), `CopySign` (hpp:640), parser (cpp:317), `normalize` (cpp:101) and unary `operator-` (cpp:400) all **erase the sign of a zero mantissa**, so `GetBits(Decimal(0,0,0,true,0)).flags == 0` where .NET reports `0x80000000` | raw ctor sets `SignMask` **unconditionally** (`Decimal.cs:315`); `GetBits`, `CopySign`, unary `-` all preserve it; yet `-0m == 0m` and hashes equal | **yes** |

---

## 3. Premise corrections (measured vs. audit / plan text)

Preserve the audit narrative; append these.

1. **SR-AUD-035 sub-defect 2 (overflow taxonomy) needs an internal status
   channel, not just a message swap.** `TryParse` returns a single `bool` and
   cannot distinguish overflow from malformed input; the fix needs a private
   status-returning helper so `Parse` can throw `OverflowException` for overflow
   and `FormatException` for malformed. This is an **observable exception-type
   change** (`FormatException` → `OverflowException` for an overflow input), a
   compatible narrowing toward .NET but a semantic change — track it explicitly.
2. **SR-AUD-035 sub-defect 1 hides a silent result change.** Today `,` parses as
   a decimal point, so `Parse("1,5") == 1.5m`. Making `,` a group separator
   (.NET invariant-culture semantics) makes `Parse("1,5") == 15m`. No test covers
   comma inputs, so nothing breaks, but this **silently changes the value** of an
   existing accepted input — call it out in the ticket; it is the one part of the
   parser work that is *not* a pure widening.
3. **SR-AUD-036 == the whole of CCF-008.** The cross-cutting CCF-008 membership is
   exactly SR-AUD-036's three occurrences (Decimal + Math + MathF); there is no
   fourth type and no extra defect. **Fixing SR-AUD-036 across all three closes
   CCF-008**; fixing only one type does not.
4. **SR-AUD-038's feared equality/hash ripple is already handled.** `operator==`
   (cpp:413) already returns `mantissa_==0 && o.mantissa_==0` when signs differ
   (so `-0==+0` is already true), and `GetHashCode` (hpp:223) already computes
   `neg = negative_ && m != 0` (so a zero hashes sign-agnostically). Preserving
   negative zero in the raw ctor/`CopySign` therefore changes **only** what
   `GetBits`/`getIsNegativeProperty`/`IsNegative` report for an explicitly-built
   negative zero — exactly what the audit asks — with no equality/hash fallout.

---

## 4. SR-AUD-038 erasure sites and scoping

| Site | Line | Removing the guard makes… |
|---|---|---|
| Raw ctor | cpp:160 | `Decimal(0,0,0,true,0)` observably negative via `GetBits`/`IsNegative` — **the audit's exact ask** |
| `CopySign` | hpp:640 | `CopySign(0m, -1m)` a negative zero — **the audit's exact ask** |
| Parser | cpp:317 | `Parse("-0")` a negative zero — .NET's `Parse("-0")` is `-0m` (extends the fix consistently) |
| `normalize()` | cpp:101 | arithmetic *results* keep a sign on a canceled-to-zero magnitude — **broader** |
| unary `operator-()` | cpp:400 | `-Decimal(0)` a negative zero — **broader** |

**Recommended split:** the **core ticket** removes the guard from the raw ctor +
`CopySign` (+ the parser `"-0"` case for consistency). Whether arithmetic should
*produce* negative zeros (`normalize`/unary `-`) is a **broader semantic decision**
scoped as a follow-up note, not folded into the core.

---

## 5. Dependency graph

```
SR-AUD-036 (MidpointRounding, Decimal+Math+MathF)   independent; closes CCF-008
SR-AUD-038 (negative zero, raw ctor + CopySign)     independent
SR-AUD-035 (parser: whitespace/grouping, overflow,  independent; internal helper;
            rounding)                                 comma-semantic + exception-type change
```

All three are independent (no ordering constraint). SR-AUD-035's three sub-defects
share one function (`TryParse`), so they land together, but its overflow-taxonomy
and comma-semantic pieces carry the approval weight.

---

## 6. Implementation vs. design-first split

| Ticket | Finding | Class | Autonomous? |
|---|---|---|---|
| **CCF5D-1** | SR-AUD-036 (+ CCF-008) | compatible: reject out-of-enum `MidpointRounding` with `ArgumentException`; add the check to the 3 funnel overloads; add-only tests; no `noexcept`/layout change | **yes** |
| **CCF5D-2** | SR-AUD-038 core | compatible: preserve the sign on a zero mantissa in the raw ctor + `CopySign` (+ parser `"-0"`); equality/hash already correct; add-only tests; no layout/`noexcept` change | **yes** |
| **CCF5D-3** | SR-AUD-035 | mixed: rounding (compatible numeric change) + whitespace (compatible widening) are autonomous, but the **`,`-as-group-separator silent value change** and the **`FormatException`→`OverflowException` exception-type change** are semantic — needs a bounded decision | **design-first / decision** |
| (note) | SR-AUD-038 broad | arithmetic *producing* negative zeros (`normalize`/unary `-`) | **deferred decision** |

CCF5D-1 and CCF5D-2 are implementation-ready with the CCF-004/CCF-003 class-C
precedent. CCF5D-3 should land its two clearly-compatible sub-defects and gate the
two semantic pieces (or land them together once the comma/overflow semantics are
explicitly accepted as .NET parity).

---

## 7. .NET exact behaviour and messages

- **SR-AUD-036:** `ArgumentException`, paramName `mode`, message
  `The value '{0}' is not valid for this usage of the type MidpointRounding.`
  (`Argument_InvalidEnumValue`). Valid enum range `[ToEven=0 .. ToPositiveInfinity=4]`.
- **SR-AUD-035:** malformed → `FormatException` (port already uses
  `"Input string was not in a correct format."`); overflow →
  `OverflowException("Value was either too large or too small for a Decimal.")`
  (`Overflow_Decimal`); excess fractional precision → banker's (ties-to-even)
  rounding.
- **SR-AUD-038:** raw ctor sets the sign bit unconditionally; `GetBits` returns
  `_flags` verbatim; `-0m == 0m` is `true` and both hash to 0.

---

## 8. Permanent test matrix (add-only — no existing assertion is weakened)

| Ticket | Must add |
|---|---|
| CCF5D-1 | `Decimal::Round`, `Math::Round`, `MathF::Round` (both the `(…,mode)` and `(…,digits,mode)` overloads) throw `ArgumentException` for `static_cast<MidpointRounding>(99)` and a negative cast; every named mode 0-4 still returns its exact value |
| CCF5D-2 | `Decimal(0,0,0,true,0)` and `CopySign(0m,-1m)` report the sign via `GetBits().flags==0x80000000` / `IsNegative()==true`; `-0m == 0m` still true; `(-0m).GetHashCode()==(0m).GetHashCode()`; positive `Decimal::Zero` still `flags==0`; `Parse("-0")` sign per the chosen contract |
| CCF5D-3 | `Parse(" 1,234.5 ")==1234.5m` (whitespace+grouping); `Parse("79228162514264337593543950336")` throws `OverflowException` (not `FormatException`); `Parse("0.00000000000000000000000000006")==1e-28` (rounding); the comma-semantic change asserted explicitly |

---

## 9. Sanitizer matrix

None of the three is a memory-safety or arithmetic-UB defect — they are
value/exception-contract defects. **No sanitizer reproduction is required**
(unlike the CCF-005 memory-safety slice). Correctness is proven by the add-only
value/exception tests plus `scripts/local_ci_check.sh build`.

---

## 10. Recommended ticket order

1. **CCF5D-1 — SR-AUD-036** (`MidpointRounding`, closes CCF-008) — cleanest, fully
   compatible, highest cross-cutting value; ideal first.
2. **CCF5D-2 — SR-AUD-038 core** (negative-zero raw ctor + `CopySign`) — compatible,
   ripples already handled.
3. **CCF5D-3 — SR-AUD-035** (parser) — land rounding + whitespace; gate/accept the
   comma-semantic and exception-type pieces explicitly.

---

## 11. Family completion criteria

The CCF-005 Decimal slice is complete when SR-AUD-035, 036, 038 are all
`remediated`, each with a fix matching the .NET reference, an add-only test, and
`scripts/local_ci_check.sh build` green with no test-count regression and Doxygen
inside 1,942. **CCF-008 closes with CCF5D-1** (all three `MidpointRounding`
overload families validated). The `normalize`/unary-`-` negative-zero *production*
question and the Decimal 16-vs-32-byte layout deviation are **not** part of this
completion criterion.

---

## 12. Implementation status

**CCF5D-1 — SR-AUD-036 — DONE (#1855, 2026-07-30).** All three
`Round(…, MidpointRounding)` funnels reject an out-of-range enum value with
`System::ArgumentException(paramName "mode")`, message
`The value '{n}' is not valid for this usage of the type MidpointRounding.`
Decimal (`Decimal.cpp`) validates `(uint)mode > (uint)ToPositiveInfinity` before
the scale-vs-decimals early-out; Math/MathF (`Math.hpp`/`MathF.hpp`) throw from
the `switch` default. **CCF-008 closed** (SR-AUD-036 was its sole member). Every
named mode 0-4 still returns its exact value. +9 add-only tests (3 per type:
invalid throws, paramName is `mode`, every named mode exact). Premise confirmed
against .NET: the `Round(value, digits, mode)` overloads validate the mode *only
through the funnel*, so Math ≥ 1e16 / MathF ≥ 1e8 magnitudes return unchanged
without validating the mode — this faithfully matches .NET's Math.cs/MathF.cs and
is intentional, not a gap; Decimal validates unconditionally.

**CCF5D-2 — SR-AUD-038 core — DONE (#1856, 2026-07-30).** The raw ctor
(`Decimal.cpp`: `negative_ = isNegative`), `CopySign` (`Decimal.hpp`:
`sign.negative_`), and `TryParse` (`Decimal.cpp`: `Decimal(mantissa, scale,
neg)`) all now preserve the sign of a zero magnitude, so `Decimal(0,0,0,true,0)`,
`CopySign(0m,-1m)` and `Parse("-0")` are negative zeros observable through
`GetBits().flags==0x80000000` / `IsNegative()==true`. As §3.4 predicted, the
equality/hash ripple needed no change — `operator==` already returns true when
both mantissas are zero regardless of sign, and `GetHashCode` already computes
`neg = negative_ && m != 0`, so `−0m == 0m` stays true and both hash equal.
Positive `Decimal::Zero` still reports `flags==0`. +6 add-only tests. The
`Parse("-0")` contract was chosen as **.NET parity (−0)**, confirmed against the
reference `NumberBufferKind.Decimal` path which does not clear `IsNegative` for a
zero value. `normalize()`/unary-`−` negative-zero *production* stays out of scope
(the deferred broader decision in §4/§6).

**CCF5D-3 — SR-AUD-035 — PARTIAL (#1857 landed, #1858 blocks the rest,
2026-07-30).** `TryParse` (`Decimal.cpp`) now lands the two clearly-compatible
sub-defects: (a) it skips leading/trailing whitespace (`std::isspace`, a pure
widening — no previously accepted input changes value), and (b) it rounds excess
fractional precision beyond scale 28 round-half-to-even instead of discarding it,
so `0.0…06` → `1e-28` (with a defensive scale-drop guarding the rare rounding
carry past the 96-bit mantissa). +4 add-only tests, two of which
(`Parse_CommaIsStillDecimalPoint_PendingApproval`,
`Parse_OverflowStillFormatException_PendingApproval`) pin the *current* behaviour
of the two approval-sensitive pieces. Those two pieces — **comma-as-group-
separator** (silently changes `Parse("1,5")` 1.5→15; §3 premise 2) and the
**`FormatException`→`OverflowException` overflow taxonomy** (needs the internal
status channel; §3 premise 1) — are split out to **blocked ticket #1858
(`needs_user`)**; each is an observable semantic change requiring an explicit
decision (adopt full .NET `NumberStyles.Number` semantics vs. keep the current
comma/overflow behaviour as an accepted deviation). SR-AUD-035 stays `confirmed`
until #1858 lands. The comma fix was **not** applied silently, per the batch
directive.
