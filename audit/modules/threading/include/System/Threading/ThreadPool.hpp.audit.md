# Audit: `modules/threading/include/System/Threading/ThreadPool.hpp`

## Metadata

- AUDITED: 101-line detached-thread ThreadPool implementation, fully read.
- Validation: `ThreadPoolTests.*:IThreadPoolWorkItemTests.*:`
  `RegisteredWaitHandleTests.*` passed 10/10 in `SharpRuntimeTests_Threading`
  on 2026-07-27; relevant large fixture sources remain pending audit.
- Reference/probe: current .NET ThreadPool validation contract; ASan work-item
  lifetime probe and C++/managed thread-minimum configuration probes.

## SR-AUD-187 — high — UnsafeQueueUserWorkItem releases a borrowed work item to a detached thread without retaining its lifetime

The public overload captures `IThreadPoolWorkItem*` in a detached lambda and
then invokes `Execute()` with no ownership, join, or retention mechanism. The
ASan probe queues a heap item, waits until Execute has entered, deletes it,
then permits Execute to touch its member; ASan reports heap-use-after-free in
`DelayedWorkItem::Execute`, called from the ThreadPool lambda.

The only local work-item test keeps its stack object alive until a sleep-based
counter check succeeds, so it does not exercise the public lifetime boundary.
Current managed work-item infrastructure retains queued work through managed
object lifetime; the public C++ raw-pointer form offers no equivalent
requirement or safe owning handle.

## SR-AUD-189 — medium — ThreadPool configuration setters report success while ignoring requested values and invalid arguments

`SetMinThreads` and `SetMaxThreads` unconditionally return true and store no
state. The C++ probe prints `before=1,1 set_min=1 after=1,1 invalid_min=1`
for `SetMinThreads(7, 9)` followed by `SetMinThreads(-1, -1)`. The managed
probe prints `before=16,16 set_min=True after=7,9 invalid_min=False`: a
successful call changes observable minima and an invalid request is rejected.

The local test asserts only true for valid values, thereby locking in the
no-op behavior. This is not an implementation-detail absence: callers use
the returned result to decide whether their concurrency policy took effect.

## Assessment

Normal callback queuing validates empty std::function inputs and the
Emscripten no-pthreads branch gives an explicit PlatformNotSupportedException.
The dedicated-thread registered-wait adaptation is documented. It remains
distinct from a managed shared worker pool and does not provide current
ThreadPool throughput, availability, execution-context, cancellation, or
worker-accounting contracts.

## Other missing assertions and diagnostics

- Tests omit raw work-item lifetime/destruction (SR-AUD-187), callback
  exception/termination policy, queue pressure, many concurrent submissions,
  state-pointer lifetime, and work scheduling/worker reuse.
- No configuration case checks read-after-set, invalid/zero/overflow inputs,
  min/max consistency, false results, or platform-specific maximum values
  (SR-AUD-189).
- RegisterWait input validation, zero/finite timeout accuracy, callback
  exceptions, cancellation, and safe wait-handle lifetime are absent; null
  wait is separately confirmed as SR-AUD-188.

## Final assessment

SR-AUD-187 and SR-AUD-189 are confirmed by ASan and C++/managed probes. No
source or test was modified.


---

## Correction and remediation -- ticket #1971, 2026-08-03 (SR-AUD-189 only)

*Audit text above preserved verbatim; this section is appended.*
**SR-AUD-187 is untouched and remains `confirmed`** -- its lifetime half is owned by #1959,
and its null half was already closed before this batch
(`docs/ThreadingNamespaceReviewPlan.md` §3.1 item 1).

**Evidence:** `build-probe/1971_probe1_group_a.cpp`, logs `1971_probe1_before.log` /
`1971_probe1_after.log`.

### The finding reproduced exactly, and is wider than the probe it quotes

Measured before: `SetMinThreads(7,9)` returned true and left `GetMinThreads` reporting `1,1`;
`SetMinThreads(-1,-1)`, `SetMinThreads(0,0)`, `SetMaxThreads(0,0)` and a maximum below the
current minimum **all returned true**. The report's C++ probe covers the first two; the last
three are the same defect at three further inputs.

### Repair, with each rule's provenance stated

`SetMinThreads`/`SetMaxThreads` now validate, store, and return `false` for input they reject;
`GetMinThreads`/`GetMaxThreads` report what was stored. Because
`/rv/tmp/runtime/src/libraries/` is **not present in this environment**, no rule below is
presented as freshly read from .NET source:

| Rule | Provenance |
|---|---|
| a negative minimum is rejected | **measured** -- the audit's own managed probe (`invalid_min=False`) |
| a valid pair is stored and observable through the getter | **measured** -- the same probe (`set_min=True after=7,9`) |
| a minimum above the corresponding maximum, or a maximum below it, is rejected | **.NET-documented** consistency rule |
| a maximum below 1 is rejected | **.NET-documented** (a pool with a maximum of zero threads cannot run anything) |
| zero is accepted as a *minimum* | **reasoned** -- not measured and not documented either way; a minimum of zero is meaningful, so no rejection was invented |

**Deliberately not adopted:** .NET additionally refuses a maximum below the machine's processor
count. It could not be verified here and would make this port's observable result depend on the
core count of whatever machine runs it. Same reasoning as ticket #1952's non-adoption of
`MaxWaitHandles`.

### One measured consequence of the consistency rule, worth stating

On this four-core container the default maximum is 8, so `SetMinThreads(7, 9)` now returns
**false** -- 9 exceeds the completion-port maximum. The audit's managed probe got `True` for the
same call on a 16-core machine whose default maximum was 16. Both are the same rule; the answer
is machine-dependent because .NET's *default maximum* is. The regressions therefore set an
explicit maximum before exercising a minimum, rather than relying on the default.

### Layout, ABI and the `intcs` correction

`ThreadPool` is a static-only class (`ThreadPool() = delete`) with no instances, so the stored
configuration lives in function-local statics behind one shared mutex -- a read-modify-write
across both pairs, which is why a lock rather than atomics. **No object layout exists to
change.**

All four entry points moved from `int` to `intcs`, the opportunistic correction
`docs/ThreadingNamespaceReviewPlan.md` §3.1 item 4 assigned to whichever ticket rewrote these
bodies. The claim that it is mangling-neutral is **measured, not asserted**: `nm` on the built
test binary reports `_ZN6System9Threading10ThreadPool13SetMinThreadsEii` and
`_ZN6System9Threading10ThreadPool13GetMinThreadsERiS0_` after the change, i.e. `int` parameters,
because `intcs` is `int32_t` is `int` on this platform.

### Concurrency

Four threads x 2,000 iterations configuring and reading the pool concurrently: **0 invariant
violations**, TSan **0 data races**, fully instrumented from source. Honestly recorded: the same
scenario also reported 0 against the pre-fix header, because setters that store nothing cannot
break an invariant -- it is evidence about the *new* state, not a before/after discriminator.

**SR-AUD-189: `confirmed` -> `remediated` (#1971, 2026-08-03).**
