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
