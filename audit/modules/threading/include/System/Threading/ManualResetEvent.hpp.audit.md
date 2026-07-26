# Audit: `modules/threading/include/System/Threading/ManualResetEvent.hpp`

## Metadata

- AUDITED: 70-line condition-variable manual-reset event, fully read.
- Validation: focused ManualResetEvent fixtures are part of the green Threading
  baseline; direct C++/.NET 10 lifecycle and multi-wait compile probes were
  run with its AutoResetEvent sibling.
- Reference basis: current .NET 10 ManualResetEvent inheritance and close
  behavior.

## SR-AUD-208 — medium — `Close` extends the no-op closed-handle defect

The concrete `Close()` is empty. The C++ probe prints `manualWait=0` after
close; current .NET 10 throws `ObjectDisposedException` before waiting. This
extends SR-AUD-208 to ManualResetEvent.

## SR-AUD-209 — medium — missing `WaitHandle` inheritance extends the multi-wait API gap

ManualResetEvent is likewise not convertible to local `WaitHandle*`, so it
cannot be passed to the local vector-based `WaitAny`/`WaitAll` routes. The
shared C++ compile probe fails while the equivalent .NET 10 event array builds.
See AutoResetEvent's report for the owning SR-AUD-209 evidence.

## Assessment

Set, Reset, persistent signal, normal timed wait, and timeout validation are
simple and coherent in the reviewed code. Lifecycle and WaitHandle composition
remain observably incomplete.

## Other missing assertions and diagnostics

- Tests omit close/disposal behavior (SR-AUD-208) and multi-wait composition
  (SR-AUD-209).
- They omit multiple waiter release on one Set, Set/Reset/timeout races,
  infinite waits, destruction while waiting, and named/kernel-event behavior.

## Final assessment

SR-AUD-208 and SR-AUD-209 extend here. No production or test source was
changed.
