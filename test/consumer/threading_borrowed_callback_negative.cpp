// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1959 (SR-AUD-187 and SR-AUD-221, CCF-019).
//
// Two `System::Threading` entry points handed out ownership they did not have,
// and #1959 makes both of them take a share instead. Each is a PUBLIC SOURCE
// BREAK, landed under `docs/StandingApprovals.md` SA-2, and this fixture is
// SA-2's condition 2: one site per outlawed spelling, so that "the old call no
// longer compiles" is verified by the compiler rather than asserted in prose.
//
//   SR-AUD-187 — `ThreadPool::UnsafeQueueUserWorkItem` captured a BORROWED raw
//     `IThreadPoolWorkItem*` in a DETACHED lambda and called `Execute()` on it
//     with no ownership, join or retention. The audit's probe queued a heap
//     item, waited until `Execute` had entered, deleted it, and let `Execute`
//     touch a member: heap-use-after-free.
//
//   SR-AUD-221 — `SynchronizationContext::SetSynchronizationContext` stored a
//     NON-OWNING raw pointer in a `thread_local` slot with no destruction or
//     reset hook, so `Current` outlived its target: set it to a stack-derived
//     context, leave the scope, call `Current->Send`, and ASan reports
//     stack-use-after-scope at the virtual call.
//
// .NET has neither hazard, and for one reason in both cases: the queue entry and
// the thread-static field are GC references that keep their target alive. C++
// has no such mechanism, and a detached thread has no owner whose destructor
// could wait, so the boundary #2134 gave `Socket` is not available here. Holding
// a share IS the boundary.
//
// The `#else` branches carry the migrated spelling, which is what a caller
// writes from now on: wrap the object in a `std::shared_ptr`.
//
// NEGATIVE-FIXTURE: component=Threading
#include <atomic>
#include <memory>

#include "System/Threading/IThreadPoolWorkItem.hpp"
#include "System/Threading/SynchronizationContext.hpp"
#include "System/Threading/ThreadPool.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Threading::IThreadPoolWorkItem;
using System::Threading::SynchronizationContext;
using System::Threading::ThreadPool;

namespace {

/// The shape the audit's probe used: a work item whose `Execute()` touches a member, so a
/// borrowed pointer to a destroyed instance is a read of freed storage rather than a no-op.
class CountingWorkItem final : public IThreadPoolWorkItem {
public:
    explicit CountingWorkItem(std::atomic<int>* counter) : counter_(counter) {}
    void Execute() override { counter_->fetch_add(1); }

private:
    std::atomic<int>* counter_;
};

} // namespace

void ExerciseBorrowedCallbackOwnership() {
    std::atomic<int> counter{0};

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(threadpool-unsafequeue-raw-pointer): cannot convert
    //     | invalid conversion
    //     | no matching function for call
    CountingWorkItem borrowed(&counter);
    ThreadPool::UnsafeQueueUserWorkItem(&borrowed, false);
#else
    auto owned = std::make_shared<CountingWorkItem>(&counter);
    ThreadPool::UnsafeQueueUserWorkItem(owned, false);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(synchronizationcontext-set-raw-pointer): cannot convert
    //     | invalid conversion
    //     | no matching function for call
    SynchronizationContext borrowedContext;
    SynchronizationContext::SetSynchronizationContext(&borrowedContext);
#else
    auto ownedContext = std::make_shared<SynchronizationContext>();
    SynchronizationContext::SetSynchronizationContext(ownedContext);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // The property's RETURN type moved too, so code that stored it in a raw pointer stops
    // compiling as well -- and that is worth its own site, because a caller could migrate the
    // setter and leave this behind.
    // NEGATIVE(synchronizationcontext-current-raw-pointer): cannot convert
    //     | invalid conversion
    //     | conversion from
    SynchronizationContext* current = SynchronizationContext::getCurrentProperty();
    (void)current;
#else
    std::shared_ptr<SynchronizationContext> current = SynchronizationContext::getCurrentProperty();
    (void)current;
#endif

    // Clearing the slot still takes a null pointer literal, on both sides of the break: this is
    // the one spelling that did NOT have to change, and leaving it outside every site is what
    // says so.
    SynchronizationContext::SetSynchronizationContext(nullptr);
}
