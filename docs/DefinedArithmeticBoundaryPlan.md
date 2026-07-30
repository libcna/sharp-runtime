<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# CCF-004 family plan — defined arithmetic at public boundaries

**Ticket #1829** (`REMED-CORE-CCF004-PLAN`, P1, size M, **design-only**).
Written 2026-07-29. No production source changed under this ticket.

This document is to CCF-004 what
[`docs/Base64FamilyPlan.md`](Base64FamilyPlan.md) is to CCF-013: the plan that
must exist before any per-file fix, because the eight findings are **not**
interchangeable and three different repairs are needed.

It issues **no `SR-AUD-*` identifier**. Audit numbering stays frozen at **364**
and all eight members keep status `confirmed`.

---

## 1. The cause, stated once

Eight public, .NET-shaped operations perform a **signed** C++ arithmetic
operation whose result is not representable. In C# the corresponding code is
either *defined* (unchecked two's-complement wrap) or *guarded* (a range check
before the arithmetic). In C++ it is **undefined behaviour** — so the value the
program currently produces is a property of today's GCC, not of the language,
and no guard placed *after* the operation is portable or optimisation-safe.

The cause is one sentence, but the *repair* is not, and that is why this plan
exists. The members split three ways:

| Class | What .NET does | What the repair is | Observable change |
|---|---|---|---|
| **A — defined wrap** | unchecked two's-complement arithmetic, result intended | redo the arithmetic in the **unsigned** counterpart type, convert back | **none** — every value stays byte-identical |
| **B — validate first** | range check before arithmetic, then throw | move the existing check **ahead** of the arithmetic | **none** — same exception, same message, same paramName |
| **C — wrong answer today** | range check, then throw or fail | add the missing check | **yes** — a call that succeeds with a wrapped value must start failing |

Only class C needs a compatibility argument. Classes A and B are pure
undefined-behaviour removal.

---

## 2. Membership, measured today

All eight were re-reproduced on 2026-07-29 under UndefinedBehaviorSanitizer with
`-fno-sanitize-recover`, one process per case
(`build-probe/1829_ccf004_survey.cpp`, logs
`build-probe/1829_ccf004_survey.log` and `…_o0.log`). **Nothing here is quoted
from the audit's own wording without re-measurement.**

| Finding | File and line | Case | Diagnostic measured today | Class |
|---|---|---|---|---|
| SR-AUD-019 | `Int128.hpp:234` (parse) | 1 | `negation of 0x800…000 cannot be represented in type '__int128'` | A |
| SR-AUD-019 | `Int128.hpp:143` (`ToString`) | 2 | same | A |
| SR-AUD-025 | `IntPtr.hpp:105` (`Add`) | 3 | `signed integer overflow: 9223372036854775807 + 1 … 'long int'` | A |
| SR-AUD-025 | `IntPtr.hpp:114` (`Subtract`) | 4 | `signed integer overflow: -9223372036854775808 - 1 … 'long int'` | A |
| SR-AUD-049 | `ReadOnlyMemory.hpp:140` | 5 | `signed integer overflow: 3 - -2147483648 … 'int'` | B |
| SR-AUD-057 | `Index.hpp:61` (`GetOffset`) | 6 | `signed integer overflow: -2147483648 - 2147483647 … 'int'` | A |
| SR-AUD-057 | `Index.hpp:61` via `Range::GetOffsetAndLength` | 7 | same line, reached through `Range.hpp:93` | A |
| SR-AUD-060 | `DateOnly.cpp:65` + **35, 37, 39** | 8 | four overflows per call — see §2.1 | C |
| SR-AUD-060 | `DateOnly.cpp:76` + **35, 37, 39** | 9 | four overflows per call | C |
| SR-AUD-060 | `DateOnly.cpp:81` (`AddMonths`) | 10 | `signed integer overflow: 12 + 2147483647 … 'int'` | C |
| SR-AUD-060 | `DateOnly.cpp:92` (`AddYears`) | 11 | `signed integer overflow: 178956971 * 12 … 'int'` | C |
| SR-AUD-062 | `Tuple.hpp:23` | 12 | `signed integer overflow: 2147483616 + 67108863 … 'int'` | A |
| SR-AUD-084 | `Utf8Parser.hpp:224` (default) | 13 | `negation of -9223372036854775808 … 'long int'` | A |
| SR-AUD-084 | `Utf8Parser.hpp:357` (`N`) | 14 | same | A |
| SR-AUD-008 | `TimeSpan.cpp:454` (`TryParse`) | 15 | `signed integer overflow: 2147483647 * 864000000000 … 'long int'`, **and `parsed=1 ticks=-7695280436664713216`** | **C** |
| SR-AUD-008 | `TimeSpan.cpp:263` (`Subtract`) | 16 | `signed integer overflow: -9223372036854775808 - 1 … 'long int'`, then a correct `OverflowException` | A |

