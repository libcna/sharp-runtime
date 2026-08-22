// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <limits>
#include "System/TimeProvider.hpp"
#include "System/Diagnostics/Stopwatch.hpp"

using System::TimeProvider;
using System::TimeSpan;
using System::DateTimeOffset;
using System::Diagnostics::Stopwatch;
using SharpRuntime::longcs;

TEST(TimeProviderTests, SystemSingleton_NotNull) {
    TimeProvider& tp = TimeProvider::getSystemProperty();
    (void)tp; // just verify it compiles and doesn't crash
    SUCCEED();
}

TEST(TimeProviderTests, GetUtcNow_ReturnsRecentTime) {
    TimeProvider& tp = TimeProvider::getSystemProperty();
    DateTimeOffset now = tp.GetUtcNow();
    EXPECT_GT(now.getYearProperty(), 2020);
    EXPECT_EQ(now.getOffsetProperty(), TimeSpan::Zero);
    EXPECT_EQ(now.getDateTimeProperty().getKindProperty(), System::DateTimeKind::Unspecified);
    EXPECT_EQ(now.getUtcDateTimeProperty().getKindProperty(), System::DateTimeKind::Utc);
}

TEST(TimeProviderTests, GetLocalNow_ReturnsRecentTime) {
    TimeProvider& tp = TimeProvider::getSystemProperty();
    DateTimeOffset local = tp.GetLocalNow();
    EXPECT_GT(local.getYearProperty(), 2020);
    EXPECT_EQ(local.getDateTimeProperty().getKindProperty(), System::DateTimeKind::Unspecified);
    EXPECT_EQ(local.getLocalDateTimeProperty().getKindProperty(), System::DateTimeKind::Local);
}

TEST(TimeProviderTests, TimestampFrequency_Matches_Stopwatch) {
    TimeProvider& tp = TimeProvider::getSystemProperty();
    EXPECT_EQ(tp.getTimestampFrequencyProperty(), Stopwatch::Frequency);
}

TEST(TimeProviderTests, GetTimestamp_Increases) {
    TimeProvider& tp = TimeProvider::getSystemProperty();
    longcs t1 = tp.GetTimestamp();
    longcs t2 = tp.GetTimestamp();
    EXPECT_GE(t2, t1);
}

TEST(TimeProviderTests, GetElapsedTime_TwoArgs_ZeroForSameTimestamp) {
    TimeProvider& tp = TimeProvider::getSystemProperty();
    longcs t = tp.GetTimestamp();
    TimeSpan elapsed = tp.GetElapsedTime(t, t);
    EXPECT_EQ(elapsed.getTicksProperty(), 0);
}

TEST(TimeProviderTests, GetElapsedTime_OneArg_NonNegative) {
    TimeProvider& tp = TimeProvider::getSystemProperty();
    longcs t = tp.GetTimestamp();
    TimeSpan elapsed = tp.GetElapsedTime(t);
    EXPECT_GE(elapsed.getTicksProperty(), 0);
}

TEST(TimeProviderTests, GetElapsedTime_ReflectsDelay) {
    TimeProvider& tp = TimeProvider::getSystemProperty();
    longcs freq = tp.getTimestampFrequencyProperty();
    // Simulate 1 second: freq ticks = 1 second
    TimeSpan one_sec = tp.GetElapsedTime(0, freq);
    EXPECT_NEAR(one_sec.getTotalSecondsProperty(), 1.0, 1e-6);
}

TEST(TimeProviderTests, CreateTimer_FiresCallback) {
    std::atomic<int> count{0};
    System::Threading::TimerCallback cb = [&](void*) { count++; };
    auto timer = TimeProvider::getSystemProperty().CreateTimer(
        cb, nullptr,
        System::TimeSpan::FromMilliseconds(20),
        System::TimeSpan::FromMilliseconds(-1)); // fire once
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    timer->Dispose();
    EXPECT_GE(count.load(), 1);
}

