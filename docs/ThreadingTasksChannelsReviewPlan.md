<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `Threading.Tasks` + `Threading.Channels` namespace review

**Ticket #1964** (`REVIEW-THREADING-TASKS-CHANNELS`, P2, size M, **review/design**).
Written 2026-08-03 on branch `feature/remediation-batch-threading-1951-1955`, immediately
after tickets #1951–#1955 closed the compatible half of the `System::Threading` namespace
review (`docs/ThreadingNamespaceReviewPlan.md`).

This is the follow-on review that document's §4 and §13.1 named: *"`Threading.Tasks` (3 open)
and `Threading.Channels` (3 open) are excluded. They are separate components with their own
dependency edges … They are the natural follow-on review, not part of this one."*

It issues **no new `SR-AUD-*` identifier** — audit numbering stays frozen at **364** — and
marks **no finding remediated merely because it is planned**. Newly discovered defects, if
any, get ordinary inactive tickets.

---

## 1. Why these two together, and why now

`System::Threading`'s compatible queue is drained: #1947–#1949 and #1951–#1955 closed nineteen
of its 38 findings, and the seventeen that remain are owned by the four approval-gated design
tickets #1956–#1959 plus verification ticket #1963. Nothing further can land there without a
user decision.

These two components are reviewed as **one unit** because they are one dependency chain and
one problem domain:

```
Core.Base ──► Threading ──► Threading.Tasks ──► Threading.Channels
```

`Threading.Channels` is an **INTERFACE** target whose only public dependency is
`Threading.Tasks`; every channel operation returns a `Task`/`TaskT`. Reviewing channels
without tasks would mean reviewing a caller without its callee. Combined, they are **six open
findings across nineteen headers** — small enough for one plan, too coupled to split.

---

## 2. Scope and file inventory

| Component | Target | Type | `PUBLIC_DEPENDENCIES` |
|---|---|---|---|
| `Threading.Tasks` | `sharp_runtime_threading_tasks` | STATIC | `Core.Base Threading` |
| `Threading.Channels` | `sharp_runtime_threading_channels` | **INTERFACE** | `Core.Base Threading.Tasks` |

| Kind | Count | Location |
|---|---|---|
| Public headers, `System::Threading::Tasks` | 14 | `modules/threading-tasks/include/System/Threading/Tasks/*.hpp` |
| Public headers, `System` (module-owned) | 1 | `IAsyncDisposable.hpp` |
| Public headers, `System::Threading::Channels` | 4 | `modules/threading-channels/include/System/Threading/Channels/*.hpp` |
| Implementation bodies | 3 | `TaskCanceledException.cpp`, `TaskScheduler.cpp`, `TaskSchedulerException.cpp` — all in `threading-tasks`; **`threading-channels` has none** |
| Test translation units | 2 | `TasksTests.cpp` (171 `TEST`s), `ChannelTests.cpp` (39 `TEST`s) |
| Total header lines | 3,015 | both trees |

Two structural facts drive everything below.

1. **`Threading.Channels` is header-only by construction** — an INTERFACE target with no
   `src/`. Every member is inline, so *every* layout question there is consumer-visible, with
   no `.cpp` containment at all. This is the same constraint `docs/ThreadingNamespaceReviewPlan.md`
   §2 identified for `System::Threading`, and here it is absolute.
2. **`Task` is a dependency of two other modules' open findings.** SR-AUD-263
   (`Net.Sockets`) and SR-AUD-310 (`Net.Http`) both name `Task.hpp` among their sources: both
   are raw-`this` captures in asynchronous methods built on `Task`. Any change to `Task`'s
   ownership model interacts with them. They are **not** members of this review (§4), but a
   `Task` repair must not make either harder.

---

## 3. Confirmed finding inventory (all six)

Status re-derived from `audit/AUDIT_FINDINGS_INDEX.md` on 2026-08-03 **and re-checked against
current source for this review**. All six still reproduce; none is a duplicate, none is stale,
none rests on a false premise.

