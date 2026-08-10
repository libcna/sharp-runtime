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


---

## Remediation record — ticket #1951 (2026-08-03), SR-AUD-190 → `remediated`

Cause **T-B** of `docs/ThreadingNamespaceReviewPlan.md`, i.e. **CCF-011** ("empty
`std::function` values cross public boundaries without an explicit policy") at a new site
in `modules/threading`. The policy was already selected, approved and implemented for
`modules/core` by tickets #1866–#1870 and is recorded in
`docs/EmptyCallableBoundaryPlan.md`; nothing new was designed here.

`Timer`'s constructor now throws `System::ArgumentNullException("callback")` for an empty
callback. The check is placed **after** the two `ArgumentOutOfRangeException::ThrowIfLessThan`
range checks and **before** every side effect, which is .NET's own order: `Timer.cs`'s public
constructor delegates to a private one that runs `ThrowIfLessThan(dueTime, -1)` and
`ThrowIfLessThan(period, -1)`, and only then calls `TimerSetup`, which opens with
`ArgumentNullException.ThrowIfNull(callback)`. No `std::thread` is created and no schedule is
recorded when it fires.

Evidence: `build-probe/1951_probe1_threading_empty_callables.cpp`, logs
`1951_probe1_before.log` / `1951_probe1_after.log`. `timer.empty_callback` moved from
`normal` to `ArgumentNullException|Value cannot be null. (Parameter 'callback')`; the control
`timer.control_callback` is unchanged. The same probe under
`-fsanitize=address,undefined` with `detect_leaks=1` reports nothing
(`1951_probe1_asan.log`); instrumentation was verified by the presence of `__asan_report_*`
symbols in the sanitized binary and their absence in the plain one.

Permanent regressions live in
`modules/threading/tests/System/Threading/ThreadingBoundaryTests.cpp`
(`ThreadingEmptyCallableTests.Timer_*`), including the range-before-callable ordering case
and a control proving a real callback still fires.

**Related observation, no ticket and no `SR-AUD-*` identifier.**
`System::TimeProvider::CreateTimer` forwards its callback into `System::Threading::Timer`, so
it inherits this rejection with the same exception and parameter name. .NET's
`TimeProvider.CreateTimer` runs `ThrowIfNull(callback)` *before* its own range handling, so
the two differ in which diagnostic wins when a caller supplies both an empty callback and an
out-of-range duration. No finding claims this, it is not a defect in either direction, and it
is recorded here rather than acted on.