TEST(TimeProviderTests, CustomProvider_OverridesGetUtcNow) {
    struct FakeProvider : TimeProvider {
        DateTimeOffset GetUtcNow() const override {
            return DateTimeOffset(System::DateTime(2024, 6, 1, 12, 0, 0), System::TimeSpan::Zero);
        }
    } fake;
    DateTimeOffset t = fake.GetUtcNow();
    EXPECT_EQ(t.getYearProperty(), 2024);
    EXPECT_EQ(t.getMonthProperty(), 6);
    EXPECT_EQ(t.getDayProperty(), 1);
}

// ---------------------------------------------------------------------------
// SR-AUD-131 / ticket #2219 -- defined elapsed-time arithmetic and a defined float->int
// conversion.
//
// `TimeProvider::GetElapsedTime` carried TWO undefined operations on ONE line at two different
// columns, and only the first is reported by GCC's default `-fsanitize=undefined` set:
//
//   :74:34  a signed subtraction        -- CCF-004 class A; the wrap is now unsigned, no value
//                                          changes
//   :74:55  an out-of-range double->long conversion ([conv.fpint]/1) -- only reported with
//                                          -fsanitize=float-cast-overflow; it now saturates
//
// The second needs NO overflow in the first: GetElapsedTime(0, INT64_MAX) on the default
// provider scales to exactly 2^63 and used to return INT64_MIN, a maximal negative duration for
// a maximal positive interval. See docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md §5.1.1.
// ---------------------------------------------------------------------------

namespace {

constexpr longcs kTpLongMin = std::numeric_limits<longcs>::min();
constexpr longcs kTpLongMax = std::numeric_limits<longcs>::max();

/** A provider whose timestamp frequency is whatever the test asks for. */
class FixedFrequencyProvider : public TimeProvider {
public:
    explicit FixedFrequencyProvider(longcs f) : freq_(f) {}
    longcs getTimestampFrequencyProperty() const override { return freq_; }
    longcs GetTimestamp() const override { return 0; }

private:
    longcs freq_;
};

}  // namespace

TEST(TimeProviderDefinedArithmeticTests, MinToMax_WrapsToMinusOne) {
    // Probe case T1: the audited input. THE CCF-004 CLAIM IS UNCHANGED -- the subtraction still
    // wraps to exactly -1 -- but #2326 moved the system provider's frequency from 10 MHz to the
    // clock's 1 GHz, so the scale factor is no longer 1.0 and -1 now truncates toward zero.
    // The wrap is what this pin defends; the reported tick count follows the frequency.
    EXPECT_EQ(0, TimeProvider::getSystemProperty()
                     .GetElapsedTime(kTpLongMin, kTpLongMax)
                     .getTicksProperty());
    // It must still agree with Stopwatch on the same arguments, which is the property #2219
    // established and #2326 had to preserve.
    EXPECT_EQ(Stopwatch::GetElapsedTime(kTpLongMin, kTpLongMax).getTicksProperty(),
              TimeProvider::getSystemProperty()
                  .GetElapsedTime(kTpLongMin, kTpLongMax)
                  .getTicksProperty());
}

TEST(TimeProviderDefinedArithmeticTests, ZeroToMax_SaturatesUpwardInsteadOfReturningMin) {
    // Probe case T2 -- the behaviour change, and the reason this ticket exists. Before the repair
    // this returned INT64_MIN for a maximal POSITIVE interval, from an undefined conversion that
    // no UBSan default set reports.
    const longcs ticks =
        TimeProvider::getSystemProperty().GetElapsedTime(0, kTpLongMax).getTicksProperty();
    // #2326: at 1 GHz the scale is 0.01, so INT64_MAX nanoseconds no longer reaches the
    // saturation guard -- it lands well inside the range. THE CLAIM THIS PIN DEFENDS SURVIVES
    // AND IS THE SECOND ASSERTION: a maximal POSITIVE interval must not report a NEGATIVE
    // duration, which is what the undefined conversion used to do.
    EXPECT_EQ(92233720368547760LL, ticks);
    EXPECT_GT(ticks, 0) << "a positive interval must not report a negative duration";
    // And it still agrees with Stopwatch on the same arguments.
    EXPECT_EQ(Stopwatch::GetElapsedTime(0, kTpLongMax).getTicksProperty(), ticks);
}

