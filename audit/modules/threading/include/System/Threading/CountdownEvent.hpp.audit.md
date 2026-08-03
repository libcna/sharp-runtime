# Audit: `modules/threading/include/System/Threading/CountdownEvent.hpp`

## Metadata

- AUDITED: 146-line counting-event implementation, fully read.
- Validation: reviewed direct fixture coverage; a bounded C++/current-.NET 10
  Reset(0)-while-waiting probe and a TSan Dispose/Wait probe were run.
- Reference basis: current .NET 10 CountdownEvent reset, wait, and
  thread-safe public-member behavior.

## SR-AUD-211 — high — `Reset(0)` changes the predicate but never wakes blocked waiters

`Reset(intcs)` assigns `currentCount_` but never notifies `cv_`. A child starts
a waiter on `CountdownEvent(1)`, calls `Reset(0)`, then joins it; C++ remains
blocked until a two-second timeout (exit 124). The identical .NET 10 program
prints `waiter-released` and `completed`. Resetting to zero is a valid signal
transition and must release existing waiters.

## SR-AUD-207 — high — unsynchronized disposal extends the public-operation race

`disposed_` is read outside `mutex_` by `ThrowIfDisposed()` and written without
synchronization by `Dispose()`. The Dispose/`Wait(0)` TSan probe reports the
direct race at line 28, extending SR-AUD-207 from SemaphoreSlim and
ManualResetEventSlim.

## Assessment

Constructor, ordinary count changes, zero/negative diagnostics, range-safe
AddCount, normal reset, and quiescent post-disposal paths are well represented
in the existing fixtures. They do not retain a waiter during Reset(0) or run a
public operation concurrently with disposal.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-211 wake-up after Reset(0), reset-to-nonzero effects on
  pending waiters, and Reset/Signal races.
- Tests omit SR-AUD-207 TSan coverage for Dispose with all Wait/Signal/Add/
  Reset/property operations.
- They also omit multiple waiters, infinite wait release, high-contention
  count accounting, WaitHandle/cancellation overload parity, and destruction
  while threads remain blocked.

## Final assessment

SR-AUD-211 is confirmed by bounded C++/.NET comparison; SR-AUD-207 extends
through TSan. No production or test source was changed.

## Post-audit remediation — ticket #1948 (2026-08-03)

**SR-AUD-211 is `remediated`.** The original evidence above is retained unchanged
and reproduced exactly as recorded: `build-probe/1948_probe1_countdown_reset_wake.cpp`
printed `reset0=TIMEOUT` and exited 1 before the repair.

`Reset(intcs)` now releases the lock and calls `cv_.notify_all()`. The
notification is unconditional rather than guarded on `count == 0`: a reset to a
non-zero count leaves `Wait`'s predicate genuinely false, so a woken waiter
re-checks it and blocks again — which spurious wakeups already require it to
tolerate. The probe's control confirms it: after the repair it prints
`reset0=released` **and** `control reset3=still-blocked`, and exits 0. Two
permanent regressions pin both halves. No public signature, object layout, vtable
or exception contract changed, and `Reset(negative)` / `Reset` after `Dispose`
keep their existing exceptions and order.

**SR-AUD-207 remains `confirmed`** here: this type's unsynchronised `disposed_`
flag is cause T-A in `docs/ThreadingNamespaceReviewPlan.md`, owned by ticket
#1955, and was deliberately not absorbed into #1948.