### 2.1 SR-AUD-060 is larger than the audit recorded

The audit named four `DateOnly.cpp` sites (65, 76, 81, 92). The measurement shows
the overflow **cascades**: a wrapped day number flows into `jdnToDate`, which
overflows three more times per call, at `DateOnly.cpp:35`, `:37` and `:39`. Cases
8 and 9 each report **four** UB operations, not one. The real site count for
SR-AUD-060 is **seven**, and a fix that guards only the four entry points named
in the audit while leaving `jdnToDate` reachable with a wrapped argument would
still be incomplete. This is an addition to the finding's surface, recorded here
by appending; the finding keeps its identifier and its `confirmed` status.

### 2.2 SR-AUD-008's two halves are in different classes

The audit treats `TryParse` and `Subtract` as one finding with a shared cause.
They are, but they need different work and must not be fixed as one edit:

- `Subtract` (case 16) is **class A**: the sign-bit guard at `TimeSpan.cpp:265`
  already produces the *correct* `OverflowException`; only the undefined
  subtraction that precedes it must go.
- `TryParse` (case 15) is **class C**: it returns `parsed=1` with
  `ticks=-7695280436664713216` for `"2147483647.00:00:00"`, a negative duration
  unrelated to the positive input. This is a **wrong answer**, not merely UB, and
  it is the only member of this family that silently produces one.

---

## 3. Methodology — how to reproduce these findings at all

**This section exists because the first survey run of this ticket got three of
eight wrong, and the reason generalises.** A naive probe reports SR-AUD-049,
SR-AUD-060 and SR-AUD-008 as "already fixed". They are not. Two independent
causes:

1. **Most sites are in `.cpp` files, and `build/` is not instrumented.**
   `DateOnly.cpp` and `TimeSpan.cpp` are compiled into
   `build/libsharp_runtime_core.a` **without** sanitizer flags. A probe compiled
   with `-fsanitize=undefined` but linked against `build/` instruments only the
   header-only members (`Int128.hpp`, `IntPtr.hpp`, `Index.hpp`, `Tuple.hpp`,
   `Utf8Parser.hpp`) and is **blind** to everything inside a `.cpp`. Link against
   **`build-asan/`**, whose `CMAKE_CXX_FLAGS` are
   `-O1 -g -fsanitize=address,undefined`, so the library code is instrumented too.
2. **At `-O1`, GCC constant-folds a header-inline overflow and emits no check.**
   Case 5 (`ReadOnlyMemory<int>::Slice(INT_MIN)` on a 3-element memory) inlines to
   a wholly compile-time expression; GCC folds `3 - INT_MIN` at compile time and
   emits no UBSan check, so the case passes silently at `-O1` and reports at
   `-O0`. **Absence of a UBSan report is not evidence of absence of UB** for any
   header-inline site in this family.

The reproduction recipe that works for all sixteen cases:

```bash
g++ -std=c++23 -fsanitize=address,undefined -fno-sanitize-recover=undefined -g -O0 \
    -Imodules/core/include -Imodules/buffers/include -Imodules/text/include \
    build-probe/1829_ccf004_survey.cpp -o build-probe/1829_ccf004_survey_o0 \
    -Lbuild-asan -lsharp_runtime_core -lsharp_runtime_text
```