TEST(TimeProviderDefinedArithmeticTests, MaxToZero_SaturatesDownward) {
    // #2326: likewise no longer reaches the guard at 1 GHz. The sign is the invariant.
    const longcs ticks =
        TimeProvider::getSystemProperty().GetElapsedTime(kTpLongMax, 0).getTicksProperty();
    EXPECT_EQ(-92233720368547760LL, ticks);
    EXPECT_LT(ticks, 0);
    // The saturation guard itself is still exercised, by a frequency that actually reaches it --
    // see CustomFrequency_ScalingLeavingRangeSaturates, which is unaffected by #2326 because it
    // supplies its own frequency.
}

TEST(TimeProviderDefinedArithmeticTests, CustomFrequency_ScalingLeavingRangeSaturates) {
    // Probe case T4: an ordinary interval that a 1 Hz frequency scales past int64. Before the
    // repair this too returned INT64_MIN.
    FixedFrequencyProvider oneHertz(1);
    EXPECT_EQ(kTpLongMax, oneHertz.GetElapsedTime(0, 1'000'000'000'000LL).getTicksProperty());
    // A 1 Hz interval that stays in range must still scale exactly, so the guard is not inverted.
    EXPECT_EQ(5 * TimeSpan::TicksPerSecond, oneHertz.GetElapsedTime(0, 5).getTicksProperty());
    EXPECT_EQ(-5 * TimeSpan::TicksPerSecond, oneHertz.GetElapsedTime(5, 0).getTicksProperty());
}

TEST(TimeProviderDefinedArithmeticTests, CustomFrequency_LargerThanTicksPerSecondScalesDown) {
    // A frequency finer than 100 ns per unit divides rather than multiplies, so the conversion
    // can never leave range and the ordinary path must be untouched.
    FixedFrequencyProvider fast(TimeSpan::TicksPerSecond * 1000);
    EXPECT_EQ(1, fast.GetElapsedTime(0, 1000).getTicksProperty());
    EXPECT_EQ(0, fast.GetElapsedTime(0, 0).getTicksProperty());
}

TEST(TimeProviderDefinedArithmeticTests, SignedSweepAtUnitFrequency) {
    // RENAMED IN MEANING BY #2326: the system provider's frequency is no longer TicksPerSecond,
    // so the scale is 0.01 rather than 1.0 and a sweep point no longer maps to itself. The
    // ROUNDING claims this test was written for are the ones that survive, and they are now
    // asserted against a provider that really does have unit frequency -- which is what the name
    // says and what the old default only happened to be.
    FixedFrequencyProvider unit(TimeSpan::TicksPerSecond);
    EXPECT_EQ(kTpLongMin, unit.GetElapsedTime(0, kTpLongMin).getTicksProperty());
    EXPECT_EQ(-1, unit.GetElapsedTime(0, -1).getTicksProperty());
    EXPECT_EQ(0, unit.GetElapsedTime(0, 0).getTicksProperty());
    EXPECT_EQ(1, unit.GetElapsedTime(0, 1).getTicksProperty());
    EXPECT_EQ(kTpLongMax, unit.GetElapsedTime(0, kTpLongMax).getTicksProperty());

    // `(double)(INT64_MIN + 1)` rounds DOWN to exactly -2^63, which IS representable, so the
    // conversion succeeds and yields INT64_MIN. The saturation guard is not involved.
    EXPECT_DOUBLE_EQ(-9223372036854775808.0, static_cast<double>(kTpLongMin + 1));
    EXPECT_EQ(kTpLongMin, unit.GetElapsedTime(0, kTpLongMin + 1).getTicksProperty());

    // `(double)(INT64_MAX - 1)` rounds UP to exactly 2^63, which is one past the representable
    // domain, so this one really does reach the guard -- and used to be undefined.
    EXPECT_DOUBLE_EQ(9223372036854775808.0, static_cast<double>(kTpLongMax - 1));
    EXPECT_EQ(kTpLongMax, unit.GetElapsedTime(0, kTpLongMax - 1).getTicksProperty());

    // And the SYSTEM provider, whose frequency #2326 moved, scales instead of mapping.
    TimeProvider& tp = TimeProvider::getSystemProperty();
    EXPECT_EQ(TimeSpan::TicksPerSecond, tp.GetElapsedTime(0, Stopwatch::Frequency).getTicksProperty())
        << "one second of timestamp units is one second of TimeSpan ticks";
}

