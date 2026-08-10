# Audit: `modules/threading/tests/System/Threading/Batch8ThreadingTests.cpp`

## Metadata

- AUDITED: 324-line Threading enum, Thread lifecycle, exception, and delegate
  fixture, fully read.
- Validation: the complete `SharpRuntimeTests_Threading` executable passed
  359/359 on 2026-07-27.
- Related implementation evidence: audited ApartmentState, ThreadPriority,
  ThreadState, Thread, and Thread.cpp reports; SR-AUD-192 through SR-AUD-194.

## SR-AUD-195 — low — Thread running-state test has a tautological zero-mask fallback

`ThreadState::Running` is zero.  The test accepts either exactly Running or
`(static_cast<int>(state) & static_cast<int>(ThreadState::Running)) == 0`.
The latter is `state & 0 == 0`, so it passes for every possible ThreadState,
including Unstarted and Stopped.  The suite reports green even if the observed
started thread never reports a running state.

## Assessment

The fixture usefully fills enum values, ordinary property round trips, start/
join state, raw-owner destruction regression, CurrentThread RunState lifetime,
and basic exception/delegate construction.  Its explicit detached-wrapper
regressions are valuable.  The full Threading binary passes, but the
tautological state branch removes the central assertion from one lifecycle
case.

## Other missing assertions and diagnostics

- It omits empty ThreadStart handling (SR-AUD-192), callback failure policy,
  parameter preservation (SR-AUD-194), self-join, repeated lifecycle stress,
  and destruction/callback races beyond one short sleep-based scenario.
- It exercises CurrentThread only from an owned Thread.  Ordinary external
  native threads, uniqueness/concurrent allocation, and ID reuse/wraparound
  omit SR-AUD-193 entirely.
- Priority tests prove only stored metadata, while apartment and interrupt
  tests lock documented no-op adaptations.  They do not observe OS priority,
  invalid enum diagnostics, platform errors, background process behavior, or
  interruptible waits.
- Exception tests omit HResult, exact type hierarchy/sealing, null/UTF-8 and
  causal-inner identity, producer integration, and actual abort/interrupt
  delivery.  Delegate tests omit empty handlers, callback exceptions,
  concurrency, and event producer lifetime.

## Final assessment

SR-AUD-195 is confirmed by direct expression analysis.  The fixture otherwise
supplies valuable normal/regression smoke coverage.  No source or test was
changed.

## Post-audit remediation — ticket #1949 (2026-08-03)

**SR-AUD-195 is `remediated`.** `Thread_ThreadState_RunningAfterStart` no longer
carries the `state & 0 == 0` fallback. Because `ThreadState::Running` is zero,
"contains Running" is not expressible as a mask test at all, so the case now
asserts the exact state a started, still-executing thread must report — `Running`,
or `Running | Background` for a background thread — and additionally that the
observed state is neither `Unstarted` nor `Stopped`. Test-only change; no
production source, signature, layout or semantic was involved.
