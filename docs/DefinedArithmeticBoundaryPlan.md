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

---

## 15. Reachability corrections from #1834 and #1835 (2026-07-30)

§2 lists each finding by the site that reports, which is the right way to enumerate a
*repair*. It is **not** the right way to enumerate a **public surface**, and both of these
tickets found more public doors onto the same site than §2 names. Neither needed extra work;
both needed extra tests, because a reader cannot otherwise tell whether the other doors were
considered.

| Finding | §2's entry points | Public doors actually measured onto the same site |
|---|---|---|
| SR-AUD-019 | `TryParse`, `ToString()` | plus `Parse` (onto `:234`) and `ToString("D")`, `ToString("d")`, `ToString("G")`, `ToString("D40")`, `ToString("D45")` (all onto `:143`) |
| SR-AUD-084 | `TryParse(int64_t)` default and `N` | plus `TryParse(int32_t)`, `TryParse(int16_t)`, `TryParse(int8_t)` and their `N` forms — each executes the undefined negation and **then** fails the caller's width check |

The SR-AUD-084 case is the sharper one. `tryParseIntegerCore`'s magnitude limit is
`INT64_MAX + 1` **regardless of `byteWidth`**, and the type-width check is applied by the
**caller afterwards**. A narrow-width parse of the int64 minimum string therefore reaches
the UB on its way to returning `false`. Nothing about the failing return value protects it.

**Rule for the remaining tickets:** after fixing a site, enumerate every public overload
that can reach it — including the ones that ultimately **fail** — and pin them. #1837's
`jdnToDate` cascade (§2.1) is the same shape seen from the other direction: one entry point,
several downstream sites.

Both findings' site counts stand at what §2 records (two each). Only the reachability is
corrected, by appending.

---

## 16. Pre-implementation reproduction of the two class C members (2026-07-30)

The batch that closed #1831, #1833, #1834 and #1835 did **not** implement #1836 or #1837. It
did reproduce both against a `build-asan` tree **proven current** — `DateOnly.cpp.o` and
`TimeSpan.cpp.o` are both newer than their sources and `libsharp_runtime_core.a` was relinked
under #1834 — with the **recovering** build and **one process per case**, so every site in every
call is visible. Evidence: `build-probe/1836_1837_classc_survey.cpp` and
`build-probe/1836_1837_prefix.log`. Four things came out of it that §2 does not say.

**16.1 — `TimeSpan::Parse` does not throw either; it returns the wrapped value.** §2 case 15
records only that `TryParse` returns `parsed=1`. Measured, `Parse("2147483647.00:00:00")`
**returns** a `TimeSpan` of `-7695280436664713216` ticks rather than raising anything. So #1836's
criterion "`Parse` raises `OverflowException`" is a **behaviour change on the throwing entry
point too**, not merely a consequence of fixing `TryParse`. Both entry points must be stated in
that ticket's compatibility argument.

**16.2 — the largest day count that must keep parsing is measured.**
`TryParse("10675199.02:48:05")` succeeds with `ticks = 9223372036850000000`. That is #1836's
"largest input that must still succeed", now a measured value rather than one to be derived.

**16.3 — SR-AUD-060's cascade is reachable WITHOUT the entry-point overflow.** §2.1 records the
cascade as what happens *after* a wrapped day number reaches `jdnToDate`. In the **negative**
direction it is reached directly: `FromDayNumber(INTCS_MIN)` and
`MinValue.AddDays(INTCS_MIN)` each report `DateOnly.cpp:35`, `:37` and `:39` and **no**
diagnostic at `:65` or `:76`, because `INTCS_MIN + JDN_EPOCH` does not overflow — it is simply a
wildly out-of-range argument that `jdnToDate` then multiplies. A repair that only guards the
four entry points against *overflow* would leave these three sites live. `jdnToDate` must be
provably unreachable with an out-of-range argument, which is what #1837's acceptance criterion
already demands and which this measurement shows is a **separate** requirement rather than a
restatement of the first one.

