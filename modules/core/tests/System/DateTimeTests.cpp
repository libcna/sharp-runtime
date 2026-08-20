// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/DateTimeOffset.hpp"
#include "System/DateTimeKind.hpp"

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/DateTime.hpp"
#include "System/TimeZone.hpp"
#include "System/ILocalTimeZone.hpp"
#include <typeinfo>
#include "System/IFormatProvider.hpp"
#include "System/Globalization/DateTimeFormatInfo.hpp"
#include "System/Globalization/NumberFormatInfo.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/FormatException.hpp"
#include "System/TimeSpan.hpp"

#include <string>

using System::DateTime;
using System::TimeSpan;

// .NET tick constants (100-ns units)
static constexpr long long kTicksPerSecond = 10'000'000LL;
static constexpr long long kTicksPerMinute = 600'000'000LL;
static constexpr long long kTicksPerHour   = 36'000'000'000LL;
static constexpr long long kTicksPerDay    = 864'000'000'000LL;
// Unix epoch expressed as .NET ticks (Jan 1 1970 relative to Jan 1 0001)
static constexpr long long kUnixEpochTicks = 621'355'968'000'000'000LL;

// ---------------------------------------------------------------------------
// Construction & Ticks
// ---------------------------------------------------------------------------

TEST(DateTimeTests, DefaultCtorHasZeroTicks) {
    DateTime dt;
    EXPECT_EQ(dt.getTicksProperty(), 0LL);
}

TEST(DateTimeTests, ConstructFromTicks) {
    DateTime dt(kUnixEpochTicks);
    EXPECT_EQ(dt.getTicksProperty(), kUnixEpochTicks);
}

