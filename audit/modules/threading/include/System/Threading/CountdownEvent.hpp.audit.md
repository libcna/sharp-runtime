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


---

## Remediation record — ticket #1955 (2026-08-03), SR-AUD-207 member → `remediated`

This type is one of SR-AUD-207's three members — the one the audit prose called an
*extension*; the namespace review's §3.1 item 3 records that it is a full member and must be
repaired in the same change or the family closes falsely. It was.

`disposed_` is now `std::atomic<bool>` with a release store in `Dispose()` and an acquire load
in `ThrowIfDisposed()`. `sizeof(CountdownEvent)` 104 → 104, `alignof` 8 → 8. Scenario
`countdownevent.disposed` reported one race before and none after
(`build-probe/1955_probe1_tsan_{before,after}.log`).

`currentCount_` needed no change: `getCurrentCountProperty()` and `getIsSetProperty()` already
read it under the type's own `mutable std::mutex`. SR-AUD-211's `Reset` notification, closed by
ticket #1948, is unaffected.

### A methodology correction worth keeping

The first version of the probe used a 2000-iteration loop per thread and reported **zero**
races for `ManualResetEventSlim` and `CountdownEvent` while reporting one for the
structurally identical `ReaderWriterLockSlim`. The code was equally racy in all three; the
probe was at fault. A writer loop of trivial stores completes before a reader that must set up
a try/catch reaches its first call, so the two threads never overlap and a happens-before
detector sees nothing. Rewriting the disposal scenarios as **1500 rounds of a fresh object
with exactly one access per thread** made all seven reproduce. A "TSan reported nothing"
result is evidence about the probe until the probe is shown to be able to report something.