**16.4 — `AddYears` produces a silent WRONG ANSWER, so SR-AUD-060 has a class C wrong-answer of
its own.** §2.2 says `TimeSpan::TryParse` "is the only member of this family that silently
produces one". That is **wrong**. `DateOnly(1,1,1).AddYears(INTCS_MIN)` reports the overflow at
`DateOnly.cpp:92` and then **returns `1-01-01` successfully** — `INTCS_MIN * 12` is exactly
`-6 · 2^32`, so it wraps to **zero** and `AddYears` degenerates into `AddMonths(0)`, i.e. an
identity operation. A caller asking for a date 2.1 billion years earlier gets the same date back
and no error. `AddMonths(INTCS_MIN)` by contrast throws cleanly with no diagnostic at all.

So the family has **two** silent-wrong-answer members, not one, and #1837 must carry the class C
compatibility argument for `AddYears` specifically as well as for the out-of-range rejections.

**16.5 — a bounded-iteration requirement, confirmed as real.** `AddMonths`'s normalisation is
`while (m > 12) { m -= 12; ++y; }` and `while (m < 1) { m += 12; --y; }` with `m = month_ + n`.
For `AddMonths(INTCS_MAX)` the wrapped `m` is `-2147483637`, so the second loop runs about
**179 million** iterations before returning. It completes, so this is not a hang, but the loop
is unbounded in principle and is on a public path. #1837's "no month-normalisation loop can run
more than a bounded number of iterations" criterion is therefore load-bearing, not defensive.

**16.6 — every current rejection names `year`.** All eight rejecting `DateOnly` cases throw
`ArgumentOutOfRangeException` with paramName `year` and the message
`"DateTime: date component out of range (Parameter 'year')"`, from the `DateTime` constructor
rather than from `DateOnly`. §4.3 predicted this; it is now measured, so #1837's paramName
decision has the exact "before" string to reason about.

**Neither ticket's classification changes.** Both remain **ready**, both remain CCF-004 members
in good standing, and §9's conclusion that neither needs a **new** approval still holds — the
inputs 16.1 and 16.4 describe are undefined behaviour producing demonstrably wrong answers, which
is the compatible-narrowing argument §4.3 already states and which each ticket must restate for
itself. What changed is that the argument is now larger: it must cover `Parse` as well as
`TryParse`, and `AddYears`'s identity-result as well as the rejections.

---

## 17. What #1836 measured, and four corrections to §2 case 15 (2026-07-30)

#1836 is the family's only member in **two** classes, and both halves are now implemented.
Evidence: `build-probe/1836_timespan_surface.cpp`, `build-probe/1836_prefix.log` and
`build-probe/1836_postfix.log`, nineteen cases, **one process per case**, recovering build for
enumeration and `-fno-sanitize-recover=undefined` afterwards to prove each site gone, linked
against a `build-asan` tree whose `TimeSpan.cpp.o` and `libsharp_runtime_core.a` were both
verified newer than the source before **and** after the edit.

### 17.1 The class A half was exactly as §4.1 predicted

`TimeSpan::Subtract`'s single expression became an unsigned subtraction converted back, and
`.NET`'s `operator -(TimeSpan, TimeSpan)` (`TimeSpan.cs:877-879`) is the specification: the wrap
is intended and the sign-bit test that follows is the real guard. **Three** public doors reach
that line, not the one §2 lists — `Subtract`, `operator-(TimeSpan)`, and `Subtract` reached
through `operator-` — and all three now produce the same `OverflowException` with the same
message, with no diagnostic. `TimeSpan::Add`, `Negate()` and `operator-()` were inventoried at
the same time and are **clear**: each validates before it computes. A count that does not move
is recorded as deliberately as one that does (§14.2).

### 17.2 The `TryParse` half is FOUR undefined columns, not one

§2 case 15 names `TimeSpan.cpp:454`, and the finding was written as one multiplication. Running
the *recovering* build one input at a time shows the same statement is undefined at **four
distinct columns**, reached by different inputs:

