// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"

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
