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

---

## 21. What #2219 measured, and SR-AUD-131's closure (2026-08-10)

`TimeProvider`'s half is implemented, so **SR-AUD-131 is `remediated`** (#2218 + #2219). The audit
index reads **179 remediated / 130 confirmed / 55 confirmed (design-complete)** of 364; numbering is
still frozen at 364 and CCF-004 is still closed 8/8 with no new member.

### 21.1 Both halves, before and after

| Case | Before | After | Before diagnostic | After |
|---|---|---|---|---|
| T1 `(INT64_MIN, INT64_MAX)` | `ticks=-1` | **identical** | `:74:34` signed overflow | none, exit 0 |
| T2 `(0, INT64_MAX)`, default provider | `ticks=-9223372036854775808` | **`ticks=9223372036854775807`** | `:74:55` float-cast (only with the extra sub-check) | none, exit 0 |
| T3 `(1000, 3000)` | `ticks=2000` | identical | none | none |
| T4 frequency 1, `(0, 1e12)` | `ticks=-9223372036854775808` | **`ticks=9223372036854775807`** | `:74:55` | none, exit 0 |
| T5 frequency 0 | `InvalidOperationException` | identical | — | still thrown before any arithmetic |
| T6 one-argument door | large negative | identical shape | `:74:34` | none, exit 0 |

### 21.2 A second methodology correction: the exit code is not the evidence for this sub-check

`-fno-sanitize-recover=undefined` names the **`undefined` group**, and `float-cast-overflow` is not
in it. Measured during mutation M2: with the saturation removed, the diagnostic at `:99:54` printed
and the process still **exited 0**. So for a floating→integral site, an aborting run proves nothing
and only the diagnostic text does — unless `-fno-sanitize-recover=all` is used, which the final
after-run does (`build-probe/2219_after.log`): all twelve `Stopwatch` and `TimeProvider` cases exit
0, and the deliberately unrepaired `Linq.hpp:236` control in the **same binary** exits 1.

This compounds §5.1.1: it is not enough to enable the right check; the recovery group has to match
it too, or the run reports "pass" for a site it did diagnose.

### 21.3 The saturation semantics, chosen and justified rather than assumed

`/rv` is absent, so the reference could not be read. The choice is stated as a decision:

- the pre-fix behaviour was **undefined**, and on this toolchain produced `INT64_MIN` for both a
  huge positive and a huge negative scaled value — indistinguishable, and wrong in sign for the
  positive case;
- saturation is defined, is what modern .NET adopted for floating→integer conversions, and is what
  AArch64 does natively;
- it makes `TimeProvider::GetElapsedTime(0, INT64_MAX)` agree with
  `Stopwatch::GetElapsedTime(0, INT64_MAX)`, which never routed through a `double` and already
  returned `INT64_MAX`. The two APIs disagreeing about the same arguments was itself a defect.

NaN maps to `0`. It is unreachable for a finite delta and a positive finite frequency and is
guarded anyway rather than left to the conversion.

### 21.4 One property pinned, not changed — precision above 2^53

`TimeProvider::GetElapsedTime` scales through a `double`, exactly as .NET's own does, so a tick
count above 2^53 is not exact. `(double)(INT64_MIN + 1)` rounds to exactly −2^63, which **is**
representable, so that sweep point converts to `INT64_MIN` and never reaches the saturation guard;
`(double)(INT64_MAX - 1)` rounds up to 2^63, which is not, and does. Both are now pinned with the
reason, together with a direct comparison against `Stopwatch`, which stays exact. This is
pre-existing and untouched; the test exists so a later reader cannot mistake it for saturation.

### 21.5 Four mutations, and which signal killed each

| Mutation | Gate tests | Sanitizer probe |
|---|---|---|
| **M1** — saturation direction swapped | **3 of 11 fail** | silent (both directions are defined) |
| **M2** — saturation removed, raw conversion restored | **3 of 11 fail** | `:99:54` float-cast diagnostic returns for T2 and T4 |
| **M3** — the boundary `>=` weakened to `>` | **2 of 11 fail** | `:108:45` returns for T2 (T4 is strictly greater and unaffected) |
| **M4** — unsigned subtraction reverted to signed | **all 11 pass** | T1 aborts, exit 1 |

M4's signal is the weaker one, for the same structural reason as #2218's M2, and is labelled rather
than counted as equal.

### 21.6 Consequences

+11 permanent regressions in `modules/threading/tests/System/TimeProviderTests.cpp`
(`TimeProviderDefinedArithmeticTests`); `SharpRuntimeTests_Threading` `TimeProvider*` 21/21.
**No member added** — the saturation is inline in the existing body — so no layout or vtable change
is possible, and a `static_assert` pins `sizeof`/`alignof`. `<limits>` is now included. No
signature, `noexcept` or symbol change.

