<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — disposal is a real state across `System::Threading` (ticket #1956)

*2026-08-19.* Four types stop treating disposal as decoration. `Mutex`, `AutoResetEvent` and
`ManualResetEvent` refuse every operation after `Close()`; `ThreadLocal<T>::IsValueCreated`
refuses after `Dispose()`; `ReaderWriterLockSlim::Dispose()` refuses while a mode is held. One
member is **deliberately excluded** and keeps returning `false`.

**Calls that succeed today start throwing.** Read §2 before upgrading.

Landed under **SA-5**, which grants this ticket's approval question verbatim — *"including where
a call that succeeds today starts throwing"* — with SA-3's layout condition discharged: **every
affected `sizeof` is unchanged**, the flags landing in padding the types already had.

---

## 1. What was wrong, per finding

| Finding | Was | Measured symptom |
|---|---|---|
| **SR-AUD-208** | `Mutex/AutoResetEvent/ManualResetEvent::Close()` were **empty bodies** | `Close()` then `WaitOne(0)` returned **success** |
| **SR-AUD-219** | `ThreadLocal<T>::IsValueCreated` never checked disposal | a disposed instance answered **`false`** — indistinguishable from "alive, no value yet" |
| **SR-AUD-203** | `ReaderWriterLockSlim::Dispose()` set the flag unconditionally | disposing **with a lock held** succeeded, leaving the holder owning a mode on a disposed object |
| *(T-G/timer)* | `ITimer::Change` returned `true` unconditionally | a **disposed** timer reported it had been rescheduled |

The three `Close()` bodies are the worst of these, because the headers **already claimed** that
`Close` "closes the handle". The documentation and the behaviour disagreed, so either the comment
was false or the API was decorative.

## 2. What changes

| Call | Was | Is |
|---|---|---|
| `Mutex::WaitOne()` / `WaitOne(ms)` / `ReleaseMutex()` after `Close()` | succeeded | `ObjectDisposedException` |
| `AutoResetEvent`/`ManualResetEvent` `Set`/`Reset`/`WaitOne` after `Close()` | succeeded | `ObjectDisposedException` |
| `Close()` twice | no-op | no-op — **idempotent, unchanged** |
| `ThreadLocal<T>::getIsValueCreatedProperty()` after `Dispose()` | `false` | `ObjectDisposedException` |
| `ReaderWriterLockSlim::Dispose()` with a mode held | succeeded | `SynchronizationLockException` |
| `ReaderWriterLockSlim::Dispose()` with nothing held | succeeded | succeeded — unchanged, and still idempotent |
| **`ITimer::Change` after `Dispose()`** | `true` | **`false` — and still does not throw** |

## 3. The one member excluded, and why that is not an inconsistency

`ITimer::Change` must **not** throw. .NET's `Timer.Change` opens

```csharp
if (_canceled)
{
    return false;                       // Timer.cs:539-542
}
```

A `false` **return** is the documented `ITimer` contract. Making it throw for symmetry with the
wait handles would contradict the interface this type implements — so the design record excluded
it, and the reference confirms the exclusion. The asymmetry is **pinned by a test**
(`Decl1956_ITimerChangeReturnsFalseAndDoesNotThrow`) rather than left to look like an oversight,
so a later "consistency" pass cannot quietly make it throw.

The flag lives in the `.cpp`-local `SystemTimeProviderTimer`, so no public type gained a member
for it. `Dispose()` became idempotent there too.

## 4. `Mutex` overrides `Dispose()` rather than shadowing `Close()`

.NET's arrangement is `public virtual void Close() => Dispose();` (`WaitHandle.cs:87`). This port
already had `WaitHandle::Close() { Dispose(); }`, but `Mutex` **shadowed** it with an empty
`Close()`. The repair removes the shadow and overrides `Dispose()` instead, so both spellings —
and a call through a `WaitHandle&` — reach the same guard. `m.Close()` is unaffected as a
spelling; a test covers all three routes.

## 5. One deliberate narrowing, stated rather than glossed

.NET's `ReaderWriterLockSlim.Dispose` performs **two** checks, in this order
(`ReaderWriterLockSlim.cs:1250-1258`):

```csharp
if (WaitingReadCount > 0 || WaitingUpgradeCount > 0 || WaitingWriteCount > 0) throw ...;
if (IsReadLockHeld || IsUpgradeableReadLockHeld || IsWriteLockHeld)          throw ...;
```

**The design record named only the second, and this port implements only the second.** The first
needs per-mode *waiter* counts, of which this port has only `waitingWriters_` (added the same day
by SR-AUD-204); counting waiting readers and upgraders is additional state on three more paths.
That is filed as **#2389**.

The narrowing is a strict **subset** of .NET's: this port never refuses a disposal .NET would
accept. That direction matters — the opposite would break callers .NET supports.

## 6. Evidence

Six mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — `Mutex::Dispose` does not set the flag | `Fix1956_AClosedMutexRefusesEveryOperation`, `Fix1956_MutexCloseReachesTheOverriddenDispose` |
| M2 — `AutoResetEvent::Set` loses its guard | `Fix1956_AClosedAutoResetEventRefusesEveryOperation` |
| M3 — `ThreadLocal::IsValueCreated` loses its guard | `Fix1956_ADisposedThreadLocalRefusesIsValueCreated` |
| M4 — `ReaderWriterLockSlim::Dispose` ignores a held mode | `Fix1956_DisposingAHeldReaderWriterLockThrows` |
| M5 — `ITimer::Change` claims success after disposal | `Decl1956_ITimerChangeReturnsFalseAndDoesNotThrow` |
| M6 — `ITimer::Change` throws instead of returning `false` | the same test — **this is the mutation the exclusion exists to stop** |

M6 is the reason that pin is worth having: without it, "make disposal consistent" would look like
an improvement and would break the `ITimer` contract.

Gate: **17,476 run, 17,476 passed, 0 failed, 0 skipped** across 38 executables — `+9` on 17,467,
exactly the nine new cases (`SharpRuntimeTests_Threading` 491 → 500). No other executable moved.
All 491 pre-existing cases passed unchanged before the new ones were added. Module graph
unchanged at 41/93.

M6 was invalid as first written (a missing include, not a behaviour) and was reformulated rather
than counted.

## 7. Downstream, measured

All six affected types measured separately rather than assumed to match: `Mutex`,
`AutoResetEvent`, `ManualResetEvent`, `ThreadLocal`, `ReaderWriterLockSlim` and `ITimer` each
appear in **zero** places in `cna` and **zero** in `mobile-eggbert`. Neither repository was
modified.