| Column measured before the repair | Input that reaches it |
|---|---|
| `:454:53` — `days * TicksPerDay` | `"2147483647.00:00:00"` (the audited input) |
| `:454:16` — the accumulation of the five terms | `"-10675199.02:48:05.4775808"` |
| `:457:22` — a second accumulation in the same chain | `"10675199.02:48:06"` |
| `:459:29` — `ticks = -ticks`, negating the int64 minimum | `"-10675199.02:48:05.4775808"` |

A repair aimed only at the day product would have left three live. This is an addition to
SR-AUD-008's surface, recorded by appending; the finding keeps its identifier.

### 17.3 A fifth undefined operation in the same function, from the C library

`std::sscanf`'s behaviour when a `%d` conversion produces a value the target object cannot
represent is **undefined** (C17 7.21.6.2p10), and it was measured wrapping silently:
`"2147483648.00:00:00"` reached the tick arithmetic as `-2147483648`, and
`"99999999999999999999.00:00:00"` as `-1` — the latter parsing *successfully* as minus one day.
The repair reads each component as `long long` and pre-rejects any decimal run longer than
eighteen digits, which makes every conversion representable and therefore defined; .NET
classifies both inputs as overflow ("contains too many digits") and so does this port now.
This is a **different cause** from CCF-004's signed arithmetic and is fixed here only because
it is in the same expression's operand chain; it is not a licence to sweep `sscanf` use
elsewhere.

### 17.4 A third silent wrong answer, and it has no undefined behaviour at all

§2.2 said `TryParse` was the only silent-wrong-answer member; §16.4 corrected that to two by
adding `DateOnly::AddYears`. There is a **third**, and it is the one the plan's method could
not have found, because **UBSan reports nothing for it**: `"--5.00:00:00"` was accepted and
returned the **positive** five-day duration. The leading `-` is consumed as the sign, `sscanf`
then reads `-5` as the day count, and the final `if (negative) ticks = -ticks` cancels the two.
Nothing overflows. The repair rejects a negative day count as a **malformed string**
(`FormatException`), which is how .NET classifies a second sign character — its tokenizer never
produces a signed component. `"-10675199.02:48:05.4775809"`, one tick past `MinValue`, is a
fourth: it returned the **positive** `MaxValue`.

**Method correction that generalises:** a UBSan sweep enumerates *undefined* operations, not
*wrong answers*. Two of this member's four wrong answers are invisible to it. Enumerate the
inputs a corrected implementation must reject, then check each one, rather than trusting the
diagnostic list to be the defect list.

### 17.5 The negative direction accepts one more magnitude than the positive one

`"-10675199.02:48:05.4775808"` — `TimeSpan::MinValue`'s canonical string — must keep parsing,
and before the repair the *right* answer came out of **two** undefined operations. .NET's
asymmetry is deliberate: `TryTimeToTicks` rejects a wrapped-negative result only `if (positive)`
(`TimeSpanParse.cs:612-618`) and `ProcessTerminal_*` then rejects the negated value only
`if (ticks > 0)` (`:816-822`), so exactly one magnitude — 2^63 — survives both tests. The port
now states that as one explicit bound per sign. Both endpoint strings are pinned by tests.

### 17.6 The class C compatibility argument, restated for this ticket

Four inputs change from success to failure:

| Input | Before | After |
|---|---|---|
| `"2147483647.00:00:00"` | `true`, ticks `-7695280436664713216` | `false` / `OverflowException` |
| `"10675200.00:00:00"` | `true`, ticks `-9223371273709551616` | `false` / `OverflowException` |
| `"--5.00:00:00"` | `true`, **+5 days** | `false` / `FormatException` |
| `"-10675199.02:48:05.4775809"` | `true`, **+`MaxValue`** | `false` / `OverflowException` |