TEST(DateTimeTests, ConstructFromKnownTicks) {
    // From .NET DateTimeTests.cs: new DateTime(999999999999999999) is valid
    DateTime dt(999'999'999'999'999'999LL);
    EXPECT_EQ(dt.getTicksProperty(), 999'999'999'999'999'999LL);
}

TEST(DateTimeTests, ConstructFromNegativeTicks_Throws) {
    EXPECT_THROW(DateTime(-1LL), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, ConstructFromTicksBeyondMaxValue_Throws) {
    EXPECT_THROW(DateTime(DateTime::MaxTicks + 1), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, AddDays_BeyondMaxValue_Throws) {
    DateTime maxDate(DateTime::MaxTicks);
    EXPECT_THROW(maxDate.AddDays(1), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, AddDays_BeforeMinValue_Throws) {
    DateTime minDate(0);
    EXPECT_THROW(minDate.AddDays(-1), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, AddDays_LargeValue_ThrowsInsteadOfOverflowing) {
    // static_cast<longcs>(days) * TicksPerDay for a merely large (not extreme) int day
    // count is real signed-overflow UB in C++ without an upfront bounds check -- must throw
    // ArgumentOutOfRangeException instead of invoking UB or wrapping to a bogus DateTime.
    DateTime dt(2000, 1, 1);
    EXPECT_THROW(dt.AddDays(1000000000), System::ArgumentOutOfRangeException);
    EXPECT_THROW(dt.AddDays(-1000000000), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, AddHours_LargeValue_ThrowsInsteadOfOverflowing) {
    DateTime dt(2000, 1, 1);
    EXPECT_THROW(dt.AddHours(1000000000), System::ArgumentOutOfRangeException);
    EXPECT_THROW(dt.AddHours(-1000000000), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, EqualityBasedOnTicks) {
    DateTime a(1000LL), b(1000LL), c(2000LL);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ---------------------------------------------------------------------------
// Add / Subtract with TimeSpan
// ---------------------------------------------------------------------------

TEST(DateTimeTests, AddTimeSpanOneDay) {
    DateTime dt(kUnixEpochTicks);
    TimeSpan oneDay = TimeSpan::FromDays(1.0);
    DateTime next = dt.Add(oneDay);
    EXPECT_EQ(next.getTicksProperty(), kUnixEpochTicks + kTicksPerDay);
}

TEST(DateTimeTests, AddTimeSpanOneHour) {
    DateTime dt(kUnixEpochTicks);
    TimeSpan oneHour = TimeSpan::FromHours(1.0);
    DateTime next = dt.Add(oneHour);
    EXPECT_EQ(next.getTicksProperty(), kUnixEpochTicks + kTicksPerHour);
}

TEST(DateTimeTests, AddTimeSpanOneSecond) {
    DateTime dt(1'000'000'000LL);
    TimeSpan oneSecond = TimeSpan::FromSeconds(1.0);
    DateTime next = dt.Add(oneSecond);
    EXPECT_EQ(next.getTicksProperty(), 1'000'000'000LL + kTicksPerSecond);
}

TEST(DateTimeTests, Add_TimeSpanNearMaxValue_ThrowsInsteadOfOverflowing) {
    // ticks_ (bounded to [0, MaxTicks]) + a TimeSpan near TimeSpan::MaxValue's ticks (up to
    // ~Int64::MaxValue) is real signed-overflow UB in C++ without the unsigned-arithmetic
    // range check AddTicks now uses -- must throw ArgumentOutOfRangeException.
    DateTime dt(2000, 1, 1);
    EXPECT_THROW(dt.Add(TimeSpan::MaxValue), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Subtract_TimeSpanNearMaxValue_ThrowsInsteadOfOverflowing) {
    DateTime dt(2000, 1, 1);
    EXPECT_THROW(dt.Subtract(TimeSpan::MaxValue), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, SubtractTimeSpan) {
    DateTime dt(kUnixEpochTicks + kTicksPerDay);
    TimeSpan oneDay = TimeSpan::FromDays(1.0);
    DateTime prev = dt.Subtract(oneDay);
    EXPECT_EQ(prev.getTicksProperty(), kUnixEpochTicks);
}

TEST(DateTimeTests, AddSubtractRoundtrip) {
    DateTime original(kUnixEpochTicks);
    TimeSpan ts = TimeSpan::FromHours(5.0);
    DateTime modified = original.Add(ts);
    DateTime restored = modified.Subtract(ts);
    EXPECT_EQ(restored, original);
}

// ---------------------------------------------------------------------------
// Subtract DateTime → TimeSpan
// ---------------------------------------------------------------------------

TEST(DateTimeTests, SubtractDateTimesGivesTimeSpan) {
    DateTime a(kUnixEpochTicks + kTicksPerDay);
    DateTime b(kUnixEpochTicks);
    TimeSpan diff = a.Subtract(b);
    EXPECT_EQ(diff.getTicksProperty(), kTicksPerDay);
}

TEST(DateTimeTests, SubtractDateTimesSameGivesZero) {
    DateTime dt(kUnixEpochTicks);
    TimeSpan diff = dt.Subtract(dt);
    EXPECT_EQ(diff.getTicksProperty(), 0LL);
}

TEST(DateTimeTests, SubtractDateTimesNegative) {
    // earlier - later → negative TimeSpan
    DateTime earlier(kUnixEpochTicks);
    DateTime later(kUnixEpochTicks + kTicksPerHour);
    TimeSpan diff = earlier.Subtract(later);
    EXPECT_EQ(diff.getTicksProperty(), -kTicksPerHour);
}

// ---------------------------------------------------------------------------
// Comparison operators
// ---------------------------------------------------------------------------

TEST(DateTimeTests, CompareEqualTicks) {
    DateTime a(1000LL), b(1000LL);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a < b);
    EXPECT_TRUE(a <= b);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a >= b);
}

TEST(DateTimeTests, CompareLessThan) {
    DateTime earlier(1000LL), later(2000LL);
    EXPECT_LT(earlier, later);
    EXPECT_GT(later, earlier);
    EXPECT_LE(earlier, later);
    EXPECT_GE(later, earlier);
}

// ---------------------------------------------------------------------------
// getTimeOfDayProperty
// ---------------------------------------------------------------------------

TEST(DateTimeTests, TimeOfDayAtStartOfDay) {
    // A DateTime at an exact day boundary → time of day is zero
    DateTime dt(2LL * kTicksPerDay);   // exactly 2 days from tick 0
    TimeSpan tod = dt.getTimeOfDayProperty();
    EXPECT_EQ(tod.getTicksProperty(), 0LL);
}

TEST(DateTimeTests, TimeOfDayHalfwayThroughDay) {
    // 1.5 days → time of day is 0.5 day = 12 hours
    long long halfDay = kTicksPerDay / 2;
    DateTime dt(kTicksPerDay + halfDay);
    TimeSpan tod = dt.getTimeOfDayProperty();
    EXPECT_EQ(tod.getTicksProperty(), halfDay);
}

TEST(DateTimeTests, TimeOfDayLessThanOneDay) {
    // Any time-of-day value must be < 1 day
    DateTime dt(kUnixEpochTicks + kTicksPerHour * 15);
    TimeSpan tod = dt.getTimeOfDayProperty();
    EXPECT_GE(tod.getTicksProperty(), 0LL);
    EXPECT_LT(tod.getTicksProperty(), kTicksPerDay);
}

// ---------------------------------------------------------------------------
// getNowProperty
// ---------------------------------------------------------------------------

TEST(DateTimeTests, NowIsAfterUnixEpoch) {
    // DateTime.Now ticks must be > Unix epoch (1970-01-01) ticks
    DateTime now = DateTime::getNowProperty();
    EXPECT_GT(now.getTicksProperty(), kUnixEpochTicks);
}

TEST(DateTimeTests, NowMonotonicallyIncreases) {
    // Two consecutive Now() calls: second >= first
    DateTime t1 = DateTime::getNowProperty();
    DateTime t2 = DateTime::getNowProperty();
    EXPECT_GE(t2.getTicksProperty(), t1.getTicksProperty());
}

// ---------------------------------------------------------------------------
// ToString
// ---------------------------------------------------------------------------

TEST(DateTimeTests, ToStringContainsTicks) {
    // ToString() now returns ISO-8601 style "YYYY-MM-DD HH:MM:SS"
    // 123456789 ticks = ~12 seconds from .NET epoch 0001-01-01
    DateTime dt(123456789LL);
    std::string s = dt.ToString();
    EXPECT_NE(s.find("0001"), std::string::npos);  // year
    EXPECT_NE(s.find("00:00:12"), std::string::npos); // ~12 seconds
}

TEST(DateTimeTests, ToStringZeroTicks) {
    // DateTime(0) == 0001-01-01 00:00:00
    std::string s = DateTime().ToString();
    EXPECT_EQ(s, "0001-01-01 00:00:00");
}

// ---------------------------------------------------------------------------
// Arithmetic stress
// ---------------------------------------------------------------------------

TEST(DateTimeTests, Add365Days) {
    // Adding 365 days via FromDays should increment ticks by 365 * TicksPerDay
    DateTime dt(kUnixEpochTicks);
    TimeSpan year = TimeSpan::FromDays(365.0);
    DateTime next = dt.Add(year);
    EXPECT_EQ(next.getTicksProperty(), kUnixEpochTicks + 365LL * kTicksPerDay);
}

TEST(DateTimeTests, ChainedAddSubtract) {
    DateTime dt(kUnixEpochTicks);
    // +1h -30min +15s → net +3600-1800+15 = +1815 seconds
    TimeSpan net = TimeSpan::FromSeconds(3600.0 - 1800.0 + 15.0);
    DateTime result = dt.Add(TimeSpan::FromHours(1.0))
                        .Subtract(TimeSpan::FromMinutes(30.0))
                        .Add(TimeSpan::FromSeconds(15.0));
    EXPECT_EQ(result.getTicksProperty(), kUnixEpochTicks + net.getTicksProperty());
}

// --- ToString(format) ---

TEST(DateTimeTests, ToString_Format_yyyyMMdd) {
    DateTime dt(2024, 3, 5);
    EXPECT_EQ(dt.ToString("yyyy-MM-dd"), "2024-03-05");
}

TEST(DateTimeTests, ToString_Format_HHmmss) {
    DateTime dt(2024, 1, 1, 9, 5, 7);
    EXPECT_EQ(dt.ToString("HH:mm:ss"), "09:05:07");
}

TEST(DateTimeTests, ToString_Format_Full) {
    DateTime dt(2024, 12, 31, 23, 59, 58);
    EXPECT_EQ(dt.ToString("yyyy-MM-dd HH:mm:ss"), "2024-12-31 23:59:58");
}

TEST(DateTimeTests, ToString_Format_ShortDate_US) {
    DateTime dt(2024, 6, 15);
    EXPECT_EQ(dt.ToString("MM/dd/yyyy"), "06/15/2024");
}

TEST(DateTimeTests, ToString_Format_SingleTokens) {
    DateTime dt(2024, 6, 5, 8, 7, 9);
    EXPECT_EQ(dt.ToString("M/d/yyyy H:m:s"), "6/5/2024 8:7:9");
}

TEST(DateTimeTests, ToString_Format_Milliseconds) {
    DateTime dt(2024, 1, 1, 0, 0, 0, 123);
    EXPECT_EQ(dt.ToString("HH:mm:ss.fff"), "00:00:00.123");
}

TEST(DateTimeTests, ToString_Format_Literal) {
    DateTime dt(2024, 6, 15);
    EXPECT_EQ(dt.ToString("'Date: 'yyyy-MM-dd"), "Date: 2024-06-15");
}

TEST(DateTimeTests, ToString_Format_12Hour) {
    DateTime dt(2024, 1, 1, 13, 0, 0);
    EXPECT_EQ(dt.ToString("hh:mm"), "01:00");
}

TEST(DateTimeTests, ToString_Format_ddd_AbbreviatedDayName) {
    DateTime dt(2015, 10, 21); // a Wednesday
    EXPECT_EQ(dt.ToString("ddd"), "Wed");
}

TEST(DateTimeTests, ToString_Format_dddd_FullDayName) {
    DateTime dt(2015, 10, 21); // a Wednesday
    EXPECT_EQ(dt.ToString("dddd"), "Wednesday");
}

TEST(DateTimeTests, ToString_Format_MMM_AbbreviatedMonthName) {
    DateTime dt(2015, 10, 21);
    EXPECT_EQ(dt.ToString("MMM"), "Oct");
}

TEST(DateTimeTests, ToString_Format_MMMM_FullMonthName) {
    DateTime dt(2015, 10, 21);
    EXPECT_EQ(dt.ToString("MMMM"), "October");
}

TEST(DateTimeTests, ToString_Format_Rfc1123Style) {
    DateTime dt(2015, 10, 21, 7, 28, 0);
    EXPECT_EQ(dt.ToString("ddd, dd MMM yyyy HH:mm:ss"), "Wed, 21 Oct 2015 07:28:00");
}

// --- Parse ---

TEST(DateTimeTests, Parse_DateOnly) {
    DateTime dt = DateTime::Parse("2024-06-15");
    EXPECT_EQ(dt.getYearProperty(),  2024);
    EXPECT_EQ(dt.getMonthProperty(), 6);
    EXPECT_EQ(dt.getDayProperty(),   15);
    EXPECT_EQ(dt.getHourProperty(),  0);
}

TEST(DateTimeTests, Parse_DateAndTime) {
    DateTime dt = DateTime::Parse("2024-06-15 10:30:45");
    EXPECT_EQ(dt.getHourProperty(),   10);
    EXPECT_EQ(dt.getMinuteProperty(), 30);
    EXPECT_EQ(dt.getSecondProperty(), 45);
}

TEST(DateTimeTests, Parse_ISO8601_T_Separator) {
    DateTime dt = DateTime::Parse("2024-06-15T10:30:45");
    EXPECT_EQ(dt.getHourProperty(), 10);
}

TEST(DateTimeTests, Parse_WithMilliseconds) {
    DateTime dt = DateTime::Parse("2024-06-15 10:30:45.123");
    EXPECT_EQ(dt.getMillisecondProperty(), 123);
}

TEST(DateTimeTests, Parse_Invalid_Throws) {
    EXPECT_THROW(DateTime::Parse("not-a-date"), std::exception);
}

// --- TryParse ---

TEST(DateTimeTests, TryParse_ValidDate_ReturnsTrue) {
    DateTime dt;
    EXPECT_TRUE(DateTime::TryParse("2024-06-15", dt));
    EXPECT_EQ(dt.getYearProperty(), 2024);
}

TEST(DateTimeTests, TryParse_ValidDateTime_ReturnsTrue) {
    DateTime dt;
    EXPECT_TRUE(DateTime::TryParse("2024-06-15 08:00:00", dt));
    EXPECT_EQ(dt.getHourProperty(), 8);
}

TEST(DateTimeTests, TryParse_Invalid_ReturnsFalse) {
    DateTime dt;
    EXPECT_FALSE(DateTime::TryParse("hello", dt));
}

TEST(DateTimeTests, TryParse_BadMonth_ReturnsFalse) {
    DateTime dt;
    EXPECT_FALSE(DateTime::TryParse("2024-13-01", dt));
}

// Regression tests for a wave-3 audit finding: TryParse computed the fractional-second
// digit count as `s.size() - 20`, which counted a trailing ISO-8601 'Z'/offset marker as if
// it were extra fraction digits, corrupting the millisecond normalisation (".123Z" was read
// as 4 digits and truncated 123ms down to 12ms instead of being recognised as 3 digits).

TEST(DateTimeTests, TryParse_MillisecondsWithTrailingZ_ParsesCorrectly) {
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:30:45.123Z", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 123);
}

TEST(DateTimeTests, TryParse_MillisecondsWithTrailingOffset_ParsesCorrectly) {
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:30:45.123+02:00", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 123);
}

// FLIPPED by #1879 (approved 2026-07-31). A fraction of more than three digits
// was rejected rather than silently discarded. The whole-string consumption
// rule made that automatic.
//
// PREMISE CORRECTION, recorded where the claim was made. The decision packet
// §20.1 asserted that ".NET rejects EVERY input in the table below". Measured
// against the reference: .NET's ParseFraction
// (Globalization/DateTimeParse.cs:479-492) reads ALL fractional digits and only
// requires that there be at least one, so .NET ACCEPTS ".1234567" and keeps
// 100-ns precision. Rejecting it is therefore a deliberate narrowing of this
// port's millisecond-resolution parse subset -- a defensible "refuse what you
// cannot represent" over "silently change the value", and what was approved --
// but it was not the removal of a divergence.
//
// FLIPPED AGAIN by #1929 rows 5-6 (approved 2026-08-01). DateTime has always
// stored ticks, so the claim that four through seven digits could not be
// represented was false. The approved shared grammar retains all seven digits;
// a bare ".", non-digits, and an eighth digit remain rejected.
TEST(DateTimeTests, TryParse_FractionOneThroughSevenDigitsRetainsTicks) {
    DateTime dt;
    const long long wholeSecond = DateTime(2024, 6, 15, 10, 30, 45).getTicksProperty();
    const char* fractions[] = {"1", "12", "123", "1234", "12345", "123456", "1234567"};
    const long long expected[] = {1000000, 1200000, 1230000, 1234000, 1234500, 1234560, 1234567};
    for (int i = 0; i < 7; ++i) {
        const std::string text = std::string("2024-06-15 10:30:45.") + fractions[i] +
                                 (i % 2 == 0 ? "Z" : "+02:00");
        ASSERT_TRUE(DateTime::TryParse(text, dt)) << text;
        EXPECT_EQ(dt.getTicksProperty(), wholeSecond + expected[i]) << text;
        EXPECT_EQ(DateTime::Parse(text).getTicksProperty(), dt.getTicksProperty()) << text;
    }
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 10:30:45.12345678", dt));
    EXPECT_THROW(DateTime::Parse("2024-06-15 10:30:45.12345678"), System::FormatException);
}

TEST(DateTimeTests, TryParse_MillisecondsNoTimezone_StillParsesCorrectly) {
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:30:45.123", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 123);
}

// Regression tests for a second, separate TryParse bug (found while verifying the fix above,
// but out of scope for that commit): the fractional-second branch was gated on
// `s.size() >= 23`, i.e. "at least 3 characters after the dot", so a 1- or 2-digit fraction
// (".5", ".56", ".5Z") was silently skipped entirely -- Millisecond stayed 0 -- instead of
// being scaled up to milliseconds like a 3+ digit fraction already was. Fixed by gating on
// "at least 1 character after the dot" and letting the digit-counting loop (added by the fix
// above) handle any length correctly.
TEST(DateTimeTests, TryParse_OneFractionalDigit_ScalesToMilliseconds) {
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:30:45.5", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 500);
}

TEST(DateTimeTests, TryParse_OneFractionalDigitWithTrailingZ_ScalesToMilliseconds) {
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:30:45.5Z", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 500);
}

TEST(DateTimeTests, TryParse_TwoFractionalDigits_ScalesToMilliseconds) {
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:30:45.56", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 560);
}

TEST(DateTimeTests, TryParse_TwoFractionalDigitsWithTrailingOffset_ScalesToMilliseconds) {
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:30:45.56+02:00", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 560);
}

// ---------------------------------------------------------------------------
// MinValue / MaxValue / UnixEpoch
// ---------------------------------------------------------------------------

TEST(DateTimeTests, MinValue_IsZeroTicks) {
    EXPECT_EQ(DateTime::MinValue.getTicksProperty(), 0LL);
}

TEST(DateTimeTests, MaxValue_Is_9999_12_31) {
    EXPECT_EQ(DateTime::MaxValue.getYearProperty(),  9999);
    EXPECT_EQ(DateTime::MaxValue.getMonthProperty(), 12);
    EXPECT_EQ(DateTime::MaxValue.getDayProperty(),   31);
}

TEST(DateTimeTests, UnixEpoch_Is_1970_01_01) {
    EXPECT_EQ(DateTime::UnixEpoch.getTicksProperty(), kUnixEpochTicks);
    EXPECT_EQ(DateTime::UnixEpoch.getYearProperty(),  1970);
    EXPECT_EQ(DateTime::UnixEpoch.getMonthProperty(), 1);
    EXPECT_EQ(DateTime::UnixEpoch.getDayProperty(),   1);
}

// ---------------------------------------------------------------------------
// Equals / CompareTo / GetHashCode
// ---------------------------------------------------------------------------

TEST(DateTimeTests, Equals_SameTicks) {
    DateTime a(1000LL), b(1000LL);
    EXPECT_TRUE(a.Equals(b));
}

TEST(DateTimeTests, Equals_DifferentTicks) {
    DateTime a(1000LL), b(2000LL);
    EXPECT_FALSE(a.Equals(b));
}

TEST(DateTimeTests, CompareTo_Less) {
    DateTime a(1000LL), b(2000LL);
    EXPECT_LT(a.CompareTo(b), 0);
}

TEST(DateTimeTests, CompareTo_Equal) {
    DateTime a(1000LL), b(1000LL);
    EXPECT_EQ(a.CompareTo(b), 0);
}

TEST(DateTimeTests, CompareTo_Greater) {
    DateTime a(2000LL), b(1000LL);
    EXPECT_GT(a.CompareTo(b), 0);
}

TEST(DateTimeTests, GetHashCode_SameTicksMatch) {
    DateTime a(123456789LL), b(123456789LL);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

// Replaces GetHashCode_DifferentTicksDiffer, which forbade a collision the contract permits.
// DateTime::GetHashCode folds 64 tick bits into 32 (`ticks ^ (ticks >> 32)`, DateTime.hpp:386),
// so collisions are not merely legal, they are reachable -- the second pair below is one. The
// contract direction is GetHashCode_SameTicksMatch above (docs/HashAssertionContractRule.md R2).
TEST(DateTimeTests, DifferentTicks_AreUnequal_AndMayShareAHash) {
    DateTime a(1000LL), b(2000LL);
    EXPECT_FALSE(a.Equals(b));
    EXPECT_NE(a, b);

    DateTime low(1LL), high(0x100000000LL);
    EXPECT_NE(low, high);
    EXPECT_EQ(low.GetHashCode(), high.GetHashCode());
}

// ---------------------------------------------------------------------------
// IsLeapYear / DaysInMonth
// ---------------------------------------------------------------------------

TEST(DateTimeTests, IsLeapYear_DivisibleBy4) {
    EXPECT_TRUE(DateTime::IsLeapYear(2024));
}

TEST(DateTimeTests, IsLeapYear_DivisibleBy100NotLeap) {
    EXPECT_FALSE(DateTime::IsLeapYear(1900));
}

TEST(DateTimeTests, IsLeapYear_DivisibleBy400IsLeap) {
    EXPECT_TRUE(DateTime::IsLeapYear(2000));
}

TEST(DateTimeTests, IsLeapYear_OutOfRangeThrows) {
    EXPECT_THROW(DateTime::IsLeapYear(0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime::IsLeapYear(10000), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, DaysInMonth_February_LeapYear) {
    EXPECT_EQ(DateTime::DaysInMonth(2024, 2), 29);
}

TEST(DateTimeTests, DaysInMonth_February_NonLeapYear) {
    EXPECT_EQ(DateTime::DaysInMonth(2023, 2), 28);
}

TEST(DateTimeTests, DaysInMonth_January) {
    EXPECT_EQ(DateTime::DaysInMonth(2024, 1), 31);
}

TEST(DateTimeTests, DaysInMonth_OutOfRangeThrows) {
    EXPECT_THROW(DateTime::DaysInMonth(2024, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime::DaysInMonth(2024, 13), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// AddMonths / AddYears / AddTicks
// ---------------------------------------------------------------------------

TEST(DateTimeTests, AddMonths_SimpleForward) {
    DateTime dt(2024, 1, 15);
    DateTime r = dt.AddMonths(2);
    EXPECT_EQ(r.getYearProperty(),  2024);
    EXPECT_EQ(r.getMonthProperty(), 3);
    EXPECT_EQ(r.getDayProperty(),   15);
}

TEST(DateTimeTests, AddMonths_ClampsDay) {
    DateTime dt(2024, 1, 31);
    DateTime r = dt.AddMonths(1); // Feb 2024 has 29 days
    EXPECT_EQ(r.getMonthProperty(), 2);
    EXPECT_EQ(r.getDayProperty(),   29);
}

TEST(DateTimeTests, AddMonths_CrossYear) {
    DateTime dt(2024, 11, 15);
    DateTime r = dt.AddMonths(3);
    EXPECT_EQ(r.getYearProperty(),  2025);
    EXPECT_EQ(r.getMonthProperty(), 2);
}

TEST(DateTimeTests, AddMonths_Negative) {
    DateTime dt(2024, 3, 15);
    DateTime r = dt.AddMonths(-2);
    EXPECT_EQ(r.getMonthProperty(), 1);
}

TEST(DateTimeTests, AddMonths_PreservesTimeOfDay) {
    DateTime dt(2024, 1, 15, 10, 30, 0);
    DateTime r = dt.AddMonths(1);
    EXPECT_EQ(r.getHourProperty(),   10);
    EXPECT_EQ(r.getMinuteProperty(), 30);
}

TEST(DateTimeTests, AddMonths_OutOfRangeThrows) {
    DateTime dt(2024, 1, 15);
    EXPECT_THROW(dt.AddMonths(-200000), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, AddYears_LeapDayClamps) {
    DateTime dt(2020, 2, 29);
    DateTime r = dt.AddYears(1);
    EXPECT_EQ(r.getYearProperty(),  2021);
    EXPECT_EQ(r.getMonthProperty(), 2);
    EXPECT_EQ(r.getDayProperty(),   28);
}

TEST(DateTimeTests, AddYears_OutOfRangeThrows) {
    DateTime dt(2024, 1, 15);
    EXPECT_THROW(dt.AddYears(-20000), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, AddTicks_Simple) {
    DateTime dt(1000LL);
    DateTime r = dt.AddTicks(500LL);
    EXPECT_EQ(r.getTicksProperty(), 1500LL);
}

TEST(DateTimeTests, AddTicks_BelowMinThrows) {
    DateTime dt(0LL);
    EXPECT_THROW(dt.AddTicks(-1LL), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, AddTicks_AboveMaxThrows) {
    DateTime dt(DateTime::MaxValue);
    EXPECT_THROW(dt.AddTicks(1LL), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// Arithmetic operators
// ---------------------------------------------------------------------------

TEST(DateTimeTests, OperatorPlus_TimeSpan) {
    DateTime dt(kUnixEpochTicks);
    DateTime r = dt + TimeSpan::FromDays(1.0);
    EXPECT_EQ(r.getTicksProperty(), kUnixEpochTicks + kTicksPerDay);
}

TEST(DateTimeTests, OperatorMinus_TimeSpan) {
    DateTime dt(kUnixEpochTicks + kTicksPerDay);
    DateTime r = dt - TimeSpan::FromDays(1.0);
    EXPECT_EQ(r.getTicksProperty(), kUnixEpochTicks);
}

TEST(DateTimeTests, OperatorMinus_DateTime) {
    DateTime a(kUnixEpochTicks + kTicksPerDay);
    DateTime b(kUnixEpochTicks);
    TimeSpan diff = a - b;
    EXPECT_EQ(diff.getTicksProperty(), kTicksPerDay);
}

// ---------------------------------------------------------------------------
// CCF-002 class A (SR-AUD-006, ticket #1877) -- component-constructor range
// validation.
//
// Before this repair `dateToTicks` validated year/month/day and then multiplied
// hour/minute/second/millisecond straight into the tick sum. Every case below
// SUCCEEDED with a normalized (or out-of-invariant, or undefined-behaviour)
// value; each one is now rejected. Exception identity is asserted verbatim
// because it is copied from real .NET (ThrowHelper.cs:234-236 and
// DateTime.cs:207) and is byte-identical to TimeOnly's, which already had it.
// ---------------------------------------------------------------------------

namespace {

    // Returns the message/paramName pair a caller can actually observe, so an
    // assertion cannot pass merely because *some* ArgumentOutOfRangeException
    // escaped.
    struct Aoore {
        std::string paramName;
        std::string message;
        long long   hresult = 0;
        bool        thrown  = false;
    };

    template <typename Fn>
    Aoore captureAoore(Fn&& fn) {
        Aoore captured;
        try {
            fn();
        } catch (const System::ArgumentOutOfRangeException& e) {
            captured.paramName = e.getParamNameProperty();
            captured.message   = e.getMessageProperty();
            captured.hresult   = e.getHResultProperty();
            captured.thrown    = true;
        }
        return captured;
    }

    constexpr const char* kBadHms =
        "Hour, Minute, and Second parameters describe an un-representable DateTime.";
    constexpr const char* kBadMs =
        "Valid values are between 0 and 999, inclusive. (Parameter 'millisecond')";

} // namespace

TEST(DateTimeTests, Ccf002_HourAboveMaximumIsRejected) {
    EXPECT_THROW(DateTime(2024, 1, 1, 24, 0, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 24, 0, 0, 0), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_MinuteAboveMaximumIsRejected) {
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 60, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 60, 0, 0), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_SecondAboveMaximumIsRejected) {
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 0, 60), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 0, 60, 0), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_MillisecondAboveMaximumIsRejected) {
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 0, 0, 1000), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_NegativeTimeComponentsAreRejected) {
    // Previously these produced a value in the PREVIOUS YEAR: DateTime(2024,1,1,-1,0,0)
    // returned 2023-12-31 23:00:00.
    EXPECT_THROW(DateTime(2024, 1, 1, -1, 0, 0),    System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, -1, 0),    System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 0, -1),    System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 0, 0, -1), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_ExtremeIntegerHourIsRejectedInsteadOfOverflowing) {
    // `hour * TicksPerHour` overflowed int64 for |hour| > 256204778 -- real UB,
    // confirmed by UBSan at DateTime.cpp's multiplication, which then returned a
    // DateTime with a NEGATIVE tick count.
    EXPECT_THROW(DateTime(2024, 1, 1,  2000000000, 0, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, -2000000000, 0, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1,  256204779,  0, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, SharpRuntime::INTCS_MAX,   0, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, SharpRuntime::INTCS_MIN,   0, 0), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_ExtremeIntegerMinuteSecondMillisecondAreRejected) {
    EXPECT_THROW(DateTime(2024, 1, 1, 0, SharpRuntime::INTCS_MAX, 0),    System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, SharpRuntime::INTCS_MIN, 0),    System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 0, SharpRuntime::INTCS_MAX),    System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 0, 0, SharpRuntime::INTCS_MAX), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(2024, 1, 1, 0, 0, 0, SharpRuntime::INTCS_MIN), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_UpperTickInvariantCannotBeBreached) {
    // DateTime(9999,12,31,24,0,0) used to store MaxTicks + 1 = 3155378976000000000,
    // bypassing DateTime(longcs)'s own range check because the component
    // constructors initialise ticks_ directly.
    EXPECT_THROW(DateTime(9999, 12, 31, 24, 0, 0),          System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(9999, 12, 31, 23, 59, 59, 1000),  System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_LowerTickInvariantCannotBeBreached) {
    EXPECT_THROW(DateTime(1, 1, 1, -1, 0, 0),   System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateTime(1, 1, 1, 0, 0, 0, -1), System::ArgumentOutOfRangeException);
}

TEST(DateTimeTests, Ccf002_MinimumAndMaximumValidValuesStillConstruct) {
    EXPECT_EQ(DateTime(1, 1, 1, 0, 0, 0, 0).getTicksProperty(), 0LL);
    EXPECT_EQ(DateTime(9999, 12, 31, 23, 59, 59, 999).getTicksProperty(),
              3155378975999990000LL);
    // Every constructed value stays inside the documented invariant.
    EXPECT_LE(DateTime(9999, 12, 31, 23, 59, 59, 999).getTicksProperty(), DateTime::MaxTicks);
    EXPECT_GE(DateTime(1, 1, 1, 0, 0, 0, 0).getTicksProperty(), 0LL);
}

TEST(DateTimeTests, Ccf002_EndOfDayAndMidnightBoundariesStillConstruct) {
    EXPECT_EQ(DateTime(2024, 6, 15, 0, 0, 0, 0).getTicksProperty() % kTicksPerDay, 0LL);
    EXPECT_EQ(DateTime(2024, 6, 15, 23, 59, 59, 999).getTicksProperty() % kTicksPerDay,
              23LL * kTicksPerHour + 59LL * kTicksPerMinute + 59LL * kTicksPerSecond
                  + 999LL * 10000LL);
}

TEST(DateTimeTests, Ccf002_LeapDayAndMonthLengthsUnchanged) {
    EXPECT_NO_THROW(DateTime(2024, 2, 29, 23, 59, 59, 999)); // leap year
    EXPECT_THROW(DateTime(2023, 2, 29), System::ArgumentOutOfRangeException); // non-leap Feb 29
    EXPECT_THROW(DateTime(2024, 4, 31), System::ArgumentOutOfRangeException); // 30-day month
    EXPECT_NO_THROW(DateTime(2024, 1, 31, 23, 59, 59, 999));
    EXPECT_NO_THROW(DateTime(1900, 2, 28)); // century non-leap
    EXPECT_THROW(DateTime(1900, 2, 29), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(DateTime(2000, 2, 29)); // 400-year leap
}

TEST(DateTimeTests, Ccf002_ValidationOrderIsDateThenTimeThenMillisecond) {
    // .NET: DateToTicks runs before TimeToTicks, which runs before the millisecond
    // check. A caller passing several invalid components must see the FIRST one.
    const Aoore dateWins = captureAoore([] { (void)DateTime(2024, 13, 1, 24, 0, 0); });
    ASSERT_TRUE(dateWins.thrown);
    EXPECT_EQ(dateWins.paramName, "year");

    const Aoore yearWins = captureAoore([] { (void)DateTime(0, 1, 1, 24, 0, 0); });
    ASSERT_TRUE(yearWins.thrown);
    EXPECT_EQ(yearWins.paramName, "year");

    const Aoore timeWins = captureAoore([] { (void)DateTime(2024, 1, 1, 24, 60, 60, 1000); });
    ASSERT_TRUE(timeWins.thrown);
    EXPECT_EQ(timeWins.paramName, "");
    EXPECT_EQ(timeWins.message, kBadHms);
}

TEST(DateTimeTests, Ccf002_ExceptionIdentityMatchesTheReference) {
    const Aoore hour = captureAoore([] { (void)DateTime(2024, 1, 1, 24, 0, 0); });
    ASSERT_TRUE(hour.thrown);
    EXPECT_EQ(hour.paramName, "");
    EXPECT_EQ(hour.message, kBadHms);
    EXPECT_EQ(hour.hresult, static_cast<long long>(static_cast<SharpRuntime::intcs>(0x80131502)));

    const Aoore ms = captureAoore([] { (void)DateTime(2024, 1, 1, 0, 0, 0, 1000); });
    ASSERT_TRUE(ms.thrown);
    EXPECT_EQ(ms.paramName, "millisecond");
    EXPECT_EQ(ms.message, kBadMs);
    EXPECT_EQ(ms.hresult, static_cast<long long>(static_cast<SharpRuntime::intcs>(0x80131502)));
}

TEST(DateTimeTests, Ccf002_SixAndSevenArgumentOverloadsRejectIdentically) {
    for (int hour : {24, -1, 100000}) {
        EXPECT_THROW(DateTime(2024, 1, 1, hour, 0, 0),    System::ArgumentOutOfRangeException);
        EXPECT_THROW(DateTime(2024, 1, 1, hour, 0, 0, 0), System::ArgumentOutOfRangeException);
    }
}

TEST(DateTimeTests, Ccf002_ThrowingConstructionPublishesNoState) {
    DateTime sentinel(kUnixEpochTicks);
    EXPECT_THROW(sentinel = DateTime(2024, 1, 1, 24, 0, 0), System::ArgumentOutOfRangeException);
    EXPECT_EQ(sentinel.getTicksProperty(), kUnixEpochTicks);
}

// The parse consequence of the constructor repair. This is NOT a grammar change:
// P1 already funnelled every parsed component through the seven-argument
// constructor inside try/catch, so an out-of-range component value now reports
// failure instead of a normalized instant. "2024-06-15 2000000000:00:00"
// previously returned TRUE with a negative tick count after signed overflow.
TEST(DateTimeTests, Ccf002_ParserRejectsOutOfRangeComponentValues) {
    DateTime out;
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 25:00:00", out));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 24:00:00", out));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 10:99:00", out));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 10:20:99", out));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 2000000000:00:00", out));
    EXPECT_THROW((void)DateTime::Parse("2024-06-15 25:00:00"), System::FormatException);
}

TEST(DateTimeTests, Ccf002_ParserStillAcceptsEveryValidShape) {
    DateTime out;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15", out));
    EXPECT_EQ(out.getTicksProperty(), DateTime(2024, 6, 15).getTicksProperty());
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 23:59:59", out));
    EXPECT_EQ(out.getTicksProperty(), DateTime(2024, 6, 15, 23, 59, 59).getTicksProperty());
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T10:20:30.123", out));
    EXPECT_EQ(out.getTicksProperty(), DateTime(2024, 6, 15, 10, 20, 30, 123).getTicksProperty());
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T00:00:00", out));
    EXPECT_EQ(out.getTicksProperty(), DateTime(2024, 6, 15).getTicksProperty());
}

// ---------------------------------------------------------------------------
// CCF-002 class D (SR-AUD-007b, ticket #1879, approved 2026-07-31): the four
// date/time parsers consume the whole string or fail.
//
// std::sscanf was replaced by a full-consumption scanner
// (System/detail/DateTimeTextScanner.hpp). Two things went with it: the PREFIX
// acceptance that made "2024-06-15junk" a valid date, and the zero substitution
// that turned an unparseable time into MIDNIGHT -- a wrong answer a caller
// cannot detect. Every row of docs/DateTimeValidationBoundaryPlan.md §20.1 is
// pinned here in BOTH directions: the malformed input rejected, and the
// well-formed input still parsed to its exact previous value.
// ---------------------------------------------------------------------------

TEST(DateTimeTests, Ccf002d_TrailingTextIsRejected) {
    DateTime dt;
    EXPECT_FALSE(DateTime::TryParse("2024-06-15junk", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 trailing", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15T10:20:30zzzz", dt));
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:20:30 ", dt));
    EXPECT_EQ(dt.getTicksProperty(), DateTime(2024, 6, 15, 10, 20, 30).getTicksProperty());
    EXPECT_THROW(DateTime::Parse("2024-06-15junk"), System::FormatException);
}

TEST(DateTimeTests, Ccf002d_UnparseableTimeNoLongerFabricatesMidnight) {
    // The defect this ticket exists for: all three used to return true with a
    // time of 00:00:00 that appears nowhere in the input.
    DateTime dt;
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 10:xx:00", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 trailing", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15T::", dt));
    EXPECT_THROW(DateTime::Parse("2024-06-15 10:xx:00"), System::FormatException);
}

TEST(DateTimeTests, Ccf002d_MalformedFractionIsRejected) {
    DateTime dt;
    EXPECT_FALSE(DateTime::TryParse("2024-06-15T10:20:30.", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15T10:20:30.abc", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15T10:20:30.-1", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15T10:20:30.12345678", dt));
}

TEST(DateTimeTests, Ccf002d_SscanfLeniencyShapesAreGone) {
    // std::sscanf's "%d" skipped leading whitespace and accepted an explicit
    // sign, so all of these parsed -- " 024-06-15" as the year 24. None is part
    // of any documented grammar, and none is listed in §20.1; they go away as an
    // unavoidable consequence of replacing sscanf, and are pinned here so that
    // consequence is a decision rather than a surprise.
    DateTime dt;
    EXPECT_FALSE(DateTime::TryParse(" 024-06-15", dt));
    EXPECT_FALSE(DateTime::TryParse("+024-06-15", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 +1:20:30", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15  1:20:30", dt));
    // An out-of-int numeral was formally undefined behaviour in sscanf; the
    // scanner never reads more digits than fit, so it is simply not a match.
    EXPECT_FALSE(DateTime::TryParse("2024-06-15T99999999999999:20:30", dt));
}

TEST(DateTimeTests, Ccf002d_EveryDocumentedShapeKeepsItsExactValue) {
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15", dt));
    EXPECT_EQ(dt.getYearProperty(), 2024);
    EXPECT_EQ(dt.getMonthProperty(), 6);
    EXPECT_EQ(dt.getDayProperty(), 15);
    EXPECT_EQ(dt.getHourProperty(), 0);
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:20:30", dt));
    EXPECT_EQ(dt.getHourProperty(), 10);
    EXPECT_EQ(dt.getMinuteProperty(), 20);
    EXPECT_EQ(dt.getSecondProperty(), 30);
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T10:20:30", dt));
    EXPECT_EQ(dt.getSecondProperty(), 30);
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T10:20:30.1", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 100);
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T10:20:30.12", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 120);
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T10:20:30.123", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 123);
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T10:20:30Z", dt));
    EXPECT_EQ(dt.getSecondProperty(), 30);
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T10:20:30.123+02:00", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 123);
    ASSERT_TRUE(DateTime::TryParse("0001-01-01", dt));
    EXPECT_EQ(dt.getYearProperty(), 1);
    ASSERT_TRUE(DateTime::TryParse("9999-12-31 23:59:59", dt));
    EXPECT_EQ(dt.getYearProperty(), 9999);
    ASSERT_TRUE(DateTime::TryParse("2024-02-29", dt));
    EXPECT_EQ(dt.getDayProperty(), 29);
}

// FLIPPED by #1929 row 3 (decided 2026-08-18). The offset is still consumed in
// full and still ignored, but its shape is now .NET's ParseTimeZone grammar
// rather than a fixed +HH:MM.
TEST(DateTimeTests, Ccf002d_TrailingOffsetKeepsItsTwoDigitFields) {
    // §20.1's "unchanged in every option" list names the ±HH:MM offset, so it
    // stays accepted (and stays ignored -- this port has no DateTimeKind).
    DateTime dt;
    ASSERT_TRUE(DateTime::TryParse("2024-06-15 10:30:45.56+02:00", dt));
    EXPECT_EQ(dt.getMillisecondProperty(), 560);
    EXPECT_TRUE(DateTime::TryParse("2024-06-15T10:20:30+2:5", dt));
    // Trailing content after the offset is still not an offset.
    EXPECT_FALSE(DateTime::TryParse("2024-06-15T10:20:30+02:00junk", dt));
}

// #1929 row 5 (approved 2026-08-01): whitespace is a boundary rule, not an
// internal-token rule. Parse and TryParse must agree on the exact tick value.
TEST(DateTimeTests, Approved1929_OuterWhitespaceOnlyIsAccepted) {
    const long long expected = DateTime(2024, 6, 15, 1, 2, 3).AddTicks(1'234'567).getTicksProperty();
    const char* accepted[] = {
        " 2024-06-15T1:2:3.1234567 ",
        "\t2024-06-15T1:2:3.1234567\r\n",
        "\f\v2024-06-15T1:2:3.1234567\v",
    };
    for (const char* text : accepted) {
        DateTime out(17);
        ASSERT_TRUE(DateTime::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), expected) << text;
        EXPECT_EQ(DateTime::Parse(text).getTicksProperty(), expected) << text;
    }

    DateTime out(17);
    const char* rejected[] = {
        "2024 -06-15T1:2:3", "2024-06-15 T 1:2:3", "2024-06-15T1 :2:3",
        "2024-06-15T1: 2:3", "2024-06-15T1:2: 3", " \t\r\n ",
        "2024-06-15T1:2:3.1234567 garbage"
    };
    for (const char* text : rejected) {
        EXPECT_FALSE(DateTime::TryParse(text, out)) << text;
        try {
            (void)DateTime::Parse(text);
            FAIL() << text;
        } catch (const System::FormatException& e) {
            EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u)) << text;
            EXPECT_EQ(std::string(e.what()), "String was not recognized as a valid DateTime: " +
                                             std::string(text)) << text;
        }
    }
}

// #1929 row 6: every clock field independently admits one or two digits. Zero,
// three digits, signs, malformed separators and trailing content remain outside
// the approved grammar.
TEST(DateTimeTests, Approved1929_ClockFieldWidthsAndBoundaries) {
    struct Accepted { const char* text; int hour; int minute; int second; };
    const Accepted accepted[] = {
        {"2024-06-15T0:0:0", 0, 0, 0},
        {"2024-06-15T1:02:03", 1, 2, 3},
        {"2024-06-15T01:2:03", 1, 2, 3},
        {"2024-06-15T01:02:3", 1, 2, 3},
        {"2024-06-15 23:59:59", 23, 59, 59},
    };
    for (const auto& c : accepted) {
        DateTime out;
        ASSERT_TRUE(DateTime::TryParse(c.text, out)) << c.text;
        EXPECT_EQ(out.getHourProperty(), c.hour) << c.text;
        EXPECT_EQ(out.getMinuteProperty(), c.minute) << c.text;
        EXPECT_EQ(out.getSecondProperty(), c.second) << c.text;
        EXPECT_EQ(DateTime::Parse(c.text).getTicksProperty(), out.getTicksProperty()) << c.text;
    }

    DateTime out;
    const char* rejected[] = {
        "2024-06-15T:2:3", "2024-06-15T001:2:3", "2024-06-15T1::3",
        "2024-06-15T1:002:3", "2024-06-15T1:2:", "2024-06-15T1:2:003",
        "2024-06-15T+1:2:3", "2024-06-15T1:-2:3", "2024-06-15T1:2:+3",
        "2024-06-15T24:0:0", "2024-06-15T0:60:0", "2024-06-15T0:0:60",
        "2024-06-15T1-2-3", "2024-06-15T1:2:3x"
    };
    for (const char* text : rejected) EXPECT_FALSE(DateTime::TryParse(text, out)) << text;
}

TEST(DateTimeTests, Approved1929_FractionFormattingRoundTripsAtTickPrecision) {
    const DateTime value = DateTime(2024, 6, 15, 1, 2, 3).AddTicks(1'234'567);
    const std::string text = value.ToString("yyyy-MM-dd'T'HH:mm:ss.fffffff");
    EXPECT_EQ(text, "2024-06-15T01:02:03.1234567");
    EXPECT_EQ(DateTime::Parse(text).getTicksProperty(), value.getTicksProperty());

    DateTime out;
    ASSERT_TRUE(DateTime::TryParse("0001-01-01T0:0:0.0000001", out));
    EXPECT_EQ(out.getTicksProperty(), 1);
    ASSERT_TRUE(DateTime::TryParse("9999-12-31T23:59:59.9999999", out));
    EXPECT_EQ(out.getTicksProperty(), DateTime::MaxTicks);
}

// The non-approved #1929 rows remain pinned. Historical row 2's fractional
// behavior is superseded only by approved item (3); it is not a blanket policy
// accepting the remaining date/offset/API/culture shapes.
TEST(DateTimeTests, Approved1929_UnapprovedRowsRemainUnchanged) {
    DateTime out;
    // Rows 1 and 3 were decided on 2026-08-18 and moved to
    // Decided1929_DateAndOffsetGrammarMatchDotNet below. What is left here is
    // what the port still does NOT do: culture patterns, month names, a
    // two-digit year, and a format run wider than seven fractional digits.
    EXPECT_FALSE(DateTime::TryParse("June 15 2024 1:2:3", out));   // culture/wider grammar
    EXPECT_FALSE(DateTime::TryParse("24-06-15", out));             // two-digit year
    EXPECT_EQ(DateTime(1'234'567).ToString("ffffffff"), "123"); // unapproved >7 format run
    // Valid controls from the pre-approved subset remain accepted.
    EXPECT_TRUE(DateTime::TryParse("2024-06-15T01:02:03+02:05", out));
    EXPECT_TRUE(DateTime::TryParse("2024-06-15Z", out));
}

// #1929 rows 1 and 3, decided by the user on 2026-08-18: widen this port's date
// and offset grammar to .NET's. Both are PURE widenings -- every string that
// parsed before parses to the identical value -- which is why they needed no
// source break and no migration of callers.
//
// Row 1 is derived, not guessed. .NET's lexer classifies a run of one or two
// digits as a NumberToken and three or more as a YearNumberToken
// (Globalization/DateTimeParse.cs:5593-5605), so "2024-6-15" reaches the same
// year-month-day terminal state as "2024-06-15" and .NET accepts it.
TEST(DateTimeTests, Decided1929_SingleDigitMonthAndDayParseToTheSameValue) {
    const long long expected = DateTime(2024, 6, 5, 1, 2, 3).getTicksProperty();
    const char* spellings[] = {
        "2024-06-05T1:2:3", "2024-6-05T1:2:3", "2024-06-5T1:2:3", "2024-6-5T1:2:3"
    };
    for (const char* text : spellings) {
        DateTime out;
        ASSERT_TRUE(DateTime::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), expected) << text;
    }
    // A bare date takes the same widening.
    DateTime bare;
    ASSERT_TRUE(DateTime::TryParse("2024-6-5", bare));
    EXPECT_EQ(bare.getTicksProperty(), DateTime(2024, 6, 5).getTicksProperty());

    // The YEAR is deliberately NOT widened. .NET would read a one- or two-digit
    // run through Calendar.ToFourDigitYear's century window, which is culture
    // state this port has no way to carry, so a short year stays a failure.
    DateTime out;
    EXPECT_FALSE(DateTime::TryParse("24-06-15", out));
    EXPECT_FALSE(DateTime::TryParse("204-06-15", out));
    // Nor is a field widened past two digits.
    EXPECT_FALSE(DateTime::TryParse("2024-006-15", out));
    EXPECT_FALSE(DateTime::TryParse("2024-06-015", out));
}

// Row 3. The offset is transcribed from ParseTimeZone (DateTimeParse.cs:530-556):
// a one- or two-digit run is an hour and may be followed by ':' and a one- or
// two-digit minute; a three- or four-digit run is split value/100 and value%100.
// DateTime still IGNORES the offset -- it has no DateTimeKind (§16.4) -- so this
// test is about which strings are accepted, and DateTimeOffsetTests2 is where the
// values are pinned.
TEST(DateTimeTests, Decided1929_OffsetGrammarIsDotNetsParseTimeZone) {
    const long long expected = DateTime(2024, 6, 15, 10, 20, 30).getTicksProperty();
    const char* offsets[] = {
        "Z", "z", "+02:05", "+2:5", "+2:05", "+02:5",
        "+8", "-8", "+08", "+800", "+0800", "-0530", "+14:00", "-14:00",
        "+99", "+9959"   // ParseTimeZone's own ceiling; DateTimeOffset bounds it, DateTime does not
    };
    for (const char* suffix : offsets) {
        const std::string text = std::string("2024-06-15T10:20:30") + suffix;
        DateTime out;
        ASSERT_TRUE(DateTime::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), expected) << text;
    }

    DateTime out;
    const char* rejected[] = {
        "+", "-", "+2:", "+2:60", "+0860", "+12345", "+02:005",
        "+02:00junk", "+ 2:05", "+2 :05"
    };
    for (const char* suffix : rejected) {
        const std::string text = std::string("2024-06-15T10:20:30") + suffix;
        EXPECT_FALSE(DateTime::TryParse(text, out)) << text;
    }
}

TEST(DateTimeTests, Ccf002d_PreviouslyRejectedInputIsStillRejected) {
    DateTime dt;
    EXPECT_FALSE(DateTime::TryParse("", dt));
    EXPECT_FALSE(DateTime::TryParse("junk", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 25:00:00", dt));  // #1877
    EXPECT_FALSE(DateTime::TryParse("2024-06-15 10:99:00", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-13-01", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-02-30", dt));
    EXPECT_FALSE(DateTime::TryParse("2024-6-15garbage", dt));
}

// Ticket #1880: C# `out` is definitely assigned. Every non-throwing failure
// therefore publishes DateTime.MinValue, never a caller's prior successful value.
TEST(DateTimeTests, Ticket1880_TryParseFailureAlwaysAssignsMinValue) {
    DateTime out(638'540'436'301'234'567LL);
    const auto expectFailure = [&out](const char* text) {
        out = DateTime(638'540'436'301'234'567LL);
        EXPECT_FALSE(DateTime::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), DateTime::MinValue.getTicksProperty()) << text;
    };

    expectFailure("");                                      // initial scan / empty
    expectFailure(" \t\r\n ");                            // whitespace-only
    expectFailure("not-a-date");                            // malformed date
    expectFailure("2024-06-15T1:x:3");                      // malformed time
    expectFailure("2024-06-15T1:2:3.");                     // malformed fraction
    expectFailure("2024-06-15T1:2:3+2:60");                // malformed offset
    expectFailure("2024-06-15T1:2:3junk");                 // trailing content
    expectFailure("2024-02-30");                           // constructor rejection
    expectFailure("2024-06-15T1:2:3.12345678");            // precision boundary

    ASSERT_TRUE(DateTime::TryParse("0001-01-01T0:0:0", out));
    EXPECT_EQ(out.getTicksProperty(), DateTime::MinValue.getTicksProperty());
    ASSERT_TRUE(DateTime::TryParse("9999-12-31T23:59:59.9999999", out));
    EXPECT_EQ(out.getTicksProperty(), DateTime::MaxTicks);
    ASSERT_TRUE(DateTime::TryParse("2024-06-15T1:2:3.1234567", out));
    EXPECT_NE(out.getTicksProperty(), DateTime::MinValue.getTicksProperty());

    try {
        (void)DateTime::Parse("not-a-date");
        FAIL();
    } catch (const System::FormatException& e) {
        EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u));
        EXPECT_EQ(std::string(e.what()),
                  "String was not recognized as a valid DateTime: not-a-date");
    }
}

// ===========================================================================
// #1941 — DateTimeKind storage, phase 1 only (#1929 row 4D)
// ===========================================================================
//
// The approval is narrow and is quoted here because the boundary is the point: encode
// Unspecified/Utc/Local plus the reserved ambiguous-local marker in the high bits of an unsigned
// 64-bit payload, PRESERVING the measured layouts; add a Kind accessor, kind-taking construction
// and SpecifyKind; keep Ticks pure; range-check before packing; and audit arithmetic, comparison,
// hashing, formatting and serialization so previously kindless results stay unchanged.
//
// NOT approved and NOT present: ToLocalTime, ToUniversalTime, offset/Z parse conversion,
// AssumeLocal, AssumeUniversal, AdjustToUniversal, RoundtripKind parsing. A phase-2 approval must
// name a date-sensitive timezone provider first.

TEST(DateTimeKindStorageTests, Fix1941_TheLayoutIsUnchangedBecauseTheKindIsPacked) {
    // This is what makes the phase implementable at all. MaxTicks is 0x2BCA2875F4373FFF, so 62
    // bits carry every representable value and two are free -- .NET packs the kind into exactly
    // those (DateTime.cs:118-123). A separate member would have grown the object.
    EXPECT_EQ(sizeof(DateTime), 16u);
    EXPECT_EQ(alignof(DateTime), 8u);
    EXPECT_EQ(sizeof(System::DateTimeOffset), 48u);
}

TEST(DateTimeKindStorageTests, Fix1941_TicksStayPureAndEveryOldConstructorIsUnspecified) {
    // The whole safety claim: packing must not leak into the tick count anywhere.
    const SharpRuntime::longcs raw = 638540436301234567LL;
    EXPECT_EQ(DateTime(raw).getTicksProperty(), raw);
    EXPECT_EQ(DateTime(raw, System::DateTimeKind::Utc).getTicksProperty(), raw);
    EXPECT_EQ(DateTime(raw, System::DateTimeKind::Local).getTicksProperty(), raw);
    EXPECT_EQ(DateTime::MaxValue.getTicksProperty(), DateTime::MaxTicks);
    EXPECT_EQ(DateTime::MinValue.getTicksProperty(), 0);

    // MaxTicks with the Local flag set is the case that would break a mask off by one bit.
    EXPECT_EQ(DateTime(DateTime::MaxTicks, System::DateTimeKind::Local).getTicksProperty(),
              DateTime::MaxTicks);

    // Every constructor that does not take a kind produces Unspecified, which is .NET's default
    // and this port's previous universal behaviour -- so no existing value moved.
    EXPECT_EQ(DateTime().getKindProperty(), System::DateTimeKind::Unspecified);
    EXPECT_EQ(DateTime(raw).getKindProperty(), System::DateTimeKind::Unspecified);
    EXPECT_EQ(DateTime(2024, 6, 15).getKindProperty(), System::DateTimeKind::Unspecified);
    EXPECT_EQ(DateTime(2024, 6, 15, 1, 2, 3).getKindProperty(), System::DateTimeKind::Unspecified);
    EXPECT_EQ(DateTime::MaxValue.getKindProperty(), System::DateTimeKind::Unspecified);
}

TEST(DateTimeKindStorageTests, Fix1941_KindIsReportedAndSpecifyKindConvertsNothing) {
    const SharpRuntime::longcs raw = 638540436301234567LL;
    EXPECT_EQ(DateTime(raw, System::DateTimeKind::Utc).getKindProperty(), System::DateTimeKind::Utc);
    EXPECT_EQ(DateTime(raw, System::DateTimeKind::Local).getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(DateTime(raw, System::DateTimeKind::Unspecified).getKindProperty(),
              System::DateTimeKind::Unspecified);

    const DateTime source(raw);
    const DateTime utc = DateTime::SpecifyKind(source, System::DateTimeKind::Utc);
    EXPECT_EQ(utc.getKindProperty(), System::DateTimeKind::Utc);
    EXPECT_EQ(utc.getTicksProperty(), source.getTicksProperty())
        << "SpecifyKind must convert NOTHING -- that is the whole difference between phase 1 and "
           "the conversion phase this approval excludes";
    EXPECT_EQ(DateTime::SpecifyKind(utc, System::DateTimeKind::Unspecified).getKindProperty(),
              System::DateTimeKind::Unspecified);

    // An undeclared kind is rejected at both doors, with .NET's exact text and parameter name
    // (DateTime.cs:206,1309; SR.Argument_InvalidDateTimeKind).
    for (int bad : {3, 4, -1, 12345}) {
        const auto kind = static_cast<System::DateTimeKind>(bad);
        EXPECT_THROW((void)DateTime(raw, kind), System::ArgumentException) << bad;
        EXPECT_THROW((void)DateTime::SpecifyKind(source, kind), System::ArgumentException) << bad;
    }
    try {
        (void)DateTime(raw, static_cast<System::DateTimeKind>(3));
        FAIL();
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "kind");
        EXPECT_NE(std::string(e.what()).find("Invalid DateTimeKind value."), std::string::npos)
            << e.what();
    }
    // Value 3 is the RESERVED LocalAmbiguousDst encoding, and it is reserved rather than
    // reachable: nothing in phase 1 sets it, and the public constructor refuses it, exactly as
    // .NET's does. A consequence worth stating: the fold in getKindProperty() is UNREACHABLE in
    // this phase, so a mutation removing it is NOT caught, and was measured not to be. It is
    // transcribed because phase 2 will start setting that encoding, and a fold added later would
    // be a second change to a shipped accessor.

    // THE RANGE CHECK RUNS BEFORE PACKING, which the approval requires explicitly. Without it an
    // out-of-range tick count would collide with the flag bits and silently report a kind the
    // caller never asked for -- so this is a correctness row, not a validation nicety.
    EXPECT_THROW((void)DateTime(DateTime::MaxTicks + 1, System::DateTimeKind::Utc),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)DateTime(-1, System::DateTimeKind::Utc),
                 System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW((void)DateTime(DateTime::MaxTicks, System::DateTimeKind::Utc));
}

TEST(DateTimeKindStorageTests, Fix1941_TheKindDoesNotParticipateInAnyOldOperation) {
    // The audit clause. Comparison, equality, hashing, arithmetic and formatting must all give
    // the answers they gave before, which means none of them may see the flag bits.
    const SharpRuntime::longcs raw = 638540436301234567LL;
    const DateTime unspecified(raw);
    const DateTime asUtc(raw, System::DateTimeKind::Utc);
    const DateTime asLocal(raw, System::DateTimeKind::Local);

    EXPECT_TRUE(unspecified == asUtc) << ".NET's operator== is ((d1^d2) << 2) == 0, which "
                                         "discards the flag bits (DateTime.cs:1862)";
    EXPECT_TRUE(asUtc == asLocal);
    EXPECT_FALSE(unspecified != asLocal);
    EXPECT_EQ(unspecified.CompareTo(asLocal), 0);
    EXPECT_EQ(unspecified.GetHashCode(), asLocal.GetHashCode());
    EXPECT_EQ(unspecified.ToString(), asLocal.ToString());
    EXPECT_EQ(unspecified.getYearProperty(), asLocal.getYearProperty());
    EXPECT_EQ((asUtc - unspecified).getTicksProperty(), 0);
    EXPECT_EQ(asUtc.AddDays(1).getTicksProperty(), unspecified.AddDays(1).getTicksProperty());

    // Arithmetic does NOT carry the kind in this phase, and that is stated rather than assumed:
    // propagating it is a phase-2 question, because it is only meaningful once a conversion
    // exists to be consistent with.
    EXPECT_EQ(asUtc.AddDays(1).getKindProperty(), System::DateTimeKind::Unspecified);
}

namespace {
// The detection idiom must take a DEPENDENT parameter. gcc evaluates a non-dependent `requires`
// eagerly and reports "has no member named ToLocalTime" as a hard error instead of yielding
// false, which is the trap #2299 recorded for the same shape.
template <typename T> concept HasToLocalTime     = requires(T d) { d.ToLocalTime(); };
template <typename T> concept HasToUniversalTime = requires(T d) { d.ToUniversalTime(); };
} // namespace

TEST(DateTimeKindStorageTests, Decl1941_TheConversionSurfaceIsStillAbsent) {
    // Pinned as a declaration, so phase 2 is a deliberate act rather than a drift. If either of
    // these ever compiles, the approval boundary moved and this test must move with it.
    EXPECT_FALSE(HasToLocalTime<DateTime>);
    EXPECT_FALSE(HasToUniversalTime<DateTime>);
    // The control: a member that IS present, so the idiom is known to discriminate rather than
    // always answering false.
    EXPECT_TRUE((requires(DateTime d) { d.getKindProperty(); }));
}

// ===========================================================================================
// #1940 (SA-14 decision 1) -- a provider can now REACH the parser, and it is honoured
//
// The blocker was two things, and #1940's own record named only the first.
//   1. A COMPONENT CYCLE: DateTime is in Core.Base, DateTimeFormatInfo was in Globalization, and
//      Globalization already depends on Core.Base -- so naming it from here would have been a
//      cycle the boundary validator rejects. Shape C moved the two header files into Core.Base;
//      ownership is by logical path uniqueness, so NOT ONE INCLUDE LINE ANYWHERE CHANGED.
//   2. NOTHING IN THIS RUNTIME IMPLEMENTED IFormatProvider AT ALL -- not DateTimeFormatInfo, not
//      NumberFormatInfo, not CultureInfo. GetFormat had zero implementations, so a caller could
//      not construct a provider from a culture even in principle. That is why "shape A is
//      feasible today" was true and still not enough on its own.
// ===========================================================================================

TEST(DateTimeProvider1940Tests, TheProviderIsHonouredRatherThanAcceptedAndIgnored) {
    System::Globalization::DateTimeFormatInfo custom;
    custom.setMonthNamesProperty({"Janvier", "Fevrier", "Mars", "Avril", "Mai", "Juin", "Juillet",
                                   "Aout", "Septembre", "Octobre", "Novembre", "Decembre", ""});
    custom.setAbbreviatedMonthNamesProperty(
        {"jan", "fev", "mar", "avr", "mai", "jun", "jul", "aou", "sep", "oct", "nov", "dec", ""});

    const DateTime march(2024, 3, 15);
    EXPECT_EQ(march.ToString("MMMM", &custom), "Mars")
        << "the resolved DateTimeFormatInfo's month names must be what MMMM emits";
    EXPECT_EQ(march.ToString("MMM", &custom), "mar");

    // ...and the invariant answer is unchanged, which is the other half of the criterion.
    EXPECT_EQ(march.ToString("MMMM"), "March");
    EXPECT_EQ(march.ToString("MMM"), "Mar");
}

// The index base differs between the old hard-coded tables (1-based, empty slot 0) and
// DateTimeFormatInfo's arrays (0-based, empty slot 12, which is .NET's MonthNames convention).
// December and January are the two months an off-by-one gets wrong in opposite directions, and a
// mid-year spot check would miss both.
TEST(DateTimeProvider1940Tests, EveryMonthNameIndexesCorrectly) {
    for (int month = 1; month <= 12; ++month) {
        const DateTime date(2024, month, 1);
        EXPECT_EQ(date.ToString("MMMM"),
                  System::Globalization::DateTimeFormatInfo::getInvariantInfoProperty()
                      .getMonthNamesProperty()[static_cast<size_t>(month) - 1])
            << "month " << month;
        EXPECT_EQ(date.ToString("MMM"),
                  System::Globalization::DateTimeFormatInfo::getInvariantInfoProperty()
                      .getAbbreviatedMonthNamesProperty()[static_cast<size_t>(month) - 1])
            << "month " << month;
    }
    EXPECT_EQ(DateTime(2024, 1, 1).ToString("MMMM"), "January");
    EXPECT_EQ(DateTime(2024, 12, 1).ToString("MMMM"), "December");
}

// Day names are 0-based on BOTH sides, so the index did not move -- asserted so that a later
// "consistency" edit cannot shift them to match the month arithmetic.
TEST(DateTimeProvider1940Tests, EveryDayNameIndexesCorrectly) {
    // 2024-03-10 is a Sunday, so this walks the whole week from index 0.
    for (int offset = 0; offset < 7; ++offset) {
        const DateTime date(2024, 3, 10 + offset);
        const auto index = static_cast<size_t>(date.getDayOfWeekProperty());
        EXPECT_EQ(date.ToString("dddd"),
                  System::Globalization::DateTimeFormatInfo::getInvariantInfoProperty()
                      .getDayNamesProperty()[index]);
        EXPECT_EQ(date.ToString("ddd"),
                  System::Globalization::DateTimeFormatInfo::getInvariantInfoProperty()
                      .getAbbreviatedDayNamesProperty()[index]);
    }
    EXPECT_EQ(DateTime(2024, 3, 10).ToString("dddd"), "Sunday");
    EXPECT_EQ(DateTime(2024, 3, 16).ToString("dddd"), "Saturday");
}

// DateTimeFormatInfo.cs:307-323 -- the whole resolution chain, one row per branch.
TEST(DateTimeProvider1940Tests, TheResolutionChainIsDotNets) {
    using System::Globalization::DateTimeFormatInfo;
    const DateTime march(2024, 3, 15);

    // null -> current, which in this port is the invariant info.
    EXPECT_EQ(march.ToString("MMMM", nullptr), "March");
    EXPECT_EQ(&DateTimeFormatInfo::GetInstance(nullptr),
              &DateTimeFormatInfo::getCurrentInfoProperty());

    // a DateTimeFormatInfo IS the answer, without going through GetFormat.
    DateTimeFormatInfo itself;
    EXPECT_EQ(&DateTimeFormatInfo::GetInstance(&itself), &itself);

    // a provider that knows nothing about dates falls back to current rather than crashing.
    struct KnowsNothing : System::IFormatProvider {
        [[nodiscard]] void* GetFormat(const std::type_info&) const override { return nullptr; }
    } knowsNothing;
    EXPECT_EQ(&DateTimeFormatInfo::GetInstance(&knowsNothing),
              &DateTimeFormatInfo::getCurrentInfoProperty());
    EXPECT_EQ(march.ToString("MMMM", &knowsNothing), "March");
}

// DateTimeFormatInfo.cs:325-328 -- `formatType == typeof(DateTimeFormatInfo) ? this : null`. The
// NULL half needs its own row: a GetFormat that answers for ANYTHING satisfies every case above,
// because they only ever ask for the one type it is supposed to answer for.
TEST(DateTimeProvider1940Tests, GetFormatAnswersForItsOwnTypeAndNothingElse) {
    System::Globalization::DateTimeFormatInfo info;
    EXPECT_EQ(info.GetFormat(typeid(System::Globalization::DateTimeFormatInfo)), &info);
    EXPECT_EQ(info.GetFormat(typeid(int)), nullptr);
    EXPECT_EQ(info.GetFormat(typeid(std::string)), nullptr);
    EXPECT_EQ(info.GetFormat(typeid(System::DateTime)), nullptr);
}

// A CULTURE IS THE PROVIDER THIS WHOLE TICKET EXISTS FOR, and nothing above passed one -- so a
// CultureInfo::GetFormat that answered nullptr for dates went undetected. This is the route a
// caller uses to reach "the current culture's format info" from Core.Base without the component
// cycle: they pass the culture, and the culture answers.
TEST(DateTimeProvider1940Tests, ACultureInfoIsAUsableProvider) {
    System::Globalization::CultureInfo culture("de-DE");
    const void* fromCulture = culture.GetFormat(typeid(System::Globalization::DateTimeFormatInfo));
    ASSERT_NE(fromCulture, nullptr)
        << "a culture must answer for DateTimeFormatInfo, or it is not a provider at all";
    EXPECT_EQ(&System::Globalization::DateTimeFormatInfo::GetInstance(&culture), fromCulture);

    // ...and it really drives the output. The object is reached through the non-const `void*`
    // GetFormat hands back -- which is this interface's own contract -- so what is mutated here is
    // the culture's OWN member, not a copy. Identity alone would not show that the formatter reads
    // it; the changed output does.
    EXPECT_EQ(fromCulture, &culture.getDateTimeFormatProperty());
    auto* writable = static_cast<System::Globalization::DateTimeFormatInfo*>(
        culture.GetFormat(typeid(System::Globalization::DateTimeFormatInfo)));
    ASSERT_NE(writable, nullptr);
    writable->setMonthNamesProperty(
        {"Januar", "Februar", "Maerz", "April", "Mai", "Juni", "Juli", "August", "September",
         "Oktober", "November", "Dezember", ""});
    EXPECT_EQ(DateTime(2024, 3, 15).ToString("MMMM", &culture), "Maerz");

    // The number half of the same switch is asserted too, so a repair cannot satisfy one and
    // silently drop the other.
    EXPECT_NE(culture.GetFormat(typeid(System::Globalization::NumberFormatInfo)), nullptr);
    EXPECT_EQ(culture.GetFormat(typeid(int)), nullptr);
}

// ===========================================================================================
// #1941 PHASE 2 (SA-15.1) -- DateTime converts by its Kind, against a zone it is handed
//
// Phase 1 stored a Kind and converted nothing. The recorded blocker was "a date-sensitive
// timezone/DST model", and THAT PREMISE LOOKED AT THE WRONG TYPE: TimeZoneInfo::GetUtcOffset
// IGNORES its DateTime argument and IsDaylightSavingTime is always false -- both documented
// limitations on that type -- while System::TimeZone::CurrentTimeZone() resolves the offset AND
// the DST flag per instant on POSIX. It describes only the process-local zone, which is exactly
// the zone these two members convert against, so the model was present all along.
//
// The zone is taken EXPLICITLY. .NET's ToLocalTime() has no parameter because it reaches
// TimeZoneInfo.Local directly; Core.Base cannot name a zone type without a cycle, so the source
// is a parameter and the no-argument form stays absent. That is SA-15.1's accepted deviation, and
// the absence pin below still holds because it names the no-argument form.
// ===========================================================================================

namespace {

/// A zone with a fixed, known offset -- so the assertions are about DateTime's arithmetic and
/// Kind rules rather than about whatever zone this machine happens to be in.
class FixedZone : public System::ILocalTimeZone {
public:
    explicit FixedZone(SharpRuntime::longcs hours) : hours_(hours) {}
    [[nodiscard]] System::TimeSpan GetUtcOffset(const System::DateTime&) const override {
        return System::TimeSpan::FromHours(static_cast<double>(hours_));
    }
    [[nodiscard]] bool IsDaylightSavingTime(const System::DateTime&) const override { return false; }
private:
    SharpRuntime::longcs hours_;
};

/// A zone whose offset DEPENDS ON THE INSTANT, which is the property the blocker was about: a
/// date-insensitive implementation gives the same answer for both halves of the year.
class SummerTimeZone : public System::ILocalTimeZone {
public:
    [[nodiscard]] System::TimeSpan GetUtcOffset(const System::DateTime& t) const override {
        return System::TimeSpan::FromHours(t.getMonthProperty() >= 4 && t.getMonthProperty() <= 9
                                               ? 2.0
                                               : 1.0);
    }
    [[nodiscard]] bool IsDaylightSavingTime(const System::DateTime& t) const override {
        return t.getMonthProperty() >= 4 && t.getMonthProperty() <= 9;
    }
};

} // namespace

TEST(DateTimeKindPhase2Tests, ToLocalTimeShiftsAndStampsTheKind) {
    const FixedZone plusTwo(2);
    const DateTime utc = DateTime::SpecifyKind(DateTime(2024, 6, 15, 10, 0, 0), System::DateTimeKind::Utc);
    const DateTime local = utc.ToLocalTime(plusTwo);
    EXPECT_EQ(local.getHourProperty(), 12);
    EXPECT_EQ(local.getKindProperty(), System::DateTimeKind::Local);

    // Already local: returned unchanged, ticks and all (DateTime.cs:1707-1710).
    const DateTime again = local.ToLocalTime(plusTwo);
    EXPECT_EQ(again.getTicksProperty(), local.getTicksProperty());
    EXPECT_EQ(again.getKindProperty(), System::DateTimeKind::Local);
}

TEST(DateTimeKindPhase2Tests, ToUniversalTimeShiftsTheOtherWayAndStampsTheKind) {
    const FixedZone plusTwo(2);
    const DateTime local = DateTime::SpecifyKind(DateTime(2024, 6, 15, 12, 0, 0), System::DateTimeKind::Local);
    const DateTime utc = local.ToUniversalTime(plusTwo);
    EXPECT_EQ(utc.getHourProperty(), 10);
    EXPECT_EQ(utc.getKindProperty(), System::DateTimeKind::Utc);

    const DateTime again = utc.ToUniversalTime(plusTwo);
    EXPECT_EQ(again.getTicksProperty(), utc.getTicksProperty());
}

// THE TWO Unspecified RULES ARE NOT SYMMETRIC AND THAT IS .NET'S. ToLocalTime tests only the Local
// bit (:1707), so Unspecified converts as though it were UTC; ToUniversalTime returns early only
// for Utc (:1772), so Unspecified converts as though it were local. A repair that "harmonised"
// them would satisfy both cases above and fail here.
TEST(DateTimeKindPhase2Tests, UnspecifiedIsTreatedAsUtcOneWayAndLocalTheOther) {
    const FixedZone plusTwo(2);
    const DateTime unspecified(2024, 6, 15, 12, 0, 0);
    ASSERT_EQ(unspecified.getKindProperty(), System::DateTimeKind::Unspecified);

    EXPECT_EQ(unspecified.ToLocalTime(plusTwo).getHourProperty(), 14)
        << "ToLocalTime must read an Unspecified value as UTC and add the offset";
    EXPECT_EQ(unspecified.ToUniversalTime(plusTwo).getHourProperty(), 10)
        << "ToUniversalTime must read an Unspecified value as local and subtract the offset";
}

// The property the blocker was actually about: the offset must come from the INSTANT, not from
// the zone alone. A date-insensitive provider answers the same for both rows.
TEST(DateTimeKindPhase2Tests, TheOffsetIsTakenAtTheInstantNotFromTheZoneAlone) {
    const SummerTimeZone zone;
    const DateTime winter = DateTime::SpecifyKind(DateTime(2024, 1, 15, 10, 0, 0), System::DateTimeKind::Utc);
    const DateTime summer = DateTime::SpecifyKind(DateTime(2024, 7, 15, 10, 0, 0), System::DateTimeKind::Utc);

    EXPECT_EQ(winter.ToLocalTime(zone).getHourProperty(), 11);
    EXPECT_EQ(summer.ToLocalTime(zone).getHourProperty(), 12)
        << "the same zone must give a different offset in summer, or the model is date-insensitive";
}

// DateTime.cs:1718-1721 -- a conversion past either end CLAMPS rather than throwing. Both ends,
// because clamping one and throwing at the other is the plausible half-repair.
TEST(DateTimeKindPhase2Tests, AConversionPastTheRangeClampsRatherThanThrowing) {
    const FixedZone plusFourteen(14);
    const FixedZone minusFourteen(-14);

    const DateTime nearMax(DateTime::MaxTicks, System::DateTimeKind::Utc);
    DateTime clampedHigh = DateTime::MinValue;
    EXPECT_NO_THROW(clampedHigh = nearMax.ToLocalTime(plusFourteen));
    EXPECT_EQ(clampedHigh.getTicksProperty(), DateTime::MaxTicks);
    EXPECT_EQ(clampedHigh.getKindProperty(), System::DateTimeKind::Local);

    const DateTime nearMin(0LL, System::DateTimeKind::Utc);
    DateTime clampedLow = DateTime::MaxValue;
    EXPECT_NO_THROW(clampedLow = nearMin.ToLocalTime(minusFourteen));
    EXPECT_EQ(clampedLow.getTicksProperty(), 0);

    // THE BOUNDARY ITSELF, ONE TICK WIDE. A clamp written `< -1` instead of `< 0` still clamps
    // every ordinary underflow and differs on EXACTLY this input -- so without this row the
    // off-by-one is invisible, which is what a first run of the mutation set measured.
    class OneTickBehind : public System::ILocalTimeZone {
    public:
        [[nodiscard]] System::TimeSpan GetUtcOffset(const System::DateTime&) const override {
            return System::TimeSpan::FromTicks(-1);
        }
        [[nodiscard]] bool IsDaylightSavingTime(const System::DateTime&) const override {
            return false;
        }
    } oneTickBehind;
    EXPECT_EQ(nearMin.ToLocalTime(oneTickBehind).getTicksProperty(), 0)
        << "a one-tick underflow must clamp to MinValue, not wrap";
}

// The real local zone implements the interface, which is what makes the whole shape usable rather
// than a test-only abstraction. Only structural properties are asserted, because the machine's
// zone is an environment fact and SA-6 makes encoding one a defect in the test.
TEST(DateTimeKindPhase2Tests, TheRealLocalZoneIsAUsableILocalTimeZone) {
    const System::ILocalTimeZone& zone = System::TimeZone::CurrentTimeZone();
    const DateTime utc = DateTime::SpecifyKind(DateTime(2024, 6, 15, 10, 0, 0), System::DateTimeKind::Utc);
    const DateTime local = utc.ToLocalTime(zone);

    EXPECT_EQ(local.getKindProperty(), System::DateTimeKind::Local);
    // A round trip returns the instant, whatever this machine's offset is.
    EXPECT_EQ(local.ToUniversalTime(zone).getTicksProperty(), utc.getTicksProperty());
}