`-fno-sanitize-recover=undefined` makes a UB case **abort**, so evidence is a
non-zero exit rather than a line that can be missed in a log. Every
implementation ticket in §5 must use this recipe, and must show the diagnostic
present before and absent after.

---

## 4. The shared repair idiom, and it is already in this repository

.NET's own code is the specification, and it uses **one** idiom throughout:
*compare in the unsigned domain, then do the arithmetic in the unsigned domain.*

`DateOnly.cs:73-81` and `:121-132`:

```csharp
public static DateOnly FromDayNumber(int dayNumber)
{
    if ((uint)dayNumber > MaxDayNumber)          // one unsigned compare catches negatives too
        ThrowHelper.ThrowArgumentOutOfRange_DayNumber(dayNumber);
    return new DateOnly((uint)dayNumber);
}

public DateOnly AddDays(int value)
{
    uint newDayNumber = _dayNumber + (uint)value;   // defined wrap, not signed overflow
    if (newDayNumber > MaxDayNumber)
        throw new ArgumentOutOfRangeException(nameof(value), SR.ArgumentOutOfRange_AddValue);
    return new DateOnly(newDayNumber);
}
```

**This repository already implements that idiom, correctly, in the very file one
of the findings is in.** `ReadOnlyMemory.hpp:120-131` — the *two*-argument
`Slice`, fixed under ticket 265 — reads:

```cpp
if (static_cast<uintcs>(start)  > static_cast<uintcs>(length_) ||
    static_cast<uintcs>(length) > static_cast<uintcs>(length_ - start))
    throw System::ArgumentOutOfRangeException("start");
```

So there is **no new shared infrastructure to build**. No new helper header, no
new utility type, no new macro. The family needs the *existing* idiom applied at
seven more places, and each application is local to one function. A ticket that
proposes a new `SafeArithmetic` helper is over-engineering this family; say so and
reject it.

The one thing worth adding is **a comment convention**: every converted site
states which class (A, B or C) it is and cites the .NET line, so a later reader
cannot mistake a deliberate unsigned cast for a sloppy one and "clean it up".

### 4.1 Class A — the exact transformations

| Site | Now | Becomes |
|---|---|---|
| `IntPtr::Add` | `pointer.value + offset` | `static_cast<intptr_t>(static_cast<uintptr_t>(pointer.value) + static_cast<uintptr_t>(offset))` |
| `IntPtr::Subtract` | `pointer.value - offset` | the same with `-` |
| `Index::GetOffset` | `length - value_` | `static_cast<intcs>(static_cast<uintcs>(length) - static_cast<uintcs>(value_))` |
| `tupleHashCombine` | `((h1 << 5) + h1) ^ h2` in `intcs` | the whole expression in `uintcs`, then one conversion back |
| `Int128` parse / `ToString` | `-static_cast<__int128>(val)`, `-value_` | derive the magnitude wholly in `unsigned __int128`; never negate the signed minimum |
| `Utf8Parser` (both sites) | `-static_cast<int64_t>(v)` | build the negative from the unsigned magnitude without negating `INT64_MIN` |
| `TimeSpan::Subtract` | `ticks_internal - ts.ticks_internal` | unsigned subtraction, keep the **existing** sign-bit guard and its `OverflowException` unchanged |

Every one of these must be pinned by a test asserting the **current** observed
value, so the "no observable change" claim is proven and not assumed:
`MaxValue + 1 == MinValue`, `MinValue - 1 == MaxValue`,
`Index::FromEnd(INT_MAX).GetOffset(INT_MIN) == 1`,
`Int128::MinValue().ToString() == "-170141183460469231731687303715884105728"`,
`Utf8Parser` yielding exactly `INT64_MIN`, and `Tuple2(0x03ffffff,0).GetHashCode()`
keeping the value it has today.

### 4.2 Class B — the exact transformation

`ReadOnlyMemory<T>::Slice(intcs start)` currently evaluates `length_ - start` as
a **call argument**, so the subtraction happens before the two-argument overload's
already-correct unsigned check can run. The repair is to validate `start` in the
caller, before forming the second argument:

