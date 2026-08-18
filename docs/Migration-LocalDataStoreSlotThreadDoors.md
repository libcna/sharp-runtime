<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — thread-local data slots are real, and reached only through `Thread` (ticket #2298)

*2026-08-18.* `System::LocalDataStoreSlot` held **one `std::any` shared by every thread**, had a
public constructor, and this repository had **no `Thread` slot API at all**.

Landed under `docs/StandingApprovals.md` **SA-9**, which authorised the new `Thread` surface
explicitly, with SA-2's five conditions. Route **B** of the ticket's four.

---

## 1. What was wrong

Three things, and the middle one is the serious one:

* **There was no door.** .NET's constructor is `internal` (`LocalDataStoreSlot.cs:11`) — a public
  caller never names it. This port's was public, and the `getData`/`setData` pair beside it had no
  .NET counterpart, so the whole surface was project-owned, wearing a .NET name.
* **One value, shared by every thread.** A write from any thread replaced what every other thread
  read — the opposite of what the name promises. Two threads touching one slot with at least one
  write was a **data race**.
* **No synchronization policy**, and none implied.

## 2. What it is now

`System::Threading::Thread` gains .NET's six doors (`Thread.cs:502-507`):

| | |
|---|---|
| `Thread::AllocateDataSlot()` | a new unnamed slot |
| `Thread::AllocateNamedDataSlot(name)` | a new named slot; **throws** if the name exists |
| `Thread::GetNamedDataSlot(name)` | **get-or-create** — never throws for an unknown name |
| `Thread::FreeNamedDataSlot(name)` | removes the **name**; a slot the caller holds survives |
| `Thread::GetData(slot)` | the **calling thread's** value |
| `Thread::SetData(slot, value)` | the **calling thread's** value |

`LocalDataStoreSlot`'s constructor is private with `Thread` as its only friend, so those doors are
the only way to obtain one. It stays **copyable**, because .NET's is a reference passed around
freely, and two copies name the same storage.

The three named doors have **three different contracts**, and conflating them is the easy mistake:
`AllocateNamedDataSlot` uses `Dictionary.Add` and therefore throws on a duplicate
(`Thread.cs:679-688`), while `GetNamedDataSlot` is get-or-create (`:690-702`).

`FreeNamedDataSlot` removes the name and no more — .NET's does the same, dropping the map's
reference and leaving the rest to the GC. Here the slot stays valid and keeps its per-thread
values; only the *name* stops resolving to it, and a later `GetNamedDataSlot` allocates a **new**
slot for that name. A test asserts exactly that.

## 3. Why the slot holds an id rather than the storage

.NET's slot holds a `ThreadLocal<object?>`. `LocalDataStoreSlot` lives in `Core.Base` and `Thread`
lives in `modules/threading`, **which depends on `Core.Base`** — so a slot holding a thread-local
would invert the module graph. The slot carries an opaque id and `Thread` keeps a `thread_local`
map. The observable contract is identical either way, and the graph is unchanged at **41 modules /
92 edges**.

## 4. To migrate

```cpp
// before
System::LocalDataStoreSlot slot;
slot.setData(std::string("x"));
auto v = slot.getData();

// after
const auto slot = System::Threading::Thread::AllocateDataSlot();
System::Threading::Thread::SetData(slot, std::string("x"));
auto v = System::Threading::Thread::GetData(slot);
```

If you relied on the old **shared** value — one thread writing and another reading — that was a
data race, and it no longer works by design. Use a `Threading::ThreadLocal<T>` for per-thread
state, or an ordinary shared object with your own lock for shared state.

## 5. Evidence, and one honest limit

Four mutations, **all caught**. The first is the finding itself: dropping `thread_local` from the
store fails two tests outright, including one that runs a real worker thread and checks in both
directions that neither thread's value leaks into the other.

**ThreadSanitizer was attempted and could not run.** `build-tsan` fails to compile
`SharpRuntimeTests_Threading` on a **pre-existing** limitation unrelated to this change —
`atomic_thread_fence is not supported with -fsanitize=thread` in libstdc++'s `atomic_base.h`. So
the per-thread isolation is evidenced behaviourally, by a live two-thread test, rather than by a
race detector; that is stated rather than implied.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `LocalDataStoreSlot` — **zero sites in both**. Neither
repository was modified.
