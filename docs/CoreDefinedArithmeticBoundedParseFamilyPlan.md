<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` defined-arithmetic and bounded-parse family — plan

**Review ticket #2217** (`P1`, `review`, area `core`). Written 2026-08-10.
This document is the review deliverable; no production source changed under #2217 itself.

It issues **no `SR-AUD-*` identifier**. Audit numbering stays frozen at **364**, and all three
members keep status `confirmed` until their own implementation tickets land.

**This is one bounded family of `modules/core`, not a `modules/core` namespace review.** After
this family lands, `modules/core` still has 64 open findings. Nothing here closes the module.

---

## 1. Exact scope

| Finding | Severity | Status at the open of #2217 | Owning report |
|---|---|---|---|
| **SR-AUD-131** | high | `confirmed` | `audit/modules/core/include/System/Diagnostics/Stopwatch.hpp.audit.md` (+ the extension recorded in `audit/modules/threading/include/System/TimeProvider.hpp.audit.md`) |
| **SR-AUD-135** | high | `confirmed` | `audit/modules/core/include/System/Linq.hpp.audit.md` |
| **SR-AUD-180** | high | `confirmed` | `audit/modules/core/include/SharpRuntime/PortableFromChars.hpp.audit.md` |

Verified independently at the open of this ticket rather than inherited: the index was re-parsed
**by finding identifier** (SR-AUD-029 carries a seventh table column, so a fixed-six-column parser
is wrong), giving **178 remediated / 131 confirmed / 55 confirmed (design-complete) / 364 total**.
All three members are `high` and `confirmed`; none is remediated by later work; `plan.sqlite3`
contains **no** other ticket naming any of them, so #2217 is not stale; no ticket was `doing`.

The inherited handoff's four claims were each checked and **three hold, one does not**:

- *all three high* — **holds**;
- *not blocked, no live owner* — **holds**;
- *independent of the absent `/rv` reference tree* — **holds** (`/rv` does not exist here; every
  conclusion below rests on local source, the C++ standard, or measurement);
- *all three sanitizer-decidable* — **does not hold for SR-AUD-180**, and does not hold for half of
  SR-AUD-131 under the default sanitizer set. See §5.1 and §5.2.

---

## 2. Affected files

| File | Member | Kind |
|---|---|---|
| `modules/core/include/System/Diagnostics/Stopwatch.hpp` | SR-AUD-131 | header-only inline |
| `modules/threading/include/System/TimeProvider.hpp` | SR-AUD-131 | header-only inline |
| `modules/core/include/System/Linq.hpp` | SR-AUD-135 | header-only template |
| `modules/core/include/SharpRuntime/PortableFromChars.hpp` | SR-AUD-180 | header-only template |

Every production body in this family is **header-only and inline**. That is what makes the
sanitizer methodology of §5 sound: compiling the probe translation unit with the sanitizers
instruments the actual production code, and there is no separate implementation archive that
could be stale or uninstrumented (the same argument `build-probe/2210_san_compile.sh` records for
the preceding memory-safety family).

`modules/threading` is a second module. SR-AUD-131 is therefore a **two-module** finding; its
owning report lives under `modules/core`, and the `TimeProvider` half is recorded as an
*extension* in the threading report rather than as a separate finding, exactly as that report says.

---

## 3. Complete public-door inventory

The audit's site counts are not authoritative and were re-derived from source. Doors that were
inventoried and found **clear** are recorded as deliberately as the defective ones
(`docs/DefinedArithmeticBoundaryPlan.md` §14.2: a count that does not move has to be shown to have
been checked).

### 3.1 SR-AUD-131

| Public door | Reaches | Verdict |
|---|---|---|
| `Stopwatch::GetElapsedTime(longcs, longcs)` | `Stopwatch.hpp:133` subtraction | **defective** (cases S1, S3, S4) |
| `Stopwatch::GetElapsedTime(longcs)` | the same line, `ending = GetTimestamp()` | **defective** (case S6) |
| `Stopwatch::Stop()` | `Stopwatch.hpp:53` `elapsed_ns_ +=` | same shape, **not publicly reachable** (§4.4) |
| `Stopwatch::getElapsedMillisecondsProperty()` / `getElapsedTicksProperty()` / `getElapsedProperty()` / `ToString()` | `Stopwatch.hpp:145` `ns +=` via `currentNs()` | same shape, **not publicly reachable** (§4.4) |
| `Stopwatch::GetTimestamp()` | `/ 100LL` on a `steady_clock` count | clear — no overflow, no `INT64_MIN / -1` |
| `TimeProvider::GetElapsedTime(longcs, longcs)` | `TimeProvider.hpp:74:34` subtraction **and** `:74:55` conversion | **defective, twice** (cases T1, T2, T4) |
| `TimeProvider::GetElapsedTime(longcs)` | the same two sites | **defective** (case T6) |
| `TimeProvider::GetLocalNow()` | `TimeProvider.hpp:43` `utcTicks + offsetTicks` | **clear** — the ticks domain is `[0, 3155378975999999999]` and the offset domain `±14 h`, so the sum cannot leave `int64`; the clamp that follows already uses CCF-004's own unsigned-compare idiom |
| `TimeProvider::getTimestampFrequencyProperty()` (virtual, overridable) | supplies `freq` | not itself defective, but a **caller-controlled** input to the defective expression — an override may return any positive `int64` (case T4) |

`Stopwatch::Frequency` is `constexpr` and is not an arithmetic door. `SR-AUD-130` (the fabricated
10 MHz frequency) is in the same report and is **out of scope** — see §19.

### 3.2 SR-AUD-135

| Public door | Reaches | Verdict |
|---|---|---|
| `Linq::Sum<T>(const std::vector<T>&)` | `Linq.hpp:236` `result = result + item` | **defective** for signed integral `T` (cases L1, L2, L3, L9) |
| `Linq::Sum<T,R>(const std::vector<T>&, std::function<R(const T&)>)` | `Linq.hpp:249` | **defective** for signed integral `R` (case L6) — a **second** site the finding's wording covers but does not name by line |
| `Linq::Count<T>(const std::vector<T>&, predicate)` | `Linq.hpp:216` `++n` on `intcs` | inventoried, **excluded** (§19.3) |
| `Linq::Count<T>(const std::vector<T>&)` | `static_cast<intcs>(source.size())` | inventoried, **excluded** — a narrowing conversion, well defined since C++20, not this cause (§19.3) |
| `Linq::Skip` / `Linq::Take` | `source.begin() + count` | **clear** — both reject `count <= 0` before the cast and clamp against `source.size()` |
| `Linq::Min` / `Max` / `Distinct` / `Contains` / `OrderBy` | no arithmetic | clear (their floating-comparison half is the already-remediated SR-AUD-046 / CCF-010) |

### 3.3 SR-AUD-180

| Public door | Reaches | Verdict |
|---|---|---|
| `SharpRuntime::PortableFromCharsFloat<T>(first, last, value)` | the `strtof`/`strtod` call | **defective** — the direct door, and the one a test can reach on every platform |
| `SharpRuntime::FromCharsFloat<T>(first, last, value)` | the above, **only** where the platform's `std::from_chars` has no floating overload | defective on that platform selection |
| `System::Single::TryParse` / `Parse` | `FromCharsFloat` from `tryParseCore` | passes a **subrange** — see the premise correction in §5.3 |
| `System::Double::TryParse` / `Parse` | the same | passes a **subrange** |
| `System::Xml::XPath` `number()` (`XPathAstInternal.cpp:521`) | the same | passes the **whole** `std::string`; this one door does match the header's own comment |

---

## 4. Root-cause grouping — two subfamilies, not one, and three repair classes

The inherited label *"defined arithmetic and bounded parse"* is accurate: it names two things, and
there really are two. Forcing one abstraction over them would be wrong.

### 4.1 DAB-A — a public .NET-shaped operation computed with undefined C++ arithmetic

**SR-AUD-131 and SR-AUD-135.** Both are the cause CCF-004 records: an operation whose reference
semantics are *defined* (C# unchecked two's-complement wrap) or *checked* (C# `checked`, raising
`OverflowException`), implemented with a raw signed C++ operator whose overflow is undefined.

They are **not** interchangeable, because CCF-004's own taxonomy splits them:

| Member | .NET does | CCF-004 class | Observable change |
|---|---|---|---|
| SR-AUD-131 (subtraction, both types) | unchecked wrap, result intended | **A** | **none** — every value byte-identical |
| SR-AUD-131 (`TimeProvider` conversion) | a defined out-of-range float→int result | **C** | **yes** — a previously undefined result becomes a saturated one |
| SR-AUD-135 | `checked` accumulation, raises `OverflowException` | **C** | **yes** — a wrapped success becomes a throw |

### 4.2 DAB-B — an unbounded C parser handed a bounded range

**SR-AUD-180.** No arithmetic is involved at all. `strtof`/`strtod` take no `last`; they scan to a
NUL. The helper hands them `first` and simply discards `last`, so the parse runs past the declared
range into whatever storage follows. This is a **memory-range contract** defect, and its repair
(copy the range into a NUL-terminated buffer, then map the end pointer back) shares nothing with
DAB-A's repair. Grouping it under CCF-004 would be a category error.

### 4.3 Why DAB-A and DAB-B are nevertheless one *batch*

They share a container (`modules/core` header-only bodies), a severity, an evidence method (one
process per case against instrumented production bodies), and a completion gate. They do not share
a cause and this plan never claims they do.

### 4.4 Two sites that are real and not publicly reachable

`Stopwatch::Stop()` and `Stopwatch::currentNs()` accumulate `steady_clock` nanoseconds into a
signed `longcs`. Reaching `INT64_MAX` needs roughly **292 years** of accumulated measured interval,
and `elapsed_ns_` is private with no setter, so no caller can seed it. The audit names the pattern
("the same unchecked addition pattern exists while accumulating a very long running/stopped
interval") and it is converted anyway, because the conversion is free and changes no reachable
value. This is recorded as *hardening*, not as a reproduced defect: **no probe case demonstrates
it, and none is claimed to.**

---

## 5. Before evidence, measured 2026-08-10

Probe `build-probe/2217_probe_before.cpp`, driver `build-probe/2217_san_compile.sh`, logs
`build-probe/2217_before.log` (37 cases), `build-probe/2217_before_fco.log` and
`build-probe/2217_guard.log`.

Method, and every clause of it is load-bearing:

- **the actual production body is instrumented** — all four files are header-only, so the probe
  translation unit *is* the production code; the archives supply only `TimeSpan`, `TimeZoneInfo`
  and the exception types;
- **`-O0`, not `-O1`** — `docs/DefinedArithmeticBoundaryPlan.md` §3 cause 2 records GCC folding a
  header-inline overflow at `-O1` and emitting no check at all;
- **every operand routed through a `volatile`**, so the optimisation level cannot decide the answer;
- **one process per case** — UBSan deduplicates by source location, so several shapes sharing a
  line read as "already fixed" if run together (§13.2 of the same plan);
- **activation proved, not assumed** — `nm -C` shows 21 `__asan_*` and 8 `__ubsan_handle_*`
  imports, including `__ubsan_handle_sub_overflow` and `__ubsan_handle_add_overflow`.

### 5.1 SR-AUD-131 — measured

| Case | Input | Diagnostic | Value returned |
|---|---|---|---|
| S1 | `Stopwatch::GetElapsedTime(INT64_MIN, INT64_MAX)` | `Stopwatch.hpp:133:83 signed integer overflow: 9223372036854775807 - -9223372036854775808` | `ticks=-1` |
| S2 | `(0, INT64_MAX)` | **none** | `ticks=9223372036854775807` |
| S3 | `(INT64_MAX, INT64_MIN)` | same line | `ticks=1` |
| S4 | `(-1, INT64_MAX)` | same line | `ticks=-9223372036854775808` |
| S5 | `(1000, 3000)` | none | `ticks=2000` |
| S6 | `GetElapsedTime(INT64_MIN)` — the one-argument door | same line | negative |
| T1 | `TimeProvider::GetElapsedTime(INT64_MIN, INT64_MAX)` | `TimeProvider.hpp:74:34 signed integer overflow` | `ticks=-1` |
| T2 | `TimeProvider::GetElapsedTime(0, INT64_MAX)` | **none under `-fsanitize=undefined`** | `ticks=-9223372036854775808` |
| T3 | `(1000, 3000)` | none | `ticks=2000` |
| T4 | frequency `1`, `(0, 1e12)` | **none under `-fsanitize=undefined`** | `ticks=-9223372036854775808` |
| T5 | frequency `0` | control: `InvalidOperationException`, thrown **before** any arithmetic | — |
| T6 | `TimeProvider::GetElapsedTime(INT64_MIN)` | `:74:34` | large negative |

#### 5.1.1 Premise correction — `TimeProvider` has a **second**, independent undefined operation, and the default sanitizer set cannot see it

T2 and T4 return `-9223372036854775808` — a maximal *negative* duration for a maximal *positive*
interval — with **no diagnostic at all**. The reason is not that the code is correct. GCC's
`-fsanitize=undefined` **does not include `-fsanitize=float-cast-overflow`**. Re-running the same
three cases with it enabled (`build-probe/2217_before_fco.log`) gives:

```
modules/threading/include/System/TimeProvider.hpp:74:55: runtime error:
    9.22337e+18 is outside the range of representable values of type 'long int'      (T2)
