<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ThreadLocal`'s `trackAllValues` now means something, and `Values` exists (ticket #1958, SR-AUD-220)

*2026-08-19.* `System::Threading::ThreadLocal<T>` gains `getValuesProperty()`, and the
`trackAllValues` constructor flag — previously accepted and **never read** — now controls it.

**`sizeof(ThreadLocal<int>)` grows 56 → 128, so consumers must be recompiled.** Landed under
**SA-5** (the behaviour is derived; the property is additive) with **SA-3**'s layout condition
discharged.

---

## 1. What was wrong

Two halves of one finding, and they are inseparable: the flag was **accepted and never read**, and
the type exposed **no `Values` property at all**. A caller who asked for tracking got a silent
no-op, and had no way to notice — the flag is only observable through the property it gates.

## 2. What .NET does

```csharp
public IList<T> Values
{
    get
    {
        if (!_trackAllValues)
        {
            throw new InvalidOperationException(SR.ThreadLocal_ValuesNotAvailable);
        }

        List<T>? list = GetValuesAsList();          // returns null if disposed
        ObjectDisposedException.ThrowIf(list is null, this);
        return list;
    }
}                                                    // ThreadLocal.cs:421-434
```

The message is transcribed verbatim from `Strings.resx`.

**The tracking check comes first, and that is observable**: a disposed instance built *without*
tracking reports `InvalidOperationException`, not `ObjectDisposedException`. The order is .NET's
and a test pins both directions.

## 3. Values outlive their threads — and that decided the design

`GetValuesAsList` walks the `ThreadLocal`'s **own** linked list of `LinkedSlot`s
(`ThreadLocal.cs:437-456, 584-598`), so a value survives the thread that created it and is
released when the `ThreadLocal` is disposed.

That rules out the plausible cheap design. The registry holds **strong** references, not
`weak_ptr` — a weak registry would silently drop a dead thread's value, which .NET does not do.
The per-thread storage therefore moved from `unique_ptr<T>` to `shared_ptr<T>`, so one value is
co-owned by the owning thread's map and the instance-wide registry.

**Co-ownership, not copying**, is also why an update is reflected rather than duplicated: the
setter writes through the shared object, so `Values` sees the new value without a second entry. A
registry that stored a copy at creation time would fail both of those — and that is mutation M5.

## 4. What changes

| | Was | Is |
|---|---|---|
| `getValuesProperty()` | **absent** | returns `std::vector<T>`, a snapshot |
| …without `trackAllValues` | — | `InvalidOperationException` |
| …when disposed **and** tracking | — | `ObjectDisposedException` |
| …when disposed **and not** tracking | — | `InvalidOperationException` — tracking is checked first |
| `trackAllValues` | accepted, ignored | controls the above |
| per-thread storage | `unique_ptr<T>` | `shared_ptr<T>` — internal, no API effect |
| `Dispose()` | cleared the thread's slot | also releases the registry, as .NET unlinks its slots |
| `sizeof(ThreadLocal<int>)` | **56** | **128** |

An instance built **without** tracking pays a mutex it never locks and nothing else — the
registry is only populated when the flag is set.

`std::vector<T>` by value is the return shape because .NET's `IList<T>` has no counterpart here,
and `GetValuesAsList` builds a fresh list on every call anyway; mutating the result cannot affect
the instance, which is true of .NET's copy too.

## 5. Evidence

Six mutations, **all caught** — but two only after the tests that were supposed to catch them
were found to be vacuous, and both defects are worth recording.

| Mutation | Caught by |
|---|---|
| M1 — the tracking check is removed | `Fix1958_ValuesThrowsWhenNotTracking` |
| M2 — the check order is inverted | `Decl1958_TheTrackingCheckPrecedesTheDisposedCheck` — **after repair**, below |
| M3 — the factory path is not tracked | `Fix1958_TheFactoryPathIsTrackedToo` |
| M4 — the setter path is not tracked | four cases |
| M5 — the registry stores a copy, not the pointer | `Fix1958_AnUpdatedValueIsReflectedNotDuplicated` |
| M6 — `Dispose` does not release the registry | `Fix1958_DisposeReleasesTheTrackedValues` — **after repair**, below |

**M2 could not be asserted with `EXPECT_THROW` at all.** `ObjectDisposedException` **derives
from** `InvalidOperationException`, here as in .NET, so `EXPECT_THROW(…, InvalidOperationException)`
passes whichever check fired and the inverted order went uncaught. The test now catches the
derived type first and requires the base one — the same trap #2152 recorded for the compression
streams.

**M6's release is unreachable through the public surface.** After `Dispose`, `Values` throws
whether or not the registry was cleared, so asserting that it throws proves nothing. The only
observable is *when* the values are destroyed — so the test creates the value on a worker thread
which then **exits**, leaving the registry as the sole owner, and counts destructor calls across
`Dispose`.

The factory and setter paths are tested separately because they are **two different creation
sites** — tracking one and not the other is the easy half-repair.

Gate: **17,490 run, 17,490 passed, 0 failed, 0 skipped** across 38 executables — `+8` on 17,482,
exactly the eight new cases (`SharpRuntimeTests_Threading` 506 → 514). No other executable moved.
All 506 pre-existing cases passed unchanged before the new ones were added. Module graph
unchanged at 41/93.

## 6. Downstream, measured

`ThreadLocal` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`, so the rebuild
requirement is recorded here for future consumers rather than acted on. Neither repository was
modified.

## 7. Scope

This is one of #1958's eight findings. SR-AUD-193 (`ManagedThreadId` uniqueness) landed earlier
today; SR-AUD-189 and SR-AUD-214 landed as #1971; SR-AUD-215 was excluded there with a measured
reason. **SR-AUD-209** (making the two events derive from `WaitHandle` — a vtable and base-class
change SA-3 excludes), **SR-AUD-194** (`Thread::Start(void*)` discards its parameter — a public
signature change) and **SR-AUD-196** (`ThreadStartException` publishes constructors .NET makes
internal) remain, and #1958 stays open for them.
