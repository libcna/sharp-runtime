<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Numeric primitive-wrapper boundary family — namespace review and plan

*Authored 2026-07-30 by the batch on branch
`feature/remediation-batch-1804-namespace-review`, after ticket #1804 closed and
the CCF-004 defined-arithmetic family closed. This is the evidence-based
namespace review the handoff requested (`NEXT.md`: "Next recommended family
after CCF-004: CCF-005 or CCF-003"). It plans **CCF-003** — the numeric
primitive wrappers' boundary and formatting behaviour — and records where that
overlaps CCF-005/006/007/008 so a later batch does not re-plan the same files
from scratch.*

**This document creates no `SR-AUD-*` identifier** (audit numbering is frozen at
364). It maps existing findings to files, corrects premises where the current
tree already differs from the audit, groups the work into bounded tickets, and
records what was implemented in this batch versus deferred.

---

## 1. Namespace scope and file inventory

The family is the `System` **numeric primitive wrappers** and the three
free-function numeric helpers that share their boundary-validation shape. All
live in the `Core.Base` component (`modules/core`), which has no downstream
migration prerequisite for adding validation or a throw — the exception types it
needs (`FormatException`, `ArgumentException`, `ArgumentOutOfRangeException`,
`OverflowException`) are already in `Core.Base`.

| File | Kind | In this review's finding set |
|---|---|---|
| `modules/core/include/System/Byte.hpp` | integer wrapper | Clamp (022) |
| `modules/core/include/System/SByte.hpp` | integer wrapper | IsPositive (024), Clamp (022), format (021/023) |
| `modules/core/include/System/Int16.hpp` | integer wrapper | IsPositive (024), Clamp (022), format (021/023) |
| `modules/core/include/System/UInt16.hpp` | integer wrapper | Clamp (022), format (021/023) |
| `modules/core/include/System/Int32.hpp` | integer wrapper | Clamp (022), format (021) |
| `modules/core/include/System/UInt32.hpp` | integer wrapper | Clamp (022), format (021/023) |
| `modules/core/include/System/Int64.hpp` | integer wrapper | Clamp (022), format (021) |
| `modules/core/include/System/UInt64.hpp` | integer wrapper | Clamp (022), format (021/023) |
| `modules/core/include/System/Int128.hpp` | 128-bit wrapper | format (021); MinValue negation **remediated** (019 via #1834) |
| `modules/core/include/System/UInt128.hpp` | 128-bit wrapper | **shift UB (020) — remediated this batch, #1843**; Clamp (022), format (021/023) |
| `modules/core/include/System/Decimal.hpp` / `.cpp` | decimal | Clamp (022) |
| `modules/core/include/System/MathF.hpp` | float helpers | Clamp (022) |

Files that belong to the *adjacent* CCF families and are **excluded** from this
review's tickets (see §13): `Single.hpp`, `Double.hpp` (CCF-006/007),
`Math.hpp`/`Decimal.cpp` `MidpointRounding` (CCF-008), `Convert.*`,
`BitConverter.hpp` (CCF-005).

---

## 2. Public-surface inventory (the APIs each finding touches)

- **Shift operators** — `UInt128::operator<<(int)`, `operator>>(int)`. (020)
- **`Clamp(value, min, max)`** — one static per wrapper: 8 fixed-width integer
  types, plus `UInt128`, `Decimal`, `MathF`. (022)
- **`ToString(value, format)`** — the one/format-string overload per integer
  wrapper. (021 unknown-format fallthrough; 023 missing `"B"`/`"b"`.)
- **`IsPositive(value)`** — `SByte`, `Int16`. (024)

None of these are virtual, none appear in a public object's layout, and none is
an iterator. The only signature question is `noexcept` (see §9).

---

## 3. Confirmed finding inventory (measured against the current tree)

Each row was re-verified in this batch against current source and the .NET
reference in `/rv/tmp/runtime/src/libraries/`, not taken from the audit summary
alone.

| Finding | Sev | Files | Current behaviour (measured) | .NET reference | Class |
|---|---|---|---|---|---|
| **SR-AUD-020** | high | `UInt128.hpp` | `value_ << n` with **no mask**; `n>=128` or `n<0` is native UB (`UInt128.hpp:95:73: shift exponent 128 is too large`, UBSan `-fno-sanitize-recover`) | `shiftAmount &= 0x7F` (`UInt128.cs:2051/2087`); sibling `Int128` already masks `n & 127` | A (UB→defined; in-range byte-identical) |
| **SR-AUD-024** | med | `SByte.hpp:253`, `Int16.hpp:223` | `IsPositive(v)` is `v > 0`, so `IsPositive(0)` is **false**; `Int64.hpp:406` already uses `v >= 0` | `public static bool IsPositive(v) => v >= 0` (`SByte.cs:742`) | C (value change; still `noexcept`) |
| **SR-AUD-023** | med | `SByte, Int16, UInt16, UInt32, UInt64, UInt128` (6) | `ToString(v,"B")` falls through to decimal; no `'B'`/`'b'` branch | integral `ToString("B")` emits base-2 digits | C (value change) |
| **SR-AUD-022** | med | 8 fixed-width integer wrappers use `std::clamp` (itself **UB when `min>max`**); `UInt128, Decimal, MathF` return a bound (defined but wrong) | inverted interval is silently accepted | `if (min > max) ThrowMinMaxException` → `ArgumentException` (`Math.cs:540-544`) | C (adds throw; needs `noexcept` removal on Clamp) |
| **SR-AUD-021** | med | 12 wrappers | `ToString(v, "Q")` returns decimal instead of throwing; some **float** wide/precision variants leak `std::stoi` as `std::invalid_argument`. **The integer wrappers already funnel the width `std::stoi` through `FormatException`** (`SByte.hpp:171-175`), so the "leak" half is Single/Double, not the integer wrappers | unknown format → `FormatException` | C (adds throw) |

`std::clamp(lo>hi)` is undefined behaviour per `[alg.clamp]`, so SR-AUD-022 has a
genuine **UB** component for the eight fixed-width integer types, not only a
wrong-answer one — this is why it is prioritised P1 below.

---

## 4. Remediated / duplicate / false-premise corrections

- **SR-AUD-019** (Int128 `MinValue` negation UB) — **already `remediated`** by
  CCF-004 ticket #1834. It appears in CCF-003's list but needs no CCF-003 work.
- **SR-AUD-020** (UInt128 shift) — **remediated in this batch, ticket #1843**
  (§8, `AUDIT_FINDINGS_INDEX` updated). Left CCF-003 with four open members.
- **No false premises found** in the CCF-003 set: 020/022/023/024 reproduce
  exactly as written; 021's only nuance is that the "`std::stoi` leak" clause
  applies to the binary-float wrappers (CCF-006 territory), not the integer
  wrappers, whose width parse is already guarded. This is a *scope* correction,
  not a false premise.
- **Not a duplicate, but overlapping:** SR-AUD-021 is cited by CCF-003, CCF-005
  **and** CCF-006. One remediation of it closes the integer-wrapper slice of all
  three; the float slice stays with CCF-006/007. Do not open three tickets for
  it.

---

## 5. Cross-cutting family grouping

CCF-003 is one of five overlapping numeric families. This review owns the
integer/`Decimal`/`MathF` slice; the table records the boundary so tickets do
not collide.

| Family | Owns | Status vs this review |
|---|---|---|
| **CCF-003** | integer wrappers' boundary + formatting (020–024) | **this review** |
| CCF-005 | conversion/boundary testing-shape (021–027, 035, 036, 038, 041, 043, 047) | overlaps on 021; rest is `Convert`/`BitConverter`/`Span`/`Decimal` — separate review |
| CCF-006 | format-string normalisation (021 across integral + Single/Double) | overlaps on 021's *float* slice — separate review |
| CCF-007 | binary-float native-primitive edges (029–033) | disjoint — `Single`/`Double` internals |
| CCF-008 | `MidpointRounding` validation (036) | disjoint — `Math`/`MathF`/`Decimal.Round` |

---

## 6. Shared root causes

1. **Native primitive used without .NET's boundary rule** — a native shift
   (020), `std::clamp` (022), or a raw stream/`std::to_string` (021/023) is
   used where .NET applies a mask, a `min>max` guard, or a format grammar.
2. **A green normal-path test suite hides the invalid domain** — every finding
   has passing tests for ordinary inputs and none for the out-of-range/invalid
   input. This is CCF-005's thesis and it holds here: the fix is always
   *add the missing assertion*, and the test ticket must add the
   invalid-domain case that would have caught it.
3. **`noexcept` blocks the correct throw** — `Clamp` and several `ToString`
   overloads are `noexcept`, so the .NET-correct `ArgumentException`/
   `FormatException` cannot be added without removing the specifier (§9).

---

## 7. Dependency graph

```
#1843 SR-AUD-020 (UInt128 shift)      done — independent, no dependants
#1844 SR-AUD-024 (IsPositive)         independent, no noexcept change
#1845 SR-AUD-023 (binary ToString)    independent; touches ToString(v,fmt)
#1846 SR-AUD-022 (Clamp min>max)      independent; removes noexcept on Clamp
#1847 SR-AUD-021 (unknown format)     touches ToString(v,fmt) — SHARES the
                                       overload with #1845, so land #1845 first
                                       or fold the two into one ToString ticket
```

The only ordering constraint is #1845 → #1847 (both edit `ToString(value,
format)`; sequencing avoids a merge conflict and lets #1847 assume the `"B"`
branch exists). Everything else is independent and could be done in any order.

---

## 8. Implementation vs design-first classification

All five findings are **implementation-ready** under the CCF-004 class-A/B/C
framework (`docs/DefinedArithmeticBoundaryPlan.md` §1) — none needs a separate
design ticket, because none changes object layout, a vtable, a return
convention, or forces a downstream migration. Class C members carry their
compatibility argument **in-ticket**, exactly as CCF-004's #1836/#1837 did.

| Ticket | Finding | Class | Ready? |
|---|---|---|---|
| #1843 | 020 | A (UB→defined, in-range identical) | **done this batch** |
| #1844 | 024 | C (value change, no throw) | ready |
| #1845 | 023 | C (value change, new `"B"` output) | ready |
| #1846 | 022 | C (new throw; UB removal for fixed-width) | ready |
| #1847 | 021 | C (new throw) | ready |

---

## 9. Source / ABI / layout approval matrix

| Ticket | Layout | vtable | mangled symbol | Source-compat | Needs user approval? |
|---|---|---|---|---|---|
| #1843 | none | none | none | yes | **no** (done) |
| #1844 | none | none | none (`IsPositive` stays `noexcept`) | yes | no |
| #1845 | none | none | none | yes (new format value only) | no |
| #1846 | none | none | **`Clamp` loses `noexcept`** — top-level `noexcept` is not part of an ordinary function's Itanium mangled name, so the symbol is unchanged; a caller taking `&Clamp` as a `noexcept` pointer would break (exotic) | source-compatible for ordinary calls | no — compatible narrowing, in-ticket argument (CCF-004 precedent) |
| #1847 | none | none | same `noexcept` note as #1846 for the `ToString` overloads that are `noexcept` | source-compatible | no |

Nothing here crosses the approval boundary (public layout, virtual interfaces,
return conventions, mandatory downstream migration). The class-C behaviour
changes are compatible narrowings toward .NET parity — the same category the
user's standing CCF-004 approvals covered — and each is defended in its own
ticket, not by a blanket approval.

---

## 10. Test matrix

Per finding, the permanent test must add the **invalid/boundary** case the
audit found missing (root cause #2), and — for class A/where a value is claimed
unchanged — a **measured pre-fix** equality assertion.

| Ticket | Must add |
|---|---|
| #1843 | out-of-range shift (`<<128`, `<<130`, `>>128`, `<<-1`) equals .NET `& 0x7F`; in-range unchanged — **added** (`UInt128Tests.cpp`, 3 cases) |
| #1844 | `IsPositive(0) == true`; `IsPositive(-1) == false`; `IsPositive(1) == true` for `SByte` and `Int16`; **update the existing tests that pin `false`** |
| #1845 | `ToString(v,"B")`/`"b"` for min, max, zero, an ordinary value, per type; width-padded `"B8"` |
| #1846 | inverted interval (`Clamp(v, 5, 1)`) throws `ArgumentException` for every listed type; ordinary and equal-bound cases unchanged |
| #1847 | `ToString(v,"Q")` throws `FormatException`; every currently-supported specifier still returns its value |

Never weaken an existing assertion to make a green result; #1844's existing
`IsPositive(0)==false` tests must be *corrected*, with the correction noted as a
deliberate .NET-parity change.

---

## 11. Sanitizer matrix

| Ticket | Sanitizer | Why |
|---|---|---|
| #1843 | **UBSan** `-fno-sanitize-recover` | shift-exponent UB; reproduced pre-fix, clean post-fix (`build-probe/1843_uint128_shift_*.log`) |
| #1846 | **UBSan** | `std::clamp(lo>hi)` is UB for the 8 fixed-width types — reproduce the inverted-interval call pre-fix, prove clean post-fix |
| #1844, #1845, #1847 | none required | pure value/format/throw changes with no UB or memory component; ordinary GoogleTest suffices |

Reuse `build-asan/` (already `-fsanitize=address,undefined`) or a one-TU probe
with `-fsanitize=undefined`, never more than three jobs.

---

## 12. Recommended ticket order

1. **#1846 SR-AUD-022** (P1) — has the live `std::clamp` UB; highest severity of
   the remaining four. One structural fix across 11 files.
2. **#1844 SR-AUD-024** (P2) — smallest, no `noexcept` change; good warm-up.
3. **#1845 SR-AUD-023** (P2) — binary formatting; land before #1847.
4. **#1847 SR-AUD-021** (P2) — unknown-format throw; shares the `ToString`
   overload with #1845.

---

## 13. Explicit exclusions

- **`Single`/`Double`** internals (CCF-006/007: 029–034) — separate review.
- **`Convert` / `BitConverter` / `Span` / `HashCode`** (CCF-005: 026, 027, 041,
  043, 047) — separate review; several are high-severity memory-safety items and
  deserve their own bounded batch.
- **`MidpointRounding`** (CCF-008: 036) — separate review.
- **`Decimal` parser / negative-zero / OACurrency** (SR-AUD-035, 037, 038) —
  Decimal-specific, larger; not part of the integer-wrapper slice.
- The **float slice of SR-AUD-021** (the `std::stoi` leak in `Single`/`Double`)
  stays with CCF-006.

---

## 14. Completion criteria for CCF-003

CCF-003 is complete when SR-AUD-020, 021, 022, 023, 024 are all `remediated`
(019 already is), each with: a measured reproduction, a fix matching the .NET
reference, a permanent invalid-domain test, the applicable sanitizer clean, and
`scripts/local_ci_check.sh build` green with no test-count regression and
Doxygen inside 1,942. At authoring time **1 of 5 is done** (020, #1843); the
remaining four are ticketed #1844–#1847 and ready.

---

## 15. Implementation complete (batch `feature/remediation-batch-ccf003-ccf005-plan`, 2026-07-30)

This section records what the follow-on batch implemented against the plan
above, and every premise it re-measured. Entries are appended as each ticket
lands.

### 15.1 #1846 — SR-AUD-022 — Clamp inverted interval — **DONE**

**Surface re-inventoried before fixing** (do not trust the §1 count blind). The
complete `Clamp` surface in the numeric namespace is **wider** than the eleven
files SR-AUD-022 lists, but the extra surface was **already correct**:

| Clamp overload | Pre-fix state | Action |
|---|---|---|
| `Byte`, `SByte`, `Int16`, `UInt16`, `Int32`, `UInt32`, `Int64`, `UInt64` | `std::clamp(value,min,max)`; `noexcept` except `UInt16`/`UInt64` | **fixed** — guarded manual form, `noexcept` removed |
| `UInt128`, `Decimal`, `MathF` | manual compare, returns a bound, no guard | **fixed** — added `min > max` guard |
| `Int128`, `Single`, `Double` | already guard `min > max` → `ArgumentException` | unchanged (correct) |
| `Math::Clamp` (10 overloads: `int`/`double`/`long` in `Math.cpp`; `short`/`sbyte`/`byte`/`uint`/`ulong`/`ushort`/`float` inline in `Math.hpp`) | **all already** guard `min > max` → `ArgumentException` | unchanged (correct) — *not* part of the defect, though the task asked they be checked |

So the defective set is exactly the eleven SR-AUD-022 files; `Math`/`Int128`/
`Single`/`Double` are a **negative result** (checked, already correct), recorded
here so a later batch does not re-open them.

**Reproduction — corrected characterisation of the "UB".** `std::clamp(v, lo, hi)`
with `hi < lo` is a **library precondition violation** (`[alg.clamp]` p2), *not*
a language-level trap, so a bare `-fsanitize=undefined` does **not** flag it (a
point the §11 sanitizer matrix left implicit). It was reproduced instead by
arming libstdc++'s `__glibcxx_assert(!(__hi < __lo))` with `-D_GLIBCXX_ASSERTIONS`
and driving the **real production `Clamp` bodies** with runtime (`volatile`)
operands, one process per type. Pre-fix: all eight fixed-width types abort
(SIGABRT) at `stl_algo.h:3626 … Assertion '!(__hi < __lo)' failed` with the
matching `[with _Tp = …]`; the manual trio silently returns the wrong bound `5`
for `Clamp(3, 5, 1)` (`build-probe/1846_clamp_prefix.log`). Post-fix: all eleven
throw `ArgumentException` cleanly (`build-probe/1846_clamp_postfix.log`).

**Fix.** Every fixed-width overload drops `std::clamp` for the guarded manual
form `if (min > max) throw System::ArgumentException("min cannot be greater than max."); return value < min ? min : (value > max ? max : value);`
— identical to what `Int128`/`Single`/`Double`/`Math` already do, so the whole
`Clamp` family now shares **one** message and contract. `noexcept` was removed
from the eight fixed-width overloads (top-level `noexcept` is not part of the
Itanium mangled name, so no symbol changed). `#include "System/ArgumentException.hpp"`
added to all eleven headers.

**Message choice (deliberate).** .NET's actual text is
`SR.Argument_MinMaxValue` = `'{0}' cannot be greater than {1}.` (formatted with
the two values, no `paramName`). This batch used the repository's existing
simplified `"min cannot be greater than max."` for **internal consistency** with
the ten already-correct `Clamp` overloads, rather than fragment the family with
a second message format. The exception **type** (`ArgumentException`, no
`paramName`) and the **validation order** (`min > max` checked first, before any
comparison) match .NET exactly.

**Tests.** +12 permanent GoogleTest cases: a `Clamp_MinGreaterThanMax_Throws`
per type (11) asserting the throw *and* an equal-bound (`min == max`) valid case,
plus a new ordinary `UInt64Test.Clamp` (that overload had **no** test at all
before). `SharpRuntimeTests_Core_Base` 5056 → **5068**. Repository gate deferred
to the batch close.

**Consequences:** no object layout, vtable, return-convention, or mangled-symbol
change; source-compatible for every ordinary call. `SR-AUD-022 → remediated`.

### 15.2 #1844 — SR-AUD-024 — IsPositive(0) — **DONE**

**Surface re-inventoried.** Every `IsPositive` in the numeric namespace was
checked, not only the two the finding names:

| `IsPositive` | Pre-fix | .NET | Action |
|---|---|---|---|
| `SByte`, `Int16` | `value > 0` | `value >= 0` | **fixed** → `>= 0` |
| `Int32`, `Int64` | `value >= 0` | `value >= 0` | unchanged (already correct) |
| `Byte`, `UInt16`, `UInt32`, `UInt64`, `Int128`, `UInt128` | *no `IsPositive`* | — | none (out of scope) |
| `Decimal` (`!negative_`), `Single`/`Double` (`!signbit && !isnan`) | — | matches | unchanged (float/decimal slice, correct) |

So the defect is exactly `SByte` and `Int16`; the fix is the two-character
`>` → `>=` change the finding predicted, with **no** broadening into other
generic-math predicates (no evidence justified it).

**Fix + tests.** Both now `return value >= 0`, doc-comment updated to "zero or
positive". The two suites that pinned the wrong result
(`SByteTests.IsPositive_False`, `Int16NewTests.IsPositive_False`) were corrected
to assert `IsPositive(0) == true` and each gained a negative/`MinValue`/`MaxValue`
vector. Net **+2** test cases (two wrong ones replaced by four). Both stay
`noexcept`; no signature/layout/symbol change. `SR-AUD-024 → remediated`.

### 15.3 #1845 — SR-AUD-023 — binary ToString("B"/"b") — **DONE**

**Premise corrected: seven types, not six.** The finding lists `SByte`, `Int16`,
`UInt16`, `UInt32`, `UInt64`, `UInt128`. Re-inventory (`grep` for a `'B'` branch
in every integral `ToString(value, format)`) confirms those six lacked it — and
found **`Int128` lacked it too**: its `ToString(format)` had the same
X/D-then-fallthrough shape as `UInt128`, so `Int128(5).ToString("B")` returned
decimal `"5"`. The finding named only `UInt128`. `Byte`, `Int32`, `Int64`
already had a correct `B` branch. So #1845 fixes **seven** wrappers, extending
the finding's surface to `Int128.hpp` (recorded, identifier kept — the SR-AUD-060
precedent). Leaving `Int128` as the one integral type without binary formatting
while its unsigned sibling gained it would have been an indefensible gap, so it
is inseparable from this repair, not a separate ticket.

**.NET algorithm confirmed** (`Number.Formatting.cs` `UInt32ToBinaryStr` /
`UInt128ToBinaryStr`, and the signed `value & hexMask` path): mask to the natural
width (raw two's-complement bits), emit `max(1, requested_precision)` digits with
no surplus leading zeros. This is exactly the existing `Byte`/`Int32`/`Int64`
implementation, so the fix **replicates that pattern** per type at the right
width (`0xFF`/`0xFFFF` masks for the small signed types; the full value
otherwise) rather than inventing a new one — and deliberately does **not** add a
sign or a `-` (`Do not copy a signed implementation if it would add a sign`): the
signed types emit two's-complement bits, so `SByte(-1)="11111111"`,
`Int128(-1)=` 128 ones, `Int16::MinValue="1000000000000000"`.

**Tests.** +7 `ToString_Binary` cases (one per fixed type), each pinning: value
5, zero, an uppercase/lowercase pair, `MaxValue` (all-ones at the width), the
signed `MinValue`/`-1` two's-complement forms, and a width-padded `Bn`. All
computed against the .NET reference semantics and green.
`SharpRuntimeTests_Core_Base` 5070 → **5077**. No layout/symbol change; new
output value only. `SR-AUD-023 → remediated`.
