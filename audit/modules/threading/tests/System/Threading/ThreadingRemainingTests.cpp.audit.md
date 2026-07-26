# Audit: `modules/threading/tests/System/Threading/ThreadingRemainingTests.cpp`

## Metadata

- AUDITED: 986-line mixed Threading fixture, fully read.
- Validation: the complete `SharpRuntimeTests_Threading` executable passed
  359/359 on 2026-07-27.
- Related implementation evidence: audited EventResetMode/EventWaitHandle,
  WaitHandle, ThreadPool, and Threading callback reports; the remaining
  production types referenced here are pending their dedicated source audits.

## Assessment

The fixture provides broad normal and repaired-regression coverage for enum
values, AsyncLocal/ThreadLocal address reuse, Barrier/CountdownEvent disposal
and bounds, Lock/ReaderWriterLockSlim ownership, timeout behavior, SpinWait,
and basic ThreadPool queuing.  Several cross-thread scenarios use futures or
atomics and have real completion assertions.  The full Threading executable
passes.

Its ThreadPool section, however, demonstrates only a detached callback that
usually runs within a fixed 20 ms sleep and a setter return value.  It leaves
the audited behavior indistinguishable from a no-op configuration API and
does not cover the raw-work-item or registered-wait API at all.  No new
implementation defect is demonstrated by this test source.

## Other missing assertions and diagnostics

- `SetMinMaxThreads_ReturnsTrue` locks in SR-AUD-189: it neither reads the
  values back after a successful call nor checks invalid, zero, overflow, or
  mutually inconsistent min/max arguments.  `GetMinThreads_NonNegative` and
  `GetMaxThreads_AtLeastMin` are too weak to reveal immutable defaults.
- `QueueUserWorkItem_ReturnsTrue` uses an uncoordinated 20 ms sleep.  It omits
  a deterministic completion signal, queued callback exceptions, state
  lifetime, pressure/concurrency, worker reuse, and all
  `IThreadPoolWorkItem*` destruction cases in SR-AUD-187.
- No test registers a wait.  Null wait/callback, invalid `-2` timeout,
  finite/zero timeout delivery, recurring behavior, callback state lifetime,
  and unregistration/callback races therefore leave SR-AUD-188 unprotected.
- Event reset tests cover only the two declared values; a cast invalid mode is
  never rejected, leaving confirmed SR-AUD-184 unasserted.  The enum fixture
  similarly samples only selected valid `ApartmentState`, `ThreadPriority`,
  and `ThreadState` values.
- Many timing tests use direct short sleeps or a single wall-clock lower
  bound.  They omit scheduler-delay diagnostics, repeated stress execution,
  and cleanup checks for a failure before joining the spawned thread.
- Timer has an explicit comment saying it is not tested because of its
  suspected detached raw-`this` lifetime issue.  That comment is not evidence
  of a defect; its source review must establish the behavior before it is
  classified.

## Final assessment

The broad fixture validates many normal and prior-regression paths, but its
ThreadPool section masks all three newly confirmed boundaries and excludes
Timer behavior altogether.  No source or test was changed.