Every one is a value wrong by any definition — a negative duration from a positive input, a
positive one from a negative input, or a sign silently cancelled. Three of the four are produced
by undefined behaviour and so are not guaranteed between two builds of this repository; the
fourth (`"--5.00:00:00"`) is not undefined but is unambiguously not what the string says, and
.NET rejects it. **The rejected inputs never worked**, which is the argument the batch accepted
for #1817, #1818 and #1825 and which §4.3 states for this class. No new approval is required.

`Parse` is a **behaviour change on the throwing entry point too**, as §16.1 warned: it used to
*return* the wrapped value rather than raise anything.

### 17.7 What deliberately did NOT change

- **No already-rejected input was reclassified.** Splitting `Parse`'s failure into
  `FormatException` and `OverflowException` covers only the newly rejected inputs plus the
  digit-run and day-range cases. An out-of-range hour or minute (`"1.24:00:00"`) stays a
  `FormatException`, even though real .NET calls it an overflow — changing that would be an
  exception-type change on a path with no defect, which this ticket does not authorise. It is
  worth a separate ticket, and none was opened because no evidence of harm exists.
- **`TimeSpan.hpp` is unchanged.** The shared parse core is a `static` function with internal
  linkage in `TimeSpan.cpp`, so no declaration, member, signature, symbol or layout changes,
  and §8's table still holds.
- **The accepted format grammar is unchanged.** `sscanf` still decides the *structure*, so every
  quirk of what is accepted stays as it was; only the component widths and range checks changed.

### 17.8 The public surface, inventoried

`Parse` and `TryParse` are the only entry points in `TimeSpan` itself. One further public door
exists **in another module**: `System::Xml::XmlConvert::ToTimeSpan` forwards straight to
`TimeSpan::Parse`, so it too returned a wrapped duration and now surfaces the
`OverflowException`. It is pinned by a test in `modules/xml`. Its separately documented gap —
that it parses .NET's native colon format rather than the XML Schema `duration` lexical form —
is untouched.

SR-AUD-008's site count is therefore **six** (one in `Subtract`, four undefined columns and one
undefined `sscanf` conversion in the parse core), against the two §2 records, and its public-door
count is **five** (`Subtract`, `operator-`, `TryParse`, `Parse`, `XmlConvert::ToTimeSpan`).

---

## 18. What #1837 measured, and the paramName decision (2026-07-30)

#1837 is the family's **last** member and its second class C. All seven sites §2.1 predicted
are confirmed and gone, and the two additions §16 could only reproduce are now fixed. Evidence:
`build-probe/1837_dateonly_surface.cpp`, `build-probe/1837_prefix.log` and
`build-probe/1837_postfix.log`, ten cases, **one process per case**, against a `build-asan` tree
whose `DateOnly.cpp.o` and `libsharp_runtime_core.a` were verified newer than the source both
before (07:45) and after (relinked 07:50 the same session) the edit — recovering for enumeration,
`-fno-sanitize-recover=undefined` afterwards, every case exit 0.

### 18.1 The representation gap that shapes the whole repair

.NET `DateOnly` stores a `uint _dayNumber` and does its arithmetic on it. **This port stores
`year_`/`month_`/`day_`** and converts through Julian day numbers (`dateToJDN`/`jdnToDate`). So
.NET's idiom could not be copied byte-for-byte: it had to be *translated*. `FromDayNumber` and
`AddDays` are genuine day-number operations and take the unsigned-compare idiom directly;
`AddMonths` and `AddYears` are month/year operations and take .NET's `DateTime.AddMonths`/`AddYears`
bounds-then-divide idiom, which this repository's own `DateTime::AddMonths`
(`DateTime.cpp:183-201`) already implements. `kMaxDayNumber` was **measured** at 3652058, equal to
.NET's `DateTime.DaysTo10000 - 1`, not assumed.

### 18.2 The seven sites, all gone, and the cascade proven unreachable

