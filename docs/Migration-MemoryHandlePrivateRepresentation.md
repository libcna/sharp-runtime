<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `MemoryHandle`'s representation is private, and the destructor is declined (ticket #2059, SR-AUD-088)

*2026-08-19.* `System::Buffers::MemoryHandle`'s two data members are now `private`, matching
.NET. The RAII destructor the ticket proposed was measured against the reference and **declined**.

Landed under `docs/StandingApprovals.md` **SA-8** (public representation where .NET's is private
→ match .NET and migrate the first-party sites), with SA-2's five conditions discharged for the
source break. **No layout change** — `sizeof(MemoryHandle)` is 24 before and after — so no
consumer needs a rebuild.

---

## 1. The finding's premise does not survive the reference

SR-AUD-088 is titled *"MemoryHandle documents RAII cleanup but never unpins at scope exit"*, and
#2059 was opened to add `~MemoryHandle(){ Dispose(); }`.

.NET's type is:

```csharp
public unsafe struct MemoryHandle : IDisposable   // MemoryHandle.cs:12
{
    private void*      _pointer;
    private GCHandle   _handle;
    private IPinnable? _pinnable;
    ...
}
```

A `struct` with **no finalizer**. Scope exit does not unpin in .NET either. `using var handle =
memory.Pin();` is a *language* construct that calls `Dispose()`; it is not something the type does
for you.

So what SR-AUD-088 actually found was a **doc-comment that promised something the type never
did** — *"should call Dispose() explicitly (or let the destructor do it)"*. That promise was the
defect, and an earlier ticket had already removed it. The behaviour it described was never wrong.

**Adding the destructor would therefore be a divergence from .NET, not a repair**, and #2059
declines it. The ticket's own hazard analysis is the second, independent reason and it stands:
this is a copyable handle, so an unpinning destructor would unpin **once per copy** for a single
pin.

`Dispose()` needed no change either — it already matches `MemoryHandle.cs:41-53` statement for
statement: unpin, clear the `IPinnable`, null the pointer, so a second call does nothing.

## 2. What did change

The divergence the ticket never named. .NET's three fields are all `private` and it publishes
exactly one of them, `Pointer`, as a getter. This port published **both** of its two as mutable
data members.

| | Was | Is |
|---|---|---|
| `pointer_` | public, mutable | **private** |
| `pinnable_` | public, mutable | **private** |
| `getPointerProperty()` | public getter | unchanged |
| both constructors | public | unchanged |
| `Dispose()` | public, idempotent | unchanged |
| copyability | copyable | unchanged |
| `sizeof(MemoryHandle)` | **24** | **24** |
| a destructor | absent | **still absent, now for a stated reason** |

.NET's third field, `private GCHandle _handle`, stays deliberately absent here: this runtime has
no moving collector, so there is no GC handle to free.

## 3. Why it matters

`pinnable_` is the dangerous one, and it fails **silently**:

```cpp
MemoryHandle handle = buffer.Pin(0);
handle.pinnable_ = nullptr;   // used to compile
handle.Dispose();             // now a no-op -- the pin leaks, with no diagnostic anywhere
```

`pointer_` is the same shape one level down: a caller could retarget a live handle at an
unrelated address and then dispose a handle whose pointer no longer described what was pinned.

## 4. To migrate

| Was | Now |
|---|---|
| `h.pointer_` (read) | `h.getPointerProperty()` |
| `h.pointer_ = p` | construct a new handle: `h = MemoryHandle(p, pinnable)` |
| `h.pinnable_` (read or write) | **no replacement, by design** — .NET exposes no accessor either; to release the pin, call `h.Dispose()` |

**Zero first-party sites needed migrating** — measured across all of `modules/` and `test/`, there
were no direct accesses to either member outside the type itself. The five construction sites all
go through the public constructors and are untouched.

## 5. Evidence

Five mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — both members public again | `MemoryHandlePinTests.TheRepresentationIsPrivateAsInDotNet` (compile time) |
| M2 — only `pinnable_` public again | the same test's second `static_assert` (compile time) |
| M3 — add the destructor the ticket proposed | `MemoryHandlePinTests.ScopeExitDoesNotUnpin` |
| M4 — `Dispose()` stops clearing `pinnable_` | `MemoryHandlePinTests.ExplicitDisposeUnpinsExactlyOnce` |
| M5 — `Dispose()` leaves a dangling pointer | that test **and** `MemoryHandleTests.Dispose_ClearsPointer` |

The absence pins use a **dependent** parameter, because gcc evaluates a non-dependent `requires`
eagerly and hard-errors on the access instead of yielding `false` — the #2299 trap.

Negative consumer fixture: `test/consumer/buffers_memoryhandle_private_negative.cpp`, four sites,
all rejected. The fixture set grows to **38 fixtures / 204 sites**. Its site 3 is the detach that
breaks silently rather than loudly.

## 6. Downstream, measured

Per SA-2 condition 5: `MemoryHandle` appears in **zero** places in `cna` and **zero** in
`mobile-eggbert`. Neither repository was modified, and no downstream ticket is needed.
