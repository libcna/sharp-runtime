<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Empty-callable boundary family — CCF-011 plan

*Authored 2026-07-30 by the autonomous remediation batch on branch
`feature/remediation-batch-empty-callable`, immediately after the CCF-007
Pi-trig + parse-whitespace batch (#1861/#1864/#1865) left CCF-007 with no
remaining **compatible** ready work. This is the durable, evidence-based plan for
**CCF-011 — "empty `std::function` values cross public boundaries without an
explicit policy"** (`audit/AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-011, lines
367–398). Six findings, all `confirmed`: SR-AUD-052 (`Array`), SR-AUD-058
(`Progress<T>`), SR-AUD-065 (`Lazy<T>`), SR-AUD-099
(`AggregateException::Handle`), SR-AUD-121 (`EventHandler<TEventArgs>`),
SR-AUD-134 (`Linq`).*

*Every current-behaviour statement in this document was **measured**, not
recalled, by the probe `build-probe/1866_empty_callable_probe.cpp` compiled
against the shipped headers on 2026-07-30; the raw output is
`build-probe/1866_prefix.log`. Every .NET statement was read from the current
local reference under `/rv/tmp/runtime/src/libraries/` (`Array.cs`, `Lazy.cs`,
`AggregateException.cs`, `Progress.cs`, `System.Linq/src/System/Linq/*.cs`),
not from memory. **All six findings still reproduce; none is remediated.***

**This document creates no `SR-AUD-*` identifier** (audit numbering is frozen at
364) and **marks no finding remediated**. It fixes the family scope, verifies or
corrects the audit's premises against measurement, draws the
compatible-versus-approval-gated line, and breaks the work into bounded,
dependency-ordered tickets.

---

## 1. Exact family scope

**In scope.** Every *public* entry point in this repository that takes a
`std::function` (or a callable that is stored as one) whose emptiness is not
validated, where .NET's counterpart has a defined answer for a null delegate.
Six types, one module (`modules/core`), 36 measured public entries.

**Not in scope, deliberately** — see §14 for the full list with reasons:
`std::function` *members* that are legitimately optional (`Progress<T>`'s
constructor handler is already validated; `EventHandler::SetReplayHook`'s empty
value is the documented "clear the hook" spelling), comparer/callback surfaces
outside these six types, and the separate CCF-010 question of *what* a
comparison callable should compute.

---

## 2. Complete finding inventory

| Finding | Sev | Status | Type | One-line defect (as audited) |
|---|---|---|---|---|
| **SR-AUD-052** | medium | confirmed | `System::Array` | Delegate overloads invoke the `std::function` directly; empty ⇒ `std::bad_function_call` only if iteration reaches it, and a silent normal result on an empty array. |
| **SR-AUD-058** | medium | confirmed | `System::Progress<T>` | `addProgressChangedHandler` stores an empty callback; a later unrelated `Report` throws `std::bad_function_call`. |
| **SR-AUD-065** | medium | confirmed | `System::Lazy<T>` | An empty factory is accepted by the constructor and fails at first `Value()` with `std::bad_function_call`. |
| **SR-AUD-099** | medium | confirmed | `System::AggregateException` | `Handle` invokes an unvalidated predicate; empty ⇒ `std::bad_function_call` at the first inner exception. |
| **SR-AUD-121** | medium | confirmed | `System::EventHandler<TEventArgs>` | `Add`/`operator+=` store an empty handler (and pass it to the replay hook); `Raise` later throws `std::bad_function_call`. |
| **SR-AUD-134** | medium | confirmed | `System::Linq` | Callback overloads accept empty callables; failure is data-dependent (silent on empty/one-element input, `std::bad_function_call` otherwise). |

---

## 3. Affected modules, files and symbols

One module: **`core`** (component `Core.Base`). No cross-module edge is added or
removed by this family — every header involved already lives in
`modules/core/include/System/`, and `ArgumentNullException.hpp` is a sibling in
the same module, so the 41-module / 91-edge graph is unchanged.

