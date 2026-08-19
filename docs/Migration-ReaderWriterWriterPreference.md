<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a waiting writer now blocks new readers (ticket #1957, SR-AUD-204)

*2026-08-19.* `System::Threading::ReaderWriterLockSlim` gives writers precedence: once a writer is
waiting, a **new** reader waits behind it instead of walking straight in.

**This is a fairness change. A reader-heavy workload that never blocked can now block.** That is
the point — it is what stops the writer starving — and it is .NET's documented behaviour. Read §3
before upgrading.

Landed under **SA-5**, with SA-3's layout condition discharged as **layout-neutral**:
`sizeof(ReaderWriterLockSlim)` is **120 before and after**.

---

## 1. What was wrong

The read-admission predicate was `!writerActive_` — it asked only whether a writer *held* the
lock, never whether one was *waiting for* it. So:

1. reader A holds the lock;
2. writer W blocks in `TryEnterWriteLock`, waiting for `readers_ == 0`;
3. reader B arrives — and was admitted immediately;
4. reader C arrives — admitted;
5. …and `readers_` never reaches zero, so **W waits for ever**.

Nothing bounded that. A steady arrival of readers starved the writer indefinitely.

## 2. What .NET does

.NET keeps the same signal, packed into its single `_owners` word:

```csharp
private const uint WRITER_HELD     = 0x80000000;
private const uint WAITING_WRITERS = 0x40000000;
private const uint WAITING_UPGRADER= 0x20000000;
private const uint MAX_READER      = 0x10000000 - 2;
...
// Setting these bits will prevent new readers from getting in.
if (_numWriteWaiters == 1)        SetWritersWaiting();      // ReaderWriterLockSlim.cs:1005-1010
if (_numWriteUpgradeWaiters == 1) SetUpgraderWaiting();
```

Both waiting bits sit **above** `MAX_READER`, so .NET's single admission test
`if (_owners < MAX_READER)` refuses a new reader whenever a writer holds the lock **or** is
waiting for it. One comparison, two conditions.

This port keeps its state in named fields rather than one packed word, so the bit becomes a
counter, `waitingWriters_`, and the predicate gains one term.

**Both kinds of writer count**, exactly as both .NET bits do: a plain writer *and* an
upgrade-to-write. Counting only plain writers would leave the upgrade path starvable, which is
the easy half to miss — a test pins it.

**A writer that times out stops blocking readers.** .NET clears its bit in `WaitOnEvent`'s
`finally` (`ReaderWriterLockSlim.cs:1039-1042`); here an RAII guard decrements on every exit —
acquired, timed out, or thrown — and wakes the readers it was holding back. A guard that
decremented only on success would wedge every future reader, and that failure mode is
**permanent**, not transient.

## 3. What changes for a caller

| Situation | Was | Is |
|---|---|---|
| no writer waiting | reader enters | reader enters — **unchanged** |
| a writer is waiting | reader enters, writer starves | reader **waits** |
| an upgrader is waiting for the write lock | reader enters | reader **waits** |
| the waiting writer times out | — | readers flow again immediately |
| a thread re-entering a lock it already holds | unaffected | unaffected (§4) |

`TryEnterReadLock(timeout)` can now return `false` where it used to return `true`. If you were
relying on readers always winning, you were relying on the starvation this removes.

## 4. Writer preference cannot deadlock a recursive reader

Every path that returns *before* the predicate is untouched:

* a thread that already holds a **read** lock returns early (or throws, under `NoRecursion`);
* a thread holding the **write** or **upgrade** lock has implied read access and returns early.

So only a genuinely **new** reader can be delayed — which is precisely .NET's contract.

## 5. Evidence

Two mutations, **both caught**:

| Mutation | Caught by |
|---|---|
| M1 — the read predicate ignores waiting writers | `Fix1957_ANewReaderWaitsBehindABlockedWriter`, `Fix1957_AnUpgraderWaitingForWriteAlsoBlocksNewReaders` |
| M2 — the guard decrements only on success | `Fix1957_ANewReaderWaitsBehindABlockedWriter`, `Fix1957_ATimedOutWriterStopsBlockingReaders` |

**Every probing reader runs on its own thread**, and that is not incidental: the default
`LockRecursionPolicy` is `NoRecursion`, so a second read acquisition on the thread that already
holds one throws `LockRecursionException` rather than testing admission. An earlier draft did
exactly that and took the executable down with `terminate called without an active exception`,
because the escaping exception left two `std::thread`s joinable.

Gate: **17,467 run, 17,467 passed, 0 failed, 0 skipped** across 38 executables — `+5` on 17,462,
exactly the five new cases (`SharpRuntimeTests_Threading` 486 → 491). No other executable moved.
Module graph unchanged at 41/93.

## 6. ThreadSanitizer

`docs/ThreadingNamespaceReviewPlan.md` §12 requires TSan for this member specifically. Three
things are worth stating precisely, because two of them are limitations.

**The full test target cannot be built under TSan, and that is pre-existing.**
`Thread::MemoryBarrier()` is `std::atomic_thread_fence`, which gcc rejects outright:
*"'atomic_thread_fence' is not supported with '-fsanitize=thread' [-Werror=tsan]"*. It is reached
from an unrelated test file and has nothing to do with this change —
`ReaderWriterLockSlim.hpp` does not include `Thread.hpp` at all. This is the same incompatibility
#2298 recorded.

**So the evidence is a focused probe** (`build-probe/1957_probe4_rwls_tsan.cpp`): four readers,
two writers and an upgrader hammering the repaired type for three seconds. Result:

```
reads=1020489 refusedReads=0 writes=6747 shared=6747     TSan: clean, no reports
```

**And the probe was shown capable of reporting**, per the plan's §19.4 rule that a silent
sanitizer is evidence about the probe until proven otherwise: the same probe with one deliberate
unsynchronised counter added produces **2** `WARNING: ThreadSanitizer: data race` reports. The
clean result above is therefore meaningful.

**What the probe does NOT show, stated rather than implied**: it does not reproduce the
starvation. Run against the *pre-repair* predicate it still reports `writes=6089` — its writers
use a timed `TryEnterWriteLock` and its readers sleep, so `readers_` reaches zero often enough
for writers to get in anyway. Reproducing starvation needs readers that overlap *continuously*,
which is what the deterministic unit test does by holding one reader open across the writer's
arrival. The probe's job here is the race question; the gtest's is the fairness question.

## 7. Downstream, measured

`ReaderWriterLockSlim` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`.
Neither repository was modified.
