# Audit: `modules/timers/include/System/Timers/Timer.hpp`

## Metadata

- AUDITED: public Timer state, lifecycle, documented reductions, and callback
  ownership contract.
- Validation: complete native Timers fixture passed 9/9. Direct native and
  current-.NET probes compared sender identity and a throwing Elapsed handler.

## SR-AUD-238 — high — an exception from Elapsed escapes the background std::thread and aborts the process

Timer exposes a handler collection whose callback runs directly on the
background Threading::Timer worker.  The native probe registers a one-shot
handler that throws `std::runtime_error`; the process prints
`terminate called after throwing` and exits 134.  Current .NET's
`Timer.MyTimerCallback` wraps its event invocation in `try/catch`, and the
same managed probe prints `throw_process=alive`.  A consumer callback can
therefore convert an ordinary notification failure into process termination.

## SR-AUD-239 — medium — Elapsed reports a null sender rather than the timer that raised the event

The public event uses an `Object* sender`, but the implementation raises it
with `nullptr`.  The native one-shot probe prints
`fired=1 sender_is_timer=0 sender_is_null=1`; current .NET prints
`fired=True sender_is_timer=True sender_is_null=False`.  The documented
Component reduction does not document replacing the event source identity with
null, so handlers cannot recover the timer instance through their supplied
sender.

## Assessment

The public lifecycle and interval shape are recognizable, and the header makes
the SynchronizingObject/Component reduction explicit.  Its documented raw
`this` callback lifetime caveat remains a serious design risk, but this audit
does not count it separately without a deterministic reproducer; SR-AUD-238
and SR-AUD-239 are independently demonstrated observable failures.

## Other missing assertions and diagnostics

- Add a process-isolated throwing-handler regression (SR-AUD-238), a
  non-null/same-instance sender assertion (SR-AUD-239), multiple handler and
  handler-removal coverage, and a handler that throws while another handler
  remains subscribed.
- Add synchronization/lifetime tests for Close, destructor, self-disposal,
  disable/re-enable, interval and AutoReset mutation from handlers, and a
  pending tick racing destruction; run the latter under ASan/TSan.
- Test fractional/maximum/non-finite intervals, TimeSpan overflow, elapsed
  SignalTime range, BeginInit repetition, and use-after-Dispose semantics.

## Final assessment

SR-AUD-238 and SR-AUD-239 are directly reproduced. No source or test was
changed during this audit.
