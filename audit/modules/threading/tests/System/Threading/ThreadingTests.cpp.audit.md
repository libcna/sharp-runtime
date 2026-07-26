# Audit: `modules/threading/tests/System/Threading/ThreadingTests.cpp`

## Metadata

- AUDITED: 765-line principal Threading fixture, fully read.
- Validation: the complete `SharpRuntimeTests_Threading` executable passed
  359/359 on 2026-07-27.
- Related implementation evidence: audited Thread, Timeout, ThreadState, and
  existing synchronization/cancellation reports; SR-AUD-183, SR-AUD-192
  through SR-AUD-194.

## SR-AUD-197 — low — CurrentThread managed-ID test discards the only observed value

`Thread_CurrentThread_ReturnsManagedThreadId` calls the property and then
casts its result to void with the comment that any value is valid.  It has no
assertion at all, so it passes for zero, a constant, or an invalid identity.
It cannot protect the external-thread ID collision confirmed by SR-AUD-193.

## Assessment

This is otherwise a valuable wide fixture: it covers ordinary Thread start/
join, Monitor ownership/wait/pulse, primitive wait validation, cancellation
ordering/disposal, SpinLock ownership, and Volatile round trips, including
several real concurrency regressions.  The complete executable passes.  Its
normal-path scope still omits the invalid/lifetime boundaries identified in
the corresponding source audits.

## Other missing assertions and diagnostics

- Thread tests omit empty callbacks (SR-AUD-192), external native-thread IDs
  (SR-AUD-193), parameterized start (SR-AUD-194), callback throw policy,
  self-join, post-destruction scheduling, and deterministic lifecycle stress.
- Interlocked cases omit extrema/overflow, pointer/object/reference forms,
  cross-thread linearizability, and memory-order diagnostics.
- Monitor and wait primitives omit null/raw-owner lifetime, destruction while
  blocked, many-waiter fairness/spurious wakeup stress, exception callbacks,
  and wall-clock jitter bounds.  Cancellation's 200-iteration race does not
  assert a defined result and needs a sanitizer run to detect reintroduced
  data races.
- SpinLock tests omit zero/infinite TimeSpan overloads, disabled ownership
  behavior across threads, high-contention starvation, and exception cleanup;
  Volatile covers only same-thread scalar round trips, not publish/consume
  ordering or pointer/reference forms.

## Final assessment

SR-AUD-197 is confirmed by direct test inspection.  The fixture remains useful
for normal and repaired-regression paths.  No source or test was changed.
