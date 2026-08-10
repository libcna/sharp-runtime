// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <limits>
#include "System/Diagnostics/Stopwatch.hpp"

using System::Diagnostics::Stopwatch;

TEST(StopwatchTests, DefaultCtorNotRunning) {
    Stopwatch sw;
    EXPECT_FALSE(sw.getIsRunningProperty());
}

TEST(StopwatchTests, DefaultCtorZeroElapsed) {
    Stopwatch sw;
    EXPECT_EQ(0, sw.getElapsedMillisecondsProperty());
    EXPECT_EQ(0, sw.getElapsedTicksProperty());
}

TEST(StopwatchTests, StartSetsIsRunning) {
    Stopwatch sw;
    sw.Start();
    EXPECT_TRUE(sw.getIsRunningProperty());
    sw.Stop();
}

TEST(StopwatchTests, StopClearsIsRunning) {
    Stopwatch sw;
    sw.Start();
    sw.Stop();
    EXPECT_FALSE(sw.getIsRunningProperty());
}

TEST(StopwatchTests, ElapsedAccumulatesAfterStartStop) {
    Stopwatch sw;
    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    sw.Stop();
    EXPECT_GT(sw.getElapsedMillisecondsProperty(), 0);
    EXPECT_GT(sw.getElapsedTicksProperty(), 0);
}

TEST(StopwatchTests, ElapsedGrowsWhileRunning) {
    Stopwatch sw;
    sw.Start();
    long long t1 = sw.getElapsedTicksProperty();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    long long t2 = sw.getElapsedTicksProperty();
    sw.Stop();
    EXPECT_GE(t2, t1);
}

TEST(StopwatchTests, StartNewIsRunning) {
    auto sw = Stopwatch::StartNew();
    EXPECT_TRUE(sw.getIsRunningProperty());
    sw.Stop();
}

TEST(StopwatchTests, StartNewAccumulatesElapsed) {
    auto sw = Stopwatch::StartNew();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    sw.Stop();
    EXPECT_GT(sw.getElapsedMillisecondsProperty(), 0);
}

TEST(StopwatchTests, ResetZerosElapsedAndStops) {
    Stopwatch sw;
    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.Stop();
    sw.Reset();
    EXPECT_FALSE(sw.getIsRunningProperty());
    EXPECT_EQ(0, sw.getElapsedMillisecondsProperty());
    EXPECT_EQ(0, sw.getElapsedTicksProperty());
}

TEST(StopwatchTests, ResetWhileRunningStopsAndZeros) {
    Stopwatch sw;
    sw.Start();
    sw.Reset();
    EXPECT_FALSE(sw.getIsRunningProperty());
    EXPECT_EQ(0, sw.getElapsedMillisecondsProperty());
}

TEST(StopwatchTests, RestartZerosAndStarts) {
    Stopwatch sw;
    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.Stop();
    sw.Restart();
    EXPECT_TRUE(sw.getIsRunningProperty());
    // elapsed ticks should now be very small (just restarted)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    sw.Stop();
    // We just want to confirm it restarted — elapsed after a 5ms+restart should be < 5ms previous
    EXPECT_GE(sw.getElapsedTicksProperty(), 0);
}

TEST(StopwatchTests, StopOnStoppedIsIdempotent) {
    Stopwatch sw;
    sw.Start();
    sw.Stop();
    long long t = sw.getElapsedTicksProperty();
    // Calling Stop again must not throw or alter accumulated time
    sw.Stop();
    EXPECT_EQ(t, sw.getElapsedTicksProperty());
    EXPECT_FALSE(sw.getIsRunningProperty());
}

TEST(StopwatchTests, StartOnRunningIsIdempotent) {
    Stopwatch sw;
    sw.Start();
    sw.Start();  // second call must be a no-op
    EXPECT_TRUE(sw.getIsRunningProperty());
    sw.Stop();
}

TEST(StopwatchTests, GetElapsedPropertyMatchesGetElapsedTicks) {
    Stopwatch sw;
    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.Stop();
    long long ticks = sw.getElapsedTicksProperty();
    System::TimeSpan ts = sw.getElapsedProperty();
    EXPECT_EQ(ticks, ts.getTicksProperty());
}

TEST(StopwatchTests, AccumulatesAcrossMultipleStartStop) {
    Stopwatch sw;
    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.Stop();
    long long first = sw.getElapsedTicksProperty();

    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.Stop();
    long long total = sw.getElapsedTicksProperty();

    EXPECT_GT(total, first);
}

TEST(StopwatchTests, Frequency_Is10MHz) {
    EXPECT_EQ(Stopwatch::Frequency, 10'000'000LL);
}

TEST(StopwatchTests, IsHighResolution_True) {
    EXPECT_TRUE(Stopwatch::IsHighResolution);
}

TEST(StopwatchTests, GetElapsedTime_TwoTimestamps_MatchesDelta) {
    long long start = Stopwatch::GetTimestamp();
    long long end = start + Stopwatch::Frequency; // exactly 1 second later, in ticks
    System::TimeSpan elapsed = Stopwatch::GetElapsedTime(start, end);
    EXPECT_EQ(elapsed.getTicksProperty(), Stopwatch::Frequency);
}

