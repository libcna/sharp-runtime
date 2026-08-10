// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <functional>
#include <string>
#include <vector>
#include "System/Progress.hpp"
#include "System/ArgumentNullException.hpp"

using System::Progress;

TEST(ProgressTest, DefaultCtorDoesNotThrow) {
    Progress<int> p;
    EXPECT_NO_THROW(p.Report(42));
}

TEST(ProgressTest, HandlerCtorInvokesHandler) {
    int received = -1;
    Progress<int> p([&](int v) { received = v; });
    p.Report(99);
    EXPECT_EQ(received, 99);
}

TEST(ProgressTest, AddProgressChangedHandlerInvoked) {
    int received = 0;
    Progress<int> p;
    p.addProgressChangedHandler([&](int v) { received = v; });
    p.Report(7);
    EXPECT_EQ(received, 7);
}

TEST(ProgressTest, MultipleHandlersAllInvoked) {
    int a = 0, b = 0;
    Progress<int> p;
    p.addProgressChangedHandler([&](int v) { a = v; });
    p.addProgressChangedHandler([&](int v) { b = v * 2; });
    p.Report(5);
    EXPECT_EQ(a, 5);
    EXPECT_EQ(b, 10);
}

TEST(ProgressTest, ConstructorHandlerAndAddedHandlerBothInvoked) {
    int fromCtor = 0, fromAdd = 0;
    Progress<int> p([&](int v) { fromCtor = v; });
    p.addProgressChangedHandler([&](int v) { fromAdd = v + 1; });
    p.Report(3);
    EXPECT_EQ(fromCtor, 3);
    EXPECT_EQ(fromAdd, 4);
}

TEST(ProgressTest, ReportCalledMultipleTimes) {
    int count = 0;
    Progress<int> p([&](int) { ++count; });
    p.Report(1);
    p.Report(2);
    p.Report(3);
    EXPECT_EQ(count, 3);
}

TEST(ProgressTest, IsIProgress) {
    Progress<double> p;
    System::IProgress<double>& ref = p;
    int called = 0;
    p.addProgressChangedHandler([&](double) { ++called; });
    ref.Report(1.0);
    EXPECT_EQ(called, 1);
}

TEST(ProgressTest, StringProgress) {
    std::string last;
    Progress<std::string> p([&](const std::string& s) { last = s; });
    p.Report("hello");
    EXPECT_EQ(last, "hello");
}

TEST(ProgressTest, NullHandlerCtor_Throws) {
    // .NET's Progress<T>(Action<T>) throws ArgumentNullException for a null handler
    // (verified against Progress.cs: ArgumentNullException.ThrowIfNull(handler)).
    EXPECT_THROW(Progress<int>(std::function<void(int)>{}), System::ArgumentNullException);
}

// --- Empty subscription is a no-op (#1868 / SR-AUD-058 / CCF-011) ---
//
// addProgressChangedHandler used to append any std::function, and OnReport called every
// stored element unconditionally, so an empty subscription made a subsequent, entirely
// unrelated Report() raise the native std::bad_function_call. C# event subscription of a
// null delegate is Delegate.Combine(d, null) == d -- a no-op that cannot create a later
// invocation failure. See docs/EmptyCallableBoundaryPlan.md.

TEST(ProgressTest, EmptyAddedHandler_AddDoesNotThrow) {
    Progress<int> p;
    EXPECT_NO_THROW(p.addProgressChangedHandler(std::function<void(int)>{}));
}

TEST(ProgressTest, EmptyAddedHandler_LaterReportDoesNotThrow) {
    Progress<int> p;
    p.addProgressChangedHandler(std::function<void(int)>{});
    EXPECT_NO_THROW(p.Report(1));
    EXPECT_NO_THROW(p.Report(2));
}

TEST(ProgressTest, EmptyAddedHandler_DoesNotDisturbRealHandlers) {
    std::vector<int> seen;
    Progress<int> p;
    p.addProgressChangedHandler([&](int v) { seen.push_back(v * 10); });
    p.addProgressChangedHandler(std::function<void(int)>{});
    p.addProgressChangedHandler([&](int v) { seen.push_back(v * 100); });
    p.Report(3);
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], 30);   // registration order preserved
    EXPECT_EQ(seen[1], 300);
}

TEST(ProgressTest, EmptyAddedHandler_DoesNotSuppressConstructorHandler) {
    int received = -1;
    Progress<int> p([&](int v) { received = v; });
    p.addProgressChangedHandler(std::function<void(int)>{});
    p.Report(11);
    EXPECT_EQ(received, 11);
}

TEST(ProgressTest, EmptyAddedHandler_ManyTimes_StillSilent) {
    Progress<int> p;
    for (int i = 0; i < 100; ++i) p.addProgressChangedHandler(std::function<void(int)>{});
    int received = 0;
    p.addProgressChangedHandler([&](int v) { received = v; });
    EXPECT_NO_THROW(p.Report(5));
    EXPECT_EQ(received, 5);
}
