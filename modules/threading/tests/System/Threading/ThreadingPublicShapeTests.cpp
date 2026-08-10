// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for cause T-H -- "the public shape itself diverges" --
// docs/ThreadingNamespaceReviewPlan.md §5 and §20.3.
//
// Ticket #1971 landed the two members of #1958's Group A that were verified compatible by
// measurement: SR-AUD-214 (AsyncLocal<T> must commit the new value before notifying) and
// SR-AUD-189 (ThreadPool's configuration setters must validate and store). The third member
// §20.3 listed, SR-AUD-215, was EXCLUDED after that verification and stays with the blocked
// #1958; see the ExecutionContext section at the bottom of this file, which pins the current
// contract so the exclusion is visible rather than merely recorded in a document.
#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Threading/AsyncLocal.hpp"
#include "System/Threading/AsyncLocalValueChangedArgs.hpp"
#include "System/Threading/ExecutionContext.hpp"
#include "System/Threading/ThreadPool.hpp"

using namespace System::Threading;
using SharpRuntime::intcs;

// ---------------------------------------------------------------------------------------------
// SR-AUD-214 -- AsyncLocal<T> commits before it notifies
// ---------------------------------------------------------------------------------------------

TEST(AsyncLocalNotificationOrderTests, HandlerReadingTheProperty_SeesTheNewValue) {
    AsyncLocal<int>* self = nullptr;
    int seenThroughProperty = -1;
    int seenAsArgument = -1;
    int previousAsArgument = -1;

    AsyncLocal<int> local([&](const AsyncLocalValueChangedArgs<int>& args) {
        seenAsArgument = args.getCurrentValueProperty();
        previousAsArgument = args.getPreviousValueProperty();
        seenThroughProperty = self->getValueProperty();
    });
    self = &local;

    local.setValueProperty(5);

    EXPECT_EQ(seenAsArgument, 5);
    EXPECT_EQ(previousAsArgument, 0);
    // The defect: this used to be 0 while the argument said 5.
    EXPECT_EQ(seenThroughProperty, 5);
    EXPECT_EQ(local.getValueProperty(), 5);
}

// A reentrant write from inside the handler used to be silently overwritten by the outer
// assignment that ran afterwards.
TEST(AsyncLocalNotificationOrderTests, ReentrantWriteFromHandler_IsNotOverwritten) {
    AsyncLocal<int>* self = nullptr;
    int depth = 0;
    AsyncLocal<int> local([&](const AsyncLocalValueChangedArgs<int>&) {
        if (depth == 0) {
            ++depth;
            self->setValueProperty(99);
        }
    });
    self = &local;

    local.setValueProperty(5);
    EXPECT_EQ(local.getValueProperty(), 99);
    EXPECT_EQ(depth, 1);
}

// The equal-value no-op is .NET's own contract and must survive the reordering untouched.
TEST(AsyncLocalNotificationOrderTests, EqualAssignment_IsStillACompleteNoOp) {
    int calls = 0;
    AsyncLocal<int> local([&](const AsyncLocalValueChangedArgs<int>&) { ++calls; });

    local.setValueProperty(7);
    EXPECT_EQ(calls, 1);
    local.setValueProperty(7);
    EXPECT_EQ(calls, 1) << "assigning the same value must not notify";
    EXPECT_EQ(local.getValueProperty(), 7);

    local.setValueProperty(0); // back to the default value -- still a change
    EXPECT_EQ(calls, 2);
}

TEST(AsyncLocalNotificationOrderTests, NoHandler_StillCommits) {
    AsyncLocal<int> local;
    local.setValueProperty(3);
    EXPECT_EQ(local.getValueProperty(), 3);
}

// Each thread keeps its own slot; the reordering must not leak a value across threads.
TEST(AsyncLocalNotificationOrderTests, ValuesStayPerThread) {
    AsyncLocal<int> local;
    local.setValueProperty(1);

    std::atomic<int> observedOnOtherThread{-1};
    std::thread other([&] {
        observedOnOtherThread.store(local.getValueProperty());
        local.setValueProperty(42);
    });
    other.join();

    EXPECT_EQ(observedOnOtherThread.load(), 0);
    EXPECT_EQ(local.getValueProperty(), 1);
}

// ---------------------------------------------------------------------------------------------
// SR-AUD-189 -- ThreadPool's configuration setters validate and store
//
// The configuration is process-global, so every test here restores what it found.
// ---------------------------------------------------------------------------------------------

namespace {