---

## 22. What #2220 measured, and SR-AUD-135's closure (2026-08-10)

`Linq::Sum` is checked on the signed integral domain, so **SR-AUD-135 is `remediated`**. The audit
index reads **180 remediated / 129 confirmed / 55 confirmed (design-complete)** of 364.

### 22.1 One shared step, two overloads, one stated domain

`System::Linq::detail::addForSum` — one new `inline` function template in the namespace #1870
already created — drives both `Sum` overloads. The checked branch forms the sum in the **unsigned**
counterpart and recognises the overflow from the operand signs, so **nothing undefined is executed
on the way to diagnosing it**. That distinction is the whole point of mutation M1 below.

§8's table is implemented verbatim; nothing in it was decided during implementation.

### 22.2 Before and after

| Case | Before | After |
|---|---|---|
| L1 `Sum<int>({INT_MAX,1})` | `-2147483648`, `Linq.hpp:236:48` UB | `OverflowException`, no diagnostic, exit 0 |
| L2 `Sum<long long>({INT64_MAX,1})` | `INT64_MIN`, UB | `OverflowException` |
| L3 `Sum<int>({INT_MIN,-1})` | `2147483647`, UB | `OverflowException` |
| L4 `Sum<int>({1,2,3})` | `6` | `6` |
| L5 `Sum<double>({DBL_MAX,DBL_MAX})` | `inf` | `inf` (pinned unchanged) |
| L6 selector overload | `-2147483648`, `Linq.hpp:249:48` UB | `OverflowException` |
| L7 `Sum<unsigned>({UINT_MAX,1})` | `0` | `0` (pinned unchanged) |
| L8 `Sum<short>({SHRT_MAX,1})` | `-32768`, **no UB** | `OverflowException` |
| L9 `Sum<int>({INT_MAX,INT_MAX,INT_MIN})` | **`2147483646` — correct — through UB** | `OverflowException` |
| L10 empty / single | `0`, `7` | unchanged |
| L11 ×1000 throw/catch | — | LSan clean |

### 22.3 The two behaviour changes that are not undefined behaviour, said out loud

Most of this family's changes replace undefined behaviour. Two do not, and both are deliberate:

- **L8** (`short`, `signed char`) was already defined — integral promotion means the addition
  happens in `int` and only the store truncates. It now throws, because a silently truncated total
  is a wrong answer whether or not the language calls it undefined.
- **L9** returned the mathematically **correct** total. It now throws, because .NET's `Sum` is
  checked **per element** and raises at the same intermediate. Faithfulness to the reference was
  chosen over faithfulness to the final sum, and this line is here so that choice is visible rather
  than buried.

**No `Linq::Sum` call site exists anywhere in this repository outside its own tests** (verified by
search across `modules/`, `tests/`, `test/` and `bench/`), so nothing in-repo changes.

### 22.4 Activation proved by two deliberate controls, not by absence

Every arithmetic finding in this family is repaired by this point, so no unrepaired production site
was left to serve as the control. Two were added to the probe instead, in the **same binary** as the
verdict:

```
CONTROL-UB   -> exit 1, "signed integer overflow: 2147483647 + 1" at the probe's own line
CONTROL-LEAK -> exit 1, "LeakSanitizer: detected memory leaks ... 256 byte(s) in 1 allocation(s)"
```

### 22.5 Five mutations, and which signal killed each

| Mutation | Gate tests | Probe |
|---|---|---|
| **M1** — detect the overflow **after** a raw signed `a + b` | **all 13 pass** | `Linq.hpp:113:25` UB returns, exit 1 for L1 and L3 |
| **M2** — widen the checked path to unsigned | **5 of 13 fail** | — |
| **M3** — over-reject every same-sign pair | **4 of 13 fail** | — |
| **M4** — drop `short`/`signed char` from the checked domain | **1 of 13 fails** | — |
| **M5** — leave the selector overload on the raw accumulation | **1 of 13 fails** | — |

M1 is the important one and its signal is the weak one: the naive "add, then look at the result"
repair passes every value test, because GCC's wrap gives the same number the unsigned domain gives.
Only the sanitizer distinguishes a defined computation from an undefined one, and the record says so
rather than counting M1 as equal to the other four.

### 22.6 Consequences

+13 permanent regressions in `modules/core/tests/System/LinqTests.cpp` (`LinqCheckedSumTests`);
`Linq*` 77/77. Both overloads are free function templates, so no mangled symbol exists to change;
no signature, `noexcept` or default argument changed, and `addForSum` holds no state.
`System/OverflowException.hpp` is now included by `Linq.hpp`.

