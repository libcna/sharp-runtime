# Audit: `modules/threading/include/System/Threading/RegisteredWaitHandle.hpp`

## Metadata

- AUDITED: 99-line dedicated-thread registered-wait implementation, fully read.
- Validation: `RegisteredWaitHandleTests.*` passed 2/2 within the focused
  10/10 Threading filter on 2026-07-27; its Batch9 fixture source is pending
  complete audit.
- Reference/probe: current .NET ThreadPool registered-wait argument contract;
  C++/managed validation probes plus an isolated null-wait child process.

## SR-AUD-188 — high — RegisterWaitForSingleObject accepts a null WaitHandle and starts a background null dereference

Neither ThreadPool nor the private RegisteredWaitHandle constructor validates
`waitObject`. The worker subsequently calls `waitObject->WaitOne`, so an
isolated C++ registration with null terminates the child process with a core
dump. Current .NET rejects the same null input synchronously with
`ArgumentNullException`.

The defect is asynchronous: registration can appear to return normally while
the detached/background worker crashes later. The focused tests construct only
a valid Semaphore and therefore cannot observe the boundary.

## Assessment

For valid input, the class's polling loop and blocking Unregister adaptation
make the reviewed in-flight wait lifetime safer than a detached raw wait. The
blocking difference from .NET is explicitly justified by the lack of a
ref-counted native handle. It does not justify accepting null input.

## Other missing assertions and diagnostics

- Tests omit null wait/callback, invalid timeouts, timeout callback delivery,
  non-null state, recurring wait behavior, zero timeout latency, exception
  propagation, and self-unregistering callbacks.
- The C++ probe reports `empty_callback=normal timeout_minus2=normal`, where
  the managed probe reports `argument_null` and `argument_out_of_range`.
  These validation gaps should be repaired with the null-wait crash path.
- No consumer test establishes wait-object lifetime when callers never call
  Unregister, or safe user callback destruction after unregistration.

## Final assessment

SR-AUD-188 is confirmed by a crashing isolated C++ probe and managed boundary
comparison. No source or test was modified.


---

## Remediation record — ticket #1953 (2026-08-03), SR-AUD-188 → `remediated`

Cause **T-C** of `docs/ThreadingNamespaceReviewPlan.md`, scheduled first within that cause
because the failure mode is memory-unsafety rather than a wrong diagnostic.

The private constructor now validates before the `std::thread` is created:

```cpp
if (waitObject == nullptr) throw System::ArgumentNullException("waitObject");
if (!callback)             throw System::ArgumentNullException("callBack");
```

and `ThreadPool::RegisterWaitForSingleObject` runs
`ArgumentOutOfRangeException::ThrowIfLessThan(millisecondsTimeOutInterval, -1,
"millisecondsTimeOutInterval")` before delegating. That split is .NET's own: the public
`RegisterWaitForSingleObject(WaitHandle, WaitOrTimerCallback, object?, int, bool)` overload
performs the range check and hands off to a private overload that opens with
`ArgumentNullException.ThrowIfNull(waitObject)` and
`ArgumentNullException.ThrowIfNull(callBack)`. Putting the null checks in the constructor
rather than in `ThreadPool` is what makes "no worker thread is created for an invalid
registration" structural instead of incidental.

**Scope note.** This report's "Other missing assertions" section records that the C++ probe
printed `empty_callback=normal timeout_minus2=normal` where the managed probe printed
`argument_null` and `argument_out_of_range`, and asks for those to be *"repaired with the
null-wait crash path"*. Both are therefore closed here rather than split off. The empty
callback is also a cause-T-B shape, but it was never in #1951's site list because SR-AUD-188
owns it; no separate ticket or `SR-AUD-*` identifier is issued.

Evidence: `build-probe/1953_probe1_null_argument_crashes.cpp`, logs `1953_probe1_before.log`,
`1953_probe1_after.log`, `1953_probe1_asan.log`. Each case runs in a forked child under
`alarm(3)` so a crash or a hang is reported rather than suffered.

| Row | Before | After |
|---|---|---|
| `threadpool.register.null_waitobject` | `child-signal:11(SEGV)` | `ArgumentNullException(waitObject)` |
| `threadpool.register.empty_callback` | `normal` (registration waits, then notifies nobody) | `ArgumentNullException(callBack)` |
| `threadpool.register.timeout_minus2` | `normal` | `ArgumentOutOfRangeException(millisecondsTimeOutInterval)` |
| `threadpool.register.valid` | `normal` | `normal` (control: the callback still fires) |

The same probe compiled with `-fsanitize=address,undefined` and `ASAN_OPTIONS=detect_leaks=1`
reports nothing; instrumentation was verified by symbol inspection (32 sanitizer symbols
present in the sanitized binary, none in the plain one).

Coverage: `modules/threading/tests/System/Threading/ThreadingBoundaryTests.cpp`,
`ThreadingNullArgumentTests.RegisterWaitForSingleObject_*` — null wait object, empty callback,
`-2` timeout, the range-before-null ordering, and a control asserting a valid registration
still fires. No signature, layout, vtable or exception-specification change.
