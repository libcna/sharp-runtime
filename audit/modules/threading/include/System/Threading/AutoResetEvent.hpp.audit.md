# Audit: `modules/threading/include/System/Threading/AutoResetEvent.hpp`

## Metadata

- AUDITED: 81-line condition-variable auto-reset event, fully read.
- Validation: focused AutoResetEvent fixture cases are part of the green
  Threading baseline; direct C++/.NET 10 lifecycle probes and a C++ versus C#
  WaitHandle multi-wait compile probe were run.
- Reference basis: current .NET 10 AutoResetEvent inheritance and close
  behavior.

## SR-AUD-208 — medium — `Close` extends the no-op closed-handle defect

The explicit `Close()` body is empty. After `Close()`, the C++ lifecycle probe
prints `autoWait=0`, proving normal timed operation remains possible; the
matching .NET 10 call throws `ObjectDisposedException`. This extends
SR-AUD-208 from `Mutex` to AutoResetEvent.

## SR-AUD-209 — medium — AutoResetEvent and ManualResetEvent are not `WaitHandle`s and cannot participate in multi-wait APIs

Unlike their managed counterparts, this class does not derive from local
`WaitHandle`; it only includes that header to reuse timeout validation. A C++
compile probe constructing `std::vector<WaitHandle*>` from AutoResetEvent and
ManualResetEvent fails with no conversion from either event pointer. The
equivalent .NET 10 `WaitHandle[] { new AutoResetEvent(...), new
ManualResetEvent(...) }` builds successfully and supports `WaitAny`/`WaitAll`.
The local events are thus disconnected from the only multi-wait public path.

## Assessment

The local condition-variable state machine correctly preserves a pending signal
and consumes one signal per successful wait in the reviewed normal paths. It
does not preserve the managed WaitHandle lifecycle or polymorphic composition
surface.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-208 Close-before-wait/set/reset behavior and
  ObjectDisposedException diagnostics.
- Tests omit SR-AUD-209 WaitAny/WaitAll composition with event instances,
  as well as mixed event/semaphore arrays.
- They omit multiple-waiter Set behavior, Set/Reset races, infinite waits,
  high-contention one-signal-per-wait accounting, destruction while blocked,
  and named/kernel-event behavior.

## Final assessment

SR-AUD-208 extends here and SR-AUD-209 is confirmed by paired C++/C# compile
evidence. No production or test source was changed.
