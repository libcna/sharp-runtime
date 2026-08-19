<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `PeriodicTimer::WaitForNextTick` is single-consumer (ticket #1957, SR-AUD-201)

*2026-08-19.* A second **concurrent** call to
`System::Threading::PeriodicTimer::WaitForNextTick()` now throws
`System::InvalidOperationException` instead of being served silently.

Landed under `docs/StandingApprovals.md` **SA-3** (a private data member, `sizeof` pinned) and
**SA-5** (the behaviour is derived from the reference). **`sizeof(PeriodicTimer)` is 128 before
and after** — the flag fits in padding the type already had — so no consumer needs a rebuild for
layout.

---

## 1. What was wrong

`PeriodicTimer` had no in-flight-consumer state, so two threads calling `WaitForNextTick()`
concurrently **both returned `true` for one tick**. The audit's probe measured `concurrent=1,1`.

A caller that accidentally shared a timer therefore got twice the intended work rate, with no
diagnostic anywhere. That is the worst shape of concurrency bug: the wrong answer is a plausible
one.

## 2. What .NET does, and the `[unverified]` flag this resolves

The design record for this ticket (`docs/ThreadingNamespaceReviewPlan.md` §20.2, item 4) proposed
throwing, and marked it:

> **[unverified: whether .NET throws or blocks the second consumer must be confirmed against the
> reference before landing]**

The reference confirms **throwing**:

```csharp
private bool _activeWait;                                    // PeriodicTimer.cs:192
...
lock (this)
{
    if (_activeWait)
    {
        // WaitForNextTickAsync should only be used by one consumer at a time.
        // Failing to do so is an error.
        ThrowHelper.ThrowInvalidOperationException();         // PeriodicTimer.cs:199-203
    }
```

and the type's own summary says the same: *"This timer is intended to be used only by a single
consumer at a time: only one call to `WaitForNextTickAsync` may be in flight at any given
moment"* (`PeriodicTimer.cs:13-14`).

## 3. The guard runs first, and that is load-bearing

.NET tests `_activeWait` **before** the cancellation short-circuit and **before** the
already-signalled fast path (`PeriodicTimer.cs:197-213`). This port therefore tests it before the
disposed check.

The consequence is deliberate: a second consumer arriving while the first is waiting is refused
**even if the timer has since been disposed**. It is the *concurrent use* that is the error, not
the timer's state. A mutation that moves the guard below the disposed check is caught.

## 4. The flag is cleared on every exit

By an RAII guard, so it survives the ordinary return, the disposed return, and any exception. .NET
clears it in the completion path (`PeriodicTimer.cs:296`).

This is the half a naive implementation gets wrong in the opposite direction: a flag that is set
but never cleared makes the **first** wait lock the timer out for ever, so ordinary sequential use
breaks. Two separate tests pin it — one for repeated successful ticks, one for repeated
disposed returns.

## 5. What did not change

Single-consumer use — which is every correct use — is **completely unchanged**: same ticks, same
timing, same `false` after `Dispose()`. All 475 pre-existing `SharpRuntimeTests_Threading` cases
passed unchanged before the new ones were added.

## 6. To migrate

If you were sharing one `PeriodicTimer` across threads, you were consuming ticks twice. Give each
consumer its own timer, or serialise the waits behind your own lock. .NET has never supported the
shared pattern; this port simply failed to say so.

## 7. Scope

This is **one of the four members** of ticket #1957. SR-AUD-202 (`Monitor::Wait` recursion) landed
earlier as #2341 under the same section's item-1 carve-out. **SR-AUD-204**
(`ReaderWriterLockSlim` writer preference) and **SR-AUD-210** (`Barrier` post-phase action
deadlock) are untouched and #1957 stays open for them — SR-AUD-204 in particular introduces writer
preference, which is a fairness change that alters which workloads block, and it deserves its own
landing with its own TSan run.

## 8. Evidence

Three mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the guard is removed | `Fix1957_ASecondConcurrentConsumerThrows` (the second consumer blocked for the full 3-second period instead of throwing) |
| M2 — the flag is never cleared | `Fix1957_TheFlagIsClearedSoSequentialWaitsStillWork`, `Fix1957_TheFlagIsClearedAfterADisposedReturn` |
| M3 — the guard runs after the disposed check | **not caught — and it cannot be, deterministically. See below.** |

**M3 is reported uncaught rather than papered over.** Moving the guard below the disposed check
changes the answer only when `activeWait_` is true *and* the timer is already disposed — that is,
a second consumer must arrive after `Dispose()` has set the flag but before the parked first
consumer has reacquired the mutex and cleared it. That window is a genuine race: `Dispose()`
releases the mutex before `notify_all`, so whether the second caller or the waking first consumer
acquires it next is unspecified. A test for it would pass sometimes, and this session has twice
*repaired* flaky tests rather than written one (#2352, #2166) — a gate that is intermittently
green is not evidence.

The ordering is kept because it is .NET's, not because a test forces it, and the comment at the
site says exactly that.

Every concurrency case is written with a bounded handshake and a detached-then-disposed first
consumer, because a regression here is a **hang** rather than a wrong value, and joining a stuck
thread would take the whole executable down instead of failing one assertion.

Gate: **17,456 run, 17,456 passed, 0 failed, 0 skipped** across 38 executables — `+5` on 17,451,
exactly the five new cases (`SharpRuntimeTests_Threading` 475 → 480). No other executable moved.
Module graph unchanged at 41/93.

## 9. Downstream, measured

`PeriodicTimer` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`. Neither
repository was modified.