    class ThreadPoolConfigurationTest : public ::testing::Test {
    protected:
        void SetUp() override {
            ThreadPool::GetMinThreads(savedMinWorker_, savedMinCompletionPort_);
            ThreadPool::GetMaxThreads(savedMaxWorker_, savedMaxCompletionPort_);
        }
        void TearDown() override {
            // Lower the minimum first, so raising or lowering the maximum can never be refused
            // by the consistency rule while restoring.
            ThreadPool::SetMinThreads(0, 0);
            ThreadPool::SetMaxThreads(savedMaxWorker_, savedMaxCompletionPort_);
            ThreadPool::SetMinThreads(savedMinWorker_, savedMinCompletionPort_);
        }

        intcs savedMinWorker_ = 0, savedMinCompletionPort_ = 0;
        intcs savedMaxWorker_ = 0, savedMaxCompletionPort_ = 0;
    };

} // namespace

TEST_F(ThreadPoolConfigurationTest, SetMaxThreads_StoresAndIsObservable) {
    EXPECT_TRUE(ThreadPool::SetMaxThreads(64, 48));
    intcs worker = 0, completionPort = 0;
    ThreadPool::GetMaxThreads(worker, completionPort);
    EXPECT_EQ(worker, 64);
    EXPECT_EQ(completionPort, 48);
}

TEST_F(ThreadPoolConfigurationTest, SetMinThreads_StoresAndIsObservable) {
    ASSERT_TRUE(ThreadPool::SetMaxThreads(64, 64));
    EXPECT_TRUE(ThreadPool::SetMinThreads(7, 9));
    intcs worker = 0, completionPort = 0;
    ThreadPool::GetMinThreads(worker, completionPort);
    EXPECT_EQ(worker, 7);
    EXPECT_EQ(completionPort, 9);
}

// Measured against the audit's managed probe: a negative minimum is rejected.
TEST_F(ThreadPoolConfigurationTest, SetMinThreads_Negative_IsRejectedAndStoresNothing) {
    ASSERT_TRUE(ThreadPool::SetMaxThreads(64, 64));
    ASSERT_TRUE(ThreadPool::SetMinThreads(4, 4));

    EXPECT_FALSE(ThreadPool::SetMinThreads(-1, -1));
    EXPECT_FALSE(ThreadPool::SetMinThreads(-1, 4));
    EXPECT_FALSE(ThreadPool::SetMinThreads(4, -1));

    intcs worker = 0, completionPort = 0;
    ThreadPool::GetMinThreads(worker, completionPort);
    EXPECT_EQ(worker, 4);
    EXPECT_EQ(completionPort, 4);
}

TEST_F(ThreadPoolConfigurationTest, SetMaxThreads_BelowOne_IsRejectedAndStoresNothing) {
    ASSERT_TRUE(ThreadPool::SetMaxThreads(32, 32));

    EXPECT_FALSE(ThreadPool::SetMaxThreads(0, 0));
    EXPECT_FALSE(ThreadPool::SetMaxThreads(-5, 32));
    EXPECT_FALSE(ThreadPool::SetMaxThreads(32, 0));

    intcs worker = 0, completionPort = 0;
    ThreadPool::GetMaxThreads(worker, completionPort);
    EXPECT_EQ(worker, 32);
    EXPECT_EQ(completionPort, 32);
}

// .NET's documented minimum/maximum consistency rule, in both directions.
TEST_F(ThreadPoolConfigurationTest, MinimumAboveMaximum_IsRejected) {
    ASSERT_TRUE(ThreadPool::SetMinThreads(0, 0));
    ASSERT_TRUE(ThreadPool::SetMaxThreads(8, 8));
    EXPECT_FALSE(ThreadPool::SetMinThreads(9, 8));
    EXPECT_FALSE(ThreadPool::SetMinThreads(8, 9));
    EXPECT_TRUE(ThreadPool::SetMinThreads(8, 8)) << "equal to the maximum is allowed";
}

TEST_F(ThreadPoolConfigurationTest, MaximumBelowMinimum_IsRejected) {
    ASSERT_TRUE(ThreadPool::SetMaxThreads(64, 64));
    ASSERT_TRUE(ThreadPool::SetMinThreads(10, 10));
    EXPECT_FALSE(ThreadPool::SetMaxThreads(9, 64));
    EXPECT_FALSE(ThreadPool::SetMaxThreads(64, 9));
    EXPECT_TRUE(ThreadPool::SetMaxThreads(10, 10)) << "equal to the minimum is allowed";
}