---

## 23. What #2221 measured, and SR-AUD-180's closure (2026-08-10)

The bounded-parse subfamily's own finding is closed. **SR-AUD-180 is `remediated`**; the audit index
reads **181 remediated / 128 confirmed / 55 confirmed (design-complete)** of 364.

### 23.1 The primary evidence is a guard page, not a sanitizer

| Run | `std::from_chars` | `PortableFromCharsFloat` |
|---|---|---|
| **before** | `survived ec=0 ptr_offset=2 value=12` | **Segmentation fault**, exit 139 |
| **after** | unchanged | `survived ec=0 ptr_offset=2 value=12` |

That is `build-probe/2217_probe_guard.cpp`: two pages, the second `PROT_NONE`, `"12"` in the last
two bytes of the readable one so `last` is exactly the boundary. **AddressSanitizer says nothing
about the same read on a heap allocation**, because it happens inside glibc's `strtod` — neither
instrumented nor intercepted (§5.3.1). §13's matrix predicted this and is confirmed.

### 23.2 Every measured case now returns `std::from_chars`'s answer

| Case | Before (value / ptr) | After | `std::from_chars` |
|---|---|---|---|
| P1 `"12"` heap, `[0,1)` | 12 / 2 | **1 / 1** | 1 / 1 |
| P2 `"12"`, `[0,1)` | 12 / 2 | **1 / 1** | 1 / 1 |
| P12 `"1e3"`, `[0,1)` | 1000 / 3 | **1 / 1** | 1 / 1 |
| P13 `"-57"` heap, `[0,2)` | −57 / 3 | **−5 / 2** | −5 / 2 |
| P14 `"12"`, `[0,1)`, `float` | 12 / 2 | **1 / 1** | 1 / 1 |
| P5–P11, P15 full-range controls | correct | unchanged | — |

### 23.3 The design decisions, and why each is the way it is

- **Copy rather than scan.** The C parser has no bound; the only way to stop it is a terminator it
  owns. Exactly `length + 1` bytes — no multiplication, no amplification of a caller-controlled
  length.
- **Nothrow heap rather than truncation** for a range longer than the 512-byte stack buffer.
  Truncating a digit run changes the value: a 600-digit integer is not its first 511 digits.
  Mutation M4 exists to keep that honest.
- **`noexcept` added.** `std::from_chars` is `noexcept`; the "drop-in" claim was false without it.
  It is load-bearing rather than cosmetic: `Single::tryParseCore` and `Single::TryParse` are
  `noexcept`, so on the fallback platform a throwing helper would have called `std::terminate`.
  Adding a guarantee needs no approval — this is the **opposite** direction from #2215.
- **`std::errc::not_enough_memory`** is the one status a real `std::from_chars` never returns. It is
  reachable only when a copy of the caller's own range cannot be allocated, and every caller in this
  repository treats any non-`errc{}` value as failure.

### 23.4 The coverage gap the audit named is closed on every platform

The report says "no test forces the fallback path; Linux normally chooses native floating
`std::from_chars`". That is true of `FromCharsFloat`, the dispatching wrapper — and **not** of
`PortableFromCharsFloat`, which is an ordinary public function template any platform can call
directly. `modules/core/tests/SharpRuntime/PortableFromCharsTests.cpp` calls it directly, so the
Apple-only fallback now runs on the Linux gate: **23 tests**, asserting acceptance, consumption,
value and status **separately**, and agreeing with the platform's real `std::from_chars` rather than
with hand-written expectations wherever one exists.

### 23.5 Four mutations, all killed

| Mutation | Gate tests | Guard probe |
|---|---|---|
| **M1** — copy without the terminator | **11 of 23 fail** | survives (stack garbage happened to terminate) |
| **M2** — return the copy's pointer, unrebased | **18 of 23 fail** | `ptr_offset=566078643028` |
| **M3** — parse `first` directly again (the original defect) | **6 of 23 fail** | **segfault, exit 139** |
| **M4** — truncate an over-long range instead of the heap path | **1 of 23 fails** | survives |

### 23.6 Consequences

+23 permanent regressions in a new file; `PortableFromChars*` 23/23. The three consumers are
unaffected — `Single*`/`Double*` 372/372 and XPath 90/90 — which is expected: on Linux
`FromCharsFloat` selects the native overload and the fallback is not on their path at all. Both
entry points are free function templates, so no mangled symbol exists to change; the only contract
change is the **added** `noexcept`, pinned by `static_assert`.

