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
**#1947**, **#1948** and **#1949** were implemented in the same batch and their
results are recorded in §16. Tickets **#1951–#1955** are `todo` and unstarted;
**#1956–#1959** are `blocked` on the four approvals in §9.

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