```cpp
[[nodiscard]] ReadOnlyMemory<T> Slice(intcs start) const {
    if (static_cast<uintcs>(start) > static_cast<uintcs>(length_))
        throw System::ArgumentOutOfRangeException("start");
    return Slice(start, length_ - start);   // now provably non-overflowing
}
```

Exception type, message and `paramName` are **unchanged** — the two-argument
overload already throws `ArgumentOutOfRangeException("start")` for this input,
which case 5 confirms it does today. **Check whether `Memory<T>`, `Span<T>` and
`ReadOnlySpan<T>` have the same one-argument shape** before writing the fix; if
they do, they belong in the same ticket, and if they do not, say so explicitly.

### 4.3 Class C — the two that change behaviour

**SR-AUD-060 (`DateOnly`).** Follow `DateOnly.cs` exactly: one unsigned compare
at the top of `FromDayNumber`, unsigned day arithmetic plus one bound check in
`AddDays`, and a bounded month/year domain check in `AddMonths`/`AddYears` before
any multiplication. The cascade at `DateOnly.cpp:35/37/39` then becomes
unreachable rather than needing its own guards. Observable change: the *exception
type* is already `ArgumentOutOfRangeException`, but today it comes from the
`DateTime` constructor and names `year`; .NET names the actual parameter
(`dayNumber`, `value`). Deciding whether to adopt .NET's `paramName` is that
ticket's call, and it must state which it chose and why.

**SR-AUD-008 (`TimeSpan::TryParse`).** Range-check the day/hour/minute/second
fields before multiplying, and return `false` where a wrapped value is produced
today. `Parse` must raise the corresponding `OverflowException`.

**Compatibility argument for class C, and why neither needs a new approval.**
Both currently produce a value that is *wrong by any definition* — a negative
duration from a positive input, and a date derived from a wrapped day number.
No caller can be depending on a specific wrapped result, because the result is
undefined behaviour and not guaranteed even between two builds of this
repository. This is the same reasoning the batch accepted for #1817 and #1818
(rejecting Base64 inputs that had silently decoded to garbage) and for #1825
(rejecting writes that had silently been dropped): **the rejected inputs never
worked.** Each implementation ticket must restate this argument for itself rather
than inherit it.

---

## 5. Implementation ticket split and dependency order

Seven tickets. The order is chosen so that the cheapest, most mechanical, fully
compatible work lands first and the two behaviour-changing tickets land last,
after the family's test and probe scaffolding already exists.

| # | Ticket | Findings | Class | Size | Depends on |
|---|---|---|---|---|---|
| 1 | **#1830** — `Index::GetOffset` and its `Range` consumer | SR-AUD-057 | A | XS | — |
| 2 | **#1831** — `tupleHashCombine` | SR-AUD-062 | A | XS | — |
| 3 | **#1832** — `IntPtr::Add` / `Subtract` | SR-AUD-025 | A | XS | — |
| 4 | **#1833** — `ReadOnlyMemory<T>::Slice(start)` and any same-shaped sibling | SR-AUD-049 | B | S | — |
| 5 | **#1834** — `Int128` `MinValue` parse and format | SR-AUD-019 | A | S | — |
| 6 | **#1835** — `Utf8Parser` `Int64` minimum, both formats | SR-AUD-084 | A | S | — |
| 7 | **#1836** — `TimeSpan::Subtract` (A) **and** `TryParse` (C) | SR-AUD-008 | A + C | M | — |
| 8 | **#1837** — `DateOnly` arithmetic, all seven sites | SR-AUD-060 | C | M | — |

There are **no hard dependencies between them**: every site is local to one
function and no two share a symbol. They are ordered by risk, not by need. Each
may be taken independently, and a later batch may take them in any order — but
**#1836 and #1837 should not be first**, because they are the two that need a
behaviour-change argument and both benefit from the survey probe being already
committed.

