// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2298 (SR-AUD-129), route B.
//
// This suite MOVED here from modules/core/tests, and the move is the finding in miniature: a
// LocalDataStoreSlot cannot be exercised without Thread, because Thread is the only door to it --
// which is exactly what .NET's internal constructor arranges and what this port did not have.
//
// Before #2298 the type had a public constructor and a getData/setData pair, and the single
// std::any it held was ONE VALUE SHARED BY EVERY THREAD: a write from any thread replaced what
// every other thread read, and two threads touching one slot with at least one write was a data
// race. That is the opposite of what the name promises.
#include <gtest/gtest.h>

#include <any>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/LocalDataStoreSlot.hpp"
#include "System/Threading/Thread.hpp"

using System::LocalDataStoreSlot;
using System::Threading::Thread;

TEST(LocalDataStoreSlotTests, Fix2298_ThereIsNoPublicDoorButThread) {
    // .NET's constructor is internal (`LocalDataStoreSlot.cs:11`), so a public caller never names
    // it. The port's was public, which is what made the whole surface project-owned.
    static_assert(!std::is_default_constructible_v<LocalDataStoreSlot>,
                  "#2298: the constructor is private; Thread::AllocateDataSlot is the door");
    static_assert(!std::is_constructible_v<LocalDataStoreSlot, std::size_t>,
                  "#2298: not even the id-taking constructor is reachable");

    // A slot is a HANDLE and stays copyable, because .NET's is a reference passed around freely.
    static_assert(std::is_copy_constructible_v<LocalDataStoreSlot>, "a slot is a handle");
    const LocalDataStoreSlot slot = Thread::AllocateDataSlot();
    const LocalDataStoreSlot copy = slot;
    EXPECT_EQ(slot, copy) << "two copies must name the same storage";
}

TEST(LocalDataStoreSlotTests, Fix2298_EachThreadSeesItsOwnValue) {
    // THE HEADLINE REPAIR. Before #2298 this could not be written at all: every thread shared one
    // std::any, so the worker's write would have been visible here.
    const LocalDataStoreSlot slot = Thread::AllocateDataSlot();
    Thread::SetData(slot, std::string("main-thread"));

    std::string observedInWorker = "<unset>";
    bool workerSawItsOwn = false;
    std::thread worker([&] {
        // The worker starts with NOTHING in the slot -- not with the main thread's value.
        observedInWorker = Thread::GetData(slot).has_value() ? "<leaked>" : "<empty>";
        Thread::SetData(slot, std::string("worker-thread"));
        workerSawItsOwn =
            std::any_cast<std::string>(Thread::GetData(slot)) == "worker-thread";
    });
    worker.join();

    EXPECT_EQ("<empty>", observedInWorker) << "the main thread's value must not leak across";
    EXPECT_TRUE(workerSawItsOwn);
    EXPECT_EQ("main-thread", std::any_cast<std::string>(Thread::GetData(slot)))
        << "and the worker's write must not have overwritten this thread's value";
}

TEST(LocalDataStoreSlotTests, Fix2298_AnUnsetSlotReadsAsEmptyRatherThanThrowing) {
    const LocalDataStoreSlot slot = Thread::AllocateDataSlot();
    EXPECT_FALSE(Thread::GetData(slot).has_value());
    Thread::SetData(slot, 42);
    EXPECT_EQ(42, std::any_cast<int>(Thread::GetData(slot)));
}

TEST(LocalDataStoreSlotTests, Fix2298_TwoSlotsAreIndependent) {
    const LocalDataStoreSlot a = Thread::AllocateDataSlot();
    const LocalDataStoreSlot b = Thread::AllocateDataSlot();
    EXPECT_NE(a, b);
    Thread::SetData(a, std::string("A"));
    Thread::SetData(b, std::string("B"));
    EXPECT_EQ("A", std::any_cast<std::string>(Thread::GetData(a)));
    EXPECT_EQ("B", std::any_cast<std::string>(Thread::GetData(b)));
}

TEST(LocalDataStoreSlotTests, Fix2298_NamedSlotsFollowDotNetsThreeDoors) {
    // .NET has THREE named doors with different contracts, and conflating them is the easy
    // mistake: AllocateNamedDataSlot uses Dictionary.Add and THROWS on a duplicate name
    // (`Thread.cs:679-688`), while GetNamedDataSlot is get-or-create and never throws
    // (`:690-702`).
    const std::string name = "sharp-runtime-2298-slot";
    Thread::FreeNamedDataSlot(name);   // leave no residue from a previous run

    const LocalDataStoreSlot allocated = Thread::AllocateNamedDataSlot(name);
    EXPECT_THROW((void)Thread::AllocateNamedDataSlot(name), System::ArgumentException)
        << "AllocateNamedDataSlot is not get-or-create";
    EXPECT_EQ(allocated, Thread::GetNamedDataSlot(name))
        << "GetNamedDataSlot returns the SAME slot for a name already allocated";

    // FreeNamedDataSlot removes the NAME, and .NET's does no more than that either: a slot the
    // caller still holds stays valid and keeps its values.
    Thread::SetData(allocated, std::string("still here"));
    Thread::FreeNamedDataSlot(name);
    EXPECT_EQ("still here", std::any_cast<std::string>(Thread::GetData(allocated)))
        << "freeing the NAME must not destroy a slot the caller holds";

    // ...and the name now resolves to a DIFFERENT slot, because get-or-create allocates a new one.
    EXPECT_NE(allocated, Thread::GetNamedDataSlot(name));
    Thread::FreeNamedDataSlot(name);

    // Freeing an unknown name is a no-op, as in .NET.
    EXPECT_NO_THROW(Thread::FreeNamedDataSlot("sharp-runtime-2298-never-allocated"));
}

TEST(LocalDataStoreSlotTests, Fix2298_ANamedSlotIsStillPerThread) {
    // The named map is process-wide -- as .NET's static dictionary is -- but the VALUES are not.
    const std::string name = "sharp-runtime-2298-named-per-thread";
    Thread::FreeNamedDataSlot(name);
    const LocalDataStoreSlot slot = Thread::AllocateNamedDataSlot(name);
    Thread::SetData(slot, std::string("main"));

    bool workerFoundTheSameSlot = false;
    bool workerSawNoValue = false;
    std::thread worker([&] {
        const LocalDataStoreSlot fromName = Thread::GetNamedDataSlot(name);
        workerFoundTheSameSlot = (fromName == slot);
        workerSawNoValue = !Thread::GetData(fromName).has_value();
    });
    worker.join();

    EXPECT_TRUE(workerFoundTheSameSlot) << "the NAME is shared";
    EXPECT_TRUE(workerSawNoValue) << "...but the VALUE is not";
    Thread::FreeNamedDataSlot(name);
}
