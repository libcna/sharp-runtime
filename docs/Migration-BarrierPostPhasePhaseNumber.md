<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a `Barrier` post-phase action can read the phase number (ticket #1957, SR-AUD-210)

*2026-08-19.* `System::Threading::Barrier::getCurrentPhaseNumberProperty()` no longer takes the
barrier's lock, so calling it from inside a post-phase action no longer self-deadlocks. The phase
increment moved to **after** the action, which is where .NET performs it.

Landed under **SA-5**, with SA-3's layout condition discharged as **layout-neutral**:
`sizeof(Barrier)` is **160 before and after**.

---

## 1. What was wrong

`FinishPhase()` invokes the post-phase action while **holding** `mutex_`, and
`getCurrentPhaseNumberProperty()` took that same non-recursive `std::mutex`. So:

```cpp
Barrier barrier(1, [](Barrier& b) {
    auto phase = b.getCurrentPhaseNumberProperty();   // deadlocked, permanently
});
barrier.SignalAndWait();
```

This is not a new discovery. Ticket #1955 fixed the sibling property
`getParticipantCountProperty()` in exactly this way and **named this one as the remaining case**,
in a comment that is still in the header.

## 2. What .NET does

`CurrentPhaseNumber` is a lock-free read of a plain field:

```csharp
public long CurrentPhaseNumber
{
    // use the new Volatile.Read/Write method because it is cheaper than Interlocked.Read
    get { return Volatile.Read(ref _currentPhase); }      // Barrier.cs:184-188
```

so the reference settles the design: the property must not take the barrier's lock. `phaseCount_`
becomes a `std::atomic<longcs>` and is read with `memory_order_acquire`, which is the same repair
#1955 applied to `participantCount_`.

## 3. The half the design record did not name

.NET increments the phase in `SetResetEvents`, which `FinishPhase` calls from its `finally` —
**after** the action has run, and on the throwing path too (`Barrier.cs:804-812, 834-836`).

This port incremented **first**. That was unobservable only because the property that would have
seen it deadlocked. **Fixing the deadlock alone would have shipped a newly reachable wrong answer
in place of a hang** — the action would have read the phase about to begin, where .NET reports the
one that is ending.

So both changes land together:

| Read from | Was | Is |
|---|---|---|
| inside the post-phase action | **deadlock** | the phase that is **ending** (0, 1, 2, …) |
| after `SignalAndWait()` returns | 1, 2, 3, … | 1, 2, 3, … — **unchanged** |
| after a **throwing** action | advanced | advanced — unchanged |

Nothing outside the action can see the difference: `mutex_` is held for the whole of
`FinishPhase`, so no waiter can run until it is released. The increment is still inside the
critical section, before `notify_all`.

## 4. The boundary that did not move

Only the two **read-only** properties are callable from the action.
`AddParticipant`, `RemoveParticipant`, `SignalAndWait` and `Dispose` all call
`ThrowIfCalledFromPostPhaseAction()` **before** taking the lock, so they throw
`InvalidOperationException` rather than deadlocking — matching .NET's `_actionCallerID` guard.
That is pinned by a test asserting all four still refuse.

## 5. Evidence

Mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the property takes the lock again | `Fix1957_ThePostPhaseActionCanReadThePhaseNumber` |
| M2 — the increment moves back before the action | `Fix1957_ThePostPhaseActionCanReadThePhaseNumber`, `Fix1957_ThePhaseAdvancesAfterTheActionNotBefore`, `Fix1957_TheOtherMembersStillRefuseReentrancy` |
| M3 — the phase advances only on the success path | `Fix1957_ThePhaseStillAdvancesWhenTheActionThrows` |

**M2 was invalid as first written and was reformulated rather than counted**: adding the
increment before the action without removing it afterwards is a *double* increment, not a move,
and it broke two pre-existing tests for the wrong reason.

**M3 was first reported "not caught", and that was a defect in the mutation harness, found by
checking rather than by believing it.** M3 makes a *pre-existing* multi-participant `BarrierTests`
case **hang** — waiters block on `phaseCount_ > myPhase` and the phase never advances — so the
whole-suite run hit its timeout and produced no `[  FAILED  ]` lines at all, which the harness
read as "no failures". Run on its own, `Fix1957_ThePhaseStillAdvancesWhenTheActionThrows` fails
in 0 ms. The mutation is therefore caught **by name**, and it is also caught as a hang; a harness
that only greps for failure lines cannot tell those two apart from a pass.

Every case that exercises the action runs its work on a **detached** thread behind a bounded
four-second handshake, because the pre-repair behaviour is a **hang**: joining a stuck thread
would take the whole executable down instead of failing one assertion. That is the same shape
#2341 used for `Monitor::Wait`.

`Fix1957_ThePhaseAdvancesAfterTheActionNotBefore` asserts the whole sequence across three phases
(`{0,1,2}` inside, `{1,2,3}` outside) rather than a single value, so an off-by-one cannot pass by
accident.

Gate: **17,462 run, 17,462 passed, 0 failed, 0 skipped** across 38 executables — `+6` on 17,456,
exactly the six new cases (`SharpRuntimeTests_Threading` 480 → 486). No other executable moved.
Module graph unchanged at 41/93.

## 6. Scope

This is the **third** of ticket #1957's four members. SR-AUD-202 (`Monitor::Wait` recursion)
landed as #2341; SR-AUD-201 (`PeriodicTimer` single-consumer) landed earlier today. **SR-AUD-204**
(`ReaderWriterLockSlim` writer preference) remains, and #1957 stays open for it — it introduces
writer preference, a fairness change that alters which workloads block, and the plan's §12
requires a ThreadSanitizer run for it specifically.

## 7. Downstream, measured

`System::Threading::Barrier` appears in **zero** places in `cna` and **zero** in
`mobile-eggbert`. Neither repository was modified.
