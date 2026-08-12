<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Threading` namespace review

**Ticket #1950** (`REVIEW-THREADING-NAMESPACE`, P1, size L, **review/design**).
Written 2026-08-03 on branch `claude/remediation-batch-1804-namespace-b1yjh5`.

This is the evidence-based namespace review that converts the **38 open audit
findings** in `modules/threading/` into a bounded, ordered ticket queue. It issues
**no new `SR-AUD-*` identifier** — audit numbering stays frozen at **364** — and it
marks **no finding remediated merely because it is planned**.

It is the successor deliverable to the collections-family reviews
(`docs/CollectionsComparisonContractPlan.md`, `docs/ComparisonContractPlan.md`) and
follows their structure. Where a cause here is a **new site of an already-closed
cross-cutting family**, this document says so explicitly and reuses that family's
selected policy rather than inventing a second one.

---

## 1. Why `System::Threading` is next

The queue selection is not alphabetical. At the time of writing, `plan.sqlite3`
contains **zero `todo` tickets**; every remaining row is `blocked` (downstream or
approval) or `needs_user`. The `task` table's mechanical porting queue is
**exhausted** (0 rows at `''`/`todo`), so `prompt.md` Step 1 selects nothing and the
namespace question has to be answered from the audit inventory instead.

Open confirmed findings by owning module, measured from
`audit/AUDIT_FINDINGS_INDEX.md` on 2026-08-03:

| Module | Open | Module | Open |
|---|---|---|---|
| `core` | 72 | `net` | 10 |
| **`threading`** | **38** | `net-http` | 9 |
| `runtime` | 21 | `diagnostics` | 8 |
| `uri` | 14 | `xml` | 8 |
| `text` | 14 | everything else | ≤ 7 each |

`core` is larger but is **not** a coherent review unit: its 72 findings are already
carved up by seven closed or in-flight `CCF-*` families (CCF-003 numeric wrappers,
CCF-005 conversion boundary, CCF-006/007/008 numeric formatting and rounding,
CCF-011 empty callables, CCF-012 composite format), so a "core namespace review"
would mostly re-plan work that already has a durable family plan.

`System::Threading` is selected because all six selection criteria hold at once:

1. **Density and severity.** 38 open findings, **14 high**, 22 medium, 2 low — the
   largest untouched severity concentration in the repository. Sixteen of them are
   memory-safety, undefined-behaviour, or hang defects with sanitizer or
   timeout evidence already captured at audit time.
2. **No family plan exists.** Unlike `core`, not one `docs/*Plan.md` or
   `docs/*Design.md` covers a threading type. Nothing here would be re-planned.
3. **Dependency readiness.** `modules/threading` declares only
   `PUBLIC_DEPENDENCIES Core.Base TimeZone`. Every repair proposed below is
   internal to the module or reuses a policy already landed in `Core.Base`; none
   requires a new component edge, so the module graph stays at **41 / 91**.
4. **Coherent boundaries.** The 38 findings collapse into **nine** shared root
   causes (§5), not 38 independent bugs. Three of the nine are new sites of
   *already-approved, already-closed* repository policies — which is exactly the
   "one structural fix closes a family" shape this review is supposed to find.
5. **Independent validation.** `SharpRuntimeTests_Threading` is a single
   executable; every repair below can be validated focused before the full gate.
6. **Not blocked on anything.** No threading finding depends on #1773 (downstream),
   on the `#1929` date/time approval chain, or on any declined layout approval.

Explicitly **not** selected, and why: `runtime` (21) is dominated by reflection and
serialization surfaces that CLAUDE.md already classifies as permanent deviations;
`uri` (14) and `text` (14) are smaller and lower-severity; `core` (72) is covered by
existing family plans.

---

## 2. Scope and file inventory

Component **`Threading`** → target `sharp_runtime_threading`,
`modules/threading/CMakeLists.txt`, `PUBLIC_DEPENDENCIES Core.Base TimeZone`.

| Kind | Count | Location |
|---|---|---|
| Public headers, `System::Threading` | 55 | `modules/threading/include/System/Threading/*.hpp` |
| Public headers, `System` (module-owned) | 4 | `AsyncCallback.hpp`, `IAsyncResult.hpp`, `OperationCanceledException.hpp`, `TimeProvider.hpp` |
| Implementation bodies | 4 | `src/System/OperationCanceledException.cpp`, `src/System/Threading/CancellationToken.cpp`, `src/System/Threading/Thread.cpp`, `src/System/TimeProvider.cpp` |
| Test translation units | 7 | `tests/System/Threading/{ThreadingTests,ThreadingRemainingTests,Batch8ThreadingTests,Batch9ThreadingTests}.cpp`, `tests/System/{AsyncCallbackTests,OperationCanceledExceptionTests,TimeProviderTests}.cpp` |
| `TEST(...)` cases in those units | 343 | 91 + 135 + 42 + 46 + 4 + 15 + 10 |

The module is **overwhelmingly header-only**: 55 of 59 public headers carry their
whole implementation inline. That is the single most important structural fact for
§10 — a change to a private data member in this module has **no** `.cpp`-only
containment, so every layout question is a consumer-visible question.

### 2.1 Public-surface inventory by defect-bearing type

| Type | Header | Public surface touched by a finding |
|---|---|---|
| `Barrier` | `Barrier.hpp` | `getParticipantCountProperty`, `getCurrentPhaseNumberProperty`, post-phase action invocation |
| `CancellationToken` | `CancellationToken.hpp` | shared-state constructor, `Register` |
| `CountdownEvent` | `CountdownEvent.hpp` | `Reset(intcs)`, `Dispose`, `ThrowIfDisposed` |
| `EventWaitHandle` | `EventWaitHandle.hpp` | `EventWaitHandle(bool, EventResetMode)` |
| `ExecutionContext` | `ExecutionContext.hpp` | `Run(ExecutionContext*, …)` |
| `LazyInitializer` | `LazyInitializer.hpp` | `EnsureInitialized` (both overloads) |
| `ManualResetEventSlim` | `ManualResetEventSlim.hpp` | `Dispose`, disposal-guarded members |
| `Monitor` | `Monitor.hpp` | `Wait` |
| `Mutex` / `AutoResetEvent` / `ManualResetEvent` | three headers | `Close` |
| `PeriodicTimer` | `PeriodicTimer.hpp` | constructor, `WaitForNextTick` |
| `ReaderWriterLockSlim` | `ReaderWriterLockSlim.hpp` | `Dispose`, all `TryEnter*`, `getRecursionPolicyProperty` |
| `RegisteredWaitHandle` | `RegisteredWaitHandle.hpp` | construction via `ThreadPool::RegisterWaitForSingleObject` |
| `Semaphore` / `SemaphoreSlim` | two headers | `Release(intcs)`, `getCurrentCountProperty`, `Dispose` |
| `SpinWait` | `SpinWait.hpp` | both `SpinUntil` overloads |
| `SynchronizationContext` | `SynchronizationContext.hpp` | `SetSynchronizationContext`, `Current`, `Send` |
| `Thread` | `Thread.hpp` | constructor, `Start()`, `Start(void*)`, `CurrentThread().ManagedThreadId` |
| `ThreadLocal` | `ThreadLocal.hpp` | factory constructors, `Dispose`, `getIsValueCreatedProperty`, `trackAllValues` |
| `ThreadPool` | `ThreadPool.hpp` | `UnsafeQueueUserWorkItem`, `SetMinThreads`, `SetMaxThreads`, `RegisterWaitForSingleObject` |
| `ThreadStartException` | `ThreadStartException.hpp` | all three constructors |
| `Timer` / `ITimer` | `Timer.hpp`, `TimeProvider.cpp` | constructor callback, `Change` after `Dispose` |
| `AsyncLocal` | `AsyncLocal.hpp` | `setValueProperty` notification order |
| `WaitHandle` | `WaitHandle.hpp` | `WaitAll`, `WaitAny` (both overloads each) |

---

## 3. Confirmed finding inventory (all 38)

Status column is the **verified** status as of 2026-08-03, re-derived from
`audit/AUDIT_FINDINGS_INDEX.md` and re-checked against current source — not copied
from the audit prose. Every one is still `confirmed`; **none** has been silently
remediated since the audit, and **none** is a duplicate or a false premise.

| ID | Sev | Type | One-line defect | Cause |
|---|---|---|---|---|
| SR-AUD-183 | med | `WaitHandle` | `WaitAll`/`WaitAny` accept empty vectors, null elements and `-2` timeouts; `WaitAny({})` never terminates | **T-C** |
| SR-AUD-184 | med | `EventWaitHandle` | underlying `EventResetMode` 42 accepted, giving mixed auto/manual behaviour | **T-C** |
| SR-AUD-187 | **high** | `ThreadPool` | `UnsafeQueueUserWorkItem` detaches a borrowed `IThreadPoolWorkItem*` — ASan heap-use-after-free | **T-D** |
| SR-AUD-188 | **high** | `ThreadPool`/`RegisteredWaitHandle` | null `waitObject` accepted, background worker dereferences null and aborts the process | **T-C** |
| SR-AUD-189 | med | `ThreadPool` | `SetMinThreads`/`SetMaxThreads` return `true` and store nothing; invalid input accepted | **T-H** |
| SR-AUD-190 | med | `Timer` | empty callback accepted; timer created that can never notify | **T-B** |
| SR-AUD-191 | med | `ITimer` | `Change` after `Dispose` returns `true` although the worker is stopped | **T-G** |
| SR-AUD-192 | **high** | `Thread` | empty start function reaches `bad_function_call` on the worker and terminates the process | **T-B** |
| SR-AUD-193 | med | `Thread` | every externally created thread reports `ManagedThreadId == 1` | **T-H** |
| SR-AUD-194 | med | `Thread` | `Start(void*)` captures then discards its advertised parameter | **T-H** |
| SR-AUD-195 | low | test | running-state assertion is `state & 0 == 0` — passes for every `ThreadState` | **T-I** |
| SR-AUD-196 | med | `ThreadStartException` | runtime-internal constructors published as ordinary public API | **T-H** |
| SR-AUD-197 | low | test | `ManagedThreadId` test casts the only observed value to `void` — no assertion | **T-I** |
| SR-AUD-198 | med | `CancellationToken` | `Register({})` stored and silently skipped at `Cancel` | **T-B** |
| SR-AUD-199 | **high** | `CancellationToken` | public shared-state constructor accepts an empty `shared_ptr`; next property read is a null dereference | **T-C** |
| SR-AUD-200 | med | `PeriodicTimer` | 1.5 ms accepted and truncated although the contract says whole milliseconds | **T-C** |
| SR-AUD-201 | med | `PeriodicTimer` | two concurrent `WaitForNextTick` consumers both consume one tick | **T-E** |
| SR-AUD-202 | **high** | `Monitor` | `Wait` at recursion depth ≥ 2 deadlocks — one level stays held | **T-E** |
| SR-AUD-203 | **high** | `ReaderWriterLockSlim` | ordinary `disposed_` raced with entry (TSan); disposal while a mode is held succeeds | **T-A**, **T-G** |
| SR-AUD-204 | **high** | `ReaderWriterLockSlim` | no queued-writer state; new readers admitted past a waiting writer, starving it | **T-E** |
| SR-AUD-205 | med | `ReaderWriterLockSlim` | invalid `LockRecursionPolicy` stored and reflected verbatim instead of normalising to `NoRecursion` | **T-C** |
| SR-AUD-206 | **high** | `Semaphore`, `SemaphoreSlim` | `count_ + releaseCount > maxCount_` overflows signed `intcs` before the full-semaphore guard (UBSan) | **T-F** |
| SR-AUD-207 | **high** | `SemaphoreSlim`, `ManualResetEventSlim`, `CountdownEvent` | unlocked `CurrentCount` read and unsynchronised `disposed_` (TSan, three types) | **T-A** |
| SR-AUD-208 | med | `Mutex`, `AutoResetEvent`, `ManualResetEvent` | `Close()` is empty; every operation still succeeds afterwards | **T-G** |
| SR-AUD-209 | med | `AutoResetEvent`, `ManualResetEvent` | neither derives from `WaitHandle`, so they cannot enter any multi-wait API | **T-H** |
| SR-AUD-210 | **high** | `Barrier` | post-phase callback runs under `mutex_`, so a legal `CurrentPhaseNumber` read self-deadlocks | **T-E** |
| SR-AUD-211 | **high** | `CountdownEvent` | `Reset(0)` reaches the signalled state but never notifies `cv_`; existing waiters stay blocked | **T-E** |
| SR-AUD-212 | **high** | `Barrier` | `getParticipantCountProperty` reads `participantCount_` outside `mutex_` (TSan) | **T-A** |
| SR-AUD-213 | med | `SpinWait` | `SpinUntil` accepts `-2` and an empty condition; the latter becomes `bad_function_call` | **T-B**, **T-C** |
| SR-AUD-214 | med | `AsyncLocal` | value-change handler fires **before** the write, so it observes the old ambient value | **T-H** |
| SR-AUD-215 | med | `ExecutionContext` | `Run(nullptr, …)` completes normally where .NET throws `InvalidOperationException` | **T-H** |
| SR-AUD-216 | **high** | `LazyInitializer` | ordinary `if (!target)` read races the `atomic_ref` compare-exchange publication (TSan) | **T-A** |
| SR-AUD-217 | med | `LazyInitializer` | empty factory deferred to `bad_function_call`, and suppressed entirely if the target is already set | **T-B** |
| SR-AUD-218 | **high** | `ThreadLocal` | ordinary `disposed_` written by `Dispose` while `Value` reads it (TSan) | **T-A** |
| SR-AUD-219 | med | `ThreadLocal` | empty factory accepted; `IsValueCreated` bypasses the disposal guard | **T-B**, **T-G** |
| SR-AUD-220 | med | `ThreadLocal` | `trackAllValues` stored but never read, and no `Values` surface exists | **T-H** |
| SR-AUD-221 | **high** | `SynchronizationContext` | `Current` keeps a non-owning raw pointer with no reset hook — ASan stack-use-after-scope | **T-D** |
| SR-AUD-222 | med | `SynchronizationContext` | `Send({}, nullptr)` returns normally where the managed base faults | **T-B** |

Two findings appear under two causes (SR-AUD-203, SR-AUD-213, SR-AUD-219); each is
assigned a **primary** owner in §5 so no ticket claims it twice.

### 3.1 Corrections to the audit record

Recorded rather than silently absorbed. None changes a finding's status.

1. **SR-AUD-187's null half is already closed.** The audit report's "Other missing
   assertions" text implies `UnsafeQueueUserWorkItem` accepts anything. Current
   source (`ThreadPool.hpp:78-80`) *does* throw `ArgumentNullException("callBack")`
   for a null work item. Only the **lifetime** half of SR-AUD-187 — the detached
   lambda capturing a borrowed raw pointer at `ThreadPool.hpp:84` — is still live.
   The finding stays `confirmed` on that half alone.
2. **SR-AUD-196 is partly addressed.** Ticket #1875 (2026-08-01) gave all three
   constructors `COR_E_THREADSTART` (`0x80131525`). The *accessibility* half — .NET's
   constructors are `internal`, this port's are public — is untouched and is what
   keeps the finding open. This is already noted in the per-file report; it is
   repeated here so the ticket in §12 is scoped to accessibility only.
3. **SR-AUD-207 spans three types, not two.** The index summary names
   `SemaphoreSlim`, `ManualResetEventSlim` and `CountdownEvent`; the `CountdownEvent`
   report calls its own occurrence an *extension*. All three are members and all
   three must be repaired in the same change, or the family closes falsely.
4. **`ThreadPool::SetMinThreads`/`SetMaxThreads` use `int`, not `intcs`**
   (`ThreadPool.hpp:73-75`), against CLAUDE.md rule 7. `intcs` is `int32_t`, so this
   is a **convention** deviation with no behavioural or ABI consequence on any
   supported platform. It is **not** a new defect and gets no ticket; it is recorded
   in §13 so the next reader does not re-report it. It should be corrected
   opportunistically by whichever ticket rewrites those two bodies (§12, #1958).

---

## 4. Findings that are *not* in this namespace's queue

- **No duplicates.** All 38 IDs are distinct findings.
- **No false premises.** Every one was re-checked against current source for this
  review; §3.1 records the only three places where the *extent* of a finding
  differs from its summary.
- **No stale findings.** Nothing in `modules/threading/` has been remediated since
  the audit except SR-AUD-196's HResult half (§3.1 item 2).
- **`Threading.Tasks` (3 open) and `Threading.Channels` (3 open) are excluded.**
  They are separate components (`modules/threading-tasks`, `modules/threading-channels`)
  with their own dependency edges. Folding them in would make the review's boundary
  the *word* "Threading" rather than the component. They are the natural follow-on
  review, not part of this one.

---

## 5. Shared root causes

Nine causes account for all 38 findings. Three are **new sites of already-closed
repository policies** and inherit their selected repair verbatim — that is the
highest-leverage part of this review.

### T-A — shared mutable state is observed outside its own mutex (7 findings)

`SR-AUD-203, 207 ×3, 212, 216, 218`

Every affected type keeps a `bool disposed_` or an `intcs` counter that is written
under a mutex (or by `Dispose`) and read by a public property or guard **without**
one. Mixing synchronised and unsynchronised access to the same object is C++
undefined behaviour, and TSan confirmed each occurrence at audit time. `LazyInitializer`
is the same cause in its purest form: an ordinary read of the very object another
thread publishes through `std::atomic_ref`.

**Selected repair** (one idiom, six types): a flag that crosses threads becomes
`std::atomic<bool>` with acquire/release ordering; a counter that a public property
exposes is read **under the owning mutex** (the mutex becomes `mutable`);
`LazyInitializer`'s guard read becomes an `std::atomic_ref` load, matching the store
it races. `sizeof(std::atomic<bool>) == sizeof(bool) == 1` and
`alignof(std::atomic<bool>) == 1` on every supported target, and `mutable` is not part
of a type's layout, so this is expected to be **layout-neutral** — but each type's
`sizeof`/`alignof`/offset must be *measured before and after*, exactly as tickets
#1788/#1789 required, because these headers are inline and a growth would be
consumer-visible.

### T-B — empty `std::function` values cross public boundaries (7 findings)

`SR-AUD-190, 192, 198, 213 (part), 217, 219 (part), 222`

This is **CCF-011 in a second module**. CCF-011 was closed for `core` by tickets
#1866–#1870, and `docs/EmptyCallableBoundaryPlan.md` already records the selected,
implemented policy: decide emptiness **at the public boundary, before any input is
examined**, and choose by API *shape* — a delegate **argument** is rejected with
`System::ArgumentNullException` carrying .NET's own parameter name; an event
**subscription** is a no-op; an event **raise** skips untruthy handlers.

Nothing new needs designing. Threading's seven sites are all the *argument* shape,
so all seven become `ArgumentNullException` at entry. The severity here is higher
than in `core`: `Thread` (SR-AUD-192) does not merely throw a native exception, it
**terminates the process** from a worker thread where no caller can catch it.

### T-C — public arguments are not validated at the boundary (6 findings)

`SR-AUD-183, 184, 188, 199, 200, 205, 213 (part)`

Distinct from T-B: these are non-callable arguments — empty collections, null
pointers, out-of-domain enum values, fractional periods, `-2` timeouts. Three of
them are memory-safety defects rather than diagnostics gaps: SR-AUD-188 and
SR-AUD-199 reach a **null dereference**, and `WaitAny({})` (SR-AUD-183) is an
**unbounded loop with no handle to poll**.

**Selected repair:** validate at entry, in .NET's own order, with .NET's own
exception type and parameter name. Invalid enum values follow whichever of the two
.NET conventions the reference type uses — `EventWaitHandle` **rejects**
(`ArgumentException`), `ReaderWriterLockSlim` **normalises** to `NoRecursion` — and
this document does not unify them, because .NET does not.

### T-D — borrowed raw pointers outlive their owner (2 findings)

`SR-AUD-187, 221`

This is **CCF-019's cause** (`audit/AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-019,
"borrowed native handles outlive the owner without a liveness boundary") at two new
sites. Both have ASan evidence: heap-use-after-free for the detached work item,
stack-use-after-scope for the thread-local `Current` pointer.

Unlike T-A/T-B/T-C, CCF-019 is **not closed** — its `JsonNode` and `Xml.Linq`
members are blocked on declined layout approvals (#1888/#1889/#1896/#1899). Any
repair here changes an **ownership contract on a public signature**
(`IThreadPoolWorkItem*` → an owning handle; a raw `SynchronizationContext*` →
something with a liveness boundary). That is a public source break. **Design-first,
approval-gated** — see §9.

### T-E — synchronisation state machines are incomplete (5 findings)

`SR-AUD-201, 202, 204, 210, 211`

Five separate primitives whose state machine is missing one transition:

| Finding | Missing transition |
|---|---|
| SR-AUD-211 | `Reset` reaches the signalled state without notifying the condition variable |
| SR-AUD-210 | the post-phase callback runs holding a lock its own public properties need |
| SR-AUD-202 | `Wait` releases one recursion level, not all of them |
| SR-AUD-204 | no *waiting-writer* state exists, so the read predicate cannot honour it |
| SR-AUD-201 | no *in-flight consumer* state exists, so two waiters consume one tick |

They share a cause but **not** a repair: SR-AUD-211 is one statement, SR-AUD-204 and
SR-AUD-201 need a new private state field each (layout), and SR-AUD-202 and
SR-AUD-210 need the lock discipline restructured. They are therefore **split by
repair size**, not bundled (§12).

### T-F — a defined boundary is computed with signed overflow (1 finding)

`SR-AUD-206` (two types)

This is **CCF-004's cause** — "native-width and fixed-width boundaries must not rely
on signed C++ overflow" — at a site CCF-004's own membership sweep did not reach,
because CCF-004 was scoped to numeric and date/time types. CCF-004 is closed 8/8;
its idiom applies unchanged.

`Semaphore.hpp:91` and `SemaphoreSlim.hpp:108` both evaluate
`count_ + releaseCount > maxCount_`. `Semaphore(1, INT_MAX).Release(INT_MAX)`
overflows before the guard can fire. .NET's own `SemaphoreSlim.Release` writes the
comparison the other way round — `m_maxCount - currentCount < releaseCount` — which
cannot overflow given the class invariant `0 ≤ count_ ≤ maxCount_`. **Adopting
.NET's own expression is the whole repair.**

### T-G — the disposed/closed state is not a state (4 findings)

`SR-AUD-191, 203 (part), 208 (three types), 219 (part)`

`Mutex::Close`, `AutoResetEvent::Close` and `ManualResetEvent::Close` have **empty
bodies**, so every subsequent operation succeeds; `ITimer::Change` returns `true`
after `Dispose`; `ThreadLocal::IsValueCreated` bypasses its own disposal guard; and
`ReaderWriterLockSlim::Dispose` permits disposal while the caller owns a mode.

**Selected repair:** disposal is a real state with one guard applied uniformly.
Adding it makes previously-succeeding calls throw `ObjectDisposedException`, which is
an **observable semantic change** on a public API — compatible in the .NET-parity
sense, incompatible with any consumer relying on the no-op. Approval-gated (§9).

### T-H — the public shape itself diverges (8 findings)

`SR-AUD-189, 193, 194, 196, 209, 214, 215, 220`

These are not missing checks; the declared surface is wrong. `AutoResetEvent`/
`ManualResetEvent` not deriving from `WaitHandle` (SR-AUD-209) is a **hierarchy and
vtable** change. `ThreadLocal::Values` (SR-AUD-220) and `Thread::Start(void*)`
(SR-AUD-194) are missing or inert capability. `ThreadStartException` (SR-AUD-196) is
an accessibility question. All require design and most require approval.

### T-I — test assertions that cannot fail (2 findings)

`SR-AUD-195, 197`

`state & 0 == 0` and a `(void)id` cast. Both are **test-only**: no production source,
signature, layout or semantic is involved, and neither can regress a consumer. They
are the cheapest real closures in the namespace.

---

## 6. Dependency graph

```
        T-I  (tests only) ──────────────── independent, no prerequisite
        T-F  (overflow)   ──────────────── independent, no prerequisite
        T-E/1 (Reset notify) ───────────── independent, no prerequisite
                                   │
        T-B  (empty callables) ────┼────── reuses closed CCF-011 policy
        T-C  (argument validation) ┘       (independent of each other, but both
                                   │        touch the same seven headers, so they
                                   │        are ordered to avoid edit collisions)
                                   ▼
        T-A  (data races) ───────────────► must land AFTER T-B/T-C, because both
                                           add entry guards whose reads of
                                           `disposed_` T-A then makes atomic
                                   ▼
        T-G  (disposal state) ───────────► depends on T-A: a real disposed state
                                           must be race-free before it is enforced
                                   ▼
        T-E/2 (writer admission,   ──────► new private fields; depends on T-A
               tick consumer,              having settled each type's layout
               recursive Wait,
               callback lock)
                                   ▼
        T-D  (ownership)  ──────────────► public source break; design-first
        T-H  (public shape) ────────────► public source/vtable break; design-first
```

The order is not arbitrary: T-A rewrites the *same lines* that T-B and T-C add
guards to, and T-G's enforcement is only meaningful once T-A has made the flag it
enforces race-free.

---

## 7. Implementation versus design-first classification

| Cause | Classification | Rationale |
|---|---|---|
| **T-I** | **implement now** | test-only; no production surface |
| **T-F** | **implement now** | one expression per site; adopts .NET's own comparison; no signature, layout or accepted-input change |
| **T-E/1** (SR-AUD-211) | **implement now** | one `notify_all()`; releases waiters that today hang forever; nothing else observable changes |
| **T-B** | **implement** (policy already approved) | `docs/EmptyCallableBoundaryPlan.md` policy applies verbatim; no new decision |
| **T-C** | **implement**, one ticket per type | validation only; the two enum conventions are .NET's, not a choice |
| **T-A** | **implement after layout measurement** | expected layout-neutral, but must be *measured* per type before landing |
| **T-G** | **design-first** | changes previously-succeeding public calls into throws |
| **T-E/2** | **design-first** | needs new private state fields → layout |
| **T-D** | **design-first, approval-gated** | changes public parameter ownership |
| **T-H** | **design-first, approval-gated** | changes hierarchy, vtable, or adds public capability |

---

## 8. Source / ABI / layout approval matrix

| Cause | Public signature | Vtable | Object layout | Accepted input | Observable result | Approval |
|---|---|---|---|---|---|---|
| T-I | — | — | — | — | — | none |
| T-F | — | — | — | — | over-release now throws `SemaphoreFullException` instead of UB | none |
| T-E/1 | — | — | — | — | `Reset(0)` now releases waiters instead of hanging | none |
| T-B | — | — | — | empty callable now rejected | `ArgumentNullException` replaces `bad_function_call`/silent no-op/process abort | none (CCF-011 policy) |
| T-C | — | — | — | invalid args now rejected | `ArgumentException`/`ArgumentNullException`/`ArgumentOutOfRangeException` replace crash, `258`, or an unbounded loop | none |
| T-A | — | — | **measure** (`bool`→`atomic<bool>` expected 1→1) | — | races removed; no result change | **measurement gate**, then none if unchanged |
| T-G | — | — | possible new flag | — | post-`Close`/`Dispose` calls now throw | **required** |
| T-E/2 | — | — | **yes** (new private fields) | — | writer admission, tick consumption, recursive `Wait`, callback lock discipline all change | **required** |
| T-D | **yes** | possible | possible | — | ownership transfer | **required** |
| T-H | **yes** | **yes** (SR-AUD-209) | **yes** | — | new/changed capability | **required** |

---

## 9. Explicit approval requirements

Four questions must be answered by the user before the corresponding tickets can
leave `blocked`. Each is stated so it can be approved independently.

1. **T-G, disposal as a real state.** May `Mutex::Close`, `AutoResetEvent::Close`,
   `ManualResetEvent::Close`, `ITimer::Change`, `ThreadLocal::IsValueCreated` and
   `ReaderWriterLockSlim::Dispose` begin throwing `System::ObjectDisposedException`
   (and, for `ReaderWriterLockSlim`, `SynchronizationLockException` on
   dispose-while-held) where they currently succeed? This is .NET's behaviour and a
   deliberate break of the current no-op.
2. **T-E/2, new private state fields.** May `ReaderWriterLockSlim` gain
   waiting-writer state and `PeriodicTimer` gain in-flight-consumer state, growing
   `sizeof` for both? Exact before/after sizes must be measured and quoted in the
   approval, as #1788/#1789 did.
3. **T-D, ownership of borrowed pointers.** `ThreadPool::UnsafeQueueUserWorkItem`
   and `SynchronizationContext::SetSynchronizationContext` currently take borrowed
   raw pointers with no liveness boundary. Any fix changes the parameter's ownership
   contract, which is a **public source break** requiring a full downstream rebuild —
   the same class of change as #1771.
4. **T-H, hierarchy change.** May `AutoResetEvent` and `ManualResetEvent` derive
   from `System::Threading::WaitHandle` (SR-AUD-209)? This adds a vtable to two types
   that today have none and is both source- and ABI-breaking.

These are **not** asked one at a time as tickets are reached; they are stated
together here so the user can approve any subset in one turn.

---

## 10. Test matrix

Every implementation ticket adds permanent regressions to
`modules/threading/tests/`. Minimum coverage per cause:

| Cause | Required cases |
|---|---|
| T-F | `Release(1)` normal; `Release` to exactly `maxCount_`; `Release` one past it; `Semaphore(1, INT_MAX).Release(INT_MAX)` and `(INT_MAX - 1)`; the same four on `SemaphoreSlim`; count unchanged after the throw |
| T-E/1 | waiter blocked on `CountdownEvent(1)`, `Reset(0)` from another thread, waiter released within a bounded timeout; `Reset(n>0)` leaves a waiter blocked; `Reset` after `Dispose` still throws |
| T-B | per site: empty callable rejected at entry with the .NET parameter name; non-empty callable unaffected; **and** that no element/tick/thread is consumed before the throw |
| T-C | per site: each invalid argument rejected with .NET's exception type and order; the first valid neighbouring value accepted; `WaitAny({})` terminates |
| T-A | per type: a concurrent property-read/`Dispose` pair run under TSan; `sizeof`/`alignof` static assertions pinning the measured layout |
| T-I | the strengthened assertions must **fail** against the pre-fix production behaviour where the finding says the behaviour is wrong, or be written so they can only pass for a correct value |

Repository-wide rules that apply unchanged: no test may be weakened or deleted to go
green; every new test is add-only; the focused executable is
`./build/SharpRuntimeTests_Threading`.

---

## 11. Sanitizer matrix

| Cause | ASan | UBSan | LSan | TSan |
|---|---|---|---|---|
| T-A | — | — | — | **required** — the finding *is* a TSan report; a clean TSan run on the same probe is the closure evidence |
| T-B | — | — | — | — (entry-guard change; no concurrency claim) |
| T-C | **required** for SR-AUD-188/199 (both reach a null dereference) | — | — | — |
| T-D | **required** — both members were found by ASan | — | required | — |
| T-E/1 | — | — | — | optional |
| T-E/2 | — | — | — | **required** for SR-AUD-204 |
| T-F | — | **required** — the finding *is* a UBSan signed-overflow report at `Semaphore.hpp:91` / `SemaphoreSlim.hpp:108` | — | — |
| T-G | — | — | required | — |
| T-I | — | — | — | — |

`build-asan/` is reused, never recreated. TSan uses `build-tsan/`. Both are in
CLAUDE.md's closed set of build-directory names.

---

## 12. Recommended ticket order

| # | Ticket | Cause | Findings | Status at creation |
|---|---|---|---|---|
| 1 | **#1947** Semaphore/SemaphoreSlim release overflow | T-F | SR-AUD-206 | `todo` → **done this batch** |
| 2 | **#1948** CountdownEvent `Reset` must wake its waiters | T-E/1 | SR-AUD-211 | `todo` → **done this batch** |
| 3 | **#1949** two threading tests assert nothing | T-I | SR-AUD-195, 197 | `todo` → **done this batch** |
| 4 | **#1951** apply the CCF-011 empty-callable policy to Threading | T-B | SR-AUD-190, 192, 198, 213p, 217, 219p, 222 | `todo` |
| 5 | **#1952** validate WaitHandle multi-wait arguments | T-C | SR-AUD-183 | `todo` |
| 6 | **#1953** validate the crashing argument boundaries | T-C | SR-AUD-188, 199 | `todo` |
| 7 | **#1954** validate the remaining argument boundaries | T-C | SR-AUD-184, 200, 205, 213p | `todo` |
| 8 | **#1955** remove the six threading data races | T-A | SR-AUD-203p, 207×3, 212, 216, 218 | `todo` (layout measurement gate) |
| 9 | **#1956** design: disposal is a real state | T-G | SR-AUD-191, 203p, 208×3, 219p | `blocked` (approval 1) |
| 10 | **#1957** design: the four incomplete state machines | T-E/2 | SR-AUD-201, 202, 204, 210 | `blocked` (approval 2) |
| 11 | **#1958** design: the eight public-shape divergences | T-H | SR-AUD-189, 193, 194, 196, 209, 214, 215, 220 | `blocked` (approval 4) |
| 12 | **#1959** design: CCF-019's two threading members | T-D | SR-AUD-187, 221 | `blocked` (approval 3) |

Tickets 1–3 are deliberately first: they are the only three that need **no**
approval, **no** layout measurement and **no** new design, so they convert audit
evidence into closed findings immediately and leave the namespace's queue in a state
a fresh context can pick up.

---

## 13. Explicit exclusions

1. **`modules/threading-tasks` and `modules/threading-channels`** (3 + 3 open
   findings) — separate components; a separate review (§4).
2. **`ThreadPool::SetMinThreads`/`SetMaxThreads` using `int` instead of `intcs`** —
   a CLAUDE.md rule 7 convention deviation with no behavioural or ABI consequence
   (`intcs` is `int32_t`). No ticket; fold into #1958 when those bodies are rewritten
   (§3.1 item 4).
3. **Named / cross-process synchronisation objects** (named `Mutex`, `Semaphore`,
   `EventWaitHandle`, `OpenExisting`, ACLs, abandoned-mutex semantics) — documented
   native adaptations, out of scope for every ticket above. No finding claims them.
4. **`ExecutionContext` asynchronous flow** — CLAUDE.md's permanent deviation. Only
   SR-AUD-215's *argument-validation* half is in scope.
5. **COM apartment state** (`GetApartmentState`, `SetApartmentState`,
   `TrySetApartmentState`) — deliberate stubs, no finding.
6. **`Thread` priority not applied to the OS thread** — documented adaptation, no
   finding, and not made one here.

---

## 14. Completion criteria for the namespace

`System::Threading` is closed when **all** of the following hold:

1. All 38 findings are `remediated`, or `confirmed` with a *completed design* and a
   named blocked implementation ticket carrying an exact approval request.
2. No `bool` or counter in `modules/threading/include/` is written under a lock and
   read without one — checked by re-running each T-A TSan probe to a clean report.
3. No public entry point in the module reaches `std::bad_function_call`,
   `std::terminate`, a null dereference, or an unbounded loop for any input a caller
   can supply.
4. `SharpRuntimeTests_Threading` grows monotonically; no test is weakened.
5. The module graph stays **41 / 91** — no repair above needs a new edge.
6. Doxygen stays inside the **1,942** ceiling.
7. Every layout-affecting change quotes measured before/after `sizeof`/`alignof` and
   carries its own explicit approval.

---

## 15. Status

Written 2026-08-03. **Sections 1–14 changed no production source.** Tickets
**#1947**, **#1948** and **#1949** were implemented in the batch that wrote this
document and their results are recorded in §16. A second batch on branch
`feature/remediation-batch-threading-1951-1955` implemented **#1951** onwards;
its results and corrections are recorded in §17. **#1956–#1959** remain `blocked`
on the four approvals in §9.

---

## 16. What #1947–#1949 measured, and two corrections to §3 (2026-08-03)

### 16.1 SR-AUD-206 is **twice** the size the audit recorded — four overflow sites, not two

The audit named one overflowing expression per type: the full-semaphore guard at
`Semaphore.hpp:91` and `SemaphoreSlim.hpp:108`. The pre-fix UBSan probe
(`build-probe/1947_probe1_semaphore_release_overflow.cpp`, log
`1947_probe1_before.log`) reports **four**:

```
Semaphore.hpp:91:24     signed integer overflow: 2147483647 + 1 …   ← the guard (recorded)
Semaphore.hpp:94:20     signed integer overflow: 2147483647 + 1 …   ← the increment (NOT recorded)
SemaphoreSlim.hpp:108:24 signed integer overflow: 2147483647 + 1 …  ← the guard (recorded)
SemaphoreSlim.hpp:111:20 signed integer overflow: 2147483647 + 1 …  ← the increment (NOT recorded)
```

The increment `count_ += releaseCount` is a second, independent overflow that the
guard was supposed to have prevented. Because the guard's own comparison was the
thing that overflowed, it did not fire, and control reached the increment.

### 16.2 The observable consequence is worse than "the exception is missing"

The audit records the outcome as `Semaphore=normal` — the call returning instead of
throwing `SemaphoreFullException`. The probe shows the more serious half:

```
SemaphoreSlim(1,INT_MAX).Release(INT_MAX)=normal count=-2147483648
```

`CurrentCount` is left **negative**. Every subsequent `Wait` blocks on
`count_ > 0`, which can never hold again, so the semaphore is **permanently
unusable** and every waiter on it blocks forever. That is a liveness failure, not
only a diagnostics gap, and it is why #1947 was scheduled first among the three.

After the repair, the same probe reports **zero** UBSan diagnostics,
`SemaphoreFullException` from both types, `count=1` preserved, and the control
`Release(INT_MAX - 1)` still succeeding with `count=2147483647`
(`1947_probe1_after.log`).

### 16.3 SR-AUD-211 reproduced exactly as recorded

`build-probe/1948_probe1_countdown_reset_wake.cpp` prints `reset0=TIMEOUT` and exits
1 before the repair, `reset0=released` and `control reset3=still-blocked` and exits 0
after it. The control matters: the notification added by #1948 is unconditional, and
the probe proves that a reset to a **non-zero** count still leaves a blocked waiter
blocked, so the added `notify_all()` did not turn `Reset` into a general release.

### 16.4 Neither repair touched layout, and neither is a §9 approval case

`Semaphore`, `SemaphoreSlim` and `CountdownEvent` kept every data member, in order,
with the same types; both changes are expression-level inside an existing inline body
plus one added `cv_.notify_all()`. No public signature, vtable, exception
specification or accepted input changed. The only observable differences are the two
the findings asked for: an over-release now throws instead of corrupting the count,
and `Reset(0)` now releases waiters instead of stranding them.

### 16.5 SR-AUD-195/197 were repaired without depending on SR-AUD-193

#1949 replaced `state & 0 == 0` with the exact state a started, still-running thread
must report, and replaced the `(void)id` cast with a positivity assertion plus a
same-thread stability assertion, and added a case pinning that a thread started
through `Thread` sees its own `ManagedThreadId` from inside its own body. **SR-AUD-193
remains `confirmed`**: that externally created threads all collapse to id 1 is a
production defect owned by #1958, and none of the new assertions asserts it either
way.


---

## 17. What #1951 measured, and three corrections to §3 and §5 (2026-08-03)

Ticket **#1951** implemented cause **T-B** — the seven sites where an empty
`std::function` crosses a public `System::Threading` boundary — by applying the
already-approved CCF-011 policy from `docs/EmptyCallableBoundaryPlan.md`. All seven
reproduced from the shipped headers before the change and all seven are closed after it;
the raw evidence is `build-probe/1951_probe1_threading_empty_callables.cpp` with logs
`1951_probe1_before.log`, `1951_probe1_after.log` and `1951_probe1_asan.log`.

### 17.1 Two of the seven sites are **not** `ArgumentNullException` sites

§5's T-B paragraph says "Threading's seven sites are all the *argument* shape under the
CCF-011 policy, so all seven become `ArgumentNullException` at entry", and #1951's
acceptance criteria repeat it. **That is wrong for two of them, and following it would have
left both findings' measured divergence unclosed.**

CCF-011's policy is *"choose the .NET answer for that shape of API"*; the
`ArgumentNullException` spelling was selected in `modules/core` because .NET's own APIs there
run `ArgumentNullException.ThrowIfNull`. Two Threading entries have no such check at all:

| Site | .NET declaration | .NET observable for a null delegate |
|---|---|---|
| `LazyInitializer::EnsureInitialized(T*&, factory)` | `Volatile.Read(ref target) ?? EnsureInitializedCore(ref target, valueFactory)`, whose core calls `valueFactory()` | `NullReferenceException`, **suppressed entirely** when `target` is already initialized |
| `SynchronizationContext::Send` | `public virtual void Send(SendOrPostCallback d, object? state) => d(state);` | `NullReferenceException`, synchronously on the calling thread |

Both audit findings define their divergence *by the managed observable* — SR-AUD-217 quotes
`lazy_emptyFactory=exception:System.NullReferenceException`, SR-AUD-222 quotes
`System.NullReferenceException` — so throwing `ArgumentNullException` would have swapped one
non-matching result for another. Both now throw `System::NullReferenceException` instead,
which is .NET's answer, is inside the `System::Exception` hierarchy, and closes the defect
CCF-011 actually names: `std::bad_function_call` (or, for `Send`, a silent return) is not
something ported `catch (const System::Exception&)` code can see.

For `LazyInitializer` this also means the **data-dependence is reproduced, not removed**: the
check sits on the path that would have invoked the factory, so an already-initialized target
still returns its value with an empty factory, exactly as .NET does. Validating at entry
would have made a call .NET accepts start throwing.

The other five sites — `Timer` (`callback`), `Thread` (`start`),
`CancellationToken::Register` (`callback`), `SpinWait::SpinUntil` (`condition`) and
`ThreadLocal`'s two factory constructors (`valueFactory`) — do carry .NET
`ArgumentNullException.ThrowIfNull` calls with those exact parameter names, and are
implemented that way.

### 17.2 SR-AUD-219's stated consequence is wrong: the failure was silent, not `bad_function_call`

The finding says an accepted empty `ThreadLocal` factory "fails later with
`bad_function_call`" at first value access. Measured (`threadlocal.empty_factory_value=normal`
before the change): it did not fail at all. `getValueProperty()` wrote
`std::make_unique<T>(factory_ ? factory_() : T{})`, so an empty factory silently produced a
**default-constructed value on every thread**. The observable was a silent wrong value, which
is harder to notice than the deferred native exception the finding describes, and no
`bad_function_call` was reachable from that constructor at all.

The finding's direction is confirmed — .NET rejects the null factory at construction and this
port did not — and the ternary is deliberately retained, because the default and `bool`-only
constructors legitimately leave `factory_` empty and must keep defaulting.

### 17.3 SR-AUD-213's two halves now report in the wrong order, temporarily

#1951 owns SR-AUD-213's *callable* half and #1954 owns its *-2 timeout* half. .NET validates
`millisecondsTimeout` **first**. With only #1951 landed,
`SpinUntil({}, -2)` reports the condition error where .NET reports
`ArgumentOutOfRangeException`. #1954 must insert its check **above** the condition check
rather than beside it. Recorded here so the ordering is a requirement on #1954 rather than a
discovery.

### 17.4 Nothing in T-B touched layout, ABI or a signature

Seven inline bodies gained an entry check and one `.cpp` body gained one statement. No data
member was added, removed, reordered or retyped in any of the seven types; no signature,
template parameter, default argument, virtual, `noexcept` specification or overload set
changed; `SynchronizationContext::Send` stays `virtual` with the same signature. The module
graph stays **41 / 91** — `System::NullReferenceException` and `System::ArgumentNullException`
both live in `Core.Base`, which `Threading` already depends on publicly.

Three observable changes are intended and are what the findings asked for: an empty callable
that used to be accepted is now rejected at the boundary; `Thread` no longer terminates the
process; and `SynchronizationContext::Send` no longer silently does nothing. There is no call
site that worked before and stops working — every changed outcome was already a defect,
already uncatchable, or already a no-op that could never do the work it was asked for.

### 17.5 Sanitizers

Cause T-B needs none under §11, but the CCF-011 family plan flags **LSan** as material for
"a constructor that now throws must not leak". Run anyway: the probe compiled with
`-fsanitize=address,undefined` and `ASAN_OPTIONS=detect_leaks=1` produced **zero** reports
over all 22 cases. Instrumentation was proved rather than assumed — `__asan_report_*` symbols
are present in the sanitized binary (32 sanitizer symbols) and absent from the plain one.

### 17.6 Test count

`modules/threading/tests/System/Threading/ThreadingBoundaryTests.cpp` is a new translation
unit holding the boundary family's permanent regressions. #1951 contributed **24** cases;
`SharpRuntimeTests_Threading` went from 369 to 393, all passing.


---

## 18. What #1952–#1954 measured, and five further corrections (2026-08-03)

### 18.1 SR-AUD-183 (#1952) is larger than recorded in three ways

Probe `build-probe/1952_probe1_waithandle_multiwait.cpp` runs the unbounded cases in forked
children under `alarm(2)`.

1. **Three non-terminating shapes, not one.** §3 and the finding name the no-timeout
   `WaitAny({})`. `WaitAny({})`, `WaitAny({}, -1)` **and** `WaitAny({nullptr})` all printed
   `child-hang`: the infinite-timeout branch is a second copy of the same poll loop, and a
   null-only collection reached it because null elements were skipped rather than rejected.
2. **A `-2` timeout gave two different wrong answers.** `WaitAll({}, -2)` returned `true`, as
   recorded — but `WaitAll({handle}, -2)` returned **`false`**, a spurious timeout, because
   `now() + milliseconds(-2)` is already past. Same invalid argument, opposite result,
   decided by the collection's size.
3. **A mixed collection answered plausibly.** `WaitAny({signalled, nullptr}, 0)` returned `0`
   by skipping the null. It is the only pre-fix call in this finding's surface that produced a
   *usable* result, and therefore the sharpest observable change in the ticket.

**One deliberate non-repair.** .NET's `MaxWaitHandles` ceiling of 64 is not adopted: it exists
for Win32 `WaitForMultipleObjects`, this port waits sequentially, and adopting it would reject
input that works. The audit records the unrepresentable maximum as an undocumented
*portability boundary*, not a required rejection; it is documented in the header instead. No
identifier, no ticket.

### 18.2 SR-AUD-200 (#1954) is the one T-C member that was **not** repaired

It stays `confirmed`, and its open question is carried by **inactive ticket #1963**.

The finding concludes that a fractional `PeriodicTimer` period "must be rejected". Two facts
argue against doing that blind:

- Its per-file report carries **no managed probe** for the row. Its stated evidence is
  `fractional=normal` from a *native* probe compared against **this port's own doc-comment**,
  which promises whole milliseconds. The divergence established is doc-versus-code, not
  port-versus-.NET. Every other T-C member repaired here (SR-AUD-184, 205, 213) quotes a
  managed result in its own report.
- .NET's `PeriodicTimer` converts with `(long)period.TotalMilliseconds`, a **truncating**
  cast — the same idiom `Timer(TimerCallback, object?, TimeSpan, TimeSpan)` uses — so .NET
  appears to accept 1.5 ms as 1 ms. Rejecting it would then be a narrowing *away* from .NET
  that refuses input `TimeSpan` tick arithmetic legitimately produces, and the right repair
  would be to correct the doc-comment instead.

**The reference tree `/rv/tmp/runtime/src/libraries/` is not present in this environment**, so
neither reading could be confirmed. Nothing was changed — not the code and not the
doc-comment, since only one of them is wrong and which one is exactly the open question. The
current behaviour is pinned by a test naming #1963 so the answer can only be applied
deliberately.

This is also a general constraint on the batch: every ".NET does X" statement in §17–§18 rests
on the reference contracts quoted in the per-file audit reports plus the reviewer's reading,
**not** on a local checkout. Where that was not enough to decide — SR-AUD-200's period, and
SR-AUD-184's exact derived exception type — the uncertainty is recorded rather than resolved
by assertion.

### 18.3 SR-AUD-184's exception type is a knowingly partial parity claim (#1954)

`EventWaitHandle` rejects an undeclared `EventResetMode` with the **base**
`System::ArgumentException("Value of flags is invalid.", "mode")`. The audit's managed probe
records only the category, and .NET's two plausible spellings —
`ArgumentException(SR.Argument_InvalidFlag, nameof(mode))` and
`ArgumentOutOfRangeException(nameof(mode))` — stand in a base/derived relationship. Throwing
the base means a handler written for either catches this one; the tests assert the category
and `paramName`, not a derived type.

The measured incoherence the rejection removes: with mode 42, `Set()` took the AutoReset
`notify_one` branch *for not being ManualReset* while `WaitOne()` skipped its reset *for not
being AutoReset* (`first=1 second_without_set=1`), producing a handle that is neither kind of
event.

### 18.4 SR-AUD-205's defect was confined to the property (#1954)

The finding says the invalid policy is "stored and reflected verbatim". Measured: only the
*property* diverged. `rwls.policy2.property` moved 2 → 0, but `rwls.policy2.recursion_rejected`
reported "recursion rejected" **before** the change too, because `isReentrant()` already
tested for `SupportsRecursion`. The lock already behaved as `NoRecursion`; it merely reported
otherwise. Both halves are now pinned so they cannot drift apart again.

Note the deliberate asymmetry §5's T-C paragraph predicted: `EventWaitHandle` **rejects** and
`ReaderWriterLockSlim` **normalises**, because .NET validates one enum and derives the other
from a bool. Landing both in the same ticket makes it obvious this is reproduction, not
inconsistency.

### 18.5 A second route to SR-AUD-199's crash, not in the finding (#1953)

`CancellationToken` holds a `shared_ptr` and has an implicitly declared move constructor, so a
**moved-from** token has an empty `state_` and SEGV'd exactly like the explicitly-empty one —
through ordinary well-formed C++ that never mentions the internal-state constructor. This
decided the repair: **tolerance, not rejection.** An absent state is now the never-cancellable
token .NET's null `_source` denotes (`IsCancellationRequested` is
`_source != null && _source.IsCancellationRequested`; `Register` gives back a dummy
registration, in .NET's own words). Rejecting the argument would have closed the named route,
left the unnamed one open, and made the same absent state legal via move and illegal via
constructor. No public constructor was removed or narrowed, so #1953's source-break clause was
never reached.

`RegisteredWaitHandle`'s own report asked for the empty `callBack` and the `-2` timeout to be
repaired *with* the null-wait crash path, and both were, so SR-AUD-188 closed with three
checks rather than one.

### 18.6 Scoreboard after #1951–#1954

| Cause | Findings | Status |
|---|---|---|
| T-B (#1951) | 190, 192, 198, 217, 222 remediated; 213p, 219p landed | done |
| T-C (#1952) | 183 remediated | done |
| T-C (#1953) | 188, 199 remediated | done |
| T-C (#1954) | 184, 205 remediated; 213 now fully remediated | done, **except SR-AUD-200** (§18.2, ticket #1963) |

Eight of the namespace's 38 findings moved to `remediated` in this batch (183, 184, 188, 190,
192, 198, 199, 205, 213, 217, 222 — eleven, counting SR-AUD-213 which needed both tickets),
on top of the three (195, 197, 206, 211) closed by the previous one. SR-AUD-219 stays
`confirmed` pending its T-G half (#1956) and SR-AUD-200 stays `confirmed` pending #1963.

Neither ticket changed a signature, an object layout, a vtable, an exception specification or
a component edge. The module graph stays **41 / 91**.


---

## 19. What #1955 measured, and the T-A idiom as actually implemented (2026-08-03)

Ticket #1955 closed cause **T-A** — the six types that read shared mutable state outside its
own mutex. Evidence: `build-probe/1955_probe1_shared_state_races.cpp` under
`-fsanitize=thread`, and `build-probe/1955_probe1_layout.cpp` for the gate.

### 19.1 The layout gate passed, byte for byte

| Type | `sizeof` before → after | `alignof` before → after |
|---|---|---|
| `ReaderWriterLockSlim` | 120 → 120 | 8 → 8 |
| `SemaphoreSlim` | 104 → 104 | 8 → 8 |
| `ManualResetEventSlim` | 112 → 112 | 8 → 8 |
| `CountdownEvent` | 104 → 104 | 8 → 8 |
| `Barrier` | 160 → 160 | 8 → 8 |
| `ThreadLocal<int>` | 56 → 56 | 8 → 8 |

`sizeof(std::atomic<bool>) == sizeof(bool) == 1`, `alignof` 1, lock-free;
`sizeof(std::atomic<intcs>) == sizeof(intcs) == 4`, `alignof` 4, lock-free. The two logs are
`1955_probe1_layout_before.log` and `1955_probe1_layout_after.log`, and `diff` reports them
identical. **No user approval was required**, and the numbers are now pinned by
`ThreadingSharedStateTests.RepairedTypes_LayoutUnchanged` so a future edit that grows one of
these inline types fails a test before it reaches a consumer.

### 19.2 The counters became atomic fields, not locked properties — §5 corrected

§5's T-A paragraph selected *"a counter that a public property exposes is read **under the
owning mutex** (the mutex becomes `mutable`)"*. Both counter sites were implemented as
`std::atomic<intcs>` instead. This is a deliberate change to the selected repair, for two
reasons.

**It is what .NET does.** `SemaphoreSlim.CurrentCount` returns `m_currentCount`, declared
`private volatile int`, read with no lock; `Barrier.ParticipantCount` reads its packed count
field without taking the barrier's lock. A locking property would have been a divergence
invented by this port on a pair of properties .NET makes deliberately cheap.

**For `Barrier` it would have created a second SR-AUD-210.** `FinishPhase()` invokes the
post-phase action **while still holding `mutex_`**. A `getParticipantCountProperty()` that took
that lock would block on the lock its own caller holds, so a legal post-phase action reading
`ParticipantCount` would self-deadlock — which is exactly the defect SR-AUD-210 describes for
`getCurrentPhaseNumberProperty()` and exactly what approval-gated ticket #1957 exists to
remove. A T-A repair that manufactured a second instance of a T-E/2 defect would be a
regression wearing a fix's clothing.
`ThreadingSharedStateTests.Barrier_ParticipantCountReadableFromPostPhaseAction` pins the
property's usability from inside the action.

Every **write** still happens under the owning mutex in both types, so no compound invariant
is weakened: `0 <= count_ <= maxCount_`, the `count_ > 0` predicate, and
`participantCount_`/`remainingCount_`'s relationship are untouched. The atomics make the
unsynchronised *reads* well-defined and nothing else.

The four flags did adopt §5's stated idiom exactly: `std::atomic<bool>`, release store in
`Dispose()`, acquire load in the guard.

### 19.3 SR-AUD-216 had two racing reads, not one

The finding names the `if (!target)` guard. TSan also reported `return *target;` — a second
ordinary read of the object another caller publishes through `atomic_ref`'s compare-exchange.
Six of the pre-fix run's thirteen reports came from this one scenario. Both reads now go
through the same `std::atomic_ref`, which is the C++ spelling of .NET's opening
`Volatile.Read(ref target)`.

### 19.4 A TSan methodology correction

The probe's first version used a 2000-iteration loop per thread and reported **zero** races
for `ManualResetEventSlim` and `CountdownEvent` while reporting one for the structurally
identical `ReaderWriterLockSlim`. The code was equally racy in all three. A writer loop of
trivial stores finishes before a reader that must set up a try/catch reaches its first call,
so the threads never overlap and a happens-before detector sees nothing. Rewritten as **1500
rounds of a fresh object with one access per thread**, all seven scenarios reproduced.

Recorded because it generalises: a "TSan reported nothing" result is evidence about the probe
until the probe has been shown capable of reporting something. §11 of this plan says a clean
TSan run *is* the closure evidence for T-A — that only holds for a probe with a demonstrated
positive.

**Totals: 13 reports across seven scenarios before, zero after**, exit 66 → 0, every control
value unchanged (`semaphoreslim.final_count=1`, `barrier.final_participants=1`,
`lazyinitializer.consistent_rounds=400/400`). 132 `__tsan_*` symbols confirm the binary was
instrumented.

### 19.5 What #1955 deliberately did not touch

- **SR-AUD-203's dispose-while-held half** (T-G, #1956): the flag is now race-free, but
  disposal still succeeds while a mode is held.
- **SR-AUD-219's IsValueCreated half** (T-G, #1956): `getIsValueCreatedProperty()` still
  bypasses the — now sound — guard.
- **SR-AUD-204, 210, 201, 202** (T-E/2, #1957) and **SR-AUD-220** (T-H, #1958): untouched.

`SharpRuntimeTests_Threading` 418 → 429 (+11), all passing. No signature, vtable, exception
specification or component edge changed; the module graph stays **41 / 91**.

### 19.6 Namespace scoreboard after #1947–#1955

| State | Findings |
|---|---|
| **remediated (15)** | 183, 184, 188, 190, 192, 195, 197, 198, 199, 205, 206, 207, 211, 212, 213, 216, 217, 218, 222 — nineteen IDs, of which 195/197/206/211 closed in the previous batch |
| **confirmed, one half landed (2)** | 203 (race half done, T-G half open), 219 (factory half done, T-G half open) |
| **confirmed, untouched (17)** | 187, 189, 191, 193, 194, 196, 200, 201, 202, 204, 208, 209, 210, 214, 215, 220, 221 |

Nineteen of the namespace's 38 findings are `remediated`; two are half-landed; seventeen
remain, of which sixteen belong to the four approval-gated design tickets #1956–#1959 and one
(SR-AUD-200) to verification ticket #1963.

---

## 20. Consolidated design and approval package for #1956–#1959 (2026-08-03)

**Nothing in this section is implemented.** All four tickets remain `blocked`. §9 stated the
four approval questions in outline; this section supplies what an approval actually needs —
exact declarations, measured current behaviour, the .NET contract, the selected design, the
alternatives considered and rejected, the compatibility consequences, the test and sanitizer
plan, the rollback path, and one exact sentence to approve or decline. The four questions are
gathered so any subset can be answered in one turn.

**Evidence status.** Current behaviour below is measured — by the probes this batch built, by
the audit's own probes, or by reading the shipped headers. .NET behaviour is taken from the
contracts quoted in the per-file audit reports plus the reviewer's reading of the reference
API; **the reference tree `/rv/tmp/runtime/src/libraries/` is not present in this
environment** (§18.2). Where a design decision turns on a .NET detail that could not be
re-read here, that is marked **[unverified]** and the implementing ticket must confirm it
before landing.

---

### 20.1 #1956 — disposal is a real state (cause T-G)

Findings: SR-AUD-191, SR-AUD-203 (dispose-while-held half), SR-AUD-208 (three types),
SR-AUD-219 (IsValueCreated half).

#### Affected declarations

| File | Declaration | Today |
|---|---|---|
| `Mutex.hpp` | `void Close()` | `{}` — empty body; inherited `WaitHandle::Dispose()` is also `{}` |
| `AutoResetEvent.hpp` | `void Close()` | `{}` |
| `ManualResetEvent.hpp` | `void Close()` | `{}` |
| `src/System/TimeProvider.cpp` | `SystemTimeProviderTimer::Change(TimeSpan, TimeSpan)` | forwards and `return true;` unconditionally |
| `ThreadLocal.hpp` | `bool getIsValueCreatedProperty() const` | reads the map; never calls `ThrowIfDisposed()` |
| `ReaderWriterLockSlim.hpp` | `void Dispose()` | sets the (now atomic) flag; never checks whether the caller owns a mode |

#### Current behaviour, measured

`Mutex::Close()` then `WaitOne(0)` returns `1`; the same on both events. `ITimer::Change`
after `Dispose` returns `true` although the worker is stopped. `ThreadLocal::IsValueCreated`
returns `false` after `Dispose` rather than throwing. `ReaderWriterLockSlim::Dispose()`
succeeds with a read lock held.

#### .NET behaviour

`WaitHandle.Close()` disposes the underlying `SafeWaitHandle`, and every later wait throws
`ObjectDisposedException`. `ThreadLocal<T>.IsValueCreated` throws `ObjectDisposedException`.
`ReaderWriterLockSlim.Dispose()` throws `SynchronizationLockException` when a mode is held.
**`ITimer.Change` is the one member that does not throw**: its documented contract is a
`false` return for a disposed timer, which is what the audit's own summary says
(*"the ITimer contract at minimum requires a false result"*). Any design that made
`ITimer::Change` throw would be wrong.

#### Selected design

One guard idiom, already race-free after #1955, applied uniformly — **with `ITimer::Change`
deliberately excluded from the throwing group**:

1. `Mutex`, `AutoResetEvent`, `ManualResetEvent` gain a `std::atomic<bool> closed_{false}`;
   `Close()` sets it, and `WaitOne`, `WaitOne(intcs)`, `Set`, `Reset` and `ReleaseMutex`
   throw `System::ObjectDisposedException` when it is set. `Close()` stays idempotent.
2. `SystemTimeProviderTimer` tracks its own disposal and `Change` **returns `false`**
   afterwards. No exception, no new public type member — the flag lives in a `.cpp`-local
   class.
3. `ThreadLocal<T>::getIsValueCreatedProperty()` calls the existing `ThrowIfDisposed()`.
4. `ReaderWriterLockSlim::Dispose()` throws `System::Threading::SynchronizationLockException`
   when the calling thread's `ThreadCounts` shows a held read, write or upgradeable mode.

#### Alternatives considered

- **Leave `Close()` a no-op and document it.** Rejected: the header already says `Close`
  "closes the mutex handle", so the documentation and the behaviour disagree today; keeping
  the no-op means keeping a false statement or removing a .NET-shaped API.
- **Make disposal idempotent-but-silent (later calls no-op instead of throwing).** Rejected:
  it hides use-after-close exactly as today, only more deliberately.
- **Make `ITimer::Change` throw for symmetry with the wait handles.** Rejected: it contradicts
  the documented `ITimer` contract, which is the .NET surface this type implements.

#### Consequences

| Axis | Impact |
|---|---|
| Public source | none — no signature, overload set or default argument changes |
| Vtable | none — `Close()` is non-virtual on all three; `ITimer::Change` is already virtual and keeps its signature |
| Object layout | **yes, three types**: `Mutex`, `AutoResetEvent`, `ManualResetEvent` each gain one `std::atomic<bool>`. Exact before/after `sizeof`/`alignof` must be measured and quoted, as #1955 did; a `bool`-sized member may or may not fit existing tail padding, so this is **not** assumed layout-neutral |
| Concurrency | none new: the flag is atomic from the start, and the `ReaderWriterLockSlim` check reads only the calling thread's own counts |
| Observable behaviour | **the point of the ticket** — calls that succeed today begin throwing |
| Migration | any consumer that calls `Close()`/`Dispose()` and then keeps using the object must stop. .NET-shaped code never does; code written against this port's no-op might |

#### Test plan

Per type: operation before `Close` succeeds; the same operation after `Close` throws
`ObjectDisposedException`; `Close` twice does not throw; the exception is catchable as
`System::Exception`. `ITimer`: `Change` returns `true` while live and `false` after `Dispose`,
and `Dispose` twice is safe. `ThreadLocal`: `IsValueCreated` throws after `Dispose` and does
not before, both with and without a created value. `ReaderWriterLockSlim`: `Dispose` throws
with a read lock held, with a write lock held, with an upgradeable lock held, and succeeds
after every mode is released; a *different* thread's held lock is covered explicitly, since
the check is per-thread.

#### Sanitizer plan

LSan (§11 lists it for T-G): each newly throwing path constructed and destroyed 10,000 times
to prove the throw leaks nothing. TSan on a `Close`-versus-operation pair per type, to prove
the new flag did not reintroduce cause T-A.

#### Rollback

Each of the four repairs is independent and separately revertible; the layout half (item 1)
is the only one whose revert affects ABI, and reverting it restores the previous `sizeof`
exactly.

#### Exact approval request

> **Approve #1956:** may `Mutex::Close`, `AutoResetEvent::Close` and `ManualResetEvent::Close`
> begin invalidating their objects — each gaining one `std::atomic<bool>` member, so
> `sizeof` may grow and must be re-measured and quoted before landing — so that every
> subsequent wait, set, reset or release throws `System::ObjectDisposedException`; may
> `ThreadLocal<T>::getIsValueCreatedProperty` throw `ObjectDisposedException` after
> `Dispose`; may `ReaderWriterLockSlim::Dispose` throw
> `System::Threading::SynchronizationLockException` when the calling thread still owns a
> mode; and may `SystemTimeProviderTimer::Change` **return `false`** after `Dispose` rather
> than `true` (returning `false`, not throwing, because that is `ITimer`'s documented
> contract)?

---

### 20.2 #1957 — the four incomplete state machines (cause T-E/2)

Findings: SR-AUD-201, SR-AUD-202, SR-AUD-204, SR-AUD-210.

#### Affected declarations, current behaviour and .NET contract

| Finding | Declaration | Today | .NET |
|---|---|---|---|
| SR-AUD-202 | `Monitor::Wait(const void*)`, `Wait(const void*, intcs)` | releases **one** recursion level, so a caller at depth ≥ 2 keeps the mutex and the signaller can never `Enter`; the audit's bounded probe times out at 2 s (exit 124) | fully releases the recursive count and restores it on reacquisition |
| SR-AUD-210 | `Barrier::SignalAndWait`, post-phase action | the action runs while `FinishPhase` holds `mutex_`, so a legal `getCurrentPhaseNumberProperty()` inside it self-deadlocks | the action runs with the barrier usable |
| SR-AUD-204 | `ReaderWriterLockSlim::TryEnterReadLock*` | no waiting-writer state, so a new reader enters past a blocked writer and can starve it indefinitely | a writer waiting blocks subsequent readers |
| SR-AUD-201 | `PeriodicTimer::WaitForNextTick` | no in-flight-consumer state, so two concurrent waiters both return `true` for one tick (audit probe: `concurrent=1,1`) | documented single-consumer contract |

#### Selected design, and how the four differ

They share a cause but not a repair, which is why they are one ticket only for approval and
must be four separate commits.

1. **SR-AUD-202** — `Wait` reads the calling thread's depth `n`, unlocks `n` times, waits, then
   relocks `n` times and restores the count. **No public object layout is involved at all**:
   `Monitor` is a static-only class whose per-pointer `State` lives in a private static
   registry, so nothing a consumer can allocate changes size. This is the cheapest member and
   could be split out to land without approval if the depth bookkeeping is proven exact.
2. **SR-AUD-210** — `FinishPhase` releases `mutex_` around the post-phase action and reacquires
   it afterwards, keeping the existing `actionCallerId_` reentrancy guard and the
   `lastPostPhaseException_` propagation. Likely **no new member**, but the phase transition
   must stay atomic with respect to other participants, which needs a "phase is finishing"
   state — that may cost a field.
3. **SR-AUD-204** — `ReaderWriterLockSlim` gains a waiting-writer count (`intcs` or
   `std::atomic<intcs>`) and the read-admission predicate consults it. **New member → layout.**
4. **SR-AUD-201** — `PeriodicTimer` gains an in-flight-consumer flag and `WaitForNextTick`
   throws `System::InvalidOperationException` for a second concurrent caller **[unverified:
   whether .NET throws or blocks the second consumer must be confirmed against the reference
   before landing]**. **New member → layout.**

#### Alternatives considered

- **SR-AUD-204 without a field**, deriving writer-waiting from the condition variable:
  rejected, a `condition_variable` exposes no waiter count and any approximation reintroduces
  the starvation window.
- **SR-AUD-210 by making `mutex_` recursive**: rejected — it would let the action mutate
  barrier state mid-transition, converting a deadlock into corruption.
- **SR-AUD-201 by documenting multi-consumer as an accepted adaptation**: rejected, the
  finding is that two waiters *silently* both consume one tick; if it is to be an adaptation
  it must at least be documented and asserted, which is itself a behaviour decision.

#### Consequences

Public source: none. Vtable: none. **Object layout: yes** — `ReaderWriterLockSlim` and
`PeriodicTimer` grow (before/after `sizeof`/`alignof` to be measured and quoted, as
#1788/#1789/#1955 did); `Monitor` cannot; `Barrier` to be determined. Concurrency: every
member changes admission or wake-up ordering, which is precisely what is being approved.
Fairness: SR-AUD-204 introduces writer preference, so a reader-heavy workload that never
blocked can now block. Migration: none at the API level; a workload relying on reader
starvation of a writer is not a supported expectation.

#### Test plan

Deterministic where possible: recursive `Wait` at depths 1, 2 and 3 with a bounded timeout
(the pre-fix behaviour is a hang, so every case must be time-boxed); a post-phase action that
reads both barrier properties and one that throws, with the exception still reaching every
participant; a writer blocked behind a reader followed by a new reader, asserting the reader
waits; two concurrent `WaitForNextTick` callers asserting exactly one tick is consumed. Plus
the zero/one/maximum, timeout-zero/infinite, signal-before-wait and wait-before-signal,
cancellation-before/during/after, disposal-while-waiting and duplicate-release axes the batch
instructions list.

#### Sanitizer plan

TSan is **required** for SR-AUD-204 (§11) and should cover all four; ASan/LSan on the
`Barrier` restructure, because releasing and reacquiring a lock around a user callback is the
classic place to lose an exception or a `unique_lock`.

#### Rollback

Four independent commits; reverting any one restores its type's previous `sizeof`.

#### Exact approval request

> **Approve #1957:** may `ReaderWriterLockSlim` gain waiting-writer state and `PeriodicTimer`
> gain in-flight-consumer state — both growing `sizeof`, to be measured and quoted before
> landing — so that a new reader no longer overtakes a waiting writer and a second concurrent
> `WaitForNextTick` no longer consumes another consumer's tick; may `Monitor::Wait` release
> and restore **all** recursive levels instead of one, so that a depth-2 wait stops
> deadlocking; and may `Barrier`'s post-phase action run with `mutex_` released, so that a
> callback reading `CurrentPhaseNumber` or `ParticipantCount` no longer self-deadlocks?

---

### 20.3 #1958 — the eight public-shape divergences (cause T-H)

Findings: SR-AUD-189, 193, 194, 196, 209, 214, 215, 220.

#### The first recommendation is to split this ticket

The eight members are not one decision. Measured against the approval boundary, they fall
into three groups, and bundling them means the two cheap ones wait for the expensive one:

**Group A — compatible, no approval needed** (recommend splitting into their own ticket and
landing them like #1951–#1955):

| Finding | Change | Why compatible |
|---|---|---|
| SR-AUD-215 | `ExecutionContext::Run(nullptr, …)` throws `InvalidOperationException` | validation only; the only calls that change outcome are ones .NET rejects. **Note:** the port's own `Capture()` always returns `nullptr`, so the local test that passes `nullptr` must be rewritten, and this must be checked before landing |
| SR-AUD-214 | `AsyncLocal<T>::setValueProperty` writes the new value **before** notifying | ordering only; no signature, layout or vtable change |
| SR-AUD-189 | `SetMinThreads`/`SetMaxThreads` validate and store, returning `false` for invalid input | fixes a setter that returns `true` and stores nothing. Fold in the `int` → `intcs` correction recorded in §3.1 item 4 |

**Group B — needs approval, moderate**:

| Finding | Change | Gate |
|---|---|---|
| SR-AUD-196 | make `ThreadStartException`'s three constructors non-public | **public source break** — Batch8 tests construct all three, and so may consumers |
| SR-AUD-193 | give externally created threads distinct `ManagedThreadId`s | changes an observable id for every non-`Thread` thread; needs a lazily assigned thread-local id and a decision on whether the main thread keeps id 1 |
| SR-AUD-194 | make `Start(void*)` deliver its parameter | requires a second start-callback shape (`std::function<void(void*)>`), i.e. a new constructor and a stored variant — **layout** |
| SR-AUD-220 | add `ThreadLocal<T>::Values` and honour `trackAllValues` | new public API plus cross-thread tracking storage — **layout**, and a real lifetime design (which thread owns the tracked values, and when are they freed) |

**Group C — the expensive one**:

| Finding | Change | Gate |
|---|---|---|
| SR-AUD-209 | `AutoResetEvent` and `ManualResetEvent` derive from `WaitHandle` | **both source- and ABI-breaking**: adds a vtable to two types that have none, changes their size and alignment, makes `WaitOne()` virtual with a `bool` return where the current `AutoResetEvent::WaitOne()` returns `void`, and forces `Dispose`/`Close` overrides. Every consumer must recompile |

#### Why SR-AUD-209 is nonetheless worth doing

It is the only finding in the namespace that makes a *documented* API unusable: `WaitHandle`'s
four multi-wait entry points — repaired in #1952 — cannot accept the two event types at all,
so `WaitAny`/`WaitAll` over events, the single most common .NET multi-wait pattern, does not
compile. `EventWaitHandle` already derives from `WaitHandle` and shows the shape.

#### Consequences of Group C, precisely

`sizeof(AutoResetEvent)` and `sizeof(ManualResetEvent)` grow by at least one vptr (8 bytes on
LP64) plus alignment; a vtable appears where none existed; `AutoResetEvent::WaitOne()`'s
return type changes from `void` to `bool`, which is a **source break for any caller that
relies on the void return in an expression context** and a mangled-symbol change either way.
No mixed-version linking is safe.

#### Test plan

Group A: per-site validation and ordering cases as in #1951–#1954. Group B: id uniqueness
across external threads, parameter identity and lifetime through `Start(void*)`,
`Values` contents under `trackAllValues` true and false (the false case throwing, as .NET
documents), and a negative consumer fixture proving `ThreadStartException`'s constructors are
rejected. Group C: `WaitAll`/`WaitAny` over an `AutoResetEvent`, a `ManualResetEvent` and a
mixed array with a `Semaphore`; auto-reset consuming exactly one signal through the multi-wait
path; plus a compile-only fixture proving the composition that fails today now builds.

#### Sanitizer plan

ASan/LSan on the `ThreadLocal::Values` tracking storage, which is the only member introducing
cross-thread ownership. TSan on `ManagedThreadId` assignment and on `Values` under concurrent
access.

#### Rollback

Group A and Group B are per-finding revertible. Group C is not partially revertible: once the
two events derive from `WaitHandle`, reverting is a second ABI break.

#### Exact approval request

> **Approve #1958, and please answer the three groups separately.**
> **(A)** May the three compatible members — `ExecutionContext::Run(nullptr, …)` throwing
> `InvalidOperationException`, `AsyncLocal<T>` notifying **after** it writes, and
> `ThreadPool::SetMinThreads`/`SetMaxThreads` validating and storing instead of returning a
> meaningless `true` — be split into a separate ticket and landed without further approval?
> **(B)** May `ThreadStartException`'s three constructors stop being public (**a source break
> for existing callers, including this repository's own Batch8 tests**), may `Thread` gain a
> parameterized start callback so `Start(void*)` delivers its parameter (**new member,
> layout**), may externally created threads receive distinct `ManagedThreadId`s, and may
> `ThreadLocal<T>` gain a `Values` surface with real `trackAllValues` tracking (**new member,
> layout, plus a cross-thread lifetime contract**)?
> **(C)** May `AutoResetEvent` and `ManualResetEvent` derive from
> `System::Threading::WaitHandle`, adding a vtable to two types that today have none, growing
> both by at least one pointer, and changing `AutoResetEvent::WaitOne()`'s return type from
> `void` to `bool` — a combined **source and ABI break requiring a full downstream rebuild**
> — in exchange for making the `WaitAll`/`WaitAny` composition that .NET supports compile at
> all?

---

### 20.4 #1959 — CCF-019's two threading members (cause T-D)

Findings: SR-AUD-187, SR-AUD-221. This is the same cause as the `JsonNode` and `Xml.Linq`
members whose layout approvals were **declined** (#1888/#1889/#1896/#1899), so the precedent
matters: CCF-019 is not closed, and a declined answer here is an expected outcome, not a
failure.

#### Affected declarations and measured behaviour

| Finding | Declaration | Measured |
|---|---|---|
| SR-AUD-187 | `static bool ThreadPool::UnsafeQueueUserWorkItem(IThreadPoolWorkItem* callBack, bool preferLocal)` | the raw pointer is captured by a **detached** lambda; the audit's ASan probe deletes the item after `Execute()` has entered and reports **heap-use-after-free** |
| SR-AUD-221 | `static void SynchronizationContext::SetSynchronizationContext(SynchronizationContext*)` and `static SynchronizationContext* getCurrentProperty()` | a non-owning raw pointer in a `thread_local` slot with no reset hook; ASan reports **stack-use-after-scope** at the virtual call after the context leaves scope |

Note that the **null halves of both are already closed**: `UnsafeQueueUserWorkItem` rejects a
null `callBack` (§3.1 item 1) and `Send` rejects an empty callable (#1951). Only the
**lifetime** halves are live.

#### .NET behaviour

Managed object lifetime is the mechanism in both cases: a queued work item is kept alive by
the queue's reference, and `SynchronizationContext.Current` holds a real reference, which the
audit's managed probe confirmed by dropping its local, forcing a GC and still observing a live
weak reference. Neither contract is expressible with a borrowed raw pointer in C++.

#### Candidate designs

| Option | Shape | Break |
|---|---|---|
| **1. Owning handle** | take `std::shared_ptr<IThreadPoolWorkItem>` / `std::shared_ptr<SynchronizationContext>` | **public source break** — every call site changes; matches .NET's semantics most closely |
| **2. Weak observation** | keep the raw parameter, store a `std::weak_ptr` obtained from an `enable_shared_from_this` base | requires the base class change, so still a source break for implementers, and silently skips work whose owner died |
| **3. Scoped setter** | `SetSynchronizationContext` returns an RAII token that restores the previous value | source-compatible *addition*, but does not stop a caller from ignoring it |
| **4. Document only** | state the borrowed-pointer contract in the header and add a negative consumer fixture | **no break**, closes nothing; this is what #1899 fell back to for `Xml.Linq` |

Option 1 is the correct design and the one this section recommends; option 4 is the fallback
if approval is declined, and is exactly the precedent set by the declined `Xml.Linq` ticket.

#### Consequences of option 1

Public source: **yes** — both signatures change, so every call site must be edited. Mangled
symbols: **yes**. Object layout: none directly, though `SynchronizationContext` would gain an
`enable_shared_from_this` base under option 2. Concurrency: unchanged. Migration: the same
class of change as #1771 — a full downstream rebuild plus a mechanical call-site edit.

#### Test plan

The two ASan probes that reported today must report nothing: queue a heap work item, release
the caller's reference, and prove `Execute()` still runs on a live object; set a
scope-local context as `Current`, leave the scope, and prove `Current` is either null or
still valid. Plus a negative consumer fixture per site proving the old borrowed-pointer
spelling no longer compiles.

#### Sanitizer plan

ASan **required** (both findings were found by it), LSan required (an owning handle must not
turn a use-after-free into a leak), TSan on `Current` across threads.

#### Rollback

Reverting either signature is a second source break; if this is approved it should be
approved as final.

#### Exact approval request

> **Approve #1959:** may `ThreadPool::UnsafeQueueUserWorkItem` and
> `SynchronizationContext::SetSynchronizationContext`/`Current` change their parameter and
> return types from borrowed raw pointers to owning `std::shared_ptr` handles — **a public
> source break requiring every call site to be edited and a full downstream rebuild, the same
> class of change as #1771** — in exchange for removing the ASan-confirmed heap-use-after-free
> and stack-use-after-scope; or, if that is declined as the `Xml.Linq` member of the same
> CCF-019 family was, should the fallback be taken instead: document the borrowed-pointer
> lifetime contract in both headers and pin it with a negative consumer fixture, leaving both
> findings `confirmed` with a completed design?

---

### 20.5 Summary — the four questions in one place

| Ticket | Question | Layout | Source break | Vtable | If declined |
|---|---|---|---|---|---|
| **#1956** | disposal becomes a real state; `ITimer::Change` returns `false` rather than throwing | **yes**, 3 types | no | no | findings stay `confirmed` with a completed design |
| **#1957** | four state machines gain their missing transition | **yes**, 2 types (+1 to determine) | no | no | as above |
| **#1958** | split into A (compatible), B (moderate), C (`WaitHandle` hierarchy) | **yes**, B and C | **yes**, B and C | **yes**, C | land A only, if A is approved |
| **#1959** | borrowed raw pointers become owning handles | no | **yes** | no | documented contract + negative fixture (the #1899 precedent) |

No implementation may begin on any of these until the corresponding question is answered.


---

## 21. The #1958 Group A split, and why it is a two-member group (ticket #1971, 2026-08-03)

§20.3 recommended splitting #1958 and listed **three** members as *"compatible, no approval
needed"*. Ticket **#1971** verified that claim independently, by measurement, before splitting
anything (`build-probe/1971_probe1_group_a.cpp`). **It holds for two of the three.**

| §20.3 Group A member | verdict | outcome |
|---|---|---|
| SR-AUD-214 -- `AsyncLocal<T>` notifies after it writes | compatible, confirmed | landed in **#1971** |
| SR-AUD-189 -- `ThreadPool` setters validate and store | compatible, confirmed | landed in **#1971** |
| SR-AUD-215 -- `ExecutionContext::Run(nullptr, …)` throws | **not compatible** | **stays in the blocked #1958** |

### 21.1 Why SR-AUD-215 is not compatible

§20.3 attached its own caveat -- *"the port's own `Capture()` always returns `nullptr`, so the
local test that passes `nullptr` must be rewritten, and this must be checked before landing"* --
and treated it as a test problem. Measured, it is a **reachability** problem:

- `Capture()` returns `nullptr` unconditionally, by documented design;
- the default constructor is **private**, proved by a compile probe
  (`-DPROBE_TRY_CONSTRUCT=1` fails with *"'ExecutionContext::ExecutionContext()' is private
  within this context"*);
- `CreateCopy()` is a **non-static** member, so it needs an instance only `Capture()` could
  supply.

There is consequently **no reachable way for a consumer to obtain a non-null
`ExecutionContext*`**. Rejecting a null context would make `Run` throw for *every* call that can
be written, including `Run(Capture(), callback, state)`, which works today
(`ec.run_with_capture_result=invoked`). §9's compatibility test is *"is there a working call site
that stops working?"* — here every call site stops working, with no alternative to migrate to.

The finding is real; the repair is not a validation change. It needs a `Capture()` that returns a
real context, which is an ownership and lifetime design (who owns the pointer, when is it freed,
what `CreateCopy` then means). **#1958's approval request (A) is therefore re-worded: (A) is
satisfied for SR-AUD-214 and SR-AUD-189 only, and SR-AUD-215 joins the approval-gated
remainder.** Groups B and C are untouched and unapproved.

Two regressions pin the current contract so the exclusion is testable, not merely documented —
one fails if `ExecutionContext` ever becomes publicly constructible, which would re-open the
question.

### 21.2 What #1971 measured for the two members it did land

**SR-AUD-214.** Reproduced exactly (`callback_property=0` while `callback_argument=5`), and the
report's second, sharper half is confirmed as well: a **reentrant** write from inside the handler
was overwritten by the delayed outer assignment (a handler writing 99 left the value at 5).
Repaired by committing before notifying; 99 now survives. The equal-value no-op is untouched and
separately pinned, because reordering the commit past an early return is the obvious way to
break it.

**SR-AUD-189.** Wider than the probe the report quotes: besides `SetMinThreads(7,9)` storing
nothing and `SetMinThreads(-1,-1)` returning true, `SetMinThreads(0,0)`, `SetMaxThreads(0,0)` and
a maximum below the current minimum **all returned true**. Every rule of the repair carries its
provenance, because `/rv/tmp/runtime/src/libraries/` is absent here — negative-rejected and
valid-stored-and-observable are **measured** from the audit's own managed probe, the min/max
consistency rule and maximum >= 1 are **.NET-documented**, and accepting zero as a minimum is
**reasoned**. .NET's further rule refusing a maximum below the processor count is **deliberately
not adopted**: unverifiable here, and it would make the port's observable answer depend on the
core count of whatever machine runs it (the same reasoning as #1952's `MaxWaitHandles`).

One measured consequence is worth stating rather than discovering later: on this four-core
container the default maximum is 8, so `SetMinThreads(7, 9)` now returns **false**, where the
audit's managed probe on a 16-core machine got `True` for the same call. Same rule; .NET's
*default maximum* is machine-dependent too. The regressions set an explicit maximum first rather
than relying on the default.

### 21.3 §3.1 item 4's opportunistic `intcs` correction is done, and shown neutral

All four `ThreadPool` configuration entry points moved from `int` to `intcs`. The claim that this
is mangling-neutral is **measured**: `nm` on the built test binary reports
`_ZN6System9Threading10ThreadPool13SetMinThreadsEii` and
`…13GetMinThreadsERiS0_` after the change — `int` parameters, because `intcs` is `int32_t` is
`int` here.

### 21.4 Layout, ABI and sanitizers

`ThreadPool` is a static-only class (`ThreadPool() = delete`) with no instances, so its stored
configuration lives in function-local statics behind one shared mutex — a lock rather than
atomics because each setter is a read-modify-write across both pairs. **No object layout exists
to change**, and `AsyncLocal<T>`'s repair is the order of two statements. Nothing in #1971
changed a signature (beyond the mangling-identical rename), a vtable, an exception specification
or a component edge.

Concurrency, capability proved first per §19.4: the AsyncLocal ordering scenario reported
**8,000 violations against the pre-fix header — every one of 4 threads x 2,000 iterations — and
0 after**. The ThreadPool configuration scenario reported 0 invariant violations after and,
honestly recorded, **0 before as well**, because setters that store nothing cannot break an
invariant; it is evidence about the new state, not a before/after discriminator. TSan: 0 data
races in both runs, fully instrumented from source (18 `__tsan` symbols, no archive linked).
ASan + UBSan + LSan over the surface probe: 0 reports.

`SharpRuntimeTests_Threading` **429 -> 445** (+16), in a new translation unit
`ThreadingPublicShapeTests.cpp`. **SR-AUD-214 and SR-AUD-189: `confirmed` -> `remediated`.**
Cause T-H now has **six** open members, all in the blocked #1958.

---

## 22. One correction to §1's namespace-selection prose (2026-08-03, ticket #1972)

§1's table of open findings by module is accurate and is not changed. The **prose**
immediately below it dismissed the next namespace with:

> Explicitly **not** selected, and why: `runtime` (21) is dominated by reflection and
> serialization surfaces that CLAUDE.md already classifies as permanent deviations […]

Re-measured against `audit/AUDIT_FINDINGS_INDEX.md` on 2026-08-03, that sentence is
**wrong on all three of its claims**, and the historical text above is retained only as
the record of what was believed at the time:

| Claim | Measured |
|---|---|
| dominated by **reflection** surfaces | **0 of 21** open `runtime` findings are reflection findings |
| dominated by **serialization** surfaces | **0 of 21** — `Serialization/SerializationInfo.hpp` and `Serialization/StreamingContext.hpp` carry **no finding at all** |
| already covered by a CLAUDE.md permanent deviation | **1 of 21** (SR-AUD-168, interop attributes), and even that one has an actionable disclosure residue |

What `runtime` actually contains is **three high-severity POSIX signal-handling
defects in one `.cpp` body** — the destruction of process-wide signal policy, a
blocking write inside a raw signal handler, and a job-control stop on mere observation
— none of which any permanent deviation covers, and none of which `System::Uri`'s
fourteen findings match for severity.

The selection §1 made was still correct: `System::Threading` had 38 findings and 14
highs and was rightly first. Only the *reason given for deferring `runtime`* was
wrong. `docs/SystemRuntimeNamespaceReviewPlan.md` §1 carries the corrected selection
argument.

---

## 23. The SR-AUD-202 split from #1957, and what it did *not* unblock (ticket #2341, 2026-08-12)

§20.2 item 1 recorded that `Monitor::Wait` "**could be split out to land without approval if
the depth bookkeeping is proven exact**", because `Monitor` is a static-only class whose
per-pointer `State` lives in a private static registry and therefore has no public object
layout at all. Ticket **#2341** proved the bookkeeping and landed exactly that member, on the
same pattern by which **#1971** took SR-AUD-214 and SR-AUD-189 out of the blocked #1958.

**#1957 remains `blocked` and keeps SR-AUD-201, SR-AUD-204 and SR-AUD-210.** Its §9 approval
question 2 is unchanged and still unanswered; that question names `ReaderWriterLockSlim` and
`PeriodicTimer` and did not reach `Monitor`.

### What was measured

`build-probe/2341_probe1_monitor_depth.cpp`, every scenario time-boxed:

| scenario | before | after |
|---|---|---|
| depth 1 wait/pulse | `ok` | `ok` |
| depth 2 wait/pulse | **`TIMEOUT`** | `ok` |
| depth 3 wait/pulse | **`TIMEOUT`** | `ok` |
| depth 3, then exactly three `Exit`s, then a foreign `TryEnter` | never reached | `31` (three exits, foreign entry succeeded) |

The repair reads the caller's recursion depth, releases every level for the wait — the
condition variable releases one, `WaitCore` releases the rest — and restores exactly that many.
No public signature, layout, vtable or exception contract changed.

### A correction to §20.2's own mutation expectations, and one equivalent mutation

§20.2 anticipated the depth bookkeeping as the whole risk. Mutation testing found one of its
two published stores is **not independently observable**: dropping the pre-wait
`owner.store(std::thread::id())` kills no test, because the pre-wait `depth.store(0)` already
forces the next `Enter()` to claim ownership. It is retained on invariant grounds and the
header says so. Restoring `depth - 1`, or reverting to the single-level release, each kill four
tests.

### SR-AUD-210 (`Barrier`): the "to be determined" field question is now answered — **no new
field is needed**, and it is still approval-gated

§20.2 left `Barrier`'s cost open ("Likely **no new member**, but … that may cost a field") and
the Consequences row says "`Barrier` to be determined". Determined, by reading the type rather
than by estimate:

Measured first, so the determination rests on the live type rather than on §20.2's estimate
(`build-probe/2341_probe3_barrier_state.cpp`):

| row | value |
|---|---|
| `barrier.sizeof` / `alignof` | **160 / 8** (unchanged since #1955) |
| `barrier.callback_reads_currentphase` | **`TIMEOUT`** — SR-AUD-210 is still live |
| `barrier.callback_phase_value` | `-1`, i.e. the callback never got to read it |
| `barrier.callback_reads_participantcount` | `ok`, value `1` — #1955's control still holds |

1. The "phase is finishing" state §20.2 asks for **already exists**. `actionCallerId_` is an
   existing `std::atomic<std::thread::id>` that `FinishPhase()` sets before the post-phase
   action and clears after it — precisely the interval during which `mutex_` would now be
   released. Any admission guard the restructure needs can read that field, so
   `sizeof(Barrier)` stays 160 and `alignof` stays 8.
2. The transition must also be **reordered**, not merely unlocked. `FinishPhase()` today does
   `++phaseCount_` and `remainingCount_ = participantCount_` *before* invoking the action, and
   every blocked participant waits on the predicate `phaseCount_ > myPhase`. Releasing `mutex_`
   with that predicate already true would let waiters resume *during* the action and read a
   `lastPostPhaseException_` still holding the previous phase's value. Deferring both writes
   until after the action fixes that **and** independently gives the callback the completing
   phase — `phase=0`, which is what the audit's .NET 10 probe printed and what §20.2 requires
   the design to state.

So the repair is: defer the phase transition past the action, run the action with `mutex_`
released, guard other callers on the existing `actionCallerId_`, and keep both the reentrancy
guard and the exception propagation. **It is nevertheless not landable here.** Unlike
`Monitor`, this changes a *synchronisation guarantee* rather than only fixing a hang: the
barrier's lock is observable to third parties for the duration of a user callback, and §20.2's
"Exact approval request" asks about `Barrier` by name. What #2341 removes is only the *layout*
half of the objection — the remaining question is a pure lock-discipline one, and it is the
user's.

### Remaining T-E/2 members

| Finding | Owner | Why not here |
|---|---|---|
| SR-AUD-201 `PeriodicTimer` | #1957, blocked | needs a new in-flight-consumer field (§9 q2) **and** the still-unverified .NET question of whether a second concurrent `WaitForNextTick` throws or blocks; `/rv` is absent |
| SR-AUD-204 `ReaderWriterLockSlim` | #1957, blocked | needs a new waiting-writer field (§9 q2) and introduces writer preference, a fairness change that is part of what is being approved |
| SR-AUD-210 `Barrier` | #1957, blocked | layout question answered above; the lock-discipline question is unanswered |
