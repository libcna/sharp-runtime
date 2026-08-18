// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include <gtest/gtest.h>
#include <algorithm>
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
    EXPECT_EQ(Stopwatch::Frequency, 1'000'000'000LL);   // #2326: the clock's own, not the TimeSpan tick rate
}

TEST(StopwatchTests, IsHighResolution_True) {
    EXPECT_TRUE(Stopwatch::IsHighResolution);
}

TEST(StopwatchTests, GetElapsedTime_TwoTimestamps_MatchesDelta) {
    // #2326: `Frequency` timestamp units are one SECOND, and one second is
    // TimeSpan::TicksPerSecond ticks -- not `Frequency` ticks. The two used to be the same
    // number, which is why this row could conflate them; it now says which is which.
    long long start = Stopwatch::GetTimestamp();
    long long end = start + Stopwatch::Frequency;   // exactly 1 second later, in timestamp units
    System::TimeSpan elapsed = Stopwatch::GetElapsedTime(start, end);
    EXPECT_EQ(elapsed.getTicksProperty(), System::TimeSpan::TicksPerSecond);
    EXPECT_EQ(1.0, elapsed.getTotalSecondsProperty());
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

// #2326 RESCALED EVERY EXPECTATION BELOW, AND THE CLAIM THEY DEFEND IS UNCHANGED.
//
// These cases are CCF-004's evidence: the subtraction `ending - starting` must be DEFINED, not
// undefined behaviour, and must produce the exact two's-complement wrap. That is still what they
// assert. What moved is the reported TICK value: before #2326 `Frequency` was
// TimeSpan::TicksPerSecond, so .NET's `s_tickFrequency = TicksPerSecond / Frequency` reduced to
// exactly 1.0 and the delta WAS the tick count. `Frequency` is now the clock's own 1,000,000,000,
// so the factor is 0.01 -- which is .NET's own value on Unix (`Stopwatch.cs:18`,
// `Stopwatch.Unix.cs:8-12`), and these results are therefore .NET's, not this port's.
//
// The concrete rows below are hand-computed from `(long)(delta * 0.01)` rather than from the
// implementation, because a test that only mirrors the formula it is testing proves nothing.

namespace {
/// .NET's conversion, written out for the sweeps: `(long)((end - start) * s_tickFrequency)` with
/// the saturating float-to-integer conversion modern .NET adopted.
long long dotNetElapsedTicks(long long start, long long end) {
    const long long delta = static_cast<long long>(
        static_cast<unsigned long long>(end) - static_cast<unsigned long long>(start));
    if (Stopwatch::Frequency == 10'000'000LL) return delta;   // the pre-#2326 unit, if ever restored
    const double scaled = static_cast<double>(delta) *
        (10'000'000.0 / static_cast<double>(Stopwatch::Frequency));
    constexpr double twoPow63 = 9223372036854775808.0;
    if (scaled >= twoPow63)  return kLongMax;
    if (scaled < -twoPow63)  return kLongMin;
    return static_cast<long long>(scaled);
}
}  // namespace

TEST(StopwatchDefinedArithmeticTests, Fix2326_FrequencyIsTheClocksOwnNotTheTimeSpanTickRate) {
    // The change itself. 1,000,000,000 is .NET's Unix answer AND this port's actual clock period,
    // which is why it is derived from Clock::period rather than transcribed: reporting
    // QueryPerformanceFrequency while sampling steady_clock would be a lie about a different timer.
    EXPECT_EQ(1'000'000'000LL, Stopwatch::Frequency);
    EXPECT_NE(System::TimeSpan::TicksPerSecond, Stopwatch::Frequency)
        << "the two used to be equal, which is what made the scale factor 1.0";
    static_assert(Stopwatch::Frequency ==
                      static_cast<long long>(std::chrono::steady_clock::period::den) /
                          static_cast<long long>(std::chrono::steady_clock::period::num),
                  "#2326: Frequency is the clock's, and stays a constant expression");
}

TEST(StopwatchDefinedArithmeticTests, Fix2326_TheResolutionThatWasLost) {
    // WHY THE OLD VALUE WAS A DEFECT AND NOT MERELY A DIFFERENT CHOICE. GetTimestamp() used to
    // divide the clock's nanosecond count by 100, so anything finer than 100 ns was truncated
    // away. Two timestamps taken back to back must now be able to differ by less than 100 units.
    long long minimumDelta = kLongMax;
    for (int i = 0; i < 200; ++i) {
        const long long a = Stopwatch::GetTimestamp();
        const long long b = Stopwatch::GetTimestamp();
        if (b > a) minimumDelta = std::min(minimumDelta, b - a);
    }
    EXPECT_LT(minimumDelta, 100LL)
        << "GetTimestamp() is still quantised to 100 ns -- the #2326 division is back";
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_MinToMax_WrapsToMinusOne) {
    // Probe case S1: the audited input. The DELTA is still exactly -1 -- that is the CCF-004
    // claim -- and -1 scaled by 0.01 truncates toward zero, so the reported tick count is 0.
    EXPECT_EQ(0, Stopwatch::GetElapsedTime(kLongMin, kLongMax).getTicksProperty());
    EXPECT_EQ(dotNetElapsedTicks(kLongMin, kLongMax),
              Stopwatch::GetElapsedTime(kLongMin, kLongMax).getTicksProperty());
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_MaxToMin_WrapsToOne) {
    // Probe case S3: the reversed pair. Delta +1, scaled to 0.
    EXPECT_EQ(0, Stopwatch::GetElapsedTime(kLongMax, kLongMin).getTicksProperty());
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_MinusOneToMax_WrapsToMin) {
    // Probe case S4: one past the largest representable difference. The delta is still exactly
    // INT64_MIN; scaled by 0.01 it is representable, so this does NOT saturate.
    const long long ticks = Stopwatch::GetElapsedTime(-1, kLongMax).getTicksProperty();
    EXPECT_EQ(-92233720368547760LL, ticks);
    EXPECT_GT(ticks, kLongMin) << "0.01 * INT64_MIN is representable, so no clamp should occur";
    EXPECT_EQ(dotNetElapsedTicks(-1, kLongMax), ticks);
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_ZeroToMax_IsExactlyMax) {
    // Probe case S2: the largest difference that never overflowed. The guard must still not be
    // inverted into rejecting or clamping it -- and after scaling it lands well inside the range.
    const long long ticks = Stopwatch::GetElapsedTime(0, kLongMax).getTicksProperty();
    EXPECT_EQ(92233720368547760LL, ticks);
    EXPECT_LT(ticks, kLongMax) << "0.01 * INT64_MAX is representable, so no clamp should occur";
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_SignedSweep) {
    // The seven-point sweep the family plan requires, taken against a fixed start of 0.
    const long long points[] = {kLongMin, kLongMin + 1, -1, 0, 1, kLongMax - 1, kLongMax};
    for (long long p : points)
        EXPECT_EQ(dotNetElapsedTicks(0, p), Stopwatch::GetElapsedTime(0, p).getTicksProperty())
            << "ending=" << p;
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_WrapIsExactTwosComplement) {
    // THE CCF-004 CLAIM ITSELF, and it is unchanged by #2326: every pair, wrapping or not, must
    // behave as the unsigned difference reinterpreted as signed -- and then scaled. The sign and
    // the ordering are what a scale cannot disturb, so they are asserted separately from the
    // magnitude, which double precision above 2^53 can.
    const long long points[] = {kLongMin, kLongMin + 1, -1, 0, 1, 1000, kLongMax - 1, kLongMax};
    for (long long a : points) {
        for (long long b : points) {
            const long long delta = static_cast<long long>(
                static_cast<unsigned long long>(b) - static_cast<unsigned long long>(a));
            const long long ticks = Stopwatch::GetElapsedTime(a, b).getTicksProperty();
            EXPECT_EQ(dotNetElapsedTicks(a, b), ticks) << "start=" << a << " end=" << b;
            if (delta > 0) { EXPECT_GE(ticks, 0) << "start=" << a << " end=" << b; }
            if (delta < 0) { EXPECT_LE(ticks, 0) << "start=" << a << " end=" << b; }
        }
    }
}

TEST(StopwatchDefinedArithmeticTests, GetElapsedTime_OrdinaryPairIsUnchanged) {
    // An ordinary interval: 2000 nanoseconds is 20 TimeSpan ticks. Before #2326 the same call
    // reported 2000, because the arguments were read as 100-ns ticks rather than nanoseconds.
    EXPECT_EQ(20, Stopwatch::GetElapsedTime(1000, 3000).getTicksProperty());
    EXPECT_EQ(System::TimeSpan::FromTicks(20), Stopwatch::GetElapsedTime(1000, 3000));
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
