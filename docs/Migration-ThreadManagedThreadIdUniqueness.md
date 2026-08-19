<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — every thread now has a distinct `ManagedThreadId` (ticket #1958, SR-AUD-193)

*2026-08-19.* `System::Threading::Thread::CurrentThread().getManagedThreadIdProperty()` returned
**1** from every thread that was not created through a `System::Threading::Thread` object. The
main thread reported 1, and so did every raw `std::thread`, every thread-pool worker, and every
thread `std::async` created — all of them the same number, all of them colliding with the main
thread and with each other.

Landed under `docs/StandingApprovals.md` **SA-5** (aligning to the reference is ordinary work).
**No public signature, object layout, vtable or `noexcept` change**, so consumers need no rebuild
and no source edit. The id lives in a `thread_local`.

---

## 1. What changed

| Caller | Was | Is |
|---|---|---|
| the main thread | `1` | `1` — unchanged, but now by **identity** (§3) |
| a raw `std::thread` | `1` | a distinct id, `>= 2` |
| a `std::async` / pool worker | `1` | a distinct id, `>= 2` |
| the same thread, asked twice | `1`, `1` | the same id both times |
| two different external threads | `1`, `1` | two different ids |
| inside a `Thread` object's own body | that object's id | unchanged |
| `someThread.getManagedThreadIdProperty()` | that object's id | unchanged |

Anything that already reported a real id keeps reporting exactly the same one. The change is
confined to threads that used to fall through to the hard-coded `1`.

## 2. Why

.NET's contract is stated in `Thread.cs` and is unconditional:

> `ManagedThreadId` … "a unique identifier for the current managed thread"

and its `ManagedThreadId` is read from the runtime's own per-thread block, which every thread has
— managed, native-entered, or thread-pool. There is no path in .NET that returns a shared
constant. A caller that keys a map, a lock-ownership record, a re-entrancy guard or a log column
on this id was, in this port, keying all of them on the single value `1`.

The old body said as much:

```cpp
return currentThreadState_ ? currentThreadState_->managedThreadId : 1;
```

`currentThreadState_` is only set by this port's own `Thread::Start`, so the `: 1` arm was every
other thread in the process.

## 3. The main thread keeps 1, and how it keeps it is the interesting part

The naive repair — hand out `1` to whichever thread asks first, then `2, 3, …` — is wrong, and
silently so. Nothing guarantees the main thread asks first; a worker started during static
initialisation, or a logging thread that stamps its id before `main` runs, would take `1` and the
main thread would become an ordinary numbered worker. The failure is intermittent, which is worse
than consistent.

The id is therefore assigned by **identity**:

```cpp
inline static const std::thread::id mainThreadOsId_ = std::this_thread::get_id();
```

That initialiser runs during static initialisation, which runs on the main thread, so
`mainThreadOsId_` is the main thread's OS id no matter who asks first afterwards.

## 4. One counter, not two

External threads draw from `nextManagedId_` — the **same** counter `Thread`'s constructor uses.
That is not an implementation detail: uniqueness is across *all* threads, not within each kind. A
separate counter for external threads would give each kind internally distinct ids and still let
the two kinds collide with each other, which is the whole defect in a different costume.

This is the mutation that survived the first three tests (§6) and needed a case of its own.

## 5. Assigned on first use

A thread that never reads the property costs nothing — no counter increment, no allocation. The
`thread_local` starts at `0`, which is not a legal managed id, and is filled in on the first
call. So the id sequence reflects the order in which threads *asked*, not the order in which they
started; .NET's does the same, and no documented behaviour depends on the numeric order.

## 6. Evidence

Four mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — external threads report `1` again | `Fix1958_193_ExternalThreadsGetDistinctIds` |
| M2 — the id is reassigned on every call | `Fix1958_193_TheIdIsStableWithinAThread` |
| M3 — the main thread loses its `1` | `Fix1958_193_ExternalThreadsGetDistinctIds` |
| M4 — external ids come from a **separate** counter | `Fix1958_193_ExternalAndWrapperIdsShareOneCounter` |

**M4 is worth recording in full, because the obvious test for it does not work.** Collecting four
wrapper ids and four external ids and asserting the set has eight members *passes* against the
mutation: two counters only collide where their ranges overlap, and in a test binary that has
already created dozens of threads a freshly-started second counter sits far below the shared one,
so the ids come out distinct — for the wrong reason. The assertion that discriminates is
**ordering**: take a `Thread` object's id, then an external id assigned after it, and require the
external one to be larger. That holds under one counter and fails under two, on the first run,
with no dependence on how many threads ran before.

Gate: **17,420 run, 17,420 passed, 0 failed, 0 skipped** across 38 executables — `+4` on 17,416,
exactly the four cases added to `SharpRuntimeTests_Threading` (471 → 475). No other executable's
count moved.

## 7. Downstream, measured

Per SA-2 condition 5 (recorded here although SA-2 is not the approval this landed under, since a
behaviour change deserves the same measurement): `getManagedThreadIdProperty` appears in **zero**
places in `cna` and **zero** in `mobile-eggbert`. Neither repository was modified.

Any future consumer that used the id as a *thread-kind* discriminator — treating `1` as "not one
of ours" — must stop; that reading was never what the property meant, and it no longer works.

## 8. What #1958 still covers

Ticket #1958 remains **blocked** for its other members, each of which needs an approval this one
did not: SR-AUD-209 is a vtable/base-class change, SR-AUD-194 a public signature change,
SR-AUD-196 removes public constructors, and SR-AUD-220 needs storage on a public template.

Two members listed in #1958's description are **already closed and the description is stale**:
SR-AUD-214 and SR-AUD-189 were both landed by ticket #1971 on 2026-08-03. SR-AUD-215 was
deliberately excluded by #1971 with a measured reason — `Capture()` returns `nullptr`
unconditionally, so rejecting null would break every reachable call.