A finding is `remediated` only when **every** site listed for it in §2 is fixed.
SR-AUD-008 in particular is not remediated by fixing `Subtract` alone, and
SR-AUD-060 is not remediated by guarding four entry points while `jdnToDate`
remains reachable with a wrapped argument.

---

## 6. Test matrix

Every ticket adds, at minimum:

| Dimension | Requirement |
|---|---|
| the exact audited input | the case from §2, asserting the post-fix outcome |
| the pre-fix value, for class A | an equality assertion on the value observed today, so "no observable change" is proven |
| both boundary directions | `MaxValue`+1 **and** `MinValue`−1 wherever the operation has two ends |
| one value inside range | the guard must not be inverted |
| the ordinary path | at least one plain, non-extreme value per changed function |
| class C only | the wrapped-success input now failing, **and** the largest input that must still succeed |
| the paramName and message | pinned verbatim where an exception is thrown |

Not weakened: no existing assertion may be relaxed. The floor at the time of
writing is 14,098 tests across 37 executables.

---

## 7. Sanitizer matrix

| Ticket | UBSan | ASan | LSan | TSan |
|---|---|---|---|---|
| all eight | **yes — mandatory, and the primary evidence.** The defect *is* a UBSan finding; the diagnostic must be shown present before and absent after, using §3's recipe | only where a fix touches a pointer or a length (#1833) | no — none of these allocate | **no** |

TSan is not run: every member is a pure value computation with no shared mutable
state.

**Activation must be proven, not assumed** — the §3 methodology failure is
precisely a case where the flags were right and the evidence was still absent.
Show the diagnostic text itself, not merely a zero exit code.

---

## 8. Public and ABI impact

| Ticket | Public signature | Virtual / vtable | Object layout | Mangled symbols | Consumer rebuild |
|---|---|---|---|---|---|
| #1830 – #1836 | unchanged | unchanged | unchanged | unchanged | not required |
| #1837 | unchanged | unchanged | unchanged | unchanged | not required |

No member is added anywhere; every change is inside a function body. Note that
`Index::GetOffset` and `tupleHashCombine` are `noexcept` and must **stay**
`noexcept` — the class A repair does not introduce a throw, and adding one would
be a source-compatibility change that this plan does not authorise.

---

## 9. Explicit approval requirements

**None of the eight needs a new approval**, and this plan states that as a
conclusion rather than an assumption:

- classes A and B change **no observable value at all** — they remove undefined
  behaviour and keep every result, exception, message and `paramName`;
- class C changes results that are currently undefined behaviour producing a
  demonstrably wrong answer, which is the same compatible-narrowing argument the
  batch accepted for #1817, #1818 and #1825.

If any implementation ticket discovers that a "class A" site cannot be converted
without changing a value, that discovery **promotes it to class C** and it must
carry the compatibility argument or stop and ask. Do not silently reclassify.

---

## 10. Explicit exclusions

1. **The collection mutation counters.** Already decided and recorded: ticket
   #1786 ruled them **not** CCF-004 members, for two reasons that still hold —
   the arithmetic is not at a public boundary, and the repair is a *wider type*
   rather than defined arithmetic, because the real defect is snapshot reuse.
   `docs/CollectionVersionCounterSweep.md` is their record. **Do not fold them in.**
2. **A new shared arithmetic helper.** §4 rejects it: the idiom already exists in
   this repository and each application is a one-line local change.
3. **`Span<T>` / `ReadOnlySpan<T>` / `Memory<T>` two-argument `Slice`.** Already
   fixed under ticket 265 and used here as the reference implementation. Only the
   *one*-argument shape is in scope, and #1833 must inventory which types have it.
4. **Every other CCF cause.** CCF-003, CCF-005, CCF-006 and CCF-007 touch some of
   the same files (`Int128.hpp`, `IntPtr.hpp`) with different causes. A ticket
   here fixes its own finding and leaves the neighbours alone.
5. **`Decimal`, `Int128` and `UInt128` operator arithmetic.** The audit notes
   these already avoid the pitfall; SR-AUD-019 is specifically the `MinValue`
   parse and format paths, not the operators.

