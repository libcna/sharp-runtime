// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2156 (cause TM-C of docs/SystemTimersNamespaceReviewPlan.md; a post-audit defect, no
// SR-AUD identifier): `setIntervalProperty` validated strictly less than `Timer(double)` -- it
// accepted +inf, 2147483648 and 3e9, three values the constructor rejects -- and BOTH doors
// accepted NaN, because `value <= 0` and `std::ceil(value) > INT32_MAX` are each false for a NaN.
//
// Every value in that gap then reached
// `static_cast<SharpRuntime::intcs>(std::ceil(interval_))`, a floating-to-integral conversion of a
// value not representable in the destination: undefined behaviour per [conv.fpint]/1, confirmed by
//   Timer.cpp:51:56: runtime error: nan is outside the range of representable values of type 'int'
// (build-probe/2153_probe2_fco.log). GCC's `-fsanitize=undefined` does NOT include
// `float-cast-overflow`, which is why it survived.
//
// Measured before-matrix: build-probe/2153_probe3_before.log.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "System/ArgumentException.hpp"
#include "System/TimeSpan.hpp"
#include "System/Timers/Timer.hpp"

using namespace System::Timers;

namespace {

    constexpr double kInt32Max = 2147483647.0;

    // The 10-value x 3-door table of the plan's section 4.3. `acceptable` is the post-#2156
    // contract: one domain, applied identically at every door that writes interval_.
    struct Row { const char* label; double value; bool acceptable; };

    const Row kRows[] = {
        {"0",                0.0,                                      false},
        {"-1",               -1.0,                                     false},
        {"-inf",             -std::numeric_limits<double>::infinity(), false},
        {"NaN",              std::numeric_limits<double>::quiet_NaN(), false},
        {"+inf",             std::numeric_limits<double>::infinity(),  false},
        {"2147483648",       2147483648.0,                             false},
        {"3e9",              3e9,                                      false},
        {"0.5",              0.5,                                      true},
        {"1",                1.0,                                      true},
        {"2147483647",       kInt32Max,                                true},
    };

} // namespace

TEST(TimerIntervalDomainTests, TheDoubleConstructorAppliesTheWholeDomain) {
    for (const auto& r : kRows) {
        if (r.acceptable) {
            EXPECT_NO_THROW({ Timer t(r.value); }) << r.label;
        } else {
            EXPECT_THROW({ Timer t(r.value); }, System::ArgumentException) << r.label;
        }
    }
}

TEST(TimerIntervalDomainTests, TheSetterAppliesTheSameDomainAsTheConstructor) {
    // This is the defect: before #2156 the setter accepted +inf, 2147483648, 3e9 and NaN.
    for (const auto& r : kRows) {
        Timer t(10);
        if (r.acceptable) {
            EXPECT_NO_THROW(t.setIntervalProperty(r.value)) << r.label;
        } else {
            EXPECT_THROW(t.setIntervalProperty(r.value), System::ArgumentException) << r.label;
        }
    }
}

TEST(TimerIntervalDomainTests, TheTimeSpanConstructorInheritsTheSameDomain) {
    // Timer(TimeSpan) delegates to Timer(double), so it is covered by construction -- asserted
    // rather than assumed, because the delegation is what makes one check enough for three doors.
    EXPECT_THROW({ Timer t(System::TimeSpan::FromMilliseconds(0)); }, System::ArgumentException);
    EXPECT_THROW({ Timer t(System::TimeSpan::FromMilliseconds(-1)); }, System::ArgumentException);
    EXPECT_NO_THROW({ Timer t(System::TimeSpan::FromMilliseconds(25)); });
}

TEST(TimerIntervalDomainTests, ARejectedWriteLeavesThePreviousIntervalIntact) {
    Timer t(37);
    for (const auto& r : kRows) {
        if (r.acceptable) continue;
        EXPECT_THROW(t.setIntervalProperty(r.value), System::ArgumentException) << r.label;
        EXPECT_EQ(t.getIntervalProperty(), 37.0) << r.label << ": a rejected write changed the interval";
    }
}

TEST(TimerIntervalDomainTests, NoAcceptedValueCanReachAnUndefinedConversion) {
    // The property that makes the repair meaningful rather than cosmetic: every value the doors
    // accept survives the round trip through the schedule, which is where the undefined conversion
    // lived. A NaN or an out-of-int-range value reaching Start() was the whole defect.
    for (const auto& r : kRows) {
        if (!r.acceptable) continue;
        Timer t(r.value);
        EXPECT_NO_THROW(t.Start()) << r.label;
        EXPECT_NO_THROW(t.Stop()) << r.label;

        Timer viaSetter(10);
        viaSetter.Start();
        EXPECT_NO_THROW(viaSetter.setIntervalProperty(r.value)) << r.label << " (while running)";
        EXPECT_NO_THROW(viaSetter.Stop()) << r.label;
    }
}

TEST(TimerIntervalDomainTests, TheRejectionNamesTheParameterOfTheDoorThatWasCalled) {
    // The constructor says "interval", the setter says "value" -- both preserved exactly as they
    // were for the values each already rejected, so no caller's diagnostic changes.
    try {
        Timer t(0.0);
        ADD_FAILURE() << "the constructor accepted 0";
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find("interval"), std::string::npos) << e.what();
    }
    try {
        Timer t(10);
        t.setIntervalProperty(0.0);
        ADD_FAILURE() << "the setter accepted 0";
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find("value"), std::string::npos) << e.what();
    }
}

// ---------------------------------------------------------------------------
// Controls and pins
// ---------------------------------------------------------------------------

TEST(TimerIntervalDomainControlTests, TheAcceptedValuesStillProduceTheSameStoredInterval) {
    EXPECT_EQ(Timer(0.5).getIntervalProperty(), 1.0);      // the constructor ceils
    EXPECT_EQ(Timer(1.0).getIntervalProperty(), 1.0);
    EXPECT_EQ(Timer(kInt32Max).getIntervalProperty(), kInt32Max);
    EXPECT_EQ(Timer().getIntervalProperty(), 100.0);
}

TEST(TimerIntervalDomainPinTests, TheSetterStoresVerbatimWhereTheConstructorCeils) {
    // Measured and PINNED, not changed: the constructor stores std::ceil(interval) while the setter
    // stores the value as given, so these two timers report different intervals for the same
    // argument even though both schedule the same 1 ms tick (the schedule ceils at that point).
    // No finding names it, and either answer would be a public observable change.
    EXPECT_EQ(Timer(0.5).getIntervalProperty(), 1.0);
    Timer t(10);
    t.setIntervalProperty(0.5);
    EXPECT_EQ(t.getIntervalProperty(), 0.5);
}