| File | Symbols touched | Tests |
|---|---|---|
| `modules/core/include/System/Array.hpp` | 17 delegate overloads (§4) | `modules/core/tests/System/ArrayTests.cpp` |
| `modules/core/include/System/Linq.hpp` | 11 callback overloads (§4) | `modules/core/tests/System/LinqTests.cpp` |
| `modules/core/include/System/Lazy.hpp` | 3 factory constructors | `modules/core/tests/System/LazyTests.cpp` |
| `modules/core/include/System/AggregateException.hpp` | `Handle` | `modules/core/tests/System/ExceptionRemainingTests.cpp` |
| `modules/core/include/System/Progress.hpp` | `addProgressChangedHandler`, `OnReport` | `modules/core/tests/System/ProgressTests.cpp` |
| `modules/core/include/System/EventHandler.hpp` | `Add`, `operator+=`, `Raise` | `modules/core/tests/System/EventHandlerTests.cpp` |

---

## 4. Complete public-entry inventory (measured 2026-07-30)

### 4.1 `System::Array` — 17 entries (SR-AUD-052)

| # | Entry (line) | Callable param | .NET counterpart | .NET paramName | .NET validation order |
|---|---|---|---|---|---|
| 1 | `Sort(vector&, comparison)` (50) | `comparison` | `Array.Sort<T>(T[], Comparison<T>)` | `comparison` | array → comparison |
| 2 | `Sort(vector&, index, length, comparison)` (74) | `comparison` | *(no .NET `Comparison` range overload)* | `comparison` | port-specific: range → comparison |
| 3 | `BinarySearch(array, value, comparison)` (264) | `comparison` | *(no .NET `Comparison` overload)* | `comparison` | port-specific: comparison first |
| 4 | `BinarySearch(array, index, length, value, comparison)` (287) | `comparison` | *(no .NET `Comparison` overload)* | `comparison` | port-specific: range → comparison |
| 5 | `ConvertAll(array, converter)` (325) | `converter` | `Array.ConvertAll` | `converter` | array → converter |
| 6 | `Exists(array, predicate)` (336) | `predicate` | `Array.Exists` | **`match`** | array → (range) → match |
| 7 | `Find(array, predicate)` (344) | `predicate` | `Array.Find` | **`match`** | array → match |
| 8 | `FindLast(array, predicate)` (352) | `predicate` | `Array.FindLast` | **`match`** | array → match |
| 9 | `FindAll(array, predicate)` (360) | `predicate` | `Array.FindAll` | **`match`** | array → match |
| 10 | `FindIndex(array, predicate)` (371) | `predicate` | `Array.FindIndex` | **`match`** | array → range → match |
| 11 | `FindIndex(array, startIndex, predicate)` (384) | `predicate` | `Array.FindIndex` | **`match`** | array → range → match |
| 12 | `FindIndex(array, startIndex, count, predicate)` (399) | `predicate` | `Array.FindIndex` | **`match`** | array → **range → match** |
| 13 | `FindLastIndex(array, predicate)` (411) | `predicate` | `Array.FindLastIndex` | **`match`** | array → **match → range** |
| 14 | `FindLastIndex(array, startIndex, predicate)` (425) | `predicate` | `Array.FindLastIndex` | **`match`** | array → **match → range** |
| 15 | `FindLastIndex(array, startIndex, count, predicate)` (441) | `predicate` | `Array.FindLastIndex` | **`match`** | array → **match → range** |
| 16 | `ForEach(array, action)` (453) | `action` | `Array.ForEach` | `action` | array → action |
| 17 | `TrueForAll(array, predicate)` (459) | `predicate` | `Array.TrueForAll` | **`match`** | array → match |

**The `FindIndex` / `FindLastIndex` asymmetry is real .NET behaviour, not an
oversight to normalise.** `Array.cs:1599-1622` validates `startIndex`, then
`count`, then `match`; `Array.cs:1671-1706` validates `match` *first*, then
`startIndex`, then `count`. Both orders are pinned by tests in this family.

### 4.2 `System::Linq` — 11 entries (SR-AUD-134)

