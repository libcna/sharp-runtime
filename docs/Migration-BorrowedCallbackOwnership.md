<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — two `System::Threading` entry points now own what they were handed (ticket #1959)

*2026-08-17.* `ThreadPool::UnsafeQueueUserWorkItem` and
`SynchronizationContext::SetSynchronizationContext`/`getCurrentProperty` take and return
`std::shared_ptr` instead of a raw pointer.

**This is a public source break.** It landed under `docs/StandingApprovals.md` SA-2, with the
machinery that approval requires: this note, a `test/consumer/*_negative.cpp` site per outlawed
spelling, a downstream migration ticket, the full gate, and the measured downstream report in §5.

---

## 1. What was wrong

Both entry points accepted an object and then used it later, with no ownership of any kind
(SR-AUD-187 and SR-AUD-221, CCF-019).

`ThreadPool::UnsafeQueueUserWorkItem` captured a borrowed `IThreadPoolWorkItem*` in a **detached**
lambda and called `Execute()` on it. The audit's probe queued a heap item, waited until `Execute`
had entered, deleted it, and let `Execute` touch a member: **heap-use-after-free**.

`SynchronizationContext::SetSynchronizationContext` stored a non-owning raw pointer in a
`thread_local` slot with no destruction or reset hook, so `Current` outlived its target. Setting
it to a stack-derived context, leaving the scope and calling `Current->Send` reached a **virtual
call through freed storage** — ASan reported stack-use-after-scope.

.NET has neither hazard, and for the same reason in both cases: the queue entry and the
thread-static field are GC references that keep their target alive.

## 2. Why the answer is ownership rather than a waiting destructor

Tickets #2134, #2066 and #2088 gave the other CCF-019 async members a **liveness boundary**: the
owner's destructor waits for the work. That is not available here.

A detached thread has no owner whose destructor could wait for it, and a `thread_local` slot has
no destructor a caller controls either. Holding a share of the object **is** the boundary, and it
is the direct counterpart of the GC reference .NET uses.

## 3. What to change

```cpp
// ThreadPool
CountingWorkItem item(&counter);
ThreadPool::UnsafeQueueUserWorkItem(&item, false);            // was
auto item = std::make_shared<CountingWorkItem>(&counter);
ThreadPool::UnsafeQueueUserWorkItem(item, false);             // now

// SynchronizationContext
SynchronizationContext ctx;
SynchronizationContext::SetSynchronizationContext(&ctx);      // was
auto ctx = std::make_shared<SynchronizationContext>();
SynchronizationContext::SetSynchronizationContext(ctx);       // now

SynchronizationContext* current = SynchronizationContext::getCurrentProperty();               // was
std::shared_ptr<SynchronizationContext> current = SynchronizationContext::getCurrentProperty(); // now
```

Each of those three old spellings has its own site in
`test/consumer/threading_borrowed_callback_negative.cpp`, so none of them can quietly become
legal again.

## 4. What did not change

- `SetSynchronizationContext(nullptr)` still clears the slot, spelled exactly as before. It is
  deliberately left outside every negative site, because that is what says so.
- `UnsafeQueueUserWorkItem` still throws `ArgumentNullException("callBack")` for a null item, and
  still returns `true`, and still runs the item on a detached thread.
- No object layout, vtable or `noexcept` change anywhere; both are header-inline changes to a
  parameter and a return type.
- **Nothing leaks.** The queue's share is released with the detached thread's lambda once
  `Execute()` returns, and clearing the slot releases the context — both asserted by
  `std::weak_ptr::expired()` rather than assumed.

## 5. Downstream, measured

Per SA-2 condition 5, both consumer checkouts were searched for every affected spelling:
`UnsafeQueueUserWorkItem`, `SetSynchronizationContext` and `IThreadPoolWorkItem`. Neither `cna`
nor `mobile-eggbert` names any of them — **zero sites in both**. Neither repository was modified.
The migration ticket that records this is **#2353**.

First-party, the change touched two test call sites and nothing else: no production code in this
repository queues a work item or sets a synchronization context.