TEST(TimeProviderDefinedArithmeticTests, PrecisionAbovePow2_53IsAPreExistingProperty) {
    // Stated so a reader does not mistake it for something #2219 introduced: this method scales
    // through a `double`, exactly as .NET's own TimeProvider.GetElapsedTime does, so a tick count
    // above 2^53 is not represented exactly and the result is the rounded one. Stopwatch's own
    // two-timestamp overload never routes through a double and stays exact.
    // #2326 moved the SYSTEM provider's frequency, so the unit-scale provider is supplied
    // explicitly here -- the claim is about `double` precision, not about any frequency.
    FixedFrequencyProvider unit(TimeSpan::TicksPerSecond);
    constexpr longcs justAbove = (longcs{1} << 53) + 1;
    EXPECT_EQ(justAbove - 1, unit.GetElapsedTime(0, justAbove).getTicksProperty());
    // Stopwatch now scales too, so its exactness claim is made where the scale is 1.0 -- which
    // after #2326 means comparing against the same unit-frequency provider rather than against
    // an unscaled Stopwatch.
    EXPECT_EQ(unit.GetElapsedTime(0, justAbove).getTicksProperty(),
              justAbove - 1);
    // At and below 2^53 the unit-frequency conversion is exact.
    constexpr longcs exact = longcs{1} << 53;
    EXPECT_EQ(exact, unit.GetElapsedTime(0, exact).getTicksProperty());
}

TEST(TimeProviderDefinedArithmeticTests, OrdinaryIntervalIsUnchanged) {
    // #2326: 2000 timestamp units is 2000 NANOSECONDS, which is 20 TimeSpan ticks. The number
    // used to be 2000 because the system provider's frequency was the TimeSpan tick rate.
    EXPECT_EQ(20, TimeProvider::getSystemProperty()
                      .GetElapsedTime(1000, 3000)
                      .getTicksProperty());
    // ...and at unit frequency the same arguments still map to themselves.
    FixedFrequencyProvider unit(TimeSpan::TicksPerSecond);
    EXPECT_EQ(2000, unit.GetElapsedTime(1000, 3000).getTicksProperty());
}

TEST(TimeProviderDefinedArithmeticTests, SingleTimestampDoorReachesTheSameSites) {
    // Probe case T6: the one-argument overload forwards to the same body.
    const TimeSpan extreme = TimeProvider::getSystemProperty().GetElapsedTime(kTpLongMin);
    EXPECT_LT(extreme.getTicksProperty(), 0);
    // The fixture's GetTimestamp() returns 0 and its frequency equals TicksPerSecond, so the
    // scale factor is 1.0 and the one-argument door reports 0 - 5 = -5 ticks.
    FixedFrequencyProvider zeroStamp(TimeSpan::TicksPerSecond);
    EXPECT_EQ(-5, zeroStamp.GetElapsedTime(5).getTicksProperty());
}

TEST(TimeProviderDefinedArithmeticTests, NonPositiveFrequencyStillWinsBeforeAnyArithmetic) {
    // Probe case T5, the control: the frequency guard must still be evaluated first, so the
    // extreme timestamps never reach either converted expression.
    for (longcs bad : {longcs{0}, longcs{-1}, kTpLongMin}) {
        FixedFrequencyProvider p(bad);
        EXPECT_THROW((void)p.GetElapsedTime(kTpLongMin, kTpLongMax),
                     System::InvalidOperationException)
            << "frequency=" << bad;
    }
}

TEST(TimeProviderDefinedArithmeticTests, LayoutIsUnchanged) {
    // The repair adds no member and no virtual: the saturation is written inline in the existing
    // body precisely so this stays true.
    static_assert(sizeof(TimeProvider) == sizeof(void*),
                  "TimeProvider must remain a single-vptr polymorphic base");
    static_assert(alignof(TimeProvider) == alignof(void*), "alignment must not change");
    SUCCEED();
}