| # | Entry (line) | Callable param | .NET paramName | Reference |
|---|---|---|---|---|
| 1 | `Where(source, predicate)` (37) | `predicate` | `predicate` | `Where.cs:21` |
| 2 | `Select(source, selector)` (48) | `selector` | `selector` | `Select.cs:23` |
| 3 | `FirstOrDefault(source, predicate)` (60) | `predicate` | `predicate` | `First.cs:110` |
| 4 | `First(source, predicate)` (77) | `predicate` | `predicate` | `First.cs:110` |
| 5 | `LastOrDefault(source, predicate)` (95) | `predicate` | `predicate` | `Last.cs:117` |
| 6 | `Any(source, predicate)` (115) | `predicate` | `predicate` | `AnyAll.cs:53` |
| 7 | `All(source, predicate)` (129) | `predicate` | `predicate` | `AnyAll.cs:89` |
| 8 | `Count(source, predicate)` (139) | `predicate` | `predicate` | `Count.cs:55` |
| 9 | `Sum(source, selector)` (170) | `selector` | `selector` | `Sum.cs:228` |
| 10 | `OrderBy(source, keySelector)` (205) | `keySelector` | `keySelector` | `OrderedEnumerable.cs:80-82` |
| 11 | `OrderByDescending(source, keySelector)` (221) | `keySelector` | `keySelector` | `OrderedEnumerable.cs:80-82` |

.NET's `OrderBy` is lazily *enumerated* but its `keySelector` null check runs
**eagerly**, inside `OrderedIterator`'s constructor, before any element is
touched. This port's operators are eager throughout, so eager validation is an
exact match rather than an approximation.

### 4.3 The remaining four types — 8 entries

| Type | Entry | Callable | .NET rule |
|---|---|---|---|
| `Lazy<T>` | `Lazy(F&&)` (121) | factory | `ArgumentNullException(valueFactory)` (`Lazy.cs:301`) |
| `Lazy<T>` | `Lazy(F&&, bool)` (157) | factory | same |
| `Lazy<T>` | `Lazy(F&&, LazyThreadSafetyMode)` (171) | factory | same |
| `AggregateException` | `Handle(predicate)` (207) | predicate | `ArgumentNullException.ThrowIfNull(predicate)` (`AggregateException.cs`) |
| `Progress<T>` | `addProgressChangedHandler(handler)` (80) | handler | **no-op** — C# `event += null` is `Delegate.Combine(d, null) == d` |
| `EventHandler<TEA>` | `Add(handler)` (170) | handler | **no-op** — same rule |
| `EventHandler<TEA>` | `operator+=(handler)` (127) | handler | **no-op** — delegates to `Add` |
| `EventHandler<TEA>` | `Raise`/`Invoke` (241/258) | stored | `_event?.Invoke(...)` — a null invocation list is simply not invoked |

---

## 5. Common root cause

`std::function` has a **default-constructed empty state that is silently
constructible from `{}`, `nullptr`, a moved-from function, or a
default-constructed member**, and its `operator()` on that state is a *runtime*
error (`std::bad_function_call`), not a compile error. Every member of this
family adopted `std::function` as the C++ spelling of a .NET delegate parameter
and then inherited that empty state without deciding what it means.

Three consequences follow, all measured in §6:

1. **The failure is deferred.** It surfaces at first invocation, arbitrarily far
   from the call that supplied the bad argument, and — for `Lazy<T>` and
   `Progress<T>` — in a *different* public API than the one at fault.
2. **The failure is data-dependent.** Whether it happens at all depends on the
   input length: an empty sequence never reaches the callable, and `OrderBy` /
   `Sort` never reach it below two elements, so the same wrong call is silent or
   fatal depending on data the caller may not control.
3. **The failure is the wrong type.** `std::bad_function_call` derives from
   `std::exception`, not from `System::Exception`, so a consumer's
   `catch (const System::Exception&)` — the only handler ported C#/XNA code
   writes — does not catch it, and the process terminates.

The repair is one policy, not six: **decide emptiness at the public boundary,
before any element is examined**, and pick the .NET answer for that shape of API
(argument error for an *argument*, no-op for an *event subscription*).

---

## 6. Actual current behaviour, measured

`build-probe/1866_empty_callable_probe.cpp`, 60 cases, output in
`build-probe/1866_prefix.log` (2026-07-30). Representative rows:

| Case | Measured today |
|---|---|
| `array.exists` (1 element) | `bad_function_call` |
| `array.exists.empty` (0 elements) | **`no-throw`** (returns `false`) |
| `array.foreach.empty`, `array.trueforall.empty`, `array.convertall.empty` | **`no-throw`** |
| `array.sort.comparison` (2 elements) | `bad_function_call` |
| `array.sort.comparison.one` (1 element) | **`no-throw`** |
| `array.binarysearch.emptyarray` | **`no-throw`** |
| `array.findindex.range.badrange` | `ArgumentException … (Parameter 'index')` — range wins, matching .NET |
| `array.findlastindex.badrange` | `ArgumentException … (Parameter 'startIndex')` — range wins, **diverging from .NET**, which reports `match` |
| `linq.where` / `linq.any` / `linq.select` / `linq.count` / `linq.sum.selector` (1 element) | `bad_function_call` |
| `linq.where.empty` / `linq.any.empty` / `linq.all.empty` / `linq.select.empty` | **`no-throw`** |
| `linq.first.empty` | `InvalidOperationException` — the *sequence* error masks the argument error |
| `linq.orderby` (1 element), `linq.orderby.empty` | **`no-throw`** |
| `linq.orderby.two` / `linq.orderbydescending.two` | `bad_function_call` |
| `lazy.ctor.factory` | **`no-throw`** — construction succeeds |
| `lazy.value.factory` / `.bool` / `.mode` | `bad_function_call` at first `Value()` |
| `aggregate.handle.nonempty` | `bad_function_call` |
| `aggregate.handle.noinner` | **`no-throw`** |
| `progress.add` | **`no-throw`** |
| `progress.add.report` | `bad_function_call` from an unrelated `Report` |
| `eventhandler.add` | **`no-throw`**, and `Size()==1`, `Empty()==false` |
| `eventhandler.add.raise` | `bad_function_call` |
| `eventhandler.add.replayhook` | `bad_function_call` **inside `Add` itself**, before storage |

---

## 7. Incorrect audit premises, and corrections

The audit text is preserved as written; these are appended corrections, per this
repository's practice (the SR-AUD-081 / SR-AUD-362 convention).

**7.1 — SR-AUD-134's data-dependence is size-dependent, not merely
emptiness-dependent.** The finding says empty predicates "return normal results
on an empty vector and throw native `bad_function_call` only when an item is
reached". Measured: `OrderBy` and `OrderByDescending` also return a normal
result for a **one-element** vector (`linq.orderby=no-throw`,
`linq.orderby.two=bad_function_call`), because `std::stable_sort` never invokes
the comparator below two elements. The same holds for `Array::Sort` with a
comparison (`array.sort.comparison.one=no-throw`) and for `Array::BinarySearch`
on an empty array. The finding's shape is confirmed; its threshold is one
element wider than stated for the sort/search entries.

**7.2 — SR-AUD-134's `First` case is masked, not silent.**
`Linq::First(empty, {})` does not return a normal result: it throws
`System::InvalidOperationException("Sequence contains no matching element.")`.
.NET's `First` validates `predicate` *before* the sequence, so the argument
error must win after the repair. This is a validation-*order* change, not only
an added check, and is pinned by a test.

**7.3 — SR-AUD-099's `Handle` has a silent case too.** The finding describes
only the `bad_function_call` path. Measured: an `AggregateException` with **no**
inner exceptions accepts an empty predicate and returns normally
(`aggregate.handle.noinner=no-throw`). .NET's `ArgumentNullException.ThrowIfNull`
runs before the loop, so it throws for an empty inner list as well.