| ID | Sev | Type | Defect | Verified in current source |
|---|---|---|---|---|
| SR-AUD-230 | **high** | `TaskCanceledException` | `getTaskProperty()` returns a `const Task*` stored at construction; ASan reports `stack-use-after-scope` when the source `Task` has gone | `TaskCanceledException.hpp:20` `const Task* canceledTask_ = nullptr;`, `:56` returns it unguarded. The header *documents* the hazard, which does not make it safe |
| SR-AUD-231 | med | `Task`, `TaskT`, `TaskFactory` | `Run` and `ContinueWith` accept an empty `std::function` and fault asynchronously with `bad_function_call`; .NET throws `ArgumentNullException` synchronously | `Task.hpp:342` `static Task Run(std::function<void()> action) { return Task(std::move(action)); }` — no check; same for `:345`, `:299`, `:792`, `:840` |
| SR-AUD-232 | med | `Parallel` | `MaxDegreeOfParallelism <= 0` is silently rewritten to `hardware_concurrency()`; .NET rejects everything below −1 | `Parallel.hpp:164-166`, `if (maxDeg <= 0) maxDeg = hardware_concurrency();` in the options overload |
| SR-AUD-233 | med | `BoundedChannelOptions`, `Channel` | capacity 0 becomes a one-element buffer; .NET's zero-capacity channel is a **rendezvous** channel | `Channel.hpp:161` `effectiveCapacity() { return capacity == 0 ? 1 : capacity; }`, used at `:239` and `:277` |
| SR-AUD-234 | med | `ChannelReader::ReadAsync` | rethrows the writer's completion error directly; .NET throws `ChannelClosedException` **with that error as `InnerException`** | `Channel.hpp` default `ReadAsync()` propagates the stored exception unwrapped; the sibling `WaitToReadAsync`/`WaitToWriteAsync`/`Completion` paths are correct |
| SR-AUD-235 | med | `BoundedChannelOptions` | `FullMode` is a **public mutable enum field** with no validation; `static_cast<BoundedChannelFullMode>(99)` lets a capacity-1 channel reach `Count == 2` | `ChannelOptions.hpp:39` `BoundedChannelFullMode FullMode = BoundedChannelFullMode::Wait;` — a bare public data member |

### 3.1 Corrections and observations recorded rather than absorbed

1. **SR-AUD-235's repair is blocked by the field's *shape*, not by its logic.** `FullMode` is a
   public data member, so there is nowhere to put a check. Validating it requires converting it
   to `getFullModeProperty()`/`setFullModeProperty()` — a **public source break** for every
   consumer that writes `opts.FullMode = …`. The finding does not say this; it is the single
   most important fact about repairing it, and it is why §8 gates SR-AUD-235 on approval while
   the other four validation findings are compatible.
2. **SR-AUD-233's repair changes a *documented and tested* behaviour.** The report itself says
   *"the two existing zero-capacity tests lock in the incompatible native behavior"*. Those
   tests must be rewritten, not merely added to. Under this repository's "no test may be
   weakened to go green" rule, replacing an assertion that pins wrong behaviour is legitimate,
   but it must be called out in the ticket rather than done quietly.
3. **`Parallel` uses `std::thread::hardware_concurrency()` and that is *not* a CLAUDE.md
   violation.** The build-resource policy forbids a runtime core count "used to choose a
   **build-job** count". `Parallel::For` choosing a *worker* count at runtime is a different
   thing entirely and is what .NET's own default does. Recorded here so the next reader does
   not report it. (SR-AUD-232 is about rejecting invalid *explicit* values, not about the
   default.)
4. **SR-AUD-231's site count is larger than the finding's two named entries.** `Run` and
   `ContinueWith` are named; the same unchecked route exists on `Task::Run(action, token)`,
   `TaskT<T>`'s `ContinueWith` overloads, and `TaskFactory::StartNew` — the finding's own body
   says so ("inherited by generic `TaskT` and `TaskFactory::StartNew` overloads"). The
   implementing ticket must enumerate every entry by measurement, exactly as #1951 did for
   `System::Threading`'s seven.