---

## 24. What #2222 measured (2026-08-10)

The adjacent grammar divergence §5.3.3 recorded is closed. **No `SR-AUD-*` identifier was created**
and none of the three findings' statuses changed — this was never an audit finding, only something
the inventory turned up in the same fifteen-line function.

### 24.1 The reference behaviour was measured, not assumed

`build-probe/2222_probe.cpp` runs the platform's own `std::from_chars` over thirteen shapes
(`build-probe/2222_native_grammar.log`). It stops at the `x` and reports the leading zero it did
consume:

| Input | `std::from_chars` | fallback before | fallback after |
|---|---|---|---|
| `"0x10"` | `ec=0 ptr=1 value=0` | `ec=0 ptr=4 value=16` | `ec=0 ptr=1 value=0` |
| `"-0x10"` | `ec=0 ptr=2 value=-0` | — | matches |
| `"0X1p3"`, `"0x"`, `"-0x"`, `"0xg"`, `"0x0"` | stop at the `x` | — | match |
| `"00x1"`, `"0.0x1"` | `ptr=2`, `ptr=3` | — | unchanged, and deliberately untouched |

### 24.2 The guard keys on the position, not on the character

`00x1` and `0.0x1` are **not** hexadecimal prefixes: the `x` does not immediately follow the first
digit, and the C parser already stops in the right place. The guard therefore truncates only when
`0` immediately followed by `x`/`X` begins the number, after an optional `-`.

### 24.3 Two mutations, and the second is an **equivalent** mutant — said plainly

| Mutation | Result |
|---|---|
| **M1** — remove the guard (the pre-#2222 behaviour) | **2 of 27 tests fail** — killed |
| **M3** — do not skip the leading sign before the `0x` test | **1 of 27 fails** (`"-0x10"` returns −16 over five characters instead of −0 over two) — killed |
| **M2** — widen the guard to truncate at **any** `x` | **27/27 pass** |

M2 is **not a surviving mutant, it is an equivalent one**, and the difference matters. Outside a
leading `0x`/`0X` the C parser already stops at the `x` of its own accord, so truncating there
changes nothing observable — there is no input that can distinguish the two forms. The narrow
condition is kept because it states the intent (a *hexadecimal prefix*, not *an `x`*), not because
it changes behaviour. Recording it as a kill would have been false.

### 24.4 Consequences

+4 permanent tests (`PortableFromCharsGrammarTests`); `PortableFromChars*` 27/27. The two repairs
compose: the guard-page probe still survives, and a hexadecimal prefix split by the range boundary
still obeys `last`. Consumers unaffected — `Single*`/`Double*` 372/372, XPath 90/90 — as expected,
since Linux selects the native overload and this code is not on their path. No signature, `noexcept`
or symbol change.

---

## 25. Family reconciliation (2026-08-10)

| Finding | Disposition | Closed by |
|---|---|---|
| **SR-AUD-131** | **`remediated`** | #2218 (`Stopwatch`) **and** #2219 (`TimeProvider`) — both were required |
| **SR-AUD-135** | **`remediated`** | #2220 |
| **SR-AUD-180** | **`remediated`** | #2221 |
| — (no identifier) | closed | #2222, the adjacent grammar divergence |

**The bounded family is fully closed. There is no residual, no blocked ticket and no `needs_user`
question arising from it** — which is what §10 predicted when it classified all five tickets as
compatible-ready, and is the property that made this family the measured selection.

Against §16's criteria: (1) and (2) and (3) are met and the audit index carries all three flips;
(4) every diagnostic listed in §5 is absent afterwards, each with the sub-check that found it
enabled and, for the floating conversion, with `-fno-sanitize-recover=all` so the exit code means
something, and the guard-page probe survives in `fallback` mode; (5) the permanent tests exist and
pass; (6) every mutation in §15 is killed except the one that turned out to be an **equivalent**
mutant, which is recorded as such in §24.3 rather than counted; (7) the records are updated and
numbering is still **364**.

**No unrelated `modules/core` finding was closed, and `modules/core` is NOT closed.** SR-AUD-130,
which shares a file and a report with SR-AUD-131, is untouched and still `confirmed`, as is the
`Stopwatch` synchronisation observation, SR-AUD-046's neighbours in the LINQ report, and the 61
other open findings in that module. **CCF-004 stays closed 8/8 and gained no member; CCF-019 was not
touched; CCF-021 and CCF-022 stay unminted.**

Audit decomposition at the close of the family: **181 remediated / 128 confirmed / 55 confirmed
(design-complete) / 364 total**, from **178 / 131 / 55** at the open.
