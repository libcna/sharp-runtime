<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration: `AutoResetEvent` and `ManualResetEvent` are `WaitHandle`s (#1958 / SR-AUD-209)

**Landed:** 2026-08-19, branch `next`. **Ticket:** #1958, finding SR-AUD-209. **This closes #1958.**

## What changed

Both types had **no base class and no vtable**, and each carried its own mutex, condition
variable and signalled flag — a third and fourth copy of logic `EventWaitHandle` already had.
They are now what .NET declares:

```cpp
class AutoResetEvent   final : public EventWaitHandle { explicit AutoResetEvent(bool); };
class ManualResetEvent final : public EventWaitHandle { explicit ManualResetEvent(bool); };
```

That is not a paraphrase of the reference — it *is* the reference. .NET's
`AutoResetEvent.cs` and `ManualResetEvent.cs` are nine lines each, and their entire bodies are

```csharp
public sealed class AutoResetEvent : EventWaitHandle
{
    public AutoResetEvent(bool initialState) : base(initialState, EventResetMode.AutoReset) { }
}
```

Neither declares a member of its own. `Set`, `Reset`, `WaitOne`, `Close` and `Dispose` are all
inherited.

## Why it mattered

Because they were not `WaitHandle`s, `WaitHandle::WaitAll` and `WaitHandle::WaitAny` **could not
accept them at all** — not "returned the wrong answer", the code did not compile. Those entry
points were repaired by #1952 and documented ever since. SR-AUD-209 was the one divergence in
`System::Threading` that left a documented API unusable.

## Two things the finding did not name, both required

**1. `EventWaitHandle` had no closed state.** #1956 gave `Mutex`, `AutoResetEvent` and
`ManualResetEvent` a `closed_` flag so that `Close()` really closes; `EventWaitHandle` was its
fourth case and was missed, so `Close()` there reached `WaitHandle`'s **empty** `Dispose()` and did
nothing. Deriving the two events without fixing that would have **silently reverted #1956 for both
of them**. The guard now lives in `EventWaitHandle` and the derived types inherit it, which is
where .NET puts it too (`WaitHandle.cs:87-98,118`).

**2. `EventWaitHandle::Set()` lost wakeups.** It stored and notified **without holding `mtx_`**,
so a waiter that had evaluated the predicate as false but had not yet atomically released the lock
and slept missed the notification and blocked until some later `Set()`. `AutoResetEvent::Set()`
took the lock, so deriving it would have **introduced** the race into a type that did not have it.

Measured with `build-probe/2209_probe1_lost_wakeup.cpp` over 900 single-waiter rounds:
`EventWaitHandle` lost **2**, `AutoResetEvent` lost **0**. This is the type six `cna` data members
hold **by value**, all of them for async completion — precisely the shape a lost wakeup hangs.

## Layout — consumers must rebuild

| type | before | after |
|---|---|---|
| `AutoResetEvent` | 96 | **112** |
| `ManualResetEvent` | 96 | **112** |
| `EventWaitHandle` | 104 | **112** |

`EventWaitHandle`'s growth was **asserted at 104 first and the build rejected it**: #1956's flags
fitted into existing padding on three other types and that expectation was carried over rather
than measured. It does not fit here.

This is a vtable and base-class change (SA-3 excludes it; the user approved it directly on
2026-08-19) plus an ABI change on a type `cna` holds by value in six places, so **every consumer
must rebuild**. The layout pin asserts the *relationship* — both events must be exactly
`sizeof(EventWaitHandle)` — as well as the absolute figures, so it says "these declare no members
of their own" rather than merely "these are 112 bytes today".

## Source changes

- **`WaitOne()` returns `bool`**, not `void`. Nothing that compiled stops compiling — ignoring a
  returned value is legal — so this is a widening at every call site.
- **The `initialState` parameter has no default.** .NET's has none; the default this port
  published had **zero** call sites across `modules/`, `test/` and both downstream consumers.
- Both classes are `final`, matching `sealed`.

## Downstream

Measured 2026-08-19: **zero** `AutoResetEvent` and **zero** `ManualResetEvent` sites in `cna` and
`mobile-eggbert`. All 14 downstream hits are `EventWaitHandle`, six of them data members by value
(`StorageDevice.cpp`, `NetworkSession.hpp`, `Gamer.hpp`, `Guide.cpp`, `AvatarDescription.cpp`,
`LeaderboardReader.cpp`). Those need a **rebuild**, not an edit — and they gain both repairs
above, which is the substantive downstream effect of this ticket.

## Evidence, including what is not testable

Seven mutations. Six caught; **M6 — reverting `Set()` to store without the lock — is not caught,
and cannot be caught deterministically.** At roughly 0.2% loss per round, a bounded test would
detect it about a third of the time, and a test that is intermittently green is not evidence
(#2352). A first cut of the suite carried such a case at 200 rounds and it was **removed rather
than kept**, on the reasoning #1957/SR-AUD-201 and #2031 recorded for their own window-closing
mutations. A multi-waiter amplification was tried and is invalid: repeated `Set()` calls on an
AutoReset event coalesce into one signal, so the harness reported 100% "loss" for the **locked**
form too — it measures AutoReset semantics, not the race. The reasoning sits in the test file
where the case would have been.