**7.4 — SR-AUD-121's replay-hook path fails earlier than "during Raise".** The
finding's title says the failure is deferred to `Raise`. Measured: when a replay
hook is set, `Add` invokes the hook with the empty handler and the hook's own
call throws `bad_function_call` **inside `Add`**
(`eventhandler.add.replayhook=bad_function_call`). The finding's own body already
notes this ("If a replay hook calls the empty handler, failure occurs even
earlier"); the correction is that the ordering constraint it implies is binding
on the repair — the empty check must precede the replay hook, not follow it.

**7.5 — `Array::FindLastIndex` diverges from .NET in validation order,
independently of this family.** Measured:
`FindLastIndex(oneElement, /*startIndex=*/5, /*count=*/1, emptyPredicate)`
reports the *range* error, but .NET's `Array.FindLastIndex` checks `match`
before `startIndex`/`count` (`Array.cs:1671-1706`). Correcting this is
**inseparable** from adding the check at all — there is no way to add the
`match` check without choosing an order — so it is folded into ticket #1869
rather than opened as a separate defect. It receives no `SR-AUD-*` identifier.

**7.6 — CCF-011's prose lists `Array` "arguments need an argument error; adding a
null event delegate is a no-op; Lazy needs a constructor argument error" and is
correct on all three.** No correction; recorded here because the plan's policy
in §8 is the same three-way split, verified against the reference rather than
inherited from the summary.

---

## 8. The policy, stated once

For every entry in §4, decide emptiness **at the public boundary, before any
element of the input is examined**, and choose by API shape:

| Shape | Policy | Spelling |
|---|---|---|
| A **delegate argument** to an ordinary method (`Array`, `Linq`, `Lazy`, `AggregateException::Handle`) | reject | `throw System::ArgumentNullException("<.NET paramName>")` — message `Value cannot be null. (Parameter 'x')`, HResult `E_POINTER` |
| An **event subscription** (`Progress::addProgressChangedHandler`, `EventHandler::Add`/`operator+=`) | no-op | store nothing; leave `Size()`/`Empty()` unchanged; do not call the replay hook |
| An **event raise** over stored handlers (`Progress::OnReport`, `EventHandler::Raise`) | skip | invoke only truthy handlers (defence in depth; unreachable once subscription is a no-op) |

**Ordering rule.** Where .NET validates other arguments first, this port must
too. §4.1 records the per-overload order; the `FindIndex` (range-first) versus
`FindLastIndex` (callable-first) asymmetry is reproduced deliberately.

**Parameter-name rule.** The `paramName` carried in the exception message is the
observable, so it must be the **.NET** name. Where the port's C++ parameter is
currently named differently — the twelve `Predicate<T>`-shaped `Array` overloads
name it `predicate`, .NET names it `match` — the C++ parameter is **renamed to
the .NET name** so the signature, the doc-comment and the message agree. In C++
a function-parameter name is not part of the interface (no designated arguments,
no name mangling contribution), so this is source-compatible and ABI-neutral;
it is recorded in §9 as such rather than treated as a public-API change.

---

## 9. Compatible versus approval-gated

**All six findings are compatible.** None of the approval-boundary triggers is
reached. Justification per trigger:

| Approval trigger | Reached? | Evidence |
|---|---|---|
| public source compatibility | **No** | no signature, return type, template parameter, default argument or overload set changes; only parameter *names* change (§8), which C++ does not expose |
| adding/removing `noexcept` | **No** | none of the 36 entries is `noexcept` today — verified by reading each declaration; the new `throw` needs no specification change |
| virtual interfaces / vtables | **No** | `Progress<T>::OnReport` is already `virtual` and keeps its signature; no virtual is added, removed or reordered; `Progress<T>` gains no member |
| return calling convention | **No** | unchanged |
| public object/iterator/enumerator layout | **No** | no data member is added, removed, reordered or retyped in any of the six types |
| mandatory downstream migration | **No** | the only call that changes outcome is one that **already** fails with an uncatchable `std::bad_function_call`, or that is already meaningless (an empty subscriber that can never run). There is no working call site that stops working. |
| accepted textual grammar | **No** | no parser or formatter is touched |
| serialized/formatted output | **No** | no `ToString` is touched |
| exception taxonomy | **Not in the gated sense** | the gated case is replacing one `System::*` exception with another (#1858's `FormatException`→`OverflowException`). Here the *before* value is `std::bad_function_call`, which is outside the `System::Exception` hierarchy entirely and is what .NET-shaped consumer code cannot catch. Replacing it with the exception .NET documents is the repair, not a taxonomy choice. |
| broad observable numerical behaviour | **No** | no arithmetic is touched |

**Two behaviour changes are nevertheless observable and are called out
explicitly**, because they are the only cases where a *non-throwing* call becomes
a throwing one:

- **B1.** `Array`/`Linq` entries that today silently return a normal result for an
  empty (or one-element, §7.1) input with an empty callable will throw
  `ArgumentNullException`. This is .NET's documented behaviour for the identical
  call, and the call was already wrong.
- **B2.** `EventHandler::Add({})` stops incrementing `Size()`. A consumer that
  counted subscriptions rather than subscribers would observe a different count —
  but only for a subscriber that could never have been invoked.

Neither is gated; both are recorded here, in each ticket's acceptance criteria,
and in the per-file audit reports.

---

## 10. Source / ABI / layout / semantic matrix

| Type | Source | ABI / mangling | Object layout | `noexcept` | Semantics |
|---|---|---|---|---|---|
| `System::Array` | compatible (param renames only) | unchanged — all entries are function templates; parameter names contribute nothing to mangling | n/a (`Array` is a static-only class, `Array() = delete`) | unchanged (none were `noexcept`) | B1 |
| `System::Linq` | compatible | unchanged (free function templates) | n/a | unchanged | B1 + §7.2 order fix |
| `System::Lazy<T>` | compatible | unchanged | **unchanged** — the check is a constructor *body* statement; no member added | unchanged | throws at construction instead of first `Value()` |
| `System::AggregateException` | compatible | unchanged | unchanged | unchanged | throws for the no-inner case too (§7.3) |
| `System::Progress<T>` | compatible | unchanged | unchanged | unchanged | empty subscription is a no-op |
| `System::EventHandler<TEA>` | compatible | unchanged | unchanged | unchanged | B2 |

`sizeof`/`alignof` of `Lazy<int>`, `Progress<int>`, `EventHandler<EventArgs>` and
`AggregateException` are pinned by a static assertion in the post-fix probe, so
"layout unchanged" is measured rather than asserted.

---

## 11. Implementation dependency order

The four implementation tickets are **independent** — they share the policy in
§8 but touch disjoint files — so the order below is chosen for risk, smallest
blast radius first, not for compilation dependency.

1. **#1867 — `Lazy<T>` + `AggregateException::Handle`** (SR-AUD-065, SR-AUD-099).
   4 entries. Pure "reject an argument"; no ordering subtleties. Establishes the
   `ArgumentNullException` spelling the rest reuse.
2. **#1868 — `Progress<T>` + `EventHandler<TEventArgs>`** (SR-AUD-058,
   SR-AUD-121). 4 entries. The *other* half of the policy (no-op), plus the
   replay-hook ordering constraint from §7.4.
3. **#1869 — `System::Array`** (SR-AUD-052). 17 entries, plus the .NET parameter
   renames and the `FindIndex`/`FindLastIndex` order asymmetry (§7.5). Largest.
4. **#1870 — `System::Linq`** (SR-AUD-134). 11 entries, plus the `First`
   argument-before-sequence order fix (§7.2).

CCF-011 closes when all six findings are `remediated`.

---

## 12. Test matrix

Add-only. **No existing assertion is weakened, skipped, deleted or
recategorised.** Every entry in §4 gets, at minimum:

| Axis | Required cases |
|---|---|
| **Rejecting entries** (Array, Linq, Lazy, Handle) | (a) empty callable + **empty** input ⇒ `ArgumentNullException`; (b) empty callable + **non-empty** input ⇒ `ArgumentNullException`; (c) exact `paramName` in the message; (d) a *valid* callable still produces the pre-existing result on the same input (no regression) |
| **Ordering** | `FindIndex(v, badStart, count, {})` ⇒ range error; `FindLastIndex(v, badStart, count, {})` ⇒ `match` error; `Linq::First(empty, {})` ⇒ `ArgumentNullException`, not `InvalidOperationException` |
| **Size thresholds (§7.1)** | one-element and two-element `Sort`/`OrderBy`/`OrderByDescending` both ⇒ `ArgumentNullException` |
| **No-op entries** (Progress, EventHandler) | (a) empty add does not throw; (b) `Size()`/`Empty()` unchanged; (c) a later `Report`/`Raise` does not throw; (d) a real handler added before *and* after an empty one still runs, in order; (e) a replay hook is **not** invoked for an empty add; (f) the token returned by an empty `Add` is safe to pass to `Remove` |
| **No partial mutation** | `Array::Sort(v, {})` leaves `v` byte-identical; `Lazy` that threw at construction is not left half-initialised (the object never exists); `EventHandler` handler list unchanged after an empty add |
| **Exception identity** | the thrown object is caught by `catch (const System::ArgumentNullException&)`, by `catch (const System::ArgumentException&)`, and by `catch (const System::Exception&)` — the last being the property `std::bad_function_call` lacked |

Expected addition: ≈ 90–110 assertions across the four tickets; the exact count
each ticket lands is recorded in its `notes` and in NEXT.md.

## 13. Sanitizer matrix

| Sanitizer | Relevance here | Plan |
|---|---|---|
| **ASan** | Low-but-real. The defect is a thrown-exception contract, not a memory error, **but** `EventHandler::Raise` snapshots its handler list and `Progress::OnReport` iterates a member vector while user code may re-enter; adding an early return must not change that. | Run the post-fix probe under `-fsanitize=address,undefined`, including a re-entrant handler that calls `Add`/`Remove` during `Raise`. |
| **UBSan** | Low. No arithmetic, shift, cast or enum change. Included in the same run because it is free. | Same run. |
| **LSan** | **Material.** A constructor that now throws must not leak: `Lazy<T>`'s factory argument is a by-value `F&&` forwarded into `factory_`, and `AggregateException::Handle` takes its predicate by value. A throw after a copy is the classic leak shape. | Run the probe under `-fsanitize=address` with `detect_leaks=1`, constructing and destroying each throwing entry 10,000 times with a heap-capturing callable. |
| **TSan** | **Not applicable.** No shared mutable state is introduced; no atomic, cache or lock is touched. `Lazy<T>`'s existing `once_flag`/mutex protocol is untouched because the new check runs in the constructor, before any concurrency is possible. | Not run; recorded as a deliberate exclusion. |

Sanitizer *freshness* rule for this family: every one of the six headers is
header-only or inline, so the probe must be **compiled with** the sanitizer flags
(which recompiles the changed inline code) rather than linked against a
pre-existing `build-asan` archive. No stale-archive risk exists if that rule is
followed, and no `build-asan` target needs rebuilding.

## 14. Performance considerations

The added work is one `explicit operator bool()` test on a `std::function` per
public call — a single null-pointer comparison against the target pointer, fully
inlined, in a function that is about to iterate a container. It is strictly
*cheaper* than the status quo in the failing case and immeasurable in the passing
case.

Two places deserve a note:

- **`Array::Sort` / `Linq::OrderBy`**: the check is hoisted **out** of the
  comparator, so it runs once per sort rather than O(n log n) times. The
  comparator lambda itself is unchanged.
- **`EventHandler::Raise`**: the defensive `if (entry.second)` adds one test per
  handler per raise. `Raise` already copies the whole handler vector per call
  (the #1767-era reentrancy snapshot), so this is far below measurement noise.

No allocation is added on any path. No new header is included that was not
already in the module (`ArgumentNullException.hpp` is added to `Array.hpp`,
`Linq.hpp` and `Lazy.hpp`; `Progress.hpp` and `AggregateException.hpp` already
include it).

## 15. Explicit exclusions

1. **`EventHandler::SetReplayHook({})`** — an empty hook is the *documented*
   spelling for "clear the hook" (`EventHandler.hpp:153`). It stays legal and
   stays a no-op-at-raise. Not a CCF-011 instance.
2. **`Progress<T>`'s constructor handler** — already correctly rejects an empty
   handler with `ArgumentNullException("handler")` (`Progress.hpp:60`). The audit
   says so; re-verified. Nothing to change.
3. **`Lazy<T>`'s non-factory constructors** — `Lazy()`, `Lazy(T)`, `Lazy(bool)`,
   `Lazy(LazyThreadSafetyMode)` synthesise their own `[]{ return T{}; }` factory,
   which is never empty. Out of scope.
4. **CCF-010 (what a comparison *computes*)** — `Array::Sort`'s and
   `Linq::OrderBy`'s use of raw `<`/`==` for floating values is SR-AUD-046, a
   different finding in a different family. This plan changes *whether the
   callable exists*, never *what it computes*, and must not be read as touching
   SR-AUD-046.
5. **SR-AUD-051 / SR-AUD-044 (`Array::Copy`)** and **SR-AUD-053
   (`Array::MaxLength`)** — same file, unrelated causes, still `confirmed`. Not
   folded in.
6. **`std::function` parameters outside these six types** — e.g. `Delegate`,
   `MulticastAction`, `Threading` callbacks, collection comparers. The audit
   found no evidence for them and this plan issues no identifier for them; a
   future sweep may extend the policy, but nothing here asserts they are correct.
7. **Making emptiness a compile-time error** (e.g. constraining the templates to
   reject `std::function` and demand a concrete callable type) — rejected: it
   would be a genuine source break for every consumer that legitimately passes a
   `std::function` variable, which is the ordinary way ported C# code stores a
   delegate.

## 16. Completion criteria

CCF-011 is **closed** when all of the following hold:

1. All six findings — SR-AUD-052, 058, 065, 099, 121, 134 — are `remediated` in
   `audit/AUDIT_FINDINGS_INDEX.md` and in their per-file reports, with the §7
   corrections appended (historical text preserved).
2. All 36 public entries in §4 implement the §8 policy, in the §8 order.
3. The post-fix run of `build-probe/1866_empty_callable_probe.cpp` shows
   **zero** `bad_function_call` outcomes and zero silent `no-throw` outcomes for
   a rejecting entry; every no-op entry shows `no-throw` with an unchanged
   `Size()`.
4. The §12 test matrix is landed, add-only, and the repository gate shows no
   test-count regression against the 14,396 floor.
5. ASan + UBSan + LSan clean per §13; TSan recorded as not applicable.
6. `cmake --build build --parallel 3` is clean — zero errors, zero warnings.
7. `scripts/local_ci_check.sh build` passes; Doxygen stays within the 1,942
   ceiling; the module graph stays at 41 modules / 91 edges; negative fixtures
   stay at 9/66 and version seams at 2/18 (this family adds none).
8. `AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-011 gains a closure paragraph in the
   established form.

## 17. Ticket breakdown

| Ticket | Findings | Scope | Size | Status |
|---|---|---|---|---|
| **#1866** | — | This plan. Design/planning only; no production change; no finding remediated. | M | todo → done |
| **#1867** | SR-AUD-065, SR-AUD-099 | `Lazy<T>`'s three factory constructors reject an empty factory with `ArgumentNullException("valueFactory")` at construction; `AggregateException::Handle` rejects an empty predicate with `ArgumentNullException("predicate")` before the loop, including for an empty inner list. | S | todo |
| **#1868** | SR-AUD-058, SR-AUD-121 | `Progress<T>::addProgressChangedHandler` and `EventHandler::Add`/`operator+=` treat an empty handler as a no-op, before the replay hook; `OnReport`/`Raise` skip untruthy handlers. `Size()`/`Empty()` unchanged by an empty add. | S | todo |
| **#1869** | SR-AUD-052 | All 17 `Array` delegate overloads reject an empty callable with the .NET `paramName`, in the .NET order (range-first for `FindIndex`, callable-first for `FindLastIndex`); the twelve `Predicate<T>`-shaped parameters are renamed `predicate` → `match`. | M | todo |
| **#1870** | SR-AUD-134 | All 11 `Linq` callback overloads reject an empty callable before touching the sequence, so the argument error precedes `First`'s `InvalidOperationException`. | M | todo |

Each implementation ticket lands its own tests, re-runs the §6 probe, and updates
its per-file audit report and this document's §18 status table.

## 18. Implementation status

| Finding | Ticket | Status |
|---|---|---|
| SR-AUD-052 | #1869 | **`remediated`** (2026-07-30) |
| SR-AUD-058 | #1868 | **`remediated`** (2026-07-30) |
| SR-AUD-065 | #1867 | **`remediated`** (2026-07-30) |
| SR-AUD-099 | #1867 | **`remediated`** (2026-07-30) |
| SR-AUD-121 | #1868 | **`remediated`** (2026-07-30) |
| SR-AUD-134 | #1870 | `confirmed` |

*(Updated by each implementation ticket as it lands.)*
