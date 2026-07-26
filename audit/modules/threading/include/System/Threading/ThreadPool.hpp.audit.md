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
