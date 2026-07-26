# Audit: `modules/threading/include/System/Threading/Timer.hpp`

## Metadata

- AUDITED: 184-line dedicated-thread timer implementation, fully read.
- Validation: the complete `SharpRuntimeTests_Threading` executable passed
  359/359 on 2026-07-27; C++/managed constructor probes were run for callback
  validation.
- Reference: current .NET `TimeProvider.CreateTimer` contract documents a
  required non-null callback and `ArgumentNullException` at entry.

## SR-AUD-190 — medium — Timer accepts an empty callback and silently creates a timer that can never notify

The constructor moves an empty `std::function` into shared state and `run()`
merely skips invocation when it is false.  The C++ probe prints
`empty_callback=normal`; the managed `new Timer(null, null, 0,
Timeout.Infinite)` probe prints `empty_callback=argument_null`.  A caller
therefore receives a seemingly valid timer that performs no requested work,
instead of an entry-boundary diagnostic.

The normal TimeProvider test supplies a non-empty callback and cannot expose
this path.

## Assessment

The reviewed dedicated thread deliberately holds `State` through a
`shared_ptr`; the worker captures that state rather than raw `this`.  It
therefore does **not** substantiate the obsolete raw-`this` concern in
`ThreadingRemainingTests.cpp`.  Generation-based wakeup and pause/rearm logic
also provide a coherent native adaptation for valid integer inputs.  The
documented partial surface (dedicated threads and omitted overloads) is kept
separate from SR-AUD-190.

## Other missing assertions and diagnostics

- No test rejects an empty callback (SR-AUD-190), observes callback exceptions,
  preserves/validates state lifetime, or establishes callback concurrency and
  reentrancy under a short period.
- Tests omit zero and maximum delays, invalid due/period combinations,
  repeated rearm/pause, Change during a callback, disposal during a callback,
  destructor-only cleanup, and platform-specific Emscripten diagnostics.
- `Change` after `Dispose` has no local disposed state or result; its
  TimeProvider/ITimer-visible effect is separately recorded as SR-AUD-191.

## Final assessment

SR-AUD-190 is confirmed by direct native/managed probes.  Shared state avoids
the alleged raw-`this` worker lifetime hazard.  No source or test was changed.