// Zero is accepted as a MINIMUM (the pool starts with no threads) but not as a MAXIMUM. Neither
// was measured against .NET; the asymmetry is reasoned and pinned so it is a decision rather
// than an accident. See ThreadPool.hpp's own doc-comment for the provenance of every rule.
TEST_F(ThreadPoolConfigurationTest, ZeroIsAValidMinimumButNotAValidMaximum) {
    ASSERT_TRUE(ThreadPool::SetMaxThreads(4, 4));
    EXPECT_TRUE(ThreadPool::SetMinThreads(0, 0));
    intcs worker = -1, completionPort = -1;
    ThreadPool::GetMinThreads(worker, completionPort);
    EXPECT_EQ(worker, 0);
    EXPECT_EQ(completionPort, 0);
    EXPECT_FALSE(ThreadPool::SetMaxThreads(0, 0));
}

// The defaults are what the pre-#1971 GetMinThreads/GetMaxThreads reported, so a program that
// never configures the pool sees no change at all.
TEST_F(ThreadPoolConfigurationTest, DefaultsAreUnchangedAndConsistent) {
    // Restore the process defaults this fixture saved, then read them back.
    ThreadPool::SetMinThreads(0, 0);
    ASSERT_TRUE(ThreadPool::SetMaxThreads(savedMaxWorker_, savedMaxCompletionPort_));
    ASSERT_TRUE(ThreadPool::SetMinThreads(savedMinWorker_, savedMinCompletionPort_));

    intcs minWorker = 0, minCompletionPort = 0, maxWorker = 0, maxCompletionPort = 0;
    ThreadPool::GetMinThreads(minWorker, minCompletionPort);
    ThreadPool::GetMaxThreads(maxWorker, maxCompletionPort);
    EXPECT_GE(minWorker, 0);
    EXPECT_GE(minCompletionPort, 0);
    EXPECT_GE(maxWorker, minWorker);
    EXPECT_GE(maxCompletionPort, minCompletionPort);
    EXPECT_GE(maxWorker, 1);
}

// The configuration is shared process-global state, so the setters must be safe to call
// concurrently; the invariant min <= max must hold at every observation.
TEST_F(ThreadPoolConfigurationTest, ConcurrentConfiguration_KeepsTheInvariant) {
    ASSERT_TRUE(ThreadPool::SetMinThreads(0, 0));
    ASSERT_TRUE(ThreadPool::SetMaxThreads(64, 64));

    std::atomic<bool> violated{false};
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            while (!go.load(std::memory_order_acquire)) { }
            for (int i = 0; i < 200; ++i) {
                ThreadPool::SetMinThreads(t + 1, t + 1);
                ThreadPool::SetMaxThreads(32 + t, 32 + t);
                intcs minWorker = 0, minCompletionPort = 0, maxWorker = 0, maxCompletionPort = 0;
                ThreadPool::GetMinThreads(minWorker, minCompletionPort);
                ThreadPool::GetMaxThreads(maxWorker, maxCompletionPort);
                if (minWorker > maxWorker || minCompletionPort > maxCompletionPort) {
                    violated.store(true);
                }
            }
        });
    }
    go.store(true, std::memory_order_release);
    for (auto& thread : threads) thread.join();
    EXPECT_FALSE(violated.load()) << "min exceeded max under concurrent configuration";
}

// ---------------------------------------------------------------------------------------------
// SR-AUD-215 -- NOT repaired here; this pins why, so the exclusion is testable
//
// #1958's Group A listed ExecutionContext::Run rejecting a null context as compatible. Measured
// (build-probe/1971_probe1_group_a.cpp): the default constructor is private, Capture() returns
// nullptr unconditionally, and CreateCopy() is a non-static member needing an instance -- so a
// consumer has NO way to obtain a non-null ExecutionContext*. Rejecting null would therefore
// make Run throw for every call that can be written, including the canonical
// Run(Capture(), callback, state) that works today. The finding stays with the blocked #1958.
// ---------------------------------------------------------------------------------------------

TEST(ExecutionContextExclusionTests, NoReachableWayToObtainANonNullContext_SeeTicket1958) {
    EXPECT_EQ(ExecutionContext::Capture(), nullptr);
    EXPECT_FALSE(std::is_default_constructible<ExecutionContext>::value)
        << "if this becomes true, SR-AUD-215's exclusion must be re-examined";
}

TEST(ExecutionContextExclusionTests, RunWithCapturedContext_StillInvokes_SeeTicket1958) {
    bool invoked = false;
    ExecutionContext::Run(ExecutionContext::Capture(), [&](void*) { invoked = true; }, nullptr);
    EXPECT_TRUE(invoked)
        << "the only ExecutionContext::Run call a consumer can write must keep working until "
           "#1958's SR-AUD-215 decision is taken";
}