`:65` (`FromDayNumber`) and `:76` (`AddDays`) are removed by adding in the *unsigned* domain and
rejecting with a single unsigned compare **before** `jdnToDate` is reached, so `:35`/`:37`/`:39`
can no longer see an out-of-range argument — the cascade is unreachable by construction, not by a
per-site guard. `:81` (`AddMonths`) and `:92` (`AddYears`) are removed by bounding the delta before
the arithmetic. §16.3's finding — that `FromDayNumber(INTCS_MIN)` reaches the cascade **without**
an entry-point overflow — is why an entry-point-only overflow guard would have been insufficient;
the day-number range check is what closes it.

### 18.3 The bounded-loop requirement (16.5) is met by removing the loop

`AddMonths`'s two `while` loops (~179 million iterations for `AddMonths(INTCS_MIN)`) are replaced by
.NET's single division `q = (m > 0) ? (m - 1)/12 : m/12 - 1`. There is no loop left to bound.

### 18.4 The two silent wrong answers (16.4), now rejected

`AddYears(INTCS_MIN)` returned `0001-01-01` unchanged (the `*12` wrapped to zero); it now throws.
`AddMonths(INTCS_MIN)` was not a wrong answer — it eventually threw — but was the unbounded loop.
Both are covered by the class C compatibility argument (§4.3, §9): the rejected inputs never
produced a usable value, three of the four via undefined behaviour and the fourth via a silent
wrap, so no new approval is required.

### 18.5 The paramName decision, stated and justified

§4.3 left this to the ticket, and §16.6 measured the before-string: **every** current rejection
named `year` with `"DateTime: date component out of range (Parameter 'year')"`, because it came
from the `DateTime` constructor rather than from `DateOnly`. The decision taken is to adopt
**.NET's per-method paramNames** — `dayNumber` (`FromDayNumber`), `value` (`AddDays`, `AddYears`),
`months` (`AddMonths`) — for three reasons: (1) maximum practical parity is the project mission
and .NET names the actual parameter; (2) the inherited `year` was a *leaked implementation detail*
of the delegated-to `DateTime` constructor, not a deliberate `DateOnly` contract; (3) the exception
**type** — `ArgumentOutOfRangeException` — is unchanged, so only the `paramName` string and the
message text differ, and only on inputs that are rejected either way. Messages follow the sibling
`DateTime::AddMonths`/`AddYears` (`"DateTime: months out of range"`, `"DateTime: resulting year out
of range"`, `"DateTime: years out of range"`) for consistency within this repository, and .NET's
`SR.ArgumentOutOfRange_DayNumber`/`_AddValue` verbatim for the two day-number doors that have no
`DateTime` sibling.

**One deliberate, documented consequence.** A *moderately* out-of-range input — one whose result
leaves `[0001,9999]` but whose arithmetic never overflowed (e.g. `AddMonths(200000)`,
`AddYears(50000)`) — used to throw `year` via the `DateTime` constructor and now throws `months`
or `value`. That is a `paramName`/message change on a **non-UB rejection path**. It is authorised
by this ticket's acceptance criterion ("a stated, justified paramName choice") and by §4.3, it
changes no input from success to failure, and it changes no exception **type**. No in-repository
test pinned the old `year` string for such an input (the only Add* tests covered small valid
deltas), so nothing regressed.

### 18.6 One residual .NET divergence, recorded not fixed

.NET's `DateOnly.AddYears` reports `value` for an unrepresentable **result** too, via
`DateTime.AddYears`'s `ThrowDateArithmetic(0)`. This port's `AddYears` reproduces that by
range-checking the resulting year itself with paramName `value` before delegating the day-clamp to
`AddMonths`. `AddMonths`'s own result check still reports `months`, matching .NET's
`DateOnly.AddMonths`. No divergence remains between the two.

### 18.7 Public surface and family closure

`DateOnly.hpp` is unchanged — no declaration, member, signature, symbol or object layout changed;
`kMaxDayNumber` is a file-local `static constexpr`. Public doors: `FromDayNumber`, `AddDays`,
`AddMonths`, `AddYears`, all pinned by tests. SR-AUD-060 is `remediated`. **With #1836 and #1837
done, every one of CCF-004's eight members is remediated and the family is closed.**
