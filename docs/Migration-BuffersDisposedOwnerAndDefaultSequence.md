<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a disposed pool owner throws, and `default` is not `Empty` (tickets #2056, #2057)

*2026-08-17.* Two `System::Buffers` types gain the state .NET has. Both are object-layout
changes under `docs/StandingApprovals.md` SA-3, so **downstream consumers must be recompiled**.
No source change is needed.

`modules/buffers` is header-only and **no module outside it** includes either type, so the
repository-internal impact is nil.

---

## 1. #2056(a) — a disposed `IMemoryOwner` throws

| | Was | Is |
|---|---|---|
| `owner->getMemoryProperty()` after `Dispose()` | a **zero-length** `Memory` | `ObjectDisposedException` |
| a live `Rent(0)` | a zero-length `Memory` | **unchanged** — and now distinguishable from a disposed owner |
| `Dispose()` twice | idempotent | **unchanged** |
| `sizeof(MemoryPoolHeapOwner_<int>)` | 32 | **40** |

.NET's `ArrayMemoryPoolBuffer.Memory` does exactly this
(`ArrayMemoryPool.ArrayMemoryPoolBuffer.cs:18-25`).

**Why a flag and not .NET's own discriminator.** .NET needs no flag: it nulls `_array` and tests
`array is null`. The port's natural equivalent is `std::unique_ptr<std::vector<T>>`, which would
even make the object *smaller* — and it was rejected on purpose. `reset()` frees the storage
**deterministically**, where `clear() + shrink_to_fit()` is non-binding. When #2056 landed,
half (b) below had not yet received its final disposition, and turning the retained view from
"usually survives" into "always broken" while that contract was unsettled would have been a
practical regression dressed as parity. The storage lifetime was therefore left exactly as it
was, and a test says so. The 2026-08-22 disposition immediately below supersedes that interim
"open" state.

**Final disposition of half (b), 2026-08-22.** A `Memory<T>` obtained *before* `Dispose()` still
keeps a pointer and a length over storage the owner may have released. `Memory<T>` is explicitly
a non-owning C++ view, like `std::span`; its vector constructor already requires the backing
vector to outlive it. Sharp-runtime has no GC that can reproduce the managed array-reference
lifetime without replacing that public model across Core.Base. The final audit therefore records
this as an `accepted-deviation`, not as an open implementation defect. Callers must not retain or
dereference the view after `Dispose()` or owner destruction.

## 2. #2057 — `default` enumerates nothing, `Empty` enumerates one segment

| | Was | Is |
|---|---|---|
| `ReadOnlySequence<T>{}` enumerated | **1** segment | **0** |
| `ReadOnlySequence<T>::getEmpty()` enumerated | 1 segment | **1** |
| length, `IsEmpty`, `Start`, `End`, `IsSingleSegment` for either | — | **unchanged, and identical to each other** |
| any buffer-backed sequence, including `(nullptr, 0)` | 1 segment | **1** |
| `sizeof(ReadOnlySequence<int>)` | 32 | **40** |

.NET's `Empty` is `new ReadOnlySequence<T>(Array.Empty<T>())` (`ReadOnlySequence.cs:26`) — an
array-backed sequence with a non-null start object, so it yields one segment. `default` has a
null start object and `Enumerator.MoveNext` returns false immediately for it (`:642-647`).

A `std::vector` cannot express that difference: `default` and `Empty` both hold an empty one,
which is exactly why the review recorded that "distinguishing them needs state the type does not
have". The new member is the port's `_startObject != null`.

Note the trap the tests cover: `(nullptr, 0)` is a **valid buffer-backed** sequence with no
elements, and must still enumerate one segment. Only the default constructor yields none.

## 3. To migrate

* If you called `getMemoryProperty()` on a disposed owner expecting an empty `Memory`, catch
  `ObjectDisposedException` or stop calling it. .NET has always thrown there.
* If you relied on a default-constructed `ReadOnlySequence<T>` yielding one empty segment, use
  `getEmpty()`, which still does.
* Rebuild any consumer: both types grew.

## 4. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `System::Buffers` — **zero sites in both**. Neither
repository was modified.
