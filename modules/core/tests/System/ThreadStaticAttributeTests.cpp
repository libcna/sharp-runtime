// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>
#include "System/ThreadStaticAttribute.hpp"

using System::ThreadStaticAttribute;

TEST(ThreadStaticAttributeTest, DefaultCtor) {
    ThreadStaticAttribute a;
    (void)a;
}

TEST(ThreadStaticAttributeTest, IsAttribute) {
    ThreadStaticAttribute a;
    EXPECT_NO_THROW({ System::Attribute& ref = a; (void)ref; });
}

TEST(ThreadStaticAttributeTest, TwoInstancesIndependent) {
    ThreadStaticAttribute a;
    ThreadStaticAttribute b;
    EXPECT_NE(&a, &b);
}

// The marker carries no data: it adds nothing to `Attribute`, exactly like
// .NET's parameterless attribute. This is the pin that an attempt to "implement"
// SR-AUD-113 by hanging a registry pointer or a slot index off the class would
// trip; the header documents that no such mechanism exists in C++ and that
// `thread_local` is what supplies per-thread storage here.
// SR-AUD-113, ticket #2288.
TEST(ThreadStaticAttributeTest, CarriesNoDataBeyondTheAttributeBase) {
    EXPECT_EQ(sizeof(ThreadStaticAttribute), sizeof(System::Attribute));
    EXPECT_TRUE((std::is_base_of_v<System::Attribute, ThreadStaticAttribute>));
}

namespace {
    int sharedStaticCounter = 0;          // what a [ThreadStatic] field is NOT here
    thread_local int threadLocalCounter = 0;  // what the header tells ported code to write
}

// Executable form of the boundary the header now states, and the concurrent
// static-field isolation case this file was missing: holding a
// ThreadStaticAttribute does not isolate anything. An ordinary `static` stays
// shared across threads whether or not the marker exists; only `thread_local`
// gives the per-thread value .NET's attribute asks the runtime for. This
// demonstrates a language boundary rather than pinning a mutable behaviour of
// this port -- no change to ThreadStaticAttribute.hpp can make it pass or fail,
// which is precisely the point being recorded.
// SR-AUD-113, ticket #2288.
TEST(ThreadStaticAttributeTest, MarkerDoesNotIsolateStorageButThreadLocalDoes) {
    ThreadStaticAttribute marker;  // present, inert
    (void)marker;

    sharedStaticCounter = 5;
    threadLocalCounter = 5;

    int observedShared = -1;
    int observedThreadLocal = -1;
    std::thread other([&] {
        observedShared = ++sharedStaticCounter;
        observedThreadLocal = ++threadLocalCounter;
    });
    other.join();

    EXPECT_EQ(observedShared, 6);       // the other thread saw this thread's 5
    EXPECT_EQ(observedThreadLocal, 1);  // ... but started from its own zero

    EXPECT_EQ(sharedStaticCounter, 6);  // the marked-in-spirit static is SHARED
    EXPECT_EQ(threadLocalCounter, 5);   // thread_local is what isolates
}