modules/threading/include/System/TimeProvider.hpp:74:55: runtime error:
    1e+19 is outside the range of representable values of type 'long int'            (T4)
```

So `TimeProvider::GetElapsedTime` carries **two** undefined operations at **two different columns
of one line**: the signed subtraction at `:74:34` and the out-of-range `double`→`long` conversion
at `:74:55` ([conv.fpint]/1). T2 reaches the second **without** reaching the first — the subtraction
`INT64_MAX - 0` does not overflow — so a repair aimed only at the subtraction would leave a silent
wrong answer on the default system provider.

**Method correction that generalises:** *a UBSan sweep run with the default `-fsanitize=undefined`
set is not a sweep of undefined behaviour.* `float-cast-overflow` and `float-divide-by-zero` are
outside GCC's default group. Any future ticket in this repository that claims "UBSan clean" for a
site with a floating→integral conversion must name the sub-check it enabled.

#### 5.1.2 Premise confirmation — the audit's wording is right about the shape, narrow about the doors

The audit names "the static two-timestamp overload". Measured, **four** public doors reach a
defective site (`Stopwatch` ×2, `TimeProvider` ×2), and `Stopwatch::GetElapsedTime(0, INT64_MAX)`
is *not* defective while the `TimeProvider` call with the same arguments *is* — because only the
latter routes through a `double`.

### 5.2 SR-AUD-135 — measured

| Case | Input | Diagnostic | Value |
|---|---|---|---|
| L1 | `Sum<int>({INT_MAX, 1})` | `Linq.hpp:236:48 signed integer overflow: 2147483647 + 1` | `-2147483648` |
| L2 | `Sum<long long>({INT64_MAX, 1})` | `:236:48` | `-9223372036854775808` |
| L3 | `Sum<int>({INT_MIN, -1})` | `:236:48` | `2147483647` |
| L4 | `Sum<int>({1,2,3})` | none | `6` |
| L5 | `Sum<double>({DBL_MAX, DBL_MAX})` | none | `inf` |
| L6 | `Sum<int,int>(selector, {INT_MAX, 1})` | **`Linq.hpp:249:48`** — the selector overload, a second site | `-2147483648` |
| L7 | `Sum<unsigned>({UINT_MAX, 1})` | none | `0` (defined wrap) |
| L8 | `Sum<short>({SHRT_MAX, 1})` | none | `-32768` (integral promotion; defined) |
| L9 | `Sum<int>({INT_MAX, INT_MAX, INT_MIN})` | `:236:48` | `2147483646` |
| L10 | `Sum<int>({})`, `Sum<int>({7})` | none | `0`, `7` |

#### 5.2.1 Premise correction — L9 is the case the finding's framing misses

`Sum<int>({INT_MAX, INT_MAX, INT_MIN})` executes undefined behaviour at the *first* addition and
then returns **`2147483646`, which is the mathematically correct total.** The conceptual result is
representable; only the intermediate is not. That matters twice:

1. it proves the defect is not "the answer is wrong" but "the program has no defined meaning" — a
   guard placed *after* the addition would find nothing to complain about here;
2. it is the one case where the repair makes a currently *right* answer start throwing, because
   .NET's `Enumerable.Sum` is checked **per element** (`Sum.cs`) and raises `OverflowException` at
   the same intermediate. The repair is deliberately faithful to the reference rather than to the
   final total, and §11 carries that as a stated behaviour change.

#### 5.2.2 Premise correction — the domain is wider than "signed"

L7 and L8 show two shapes the finding lumps in with the defect that are **not undefined**:
unsigned arithmetic wraps by definition, and `short`/`signed char` are promoted to `int` before the
addition, so the addition itself cannot overflow and only the narrowing store is lossy (well
defined since C++20). Both currently produce a wrong *value* without any undefined behaviour. The
finding's own remediation note already asks for this split — "separately specify unsigned/floating/
custom type behavior rather than inheriting native overflow" — and §8 states it.

### 5.3 SR-AUD-180 — measured, and it is **not** ASan-decidable

| Case | Range | `PortableFromCharsFloat` | `std::from_chars` on the same range |
|---|---|---|---|
| P1 | `new char[2] = {'1','2'}`, `[0,1)` | `ec=0 ptr_offset=2 value=12` | — |
| P2 | `"12"`, `[0,1)` (the audit's own case) | `ec=0 ptr_offset=2 value=12` | — |
| P3 | `"12"`, `[0,1)` | — | `ec=0 ptr_offset=1 value=1` |
| P12 | `"1e3"`, `[0,1)` | `ec=0 ptr_offset=3 value=1000` | `ec=0 ptr_offset=1 value=1` |
| P13 | `new char[3] = {'-','5','7'}`, `[0,2)` | `ec=0 ptr_offset=3 value=-57` | — |
| P14 | `"12"`, `[0,1)`, `float` overload | `ec=0 ptr_offset=2 value=12` | — |
| P5–P11 | full-range controls | unchanged and correct | — |

#### 5.3.1 Premise correction — AddressSanitizer reports **nothing**, and the reason generalises

Case P1 places `"12"` in a two-byte heap allocation with **no NUL anywhere in it**, so `strtod`
must read `p[2]`, one past the allocation. ASan is **silent**: the read happens inside glibc's
`strtod`, which is neither instrumented nor an ASan interceptor. The inherited handoff's claim that
SR-AUD-180 is "ASan-decidable" is therefore **wrong**, and a batch that trusted it would have
concluded the finding was already fixed.

A hardware guard page does see it. `build-probe/2217_probe_guard.cpp` maps two pages, `mprotect`s
the second `PROT_NONE`, and places `"12"` in the **last two bytes** of the readable page so that
`last` is exactly the guard boundary (`build-probe/2217_guard.log`):

```
mode=std      -> survived ec=0 ptr_offset=2 value=12      (std::from_chars respects `last`)
mode=fallback -> Segmentation fault (exit 139)            (the fallback reads at `last`)
```

That is the whole finding in two lines, with no sanitizer involved: the standard function stops at
`last`; this one does not, and the read past it is a real fault, not a theoretical one.

#### 5.3.2 Premise correction — the header's own comment is false, and two of the three call sites do pass a subrange

`PortableFromChars.hpp:31-36` states that "every real call site here passes `s.data()`/`s.data() +
s.size()` from a `std::string`", which is why the audit records the three call sites as safe today.
Measured (case P15, replicating `Single::tryParseCore`'s own trim verbatim), for the input `" 1.5 "`:

```
first_offset=1  last_offset=4  string_size=5  last_is_terminator=0  char_at_last=32
```

`Single::tryParseCore` and `Double::tryParseCore` trim leading and trailing ASCII whitespace
(ticket #1864) **before** forming `first`/`last`, so whenever the input carries surrounding
whitespace the helper is handed a **subrange whose `last` is not the string terminator**. Only the
XPath door still matches the comment.

This makes the defect **latent rather than merely hypothetical**, and the distinction is worth
stating precisely rather than overstating it: the trimmed-away characters are whitespace, and the C
parser stops at whitespace of its own accord, so **no in-repository input is known to produce a
different value through `Single`/`Double` today**. What changed is the safety argument — it no
longer rests on "the range is always the whole string", which is false, but only on "the character
immediately after the range happens to be one the C parser would stop at anyway", which is a
property of the *input*, not of the *contract*.

#### 5.3.3 A second, different divergence in the same helper — recorded, and given its own ticket

Case P4 measures the same helper on `"0x10"`:

```
P4a fallback "0x10" -> ec=0 ptr_offset=4 value=16
P4b std      "0x10" -> ec=0 ptr_offset=1 value=0
```

`strtod` accepts C99 hexadecimal floating literals; `std::from_chars` with `chars_format::general`
explicitly does not. This is a **grammar** divergence, not a range one — a different cause in the
same fifteen-line function — so it is **not** folded into SR-AUD-180's repair and carries no
`SR-AUD-*` identifier. Ticket **#2222**, after #2221.

---

## 6. Relationship to CCF-004 — an occurrence, not a member; the family stays closed

`docs/DefinedArithmeticBoundaryPlan.md` §18.7 records CCF-004 as **closed, 8/8**, and
`audit/AUDIT_CROSS_CUTTING_FINDINGS.md` has since established the settled convention for a new site
of a closed cause **four times**: T-F (`SR-AUD-206`, ticket #1947), T-A (`SR-AUD-286`), T-C
(`SR-AUD-295`) and N-B (`SR-AUD-301`/`SR-AUD-307`). In every case the site inherits CCF-004's
selected repair, CCF-004 **gains no member**, no `SR-AUD-*` identifier is issued, and the family is
not reopened.

This family follows that convention exactly, and it earns it against the brief's three membership
tests rather than merely because UBSan reported a signed overflow:

| Test | SR-AUD-131 | SR-AUD-135 | SR-AUD-180 |
|---|---|---|---|
| same structural cause | yes — a public .NET-shaped operation whose defined arithmetic is computed with signed C++ overflow | yes | **no** — no arithmetic at all |
| same repair policy | yes — CCF-004 §4.1's unsigned-domain transformation, applied verbatim | yes — CCF-004 §4.3's "range-check before the arithmetic, then throw" | **no** — a range copy |
| same validation / unchecked semantics | yes — .NET's own model is *unchecked* here (class A) | yes — .NET's own model is *checked* here (class C) | **n/a** |

So **SR-AUD-131 and SR-AUD-135 are recorded as CCF-004 occurrences; SR-AUD-180 is not, and is not
adjacent to it either.** One refinement, and it has its own precedent: the `TimeProvider`
float→int half is **CCF-004's shape one layer up** — an undefined *conversion* rather than an
undefined *arithmetic operation* — which is exactly how N-B is framed in the cross-cutting record.
It is recorded the same way and for the same reason.

**No policy issue arises and nothing is rewritten.** CCF-004's closure condition was "every one of
its eight members is remediated", which remains true and is untouched by a site outside its
membership list. `docs/DefinedArithmeticBoundaryPlan.md` is **not edited** by this family; this
document is the durable record of these two occurrences, and CCF-021/CCF-022 stay unminted (the
cross-cutting numbering is closed).

---

## 7. Parsing contract for `PortableFromCharsFloat` after #2221/#2222

Stated once, so the tests have something to pin:

1. The function is a drop-in for `std::from_chars(first, last, value)` with
   `chars_format::general`. **No character at or beyond `last` is ever read.**
2. `[first, last)` is a half-open range. `first == last` → `{first, errc::invalid_argument}`.
3. A leading `'+'` or any leading whitespace → `{first, errc::invalid_argument}` (already true;
   `std::from_chars` accepts neither).
4. On success, `ptr` is the first character **not** consumed and satisfies
   `first < ptr <= last`. `value` is written only on success.
5. On `errc::result_out_of_range`, `value` is **not** written and `ptr` is the end of the matched
   pattern, as `std::from_chars` specifies.
6. On `errc::invalid_argument`, `ptr == first` and `value` is not written.
7. Trailing input is **permitted and not consumed** — the helper reports how much it read and the
   caller decides (both `Single`/`Double` and XPath already require `ptr == last`). Prefix-only
   parsing is the standard's contract here, not a defect.
8. A hexadecimal prefix (`0x`/`0X`, with or without a leading `-`) is **not** part of
   `chars_format::general`; the leading `0` is consumed and the parse stops at the `x` (#2222).
9. The function is `noexcept`, as `std::from_chars` is. Its one non-standard `errc`,
   `errc::not_enough_memory`, is reachable only when a copy of the caller's own range cannot be
   allocated; every caller in this repository treats any non-`errc{}` value as failure.

---

## 8. Numerical contract for `Linq::Sum` after #2220

| `T` (or `R`) | Behaviour | Reference |
|---|---|---|
| signed integral, excluding `char` and `bool` | **checked**: `System::OverflowException` on any intermediate that leaves the type's range | .NET `Enumerable.Sum(int/long)` accumulates in a `checked` context |
| unsigned integral | unchanged — defined modular wrap | .NET has **no** unsigned `Sum` overload, so there is no reference to match, and the current behaviour is defined rather than undefined |
| floating point | unchanged — native IEEE accumulation, `inf` on overflow | .NET's `float`/`double` `Sum` is **not** checked either |
| `char`, `bool` | unchanged | `char` signedness is implementation-defined, so a throw here would be platform-dependent; .NET's `char` is not a summable numeric type |
| any other `T` with `operator+` | unchanged — `a + b` | the operator's own contract; the port must not impose one |

`char` is excluded deliberately and by name, because including it is the one choice that would make
this port's observable behaviour differ between x86-64 and AArch64.

---

## 9. Dependency graph and implementation order

```
#2218  Stopwatch          (SR-AUD-131 half 1)  ──┐
#2219  TimeProvider       (SR-AUD-131 half 2)  ──┴──> SR-AUD-131 remediated
#2220  Linq::Sum          (SR-AUD-135)         ─────> SR-AUD-135 remediated
#2221  PortableFromChars range bound (SR-AUD-180) ──> SR-AUD-180 remediated
#2222  PortableFromChars grammar (no SR-AUD)   ─────> depends on #2221 (same function body)
```

Only one hard dependency exists (#2222 after #2221). #2218 and #2219 are independent of each other
but **SR-AUD-131 is not remediated until both land** — the same rule CCF-004 §5 states for its own
multi-site members.

---

## 10. Severity and classification

| Ticket | Finding | Subfamily | Class | Compatible? | Approval needed |
|---|---|---|---|---|---|
| #2218 | SR-AUD-131 | DAB-A | A | **yes** — no value changes | no |
| #2219 | SR-AUD-131 | DAB-A | A + C | **yes** — the changed values were undefined | no |
| #2220 | SR-AUD-135 | DAB-A | C | **yes** — the changed results were undefined | no |
| #2221 | SR-AUD-180 | DAB-B | — | **yes** — the changed results were out-of-range reads | no |
| #2222 | none | DAB-B | — | **yes** — no in-repository input reaches it | no |

**Nothing in this family is blocked, `needs_user`, or design-only.** All five tickets are
compatible-ready, which is the property that made this family the measured selection.

### 10.1 The compatibility argument, stated once and restated per ticket

Classes A change no observable value at all. The class C changes rest on the argument this
repository already accepted for #1817, #1818, #1825, #1836 and #1837: **the rejected or corrected
inputs never worked.** Concretely —

- `TimeProvider::GetElapsedTime(0, INT64_MAX)` returned the most negative representable duration
  for a maximal positive interval, from an undefined conversion whose result is not guaranteed
  between two builds of this repository;
- `Sum<int>({INT_MAX, 1})` returned `-2147483648`, from undefined behaviour, where the reference
  raises;
- `PortableFromCharsFloat("12", …, [0,1))` returned `12` and a pointer two past a one-character
  range, having read storage it was told not to touch — and faults outright against a guard page.

No caller can be depending on any of those, and each implementation ticket restates this for
itself rather than inheriting it.

---

## 11. Source, ABI, layout, vtable and `noexcept` consequences

| Ticket | Public signature | Virtual / vtable | Object layout | Mangled symbols | `noexcept` |
|---|---|---|---|---|---|
| #2218 | unchanged | unchanged | unchanged | unchanged | unchanged (none of the three functions had one) |
| #2219 | unchanged | unchanged | unchanged | unchanged | unchanged; **no member is added** — the saturation is written inline in the existing body |
| #2220 | unchanged | unchanged | unchanged | unchanged | unchanged (`Sum` was never `noexcept`); one new `inline` function template in the existing `System::Linq::detail` namespace |
| #2221 | unchanged | n/a | n/a | unchanged | **`noexcept` is *added*** to `PortableFromCharsFloat` and `FromCharsFloat` |
| #2222 | unchanged | n/a | n/a | unchanged | unchanged |

Two notes, because both are the kind of thing this repository gates:

- **#2221 adds `noexcept`, it does not drop one.** Adding it is a strengthening of the contract: no
  caller that compiled before can fail to compile after, and no expression changes meaning.
  `std::from_chars` is itself `noexcept`, so this is what makes the "drop-in" claim true rather
  than nearly true. It also matters at a real call site: `Single::tryParseCore` and
  `Single::TryParse` **are** `noexcept`, so on the Apple fallback a throwing helper would have
  called `std::terminate`. This is the opposite direction from #2215 (`ArraySegment`) and needs no
  approval; §12's fixture pins it.
- **#2219 adds no member.** The saturating conversion is written inline in `GetElapsedTime`'s body
  rather than as a private static helper, precisely so `sizeof`/`alignof`/vtable are provably
  untouched. `docs/DefinedArithmeticBoundaryPlan.md` §10 exclusion 2 also forbids inventing a
  shared arithmetic helper for this cause, and that exclusion is honoured.

---

## 12. Test matrix

Every ticket adds permanent GoogleTest coverage. Minimum per ticket:

| Dimension | Requirement |
|---|---|
| the exact audited input | the case from §5, asserting the post-fix outcome |
| the pre-fix value, for class A | an equality assertion on the value measured today, so "no observable change" is proven rather than asserted |
| the seven-point signed sweep | `MinValue`, `MinValue+1`, `-1`, `0`, `1`, `MaxValue-1`, `MaxValue` on every changed arithmetic door |
| intermediate-overflow chains | at least one multi-term chain whose intermediate leaves range while the conceptual total does not (case L9) |
| both boundary directions | the largest input that must still succeed **and** the smallest that must now fail |
| the ordinary path | at least one plain non-extreme value per changed function |
| exception identity | type, message and catchability as `System::Exception`, pinned verbatim |
| parser bounds (#2221) | empty, one character, shortest valid, exact longest valid, one past the bound, malformed prefix/suffix, embedded NUL, leading sign, trailing junk, leading/trailing whitespace, very long digit run, all-zero, maximum representable, one above, minimum negative, one below, repeated/missing/unexpected delimiter — each asserting **acceptance, consumption, value, status** separately |
| parser range safety (#2221) | at least one case whose backing storage extends past `last` with different characters, asserting the value and `ptr` a real `std::from_chars` produces |
| layout neutrality | `static_assert` on `sizeof`/`alignof` where a class is touched |

New test files: `modules/core/tests/SharpRuntime/PortableFromCharsTests.cpp`. Existing files
extended: `modules/core/tests/System/Diagnostics/StopwatchTests.cpp`,
`modules/core/tests/System/LinqTests.cpp`, `modules/threading/tests/System/TimeProviderTests.cpp`.

**No existing assertion may be relaxed.** The floor is the inherited gate: 16,605 tests across 38
executables, 16,597 passing.

---

## 13. Sanitizer matrix

| Ticket | UBSan | `float-cast-overflow` | ASan | LSan | TSan | Guard page |
|---|---|---|---|---|---|---|
| #2218 | **yes — primary evidence** | n/a | incidental | n/a | **no** — no shared mutable state | n/a |
| #2219 | **yes** | **yes — mandatory, and it is NOT in GCC's default `-fsanitize=undefined` set** (§5.1.1) | incidental | n/a | **no** | n/a |
| #2220 | **yes — primary evidence** | n/a | incidental | yes (the throwing path allocates an exception) | **no** | n/a |
| #2221 | incidental | n/a | **no — cannot see it** (§5.3.1) | yes (the repair allocates for long ranges) | **no** | **yes — the primary evidence** |
| #2222 | incidental | n/a | incidental | incidental | **no** | incidental |

TSan is **not applicable** to any member: `Stopwatch::GetElapsedTime`, `TimeProvider::GetElapsedTime`,
`Linq::Sum` and `PortableFromCharsFloat` are all pure value computations over caller-owned inputs
with no shared mutable state. (`Stopwatch`'s *instance* state is unsynchronised, which the audit
notes separately and which is **not** in this family's scope — see §19.)

Activation is proved, not assumed, on both sides: `nm -C` for the imported handler symbols, and a
deliberate control that must still fire in the after-run.

---

## 14. Ticket split

| # | Title | Finding | Size |
|---|---|---|---|
| **#2218** | `Stopwatch` elapsed-time arithmetic is defined | SR-AUD-131 (half) | S |
| **#2219** | `TimeProvider` elapsed-time arithmetic and its float→int conversion are defined | SR-AUD-131 (half) | S |
| **#2220** | integral `Linq::Sum` accumulates in a checked domain | SR-AUD-135 | M |
| **#2221** | `PortableFromCharsFloat` never reads at or past `last` | SR-AUD-180 | M |
| **#2222** | the fallback's accepted grammar matches `chars_format::general` | none | XS |

No `SR-AUD-*` identifier is created by any of them.

---

## 15. Mutation requirements

Each ticket must kill mutations that discriminate its specific claim, and a mutation counts only
when the source differs, the build succeeds, the affected binary is rebuilt, the mutant binary
**executes**, the intended permanent test fails, and unrelated controls stay green. Planned:

| Mutation | Must be killed by |
|---|---|
| revert the unsigned subtraction to the signed one | the pinned wrap values (not by UBSan — the gate build is uninstrumented) |
| saturate to `min` instead of `max` on positive overflow | the `GetElapsedTime(0, INT64_MAX)` sign assertion |
| drop the NaN branch | the NaN control |
| make `Sum`'s overflow test `>` instead of `>=` at the boundary | the `MaxValue - 1 + 1` must-succeed case |
| apply the checked path to unsigned too | the `Sum<unsigned>` wrap pin |
| detect overflow **after** the signed addition instead of before | UBSan under the probe |
| copy `len` bytes instead of `len + 1` (no terminator) | the guard-page probe |
| map `ptr` from the copy without rebasing | the `ptr_offset` assertions |
| allow the hex prefix again | the `"0x10"` pin |

---

## 16. Family completion criteria

The family is complete when **all** of:

1. #2218 and #2219 are done, and only then SR-AUD-131 → `remediated`;
2. #2220 is done, and SR-AUD-135 → `remediated`;
3. #2221 is done, and SR-AUD-180 → `remediated`; #2222 is done or explicitly deferred with a reason;
4. every diagnostic in §5 is shown **absent** afterwards, each with the sub-check that found it
   enabled, and the guard-page probe **survives** in `fallback` mode;
5. the permanent tests of §12 exist and pass, and the repository gate shows no failure other than
   the two inherited causes (5 `PingTests`, 1 `SocketTests`);
6. every mutation in §15 is killed;
7. the audit index, the owning reports and this document record the outcome, and audit numbering
   is still **364**.

---

## 17. What this family does **not** claim

- It does not close `modules/core`. 64 findings there stay open.
- It does not close, reopen, widen or edit **CCF-004**, which stays closed 8/8.
- It does not mint **CCF-021** or **CCF-022**, and does not touch **CCF-019**.
- It does not create any `SR-AUD-*` identifier.

---

## 18. Exclusions — named, with reasons

1. **SR-AUD-130** (`Stopwatch` publishes a fabricated 10 MHz frequency). Same file, same report,
   **different cause** — a public unit/parity question, not an arithmetic one — and its repair
   would change every timestamp this port emits. Stays `confirmed`.
2. **`Stopwatch`'s missing synchronisation contract.** The audit records that the instance's
   mutable state has no locking. That is CCF-009's shape, not this family's, and no ticket here
   touches it.
3. **`Linq::Count`.** `++n` on an `intcs` counter is undefined only past 2^31 elements, and
   `Count(source)` narrows `size()` with a conversion that is well defined since C++20. Neither is
   this cause, neither can be exercised without allocating more than 2^31 elements, and no evidence
   of harm exists. Inventoried in §3.2, excluded here.
4. **`Linq`'s remaining findings.** SR-AUD-134 (empty callables) is already `remediated` under
   CCF-011 and SR-AUD-046 under CCF-010; neither is reopened.
5. **`Single`/`Double`/XPath parse behaviour.** #2221 changes the *helper*; it deliberately changes
   nothing about which strings those three doors accept on Linux, where the native
   `std::from_chars` is selected and the fallback is not compiled in at all.
6. **A shared arithmetic helper.** Forbidden by `docs/DefinedArithmeticBoundaryPlan.md` §10
   exclusion 2, and unnecessary: each application is local to one function.
7. **Every blocked, `needs_user` and deferred ticket** listed in the batch brief — #2215, #2207,
   #2208, #2209, #2199, #2185, #2186, #2170, #2172, #2175, #2150, #2152, #2155, #2166, #2192,
   #2194, #2131, #2109, #1962, #1773 — is untouched.

---

## 19. Status at the close of #2217

| Ticket | Status |
|---|---|
| #2217 (this review) | done on delivery of this document |
| #2218 – #2222 | `todo`, all five compatible-ready, none blocked |

SR-AUD-131, SR-AUD-135 and SR-AUD-180 all keep status `confirmed` at the close of #2217. Audit
numbering stays frozen at **364**; the index reads **178 remediated / 131 confirmed / 55 confirmed
(design-complete)**.

---

## 20. What #2218 measured (2026-08-10)

`Stopwatch`'s half of SR-AUD-131 is implemented. One local, file-scoped spelling of CCF-004's
class A transformation — `subtractTimestamps` and `addNanoseconds`, both `static constexpr
noexcept` private members — is used at the three sites §3.1 lists. **No shared repository-wide
arithmetic helper was created**, as §18 exclusion 6 requires.

### 20.1 The class A claim is proven, not asserted

Under `-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=undefined` at `-O0`
with `volatile` operands, one process per case (`build-probe/2218_after.log`):

| Case | Before | After | Diagnostic before | Diagnostic after |
|---|---|---|---|---|
| S1 `(INT64_MIN, INT64_MAX)` | `ticks=-1` | `ticks=-1` | `Stopwatch.hpp:133:83` | **none**, exit 0 |
| S2 `(0, INT64_MAX)` | `ticks=9223372036854775807` | identical | none | none |
| S3 `(INT64_MAX, INT64_MIN)` | `ticks=1` | `ticks=1` | `:133:83` | **none**, exit 0 |
| S4 `(-1, INT64_MAX)` | `ticks=-9223372036854775808` | identical | `:133:83` | **none**, exit 0 |
| S5 `(1000, 3000)` | `ticks=2000` | identical | none | none |
| S6 one-argument door | negative | negative | `:133:83` | **none**, exit 0 |

**Every value is byte-identical.** That is what "class A, no observable change" has to mean, and it
is measured on both sides rather than argued from the transformation.

### 20.2 Activation proved in the same binary

An aborting build that reports nothing is indistinguishable from one whose flags did not take. The
same `build-probe/2217_probe_after` binary was run on two sites this ticket deliberately did **not**
repair, and both still abort:

```
T1 -> TimeProvider.hpp:74:34 signed integer overflow ... exit=1
L1 -> Linq.hpp:236:48       signed integer overflow ... exit=1
```

### 20.3 Two mutations, and one of them has a weaker kill signal — said plainly

| Mutation | Killed by | Signal |
|---|---|---|
| **M1** — narrow the unsigned domain from `ulongcs` to `uintcs` | **6 of the 9 permanent tests fail** | a gate test |
| **M2** — revert to the raw signed `a - b` | the sanitizer probe aborts (exit 1) at `Stopwatch.hpp:162:24` for S1, S3, S4 and S6 | **the probe only — all 9 permanent tests still PASS** |

M2's signal is genuinely weaker and is not dressed up as an equal one. It cannot be otherwise: a
class A repair is value-preserving *by definition*, and the gate build is uninstrumented, so on
this toolchain the mutant computes the same numbers. The permanent tests pin the **values**; only
the sanitizer can pin the **definedness**. Both are recorded, and a reader can see which is which.

### 20.4 Consequences

+9 permanent regressions in `modules/core/tests/System/Diagnostics/StopwatchTests.cpp`
(`StopwatchDefinedArithmeticTests`), including the seven-point signed sweep, an exhaustive 8×8
pair matrix asserting the exact two's-complement identity, both public doors and an ordinary
instance measurement. `StopwatchTests` 29/29. No signature, member ordering, `noexcept`, layout or
vtable change: the two additions are private `static constexpr` member functions, which are
neither virtual nor data members. **SR-AUD-131 stays `confirmed`** — it is not remediated until
#2219 lands its `TimeProvider` half.