---

## 11. Status

| Ticket | Status at the close of #1829 |
|---|---|
| #1829 | done (this document) |
| #1830 `Index`/`Range` | ready — class A, compatible, no approval |
| #1831 `Tuple` hash | ready — class A, compatible, no approval |
| #1832 `IntPtr` | ready — class A, compatible, no approval |
| #1833 `ReadOnlyMemory::Slice` | ready — class B, compatible, no approval |
| #1834 `Int128` `MinValue` | ready — class A, compatible, no approval |
| #1835 `Utf8Parser` `Int64` min | ready — class A, compatible, no approval |
| #1836 `TimeSpan` | ready — class A + C, compatible, argument required in-ticket |
| #1837 `DateOnly` | ready — class C, compatible, argument required in-ticket |

All eight findings keep status `confirmed`. No `SR-AUD-*` identifier was issued;
numbering stays frozen at **364**.

---

## 12. Correction found while implementing #1830 (2026-07-29)

§2 lists `Index.hpp:61` twice for SR-AUD-057 — once reached directly (case 6) and once
"via `Range.hpp:93`" (case 7) — and §4.1 lists one transformation for the finding. Both
are wrong in the same way: they describe `Range::GetOffsetAndLength` as merely
**consuming** `Index`'s operation. It has a **second, independent** overflow of its own.

For a maximal from-end range over an `INTCS_MIN` length, the unsigned bounds checks at
`Range.hpp:95-98` **pass**, and the `end - start` that follows is undefined behaviour:

```
modules/core/include/System/Range.hpp:99:33: runtime error: signed integer overflow:
                                            -2147483648 - 1 cannot be represented in type 'int'
```

It is emitted in the same process as `Index.hpp:61`'s. The survey's first pass did not
show it because `-fno-sanitize-recover` aborts at the *first* diagnostic — so §3's own
recommended flag hid a site. **When enumerating the sites of a finding, run the recovering
build too and collect every diagnostic; use the aborting build only to prove a site is
gone.** That amendment applies to every remaining ticket in this family, and #1837's seven
`DateOnly` sites are the case where it matters most.

SR-AUD-057's site count is therefore **two**, not one, and both are now fixed under #1830
with the resolved values unchanged. This is recorded by appending, so that what the plan
believed and what the implementation measured stay separately readable.

---

## 13. Corrections found while implementing #1831 (2026-07-30)

Two, both by measurement, both generalising to the rest of the family.

**13.1 — a finding's reachable surface can be narrower *and* wider than one input.**
§2 lists SR-AUD-062 as one case (`Tuple2<intcs,intcs>(0x03ffffff, 0)`). Enumerating five
reachable shapes of the same helper shows three overflow and **two do not**:

| Shape | Overflows? |
|---|---|
| `Tuple2(0x03ffffff, 0)` — the audited input | yes |
| `tupleHashCombine(INTCS_MAX, 0)` | **no** — `INTCS_MAX << 5` is `-32`, and `-32 + INTCS_MAX` fits |
| `Tuple3(0x03ffffff, 0, 0)` — the **outer** combine, on the inner result | yes |
| `Tuple8(…, INTCS_MAX)` — last operand is the **unmasked** `Rest.GetHashCode()` | yes |
| `tupleHashCombine(-2000000000, 0)` | **no** — the shifted value lands back inside range |

So "the largest operand" is *not* the worst case for a shift-then-add step, and a repair
justified only by the audited input would not have shown that the *outer* combine of a
higher arity overflows on the inner result with all later items zero. Enumerate shapes;
do not extrapolate from one.

**13.2 — §12's amendment needs one process per shape, not merely a recovering build.**
§12 says to enumerate with the recovering build. That is necessary and not sufficient:
UBSan **deduplicates diagnostics by source location**, and all five shapes above overflow
the *same* line. In one recovering process, case 1 reports and cases 3 and 4 are silent —
which reads exactly like "already fixed". Run **one process per shape** even when the
shapes share a line, and use `-fno-sanitize-recover=undefined` only afterwards, to prove
each shape is gone.

