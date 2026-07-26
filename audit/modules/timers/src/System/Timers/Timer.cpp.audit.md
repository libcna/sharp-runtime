# Audit: `modules/timers/src/System/Timers/Timer.cpp`

## Metadata

- AUDITED: interval validation/rounding, worker creation, stale-cookie check,
  enable/disable, initialization, and Close/destructor paths.
- Validation: complete native fixture passed 9/9; direct C++ and current-.NET
  probes exercised a one-shot event sender and an exception thrown by a
  handler.

## SR-AUD-238 — high — an exception from Elapsed escapes the background std::thread and aborts the process

`startTimerThread()` invokes `Elapsed.Raise` without an exception boundary.
The native one-shot throwing-handler probe terminates with
`std::runtime_error` and exit 134.  Current .NET catches exceptions around
`intervalElapsed(this, elapsedEventArgs)` and keeps the process alive.  See
the public Timer report for the complete reproduction.

## SR-AUD-239 — medium — Elapsed reports a null sender rather than the timer that raised the event

The callback calls `Elapsed.Raise(nullptr, args)` even though the managed
counterpart invokes `intervalElapsed(this, elapsedEventArgs)`.  The native
probe observes only null; current .NET observes the timer instance.  This is
an observable event contract mismatch, not merely a C++ spelling difference.

## Assessment

Cookie invalidation avoids dispatch for many stopped-timer callbacks, and
single-shot state is updated before dispatch.  The detached underlying worker,
unlocked user callback, and raw outer `this` leave shutdown/concurrent mutation
outside a demonstrated safe ownership protocol; the source header discloses
that risk, but no additional deterministic sanitizer result was produced here.

## Other missing assertions and diagnostics

- Test SR-AUD-238/239 directly; tests currently count ticks only.
- Add boundary validation for NaN/infinity/large interval inputs, update while
  enabled, Close during wait and callback, repeated Close, concurrent setter
  access, and callbacks that mutate Timer or throw.
- Log/contain handler exceptions and expose a lifecycle-completion guarantee
  suitable for a concurrent destructor/Close regression.

## Final assessment

SR-AUD-238 and SR-AUD-239 are reproduced at this implementation boundary. No
source or test was changed.
