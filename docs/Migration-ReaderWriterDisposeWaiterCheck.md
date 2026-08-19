<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ReaderWriterLockSlim::Dispose` also refuses a lock with waiters (ticket #2389)

*2026-08-19.* `Dispose()` now throws `SynchronizationLockException` when any thread is **waiting**
to acquire the lock, not only when the calling thread **holds** a mode. Both checks are .NET's,
in .NET's order.

**`sizeof(ReaderWriterLockSlim)` grows 120 → 128, so consumers must be recompiled.** Landed under
**SA-3** (private data members, `sizeof` pinned) and **SA-5** (behaviour derived from the
reference).

---

## 1. What this completes

.NET's `Dispose(bool)` performs **two** checks (`ReaderWriterLockSlim.cs:1250-1258`):

```csharp
if (WaitingReadCount > 0 || WaitingUpgradeCount > 0 || WaitingWriteCount > 0)
    throw new SynchronizationLockException(SR.SynchronizationLockException_IncorrectDispose);

if (IsReadLockHeld || IsUpgradeableReadLockHeld || IsWriteLockHeld)
    throw new SynchronizationLockException(SR.SynchronizationLockException_IncorrectDispose);
```

Ticket #1956 landed the **second** — its design record named only that one, so the omission was a
gap in the record rather than a shortcut taken while implementing it. The first needed per-mode
waiter counts. #1957/SR-AUD-204 added `waitingWriters_` for a different purpose (writer
preference); this ticket adds `waitingReaders_` and `waitingUpgraders_` and wires all three into
`Dispose`.

## 2. What changes

| `Dispose()` called while… | Was | Is |
|---|---|---|
| nothing held, nobody waiting | succeeds | succeeds — unchanged, still idempotent |
| the calling thread holds a mode | throws (#1956) | throws — unchanged |
| **another thread is waiting for a read lock** | **succeeded** | `SynchronizationLockException` |
| **another thread is waiting for the write lock** | **succeeded** | `SynchronizationLockException` |
| **another thread is waiting to upgrade** | **succeeded** | `SynchronizationLockException` |
| a waiter that has since **timed out** | — | succeeds; the counters come back down |

## 3. The new counters affect no admission decision

Only *writer*-waiting influences who may enter, which is SR-AUD-204's rule and .NET's.
`waitingReaders_` and `waitingUpgraders_` are consulted by `Dispose` **and nothing else** — their
guards are constructed with `notifyOnLast = false` precisely so they cannot perturb wake-up
ordering. That is stated at the site.

All three use the same RAII guard, so a waiter that **times out or throws** stops being counted.
Without that, a lock that ever had a waiter could never be disposed again — a permanent failure,
not a transient one, and a test pins it.

## 4. The order is transcribed, not chosen

.NET tests **waiters first**. Both arms raise the same message in this port, so which one fires
is currently *unobservable* — the ordering is nonetheless .NET's, and a test constructs the case
where both conditions hold so that if the messages ever diverge the ordering is already covered
rather than newly at risk.

## 5. Layout

| | Was | Is |
|---|---|---|
| `sizeof(ReaderWriterLockSlim)` | **120** | **128** |
| `alignof` | 8 | 8 |

SR-AUD-204's single counter was layout-neutral — it landed in padding the type already had. These
two did not fit, so this is a real object-layout change: **every consumer must be recompiled.**
No source change is needed. The pin was updated in place rather than duplicated, and it now names
both tickets so a third counter cannot arrive unnoticed.

## 6. Evidence

Four mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the waiter check is removed | all three waiting-kind tests |
| M2 — only writers are counted | `Fix2389_DisposingWithAWaitingReaderThrows`, `Fix2389_DisposingWithAWaitingUpgraderThrows` |
| M3 — the reader guard never decrements | `Fix2389_OnceTheWaiterGivesUpDisposalSucceeds`, `Decl2389_TheHeldModeCheckStillFiresOnItsOwn`, and #1956's held-mode test |
| M4 — the upgrader is not counted | `Fix2389_DisposingWithAWaitingUpgraderThrows` |

**A second process note, and it is the more useful one.** With the layout gate fixed and the
baseline green, M1 and M2 came back **NOT CAUGHT** — a real defect in the tests, not in the
repair. Every case had the **disposing thread also holding a mode**, so the *held-mode* check
fired and the waiter check was never the one that threw. The cases now use a third thread to hold
the lock, a second to wait for it, and dispose from a thread holding **nothing** — which is the
only shape where the waiter check can be observed at all. All four mutations are caught against
that.

**One process note, and it is the reason these verdicts are trustworthy.** The first mutation run
reported all four as "caught" — by `ThreadingSharedStateTests.RepairedTypes_LayoutUnchanged`,
a **pre-existing** layout gate that asserts `sizeof(ReaderWriterLockSlim) == 120`. That gate was
failing on the **unmutated** build too, because of the 120 → 128 growth, so every run had a
failure regardless of the mutation and none of the four results meant anything. The gate was
updated to 128 first, a green baseline confirmed (506 cases), and only then were the mutations
re-run. A mutation verdict is only evidence against a baseline that passes.

That gate doing its job is also the evidence that 128 is a **real** change rather than drift: it
stayed green through SR-AUD-204's earlier counter, which landed in existing padding.

**A third process note: the first robust-looking version still flaked.** With the tests
restructured so the disposer holds nothing, they passed in isolation and then **failed inside the
full gate** — a fixed 150 ms settle is not a guarantee that the waiting thread has reached the
wait and been counted, and under gate load it had not. Tuning the sleep upward would have been
the wrong repair; this repository has twice *repaired* such a test rather than tuned it (#2352,
#2166).

The cases now rebuild the whole scenario on each of up to six attempts, with a growing settle,
and pass the moment `Dispose` refuses. That is **sound rather than merely tolerant**: the case can
only pass if the waiter check genuinely fires, so every mutation that removes or narrows it fails
all six attempts and is still caught by name. Verified green three consecutive times in isolation
and once through the full gate.

Gate: **17,482 run, 17,482 passed, 0 failed, 0 skipped** across 38 executables — `+6` on 17,476,
exactly the six new cases (`SharpRuntimeTests_Threading` 500 → 506). No other executable moved.
Module graph unchanged at 41/93.

## 7. Downstream, measured

`ReaderWriterLockSlim` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`, so
the rebuild requirement is recorded here for future consumers rather than acted on. Neither
repository was modified.