**13.3 — one structurally identical site exists outside CCF-004, and stays outside.**
`System::Net::Security::SslApplicationProtocol::GetHashCode()`
(`SslApplicationProtocol.hpp:72`) runs the same signed `((h << 5) + h) ^ byte` step over
ALPN protocol-id bytes; `"spdy/3.1"` reports `signed integer overflow: 729647660 +
1873888640`. It is a different file in a different module and the audit never named it, so
it is **inactive ticket #1838** rather than a widening of SR-AUD-062. **No `SR-AUD-*`
identifier was issued; numbering stays frozen at 364.** `System/ValueTuple.hpp` was
inventoried at the same time and is **clear** — `detail::vtHashCombine` already
accumulates in `size_t`.

### 13.4 Live family status, as of the close of #1831

§11's table is a snapshot frozen at the close of #1829 and is deliberately left as written.
The current state is:

| Ticket | Finding | Status |
|---|---|---|
| #1830 `Index`/`Range` | SR-AUD-057 | **done** — finding `remediated`, two sites (§12) |
| #1831 `tupleHashCombine` | SR-AUD-062 | **done** — finding `remediated`, three of five shapes overflowed (§13.1) |
| #1832 `IntPtr` | SR-AUD-025 | **done** — finding `remediated` |
| #1833 `ReadOnlyMemory::Slice` | SR-AUD-049 | ready — class B |
| #1834 `Int128` `MinValue` | SR-AUD-019 | ready — class A |
| #1835 `Utf8Parser` `Int64` min | SR-AUD-084 | ready — class A |
| #1836 `TimeSpan` | SR-AUD-008 | ready — class A + C |
| #1837 `DateOnly` | SR-AUD-060 | ready — class C, seven sites (§2.1) |

Audit numbering remains frozen at **364**; the index reads **24 remediated / 340
confirmed** after #1831.

---

## 14. Confirmations and corrections from #1833 (2026-07-30)

**14.1 — §4.2's proposed transformation was correct as written and was applied verbatim.**
The one unsigned compare `static_cast<uintcs>(start) > static_cast<uintcs>(length_)` ahead
of the forwarding call is what real .NET does at `ReadOnlyMemory.cs:154-163`, and the
exception type, `paramName` and full `Message` are byte-identical before and after
(`build-probe/1833_prefix_values.log` vs `build-probe/1833_postfix.log`).

**14.2 — §4.2's inventory question is answered: SR-AUD-049 is ONE site, not four.**
§4.2 required checking whether `Memory<T>`, `Span<T>` and `ReadOnlySpan<T>` share the
one-argument shape. They have the overload but **not** the defect: each validates with a
signed `start < 0 || start > length_` pair-compare *before* subtracting, so the subtraction
is unreachable with an out-of-range operand. `ArraySegment<T>::Slice(intcs index)` is the
same. All four were run with `INTCS_MIN` in their own process at `-O0` and measured clean.
`ReadOnlySequence<T>`'s five one-argument forms are clear as well — each resolves through
`GetPosition`, which validates.

So the family has **one** defective member, and the audit's site count for SR-AUD-049 was
right. That is worth recording explicitly, because §2.1 and §12 each *raised* a count; a
plan that only ever finds more sites has not been checked, it has been trusted.

**14.3 — the two idioms in the family are both correct, and neither was harmonised.**
`ReadOnlyMemory` now uses .NET's unsigned single compare; its four siblings keep the signed
pair-compare. Both make the following subtraction provably safe, and changing four working
functions to match a fifth is not this family's business. Likewise
`ArraySegment<T>::Slice`'s `paramName` is `"index"`, matching .NET's own parameter name, and
is now pinned rather than aligned with the `"start"` used elsewhere.

**14.4 — §3 cause 2 was confirmed on the case it was written for.** At `-O1` GCC folds the
wholly compile-time `3 - INT_MIN` and emits no check, so the audited case passes silently.
`-O0` plus `volatile` operands is what makes the probe independent of the optimisation
level; do not relax either.