5. **`Parallel`'s empty-body case is recorded in SR-AUD-232's report but is a T-B/CCF-011
   shape, not a domain-validation one** ("an empty `std::function` becomes an aggregate of
   delayed `bad_function_call` exceptions"). It belongs with SR-AUD-231's repair, not with the
   degree-of-parallelism repair.

---

## 4. Findings that are *not* in this review's queue

- **SR-AUD-263 (`Net.Sockets`) and SR-AUD-310 (`Net.Http`)** name `Task.hpp` among their
  sources, but their defect is a raw-`this` capture in *their own* asynchronous methods, not in
  `Task`. They belong to their own modules' reviews. Recorded because a `Task` ownership change
  would touch them (§2).
- **`System::Threading`'s seventeen remaining findings** — owned by #1956–#1959 and #1963.
- **`ExecutionContext` asynchronous flow, reflection, GC and serialization** — CLAUDE.md
  permanent deviations, unchanged here.
- **`ValueTask`, `TaskScheduler`, `TaskExtensions`, `UnobservedTaskExceptionEventArgs`,
  `ConfigureAwaitOptions`, `IAsyncDisposable`, `TaskStatus`, `TaskCreationOptions`,
  `TaskContinuationOptions`, `TaskSchedulerException`, `ChannelClosedException`,
  `BoundedChannelFullMode`** — audited, **no open finding**. Not re-opened here.

---

## 5. Shared root causes

Four causes account for all six findings. **Three of the four are new sites of causes this
repository has already closed elsewhere**, which is the highest-leverage part of this review.

### TC-A — empty callables cross public boundaries (1 finding, many sites)

`SR-AUD-231` (+ `Parallel`'s empty body, §3.1 item 5).

This is **CCF-011 in a third module**, after `core` (#1866–#1870) and `threading` (#1951). The
policy in `docs/EmptyCallableBoundaryPlan.md` applies verbatim and nothing needs designing: a
delegate *argument* is rejected at the boundary with `System::ArgumentNullException` carrying
.NET's own parameter name (`action`, `continuationAction`, `body`, `function`).

The severity here matches `threading`'s: the failure is not merely deferred, it moves to a
**background thread** and reappears as an ordinary task fault, so a caller sees a task that
failed rather than a call that was wrong.

### TC-B — public arguments are not validated at the boundary (3 findings)

`SR-AUD-232, 233, 235`

Non-callable arguments outside their .NET domain: a `MaxDegreeOfParallelism` below −1, a
zero capacity silently reinterpreted, an undeclared `BoundedChannelFullMode`. Same cause as
`System::Threading`'s T-C, and the same selected repair — validate at entry, in .NET's order,
with .NET's exception type and parameter name.

**But one member is not compatible.** SR-AUD-235's `FullMode` is a public data member (§3.1
item 1), so validating it at all requires a property pair. That is a public source break and
is gated in §8.

### TC-C — an API-specific exception boundary is lost (1 finding)

`SR-AUD-234`

`ReadAsync` lets the writer's completion error escape instead of wrapping it in
`ChannelClosedException`. This is the same *shape* as the closed **CCF-004-adjacent** exception
taxonomy work and as SR-AUD-034's network-exception propagation (#1932): the inner cause must
be preserved while the API-specific outer type is restored. `ChannelClosedException` already
has the inner-exception constructor the repair needs, which the audit confirms.

### TC-D — a borrowed raw pointer outlives its owner (1 finding)

`SR-AUD-230`

This is **CCF-019's cause** at a third site, after `JsonNode`/`Xml.Linq` (approvals declined,
#1888/#1889/#1896/#1899) and `System::Threading`'s two members (#1959, blocked). The precedent
is directly binding: every previous member of this family either broke public source or was
resolved by documenting the contract. `TaskCanceledException` already *documents* it — and the
audit's explicit position is that documenting does not make the ordinary public
exception/property interaction safe.

---

## 6. Dependency graph

```
    TC-A (empty callables)  ──── independent; reuses closed CCF-011 policy
    TC-B/1 (Parallel degree) ─── independent
    TC-C (ReadAsync wrapper) ─── independent
                    │
                    ▼
    TC-B/2 (zero capacity) ───► must land AFTER TC-C: both change what ReadAsync/TryRead
                                do at the channel's boundary, and both rewrite the same
                                ChannelTests cases
                    ▼
    TC-B/3 (FullMode)     ─────► public source break; design-first, approval-gated
    TC-D (Task pointer)   ─────► public source break; design-first, approval-gated
```

TC-A and TC-B/1 touch disjoint files from the channel work and may proceed in parallel with
it; the order above is by risk and by edit collision, not by compilation dependency.

---

## 7. Implementation versus design-first classification

| Cause | Classification | Rationale |
|---|---|---|
| **TC-A** | **implement** (policy already approved) | `docs/EmptyCallableBoundaryPlan.md` applies verbatim; no new decision |
| **TC-B/1** (SR-AUD-232) | **implement** | validation only; .NET's domain, not a choice |
| **TC-C** (SR-AUD-234) | **implement** | restores an API-specific exception while preserving the cause; the constructor already exists |
| **TC-B/2** (SR-AUD-233) | **implement, with a documented test rewrite** | changes behaviour two existing tests pin (§3.1 item 2) |
| **TC-B/3** (SR-AUD-235) | **design-first, approval-gated** | needs a property pair → public source break |
| **TC-D** (SR-AUD-230) | **design-first, approval-gated** | ownership contract on a public property → public source break, CCF-019 precedent |

---

## 8. Source / ABI / layout approval matrix

| Cause | Public signature | Vtable | Object layout | Accepted input | Observable result | Approval |
|---|---|---|---|---|---|---|
| TC-A | — | — | — | empty callable now rejected | `ArgumentNullException` replaces an asynchronous `bad_function_call` | none (CCF-011) |
| TC-B/1 | — | — | — | degree < −1 and 0 now rejected | `ArgumentOutOfRangeException` replaces a silent core-count substitution | none |
| TC-C | — | — | — | — | `ChannelClosedException` with the cause as inner, replacing the bare cause | none |
| TC-B/2 | — | — | — | — | capacity 0 becomes a rendezvous channel; `TryWrite`/`TryRead` return `false` without a peer | none, **but two existing tests are rewritten** |
| TC-B/3 | **yes** (`FullMode` field → property pair) | — | possible | undeclared mode now rejected | `ArgumentOutOfRangeException` on assignment | **required** |
| TC-D | **yes** (`const Task*` → an owning handle) | — | possible | — | ownership transfer | **required** |

---

## 9. Explicit approval requirements

Two questions, stated so either can be approved independently. Neither is asked during
implementation; both are recorded here.

1. **TC-B/3 — `BoundedChannelOptions::FullMode` becomes a property pair.** May the public
   mutable field `FullMode` be replaced by `getFullModeProperty()`/`setFullModeProperty()` so
   an undeclared `BoundedChannelFullMode` can be rejected with `ArgumentOutOfRangeException` on
   assignment, as .NET does? Every consumer writing `opts.FullMode = …` must be edited. This is
   a **public source break** with no ABI-compatible alternative: a bare data member has nowhere
   to put a check.
2. **TC-D — `TaskCanceledException`'s task pointer.** May `getTaskProperty()` stop returning a
   borrowed `const Task*`, in favour of an owning handle (or of removing the accessor), so the
   ASan-confirmed `stack-use-after-scope` becomes unreachable? This is the same class of change
   as #1959 and the same family (CCF-019) whose `Xml.Linq` and `JsonNode` members were
   **declined**; the fallback if declined is the one #1899 took — document the contract and pin
   it with a negative consumer fixture — except that here the contract is *already* documented
   and the audit has already rejected documentation as sufficient, so declining leaves the
   finding open with a completed design and no further work available.

---

## 10. Test and sanitizer strategy

| Cause | Required cases | Sanitizers |
|---|---|---|
| TC-A | per entry: empty callable rejected at entry with .NET's parameter name; the first valid callable unaffected; **no task started and no continuation registered before the throw**; the exception is catchable as `System::Exception` — which `bad_function_call` was not | — |
| TC-B/1 | degrees −2, −1, 0, 1, and a value above the core count; assert the invalid ones run **no body at all** | — |
| TC-C | error-completed FIFO **and** prioritized channels: `ReadAsync` throws `ChannelClosedException` and its inner exception is the writer's original; `WaitToReadAsync`/`WaitToWriteAsync`/`Completion` keep exposing the cause unwrapped, as they correctly do today | — |
| TC-B/2 | rendezvous with and without a blocked peer; `TryWrite`/`TryRead` return `false` with no peer; a blocked reader is handed the writer's item; **the two capacity-one expectations in the existing zero-capacity tests are replaced, not deleted** | — |
| TC-B/3 | every undeclared mode rejected on assignment and at construction; each declared mode still selects its documented behaviour; `Count` never exceeds `Capacity` | — |
| TC-D | the ASan probe that reports `stack-use-after-scope` today reports nothing after | **ASan required** (the finding *is* an ASan report), **LSan required** (an owning handle must not convert a use-after-free into a leak) |

Channels are inherently concurrent, so every channel ticket additionally runs its focused
suite under **TSan**, and the batch's standing threading axes apply: zero/one/maximum counts,
timeout zero and infinite, cancellation before entry, during wait and after release,
signal-before-wait and wait-before-signal, spurious wake-ups, disposal while idle and while
waiting, lost wake-ups, duplicate releases, and counter preservation after failure.

The focused executables are `./build/SharpRuntimeTests_Threading_Tasks` and
`./build/SharpRuntimeTests_Threading_Channels`. No test may be weakened or deleted to go
green; the two rewrites in TC-B/2 are replacements of assertions that pin *wrong* behaviour and
must be identified as such in the commit.

---

## 11. Recommended ticket order

| # | Ticket | Cause | Findings | Status at creation |
|---|---|---|---|---|
| 1 | **#1965** apply the CCF-011 empty-callable policy to Threading.Tasks | TC-A | SR-AUD-231 (+ `Parallel` body) | `todo` |
| 2 | **#1966** validate Parallel's MaxDegreeOfParallelism | TC-B/1 | SR-AUD-232 | `todo` |
| 3 | **#1967** ReadAsync must throw ChannelClosedException with the cause inside | TC-C | SR-AUD-234 | `todo` |
| 4 | **#1968** a zero-capacity channel is a rendezvous, not a one-element buffer | TC-B/2 | SR-AUD-233 | `todo` |
| 5 | **#1969** DESIGN: BoundedChannelOptions::FullMode must be validatable | TC-B/3 | SR-AUD-235 | `blocked` (approval 1) |
| 6 | **#1970** DESIGN: TaskCanceledException's borrowed task pointer (CCF-019) | TC-D | SR-AUD-230 | `blocked` (approval 2) |

Tickets 1–4 need no approval, no layout measurement and no new design, exactly like
#1951–#1954 in the predecessor review. #1965 is first because it is the largest and reuses a
policy that is already written down; #1967 precedes #1968 because both rewrite the same
`ChannelTests` cases.

---

## 12. Explicit exclusions

1. **`Net.Sockets`' SR-AUD-263 and `Net.Http`' SR-AUD-310**, which merely *name* `Task.hpp`
   (§4).
2. **Asynchronous continuation scheduling parity** — this port's `ContinueWith` runs its
   continuation on the completing thread rather than through a scheduler; that is a documented
   adaptation with no finding, and it is not made one here.
3. **`TaskScheduler` as a real scheduler** — the port has no thread-pool scheduler and the
   header says so. No finding.
4. **`ValueTask`'s allocation-avoidance guarantee** — meaningless without .NET's
   `IValueTaskSource`; documented, no finding.
5. **`Parallel`'s `std::thread::hardware_concurrency()` default** — correct, and *not* a
   CLAUDE.md build-resource-policy violation (§3.1 item 3).
6. **`ChannelReader`'s `enable_shared_from_this` requirement** — a deliberate lifetime design
   already documented in the header, and the thing that makes the default `ReadAsync()` safe.
   No finding, and TC-C must not disturb it.

---

## 13. Completion criteria

`Threading.Tasks` + `Threading.Channels` is closed when:

1. all six findings are `remediated`, or `confirmed` with a completed design and a named
   blocked ticket carrying an exact approval request;
2. no public entry point in either component reaches `std::bad_function_call`, an unwrapped
   library exception, or a dangling pointer for any input a caller can supply;
3. both focused executables grow monotonically and no test is weakened — the two TC-B/2
   rewrites being replacements of wrong-behaviour assertions, identified as such;
4. the module graph stays **41 / 91** — no repair above needs a new edge, since
   `ArgumentNullException`, `ArgumentOutOfRangeException` and `ChannelClosedException` are all
   already reachable;
5. every layout- or signature-affecting change quotes measured before/after `sizeof`/`alignof`
   and carries its own explicit approval.

---

## 14. Status

Written 2026-08-03. **This document changed no production source and closed no finding.**
Tickets **#1965–#1968** are `todo` and unstarted; **#1969** and **#1970** are `blocked` on the
two approvals in §9. The predecessor review's own blocked set — #1956–#1959, with its
consolidated approval package in `docs/ThreadingNamespaceReviewPlan.md` §20 — is unaffected
and still awaiting the same user decision.

---

## 15. What #1965 measured, and five corrections to §3 and §5 (2026-08-03)

Ticket **#1965** implemented cause **TC-A** by applying the already-approved CCF-011 policy
from `docs/EmptyCallableBoundaryPlan.md`. Evidence:
`build-probe/1965_probe1_tasks_empty_callables.cpp` (37 cases) with logs
`1965_probe1_before.log`, `1965_probe1_after.log` and `1965_probe1_asan.log`.

### 15.1 §3.1 item 4 was right to warn, and the real number is 22

The finding names two entries; §3.1 item 4 predicted more. Measured, the entries that
accept an empty callable are **22**, of which **eleven distinct bodies** needed an edit —
the other eleven inherit the check by forwarding:

| Type | Bodies edited | Inheriting by forwarding | .NET parameter name |
|---|---|---|---|
| `Task` | `Task(action)`, `Task(action, token)`, `ContinueWith` | `Run(action)`, `Run(action, token)` | `action` / `continuationAction` |
| `TaskT<T>` | `TaskT(func)`, `TaskT(func, token)`, both `ContinueWith` overloads | `Run(func)`, `Run(func, token)` | **`function`** / `continuationAction` / `continuationFunction` |
| `TaskFactory` | — | four `StartNew` overloads (+ `Task::Factory()`) | `action` / `function` |
| `Parallel` | `For(opts)`, `For(state)`, both `ForEach`, `Invoke` | `For(from,to,body)` | `body` / *(none — see §15.3)* |

`TaskT<TResult>`'s result-producing `ContinueWith` takes an **unconstrained callable
type**, not a `std::function`, so it needed `detail::isEmptyCallable` — a
`requires`-guarded comparison with `nullptr` that catches `std::function`, function
pointers and pointers to members, and deliberately never fires for a lambda or functor,
which has no null state.

### 15.2 §5's TC-A severity claim is wrong for `Parallel` — the failure was catchable

§5 says the failure "reappears as an ordinary task fault, so a caller sees a task that
failed rather than a call that was wrong", inheriting CCF-011's third consequence: the
error is outside the `System::Exception` hierarchy and ported
`catch (const System::Exception&)` code cannot see it.

Measured, that is true for the sixteen `Task`/`TaskT`/`TaskFactory` entries — a bare
`std::bad_function_call` escaped `Wait()`/`getResultProperty()` — and **false for all six
`Parallel` entries**: `parallel.for.body=aggregate:bad_function_call`. `Parallel` already
collected every worker exception into `System::AggregateException`, which *is* a
`System::Exception`. What was wrong there was the diagnostic and its timing, not the
catchability.

### 15.3 `Parallel::Invoke` is not an `ArgumentNullException` site

This is `docs/ThreadingNamespaceReviewPlan.md` §17.1 repeating itself in a third module.
.NET's `Parallel.Invoke(params Action[] actions)` copies the array and rejects a null
**element** with `new ArgumentException(SR.Parallel_Invoke_ActionNull)` — an
`ArgumentException` with **no parameter name** — because the null is an element of the
argument, not the argument. Implemented as
`ArgumentException("One of the actions was null.")` and pinned by a test that *fails* if
`ArgumentNullException` is thrown, so the family's usual spelling cannot be restored by a
later reader.

**Reference-evidence limitation, stated rather than glossed:**
`/rv/tmp/runtime/src/libraries/` is **not present in this environment**. The per-entry
parameter names and this exception type come from the .NET API contract for the exact
overloads listed above rather than from a fresh reading of local source, and the audit's
own managed probe supplies only the `ArgumentNullException` *category* for `Task.Run` and
`ContinueWith`. Where the answer was uncertain the conservative choice was taken — the
**base** `ArgumentException` for `Invoke` — following #1954's precedent for SR-AUD-184.

### 15.4 Two data-dependent silent shapes the finding does not name

- **Zero iterations.** `Parallel::For(0, 0, {})`, both `For` state overloads and both
  `ForEach` overloads over an empty source returned a normally completed
  `ParallelLoopResult` with an empty body. Same wrong call, silent or fatal by iteration
  count — the exact shape `docs/EmptyCallableBoundaryPlan.md` §7.1 recorded for `core`.
- **An already-cancelled token.** `Task(action, cancelledToken)` and its `TaskT`
  counterpart short-circuit to Canceled *before* launching, so an empty action produced an
  ordinary Canceled task and never reported the bad argument. The repair puts the argument
  check **above** the short circuit, matching .NET's `InternalStartNew`, which validates
  the delegate before the task exists. This is an intended observable change: a call that
  used to yield a Canceled task now throws.

### 15.5 `ContinueWith` recorded the continuation before failing

`task.continuewith.registered_before_throw=no-throw|side_effects=1`: the empty
continuation *was* registered on the antecedent and the returned continuation Task was
left **faulted**, so `ContinueWith` returned normally and handed back a broken task. The
check therefore had to precede both the continuation Task's construction and
`registerContinuation`, which the no-partial-state regressions pin.

### 15.6 An ordering requirement this places on #1966

`ParallelOptions::MaxDegreeOfParallelism` is a **public data member** in this port, so
#1966 must validate it inside `Parallel::For` rather than at assignment. In .NET the
invalid degree is rejected by the `ParallelOptions.MaxDegreeOfParallelism` *setter*, which
necessarily runs **before** `Parallel.For` is called and therefore before .NET's own
`body` null check. #1966's degree check must consequently be inserted **above** #1965's
`requireNonEmptyBody` call, not beside it — the same shape as
`docs/ThreadingNamespaceReviewPlan.md` §17.3's `SpinUntil` constraint on #1954.

### 15.7 Nothing in TC-A touched layout, ABI or a signature

Eleven inline bodies gained an entry check; one new file-local helper
(`detail::isEmptyCallable`) and one class-private helper (`Parallel::requireNonEmptyBody`)
were added, neither a data member. No signature, template parameter, default argument,
virtual, `noexcept` specification or overload set changed. `ArgumentNullException` and
`ArgumentException` both live in `Core.Base`, which `Threading.Tasks` already depends on
publicly, so the module graph stays **41 / 91**.

### 15.8 Sanitizers and test count

ASan + UBSan + LSan over all 37 probe cases: **0 reports before, 0 after**, exit 0. This
was an exception-contract defect, not a memory defect; the sanitizer's role is CCF-011
§13's leak check on entry points that now throw *after* copying a `std::function` by
value. Instrumentation was proved, not assumed: 34 `__asan`/`__ubsan` symbols in the
sanitized binary, 0 in the plain one, and the sanitized run's outcomes are identical to
the plain run's.

`modules/threading-tasks/tests/System/Threading/Tasks/TasksBoundaryTests.cpp` is a new
translation unit holding the boundary family's permanent regressions: **+37** cases,
`SharpRuntimeTests_Threading_Tasks` **171 → 208**, all passing.

**SR-AUD-231: `confirmed` → `remediated`.** Cause TC-A is closed.
