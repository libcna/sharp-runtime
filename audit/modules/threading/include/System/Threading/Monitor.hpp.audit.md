# Audit: `modules/threading/include/System/Threading/Monitor.hpp`

## Metadata

- AUDITED: 187-line registry-backed Monitor implementation, fully read.
- Validation: complete Threading tests passed 359/359 on 2026-07-27; an
  isolated reentrant Wait/Pulse probe was run with a two-second timeout.

## SR-AUD-202 — high — Monitor.Wait deadlocks when the caller holds a recursive monitor more than once

`Wait` adopts the recursive mutex and calls `onReleasing()` once before
condition-variable wait. With two Enter calls, one recursive level remains
locked, so the signaling thread cannot Enter/Pulse. The isolated child enters
twice, waits, then attempts Enter/Pulse from a second thread; it reaches the
two-second timeout (exit 124) rather than completing. .NET Monitor.Wait fully
releases and restores recursive ownership.

## Assessment

The pointer registry provides real ordinary mutual exclusion and the audited
fixture covers depth-one wait/pulse behavior. The header itself documents the
depth-one limitation, but a public Monitor operation that deadlocks valid
recursive ownership is still a high contract failure.

## Other missing assertions and diagnostics

- Tests omit reentrant Wait (SR-AUD-202), null object arguments, invalid
  negative timed waits/TryEnter values, timed pulse races, registry growth and
  pointer-address reuse after object destruction.
- The process-lifetime registry retains a State for every distinct pointer;
  no bounded-cache, reclamation, or long-running allocation diagnostic exists.

## Final assessment

SR-AUD-202 is confirmed by the bounded deadlock probe. No source or test was
changed.


---

## Remediation record — ticket #2341 (2026-08-12), SR-AUD-202 → `remediated`

Cause **T-E/2** of `docs/ThreadingNamespaceReviewPlan.md`, "synchronisation state machines
are incomplete". Split out of the blocked design ticket **#1957**, which keeps SR-AUD-201,
SR-AUD-204 and SR-AUD-210 and **stays blocked**.

### The defect, reproduced

`build-probe/2341_probe1_monitor_depth.cpp` enters the monitor `n` times, waits, and has a
second thread `Enter`/`PulseAll`/`Exit` once the waiter has published that it is waiting.
Every scenario is time-boxed at two seconds, so a deadlock is reported rather than suffered.

| scenario | before | after |
|---|---|---|
| `monitor.wait_depth1` | `ok` | `ok` |
| `monitor.wait_depth2` | **`TIMEOUT`** | `ok` |
| `monitor.wait_depth3` | **`TIMEOUT`** | `ok` |
| `monitor.wait_depth3_restore` | **`TIMEOUT`** | `ok` |
| `monitor.wait_depth3_exits_and_foreign` | `-1` (never reached) | `31` |

`31` encodes "three `Exit` calls succeeded **and** a foreign thread could then take the
monitor", i.e. the depth was restored to exactly three and released to exactly zero.

### The repair

Both `Wait` overloads now forward to one private `WaitCore`.
`std::condition_variable_any::wait` releases the lock it is given exactly once, which for a
`std::recursive_timed_mutex` is one level; the levels beyond the first are therefore released
before the wait and reacquired after it, and the registry's `owner`/`depth` pair is written to
its absolute values rather than stepped by one. .NET's `Monitor.Wait` releases the whole
recursive count and restores it on reacquisition, which is what this now matches.

No exception path guards the restoration, deliberately: both `wait` and `wait_for` reacquire
the caller's lock from a destructor whose postcondition is "lock is held", so a failure to
reacquire calls `std::terminate` instead of unwinding through `WaitCore`. An earlier draft had
a `catch (...)` there; it was removed because no test can reach it.

### Compatibility

`Monitor` is a static-only class (`Monitor() = delete`) whose per-pointer `State` lives in a
private static registry, so there is **no public object layout to change**; `WaitCore` is a
new *private static* member. No public signature, vtable, `noexcept` or exception contract
changes, and the only behavioural transition is that a call which previously never returned
now returns. §9 approval question 2 governs new private state fields in
`ReaderWriterLockSlim` and `PeriodicTimer` and does not reach this member;
`docs/ThreadingNamespaceReviewPlan.md` §20.2 item 1 had already recorded that this member
"could be split out to land without approval if the depth bookkeeping is proven exact".

### Tests

`modules/threading/tests/System/Threading/ThreadingMonitorRecursionTests.cpp`, 7 tests:
depth 2 and depth 3 complete; depth 3 restores exactly three levels and a fourth `Exit` throws
`SynchronizationLockException`; the timed overload releases the full depth while it waits and
still returns `false` on timeout; depth 1, the not-held throws and the timeout return value are
unaffected controls. Each case runs on a **detached** worker with a four-second budget, because
joining a deadlocked thread would hang the executable instead of failing one assertion, and
every helper-thread body is exception-guarded so a bookkeeping regression fails a test rather
than calling `std::terminate`.

### Mutations

| # | mutation | outcome |
|---|---|---|
| M1 | `extra = 0` (the pre-#2341 single-level release) | **killed** — 4 tests fail |
| M2c | restore `depth - 1` instead of `depth` | **killed** — 4 tests fail |
| M3 | drop the pre-wait `depth.store(0)` | **killed** — the pre-existing `Monitor_AnotherThread_CanEnter_WhileFirstThreadIsWaiting` aborts |
| M4 | drop the pre-wait `owner.store({})` | **equivalent, survived** — the zero depth alone already makes the next `Enter()` claim ownership; the store is kept because it keeps the registry invariant "owner is empty whenever depth is zero" true throughout, and the code says so |

### ThreadSanitizer

`build-probe/2341_probe2_monitor_tsan.cpp` (19 `__tsan_*` symbols): **0 reports before, 0
after**, 200 rounds both ways. Reported honestly — this probe is **not** a liveness
discriminator. It uses the *timed* `Wait` overload, so before the repair every round fell out
on its own 200 ms timeout instead of deadlocking, and the run took **41 s** against **1 s**
after. The liveness evidence is probe 1's untimed scenarios; TSan's contribution here is only
that hand-unlocking and relocking a recursive mutex around the wait introduced no data race.