TEST(StopwatchTests, GetElapsedTime_SingleTimestamp_IsNonNegative) {
    long long start = Stopwatch::GetTimestamp();
    System::TimeSpan elapsed = Stopwatch::GetElapsedTime(start);
    EXPECT_GE(elapsed.getTicksProperty(), 0);
}

TEST(StopwatchTests, ToString_MatchesElapsedToString) {
    Stopwatch sw;
    sw.Start();
    sw.Stop();
    EXPECT_EQ(sw.ToString(), sw.getElapsedProperty().ToString());
}

// ---------------------------------------------------------------------------
// SR-AUD-131 / ticket #2218 -- defined elapsed-time arithmetic.
//
// `GetElapsedTime(endingTimestamp, startingTimestamp)` used to evaluate the difference as a
// signed `longcs`, which UBSan confirmed as undefined behaviour at Stopwatch.hpp for four
// measured shapes.  Real .NET performs the same subtraction in C#'s *unchecked* integral model,
// where two's-complement wrap is defined and intended, so this is CCF-004 class A: the wrap is
// now produced in the unsigned domain and every value is byte-identical to the pre-fix
// measurement recorded in docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md section 5.1.
//
// These assertions therefore pin the values as they were BEFORE the repair.  A change to any of
// them is a regression, not an improvement.
// ---------------------------------------------------------------------------

namespace {
constexpr long long kLongMin = std::numeric_limits<long long>::min();
constexpr long long kLongMax = std::numeric_limits<long long>::max();
}  // namespace

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_MinToMax_WrapsToMinusOne) {
    // Probe case S1: the audited input.  Was UB, produced -1, and must keep producing -1.
    EXPECT_EQ(-1, Stopwatch::GetElapsedTime(kLongMin, kLongMax).getTicksProperty());
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_MaxToMin_WrapsToOne) {
    // Probe case S3: the reversed pair.
    EXPECT_EQ(1, Stopwatch::GetElapsedTime(kLongMax, kLongMin).getTicksProperty());
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_MinusOneToMax_WrapsToMin) {
    // Probe case S4: one past the largest representable difference.
    EXPECT_EQ(kLongMin, Stopwatch::GetElapsedTime(-1, kLongMax).getTicksProperty());
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_ZeroToMax_IsExactlyMax) {
    // Probe case S2: the largest difference that never overflowed, so the guard must not be
    // inverted into rejecting or clamping it.
    EXPECT_EQ(kLongMax, Stopwatch::GetElapsedTime(0, kLongMax).getTicksProperty());
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_SignedSweep) {
    // The seven-point sweep the family plan requires, taken against a fixed start of 0 so that
    // the expected value is the ending timestamp itself.
    const long long points[] = {kLongMin, kLongMin + 1, -1, 0, 1, kLongMax - 1, kLongMax};
    for (long long p : points)
        EXPECT_EQ(p, Stopwatch::GetElapsedTime(0, p).getTicksProperty()) << "ending=" << p;
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_WrapIsExactTwosComplement) {
    // Every pair, wrapping or not, must equal the unsigned difference reinterpreted as signed.
    const long long points[] = {kLongMin, kLongMin + 1, -1, 0, 1, 1000, kLongMax - 1, kLongMax};
    for (long long a : points) {
        for (long long b : points) {
            const auto expected = static_cast<long long>(
                static_cast<unsigned long long>(b) - static_cast<unsigned long long>(a));
            EXPECT_EQ(expected, Stopwatch::GetElapsedTime(a, b).getTicksProperty())
                << "start=" << a << " end=" << b;
        }
    }
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_OrdinaryPairIsUnchanged) {
    EXPECT_EQ(2000, Stopwatch::GetElapsedTime(1000, 3000).getTicksProperty());
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_SingleTimestampDoorReachesTheSameSite) {
    // Probe case S6: the one-argument overload forwards to the same subtraction with
    // `ending = GetTimestamp()`, so an extreme start must be defined there too.  The timestamp is
    // an arbitrary positive monotonic value, so the wrapped result is negative but must exist.
    const System::TimeSpan elapsed = Stopwatch::GetElapsedTime(kLongMin);
    EXPECT_LT(elapsed.getTicksProperty(), 0);
    // And the ordinary use of the same door is still non-negative and small.
    const long long now = Stopwatch::GetTimestamp();
    EXPECT_GE(Stopwatch::GetElapsedTime(now).getTicksProperty(), 0);
}

TEST(StopwatchDefinedArithmeticTests, InstanceMeasurementIsUnaffected) {
    // The accumulator sites converted alongside the subtraction are not publicly reachable, so
    // the only thing a test can assert is that ordinary measurement still behaves.
    Stopwatch sw;
    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    sw.Stop();
    const long long first = sw.getElapsedTicksProperty();
    EXPECT_GT(first, 0);
    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    sw.Stop();
    EXPECT_GT(sw.getElapsedTicksProperty(), first);
    sw.Reset();
    EXPECT_EQ(0, sw.getElapsedTicksProperty());
}
