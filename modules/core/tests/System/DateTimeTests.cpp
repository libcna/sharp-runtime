// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include <vector>
#include "System/Globalization/TimeSpanStyles.hpp"
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ArgumentNullException.hpp"
#include "System/ILocalTimeZone.hpp"
#include "System/TimeSpan.hpp"
#include "System/DateOnly.hpp"
#include "System/TimeOnly.hpp"
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
#include "System/detail/ProcessTimeZoneState.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
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

TEST(DateTimeTests, ConsecutiveNowReadsRemainOnTheSameLocalClockScale) {
    // Local wall-clock time is deliberately NOT monotonic across a daylight-saving fall-back.
    // Two immediate reads must nevertheless remain close; allow a full transition-sized jump.
    DateTime t1 = DateTime::getNowProperty();
    DateTime t2 = DateTime::getNowProperty();
    const auto delta = t2.getTicksProperty() - t1.getTicksProperty();
    EXPECT_GT(delta, -2 * kTicksPerHour);
    EXPECT_LT(delta,  2 * kTicksPerHour);
    EXPECT_EQ(t1.getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(t2.getKindProperty(), System::DateTimeKind::Local);
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
namespace {

// The TimeZone component uses this same save/set/tzset/restore shape for deterministic POSIX
// zone tests. EST5 is a POSIX fixed-offset rule, so this regression needs no installed tzdata and
// cannot accidentally move with daylight-saving rules.
class ScopedDateTimeTestZone {
public:
    explicit ScopedDateTimeTestZone(const char* zone) {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        const char* current = std::getenv("TZ");
        hadValue_ = current != nullptr;
        if (hadValue_) saved_ = current;
        (void)::setenv("TZ", zone, 1);
        ::tzset();
    }

    ScopedDateTimeTestZone(const ScopedDateTimeTestZone&) = delete;
    ScopedDateTimeTestZone& operator=(const ScopedDateTimeTestZone&) = delete;

    ~ScopedDateTimeTestZone() {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        if (hadValue_) (void)::setenv("TZ", saved_.c_str(), 1);
        else           (void)::unsetenv("TZ");
        ::tzset();
    }

private:
    std::string saved_;
    bool hadValue_ = false;
};

SharpRuntime::longcs systemClockTicks(
    const std::chrono::system_clock::time_point& value) {
    using namespace std::chrono;
    const auto sinceEpoch = value.time_since_epoch();
    const auto wholeSeconds = floor<seconds>(sinceEpoch);
    const auto remainder = sinceEpoch - wholeSeconds;
    return DateTime::UnixEpochTicks
        + static_cast<SharpRuntime::longcs>(wholeSeconds.count()) * DateTime::TicksPerSecond
        + static_cast<SharpRuntime::longcs>(duration_cast<nanoseconds>(remainder).count() / 100);
}

} // namespace

TEST(DateTimeTests, NowAndTodayUseLocalWallClockTicksAndLocalKind) {
    const ScopedDateTimeTestZone zone("EST5"); // fixed UTC-05:00, with no DST
    constexpr SharpRuntime::longcs offset = -5 * DateTime::TicksPerHour;

    const auto before = std::chrono::system_clock::now();
    const DateTime now = DateTime::getNowProperty();
    const DateTime today = DateTime::getTodayProperty();
    const auto after = std::chrono::system_clock::now();

    EXPECT_EQ(now.getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(today.getKindProperty(), System::DateTimeKind::Local);

    // A one-second scheduling margin keeps the assertion robust under a loaded sanitizer build;
    // the old UTC result is five HOURS away and cannot fit this interval.
    const auto earliestLocal = systemClockTicks(before) + offset - DateTime::TicksPerSecond;
    const auto latestLocal = systemClockTicks(after) + offset + DateTime::TicksPerSecond;
    EXPECT_GE(now.getTicksProperty(), earliestLocal);
    EXPECT_LE(now.getTicksProperty(), latestLocal);

    const auto firstPossibleDay = (earliestLocal / DateTime::TicksPerDay) * DateTime::TicksPerDay;
    const auto lastPossibleDay = (latestLocal / DateTime::TicksPerDay) * DateTime::TicksPerDay;
    EXPECT_TRUE(today.getTicksProperty() == firstPossibleDay ||
                today.getTicksProperty() == lastPossibleDay)
        << "Today must be the local date even if this test straddles local midnight";
    EXPECT_EQ(today.getTimeOfDayProperty().getTicksProperty(), 0);
}
#endif

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

TEST(DateTimeTests, ComponentDecompositionCoversTheFullYearOneTo9999Range) {
    struct Row {
        int year;
        int month;
        int day;
        System::DayOfWeek weekday;
        int dayOfYear;
    };
    for (const Row& row : {
             Row{1, 1, 1, System::DayOfWeek::Monday, 1},
             Row{1969, 12, 31, System::DayOfWeek::Wednesday, 365},
             Row{3001, 1, 1, System::DayOfWeek::Thursday, 1},
             Row{9999, 12, 31, System::DayOfWeek::Friday, 365}}) {
        const DateTime value(row.year, row.month, row.day, 23, 58, 57, 456);
        EXPECT_EQ(value.getYearProperty(), row.year);
        EXPECT_EQ(value.getMonthProperty(), row.month);
        EXPECT_EQ(value.getDayProperty(), row.day);
        EXPECT_EQ(value.getHourProperty(), 23);
        EXPECT_EQ(value.getMinuteProperty(), 58);
        EXPECT_EQ(value.getSecondProperty(), 57);
        EXPECT_EQ(value.getMillisecondProperty(), 456);
        EXPECT_EQ(value.getDayOfWeekProperty(), row.weekday);
        EXPECT_EQ(value.getDayOfYearProperty(), row.dayOfYear);
    }
}

TEST(DateTimeTests, UnixEpoch_Is_1970_01_01) {
    EXPECT_EQ(DateTime::UnixEpoch.getTicksProperty(), kUnixEpochTicks);
    EXPECT_EQ(DateTime::UnixEpoch.getKindProperty(), System::DateTimeKind::Utc);
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
    // stays accepted. General Parse intentionally leaves it Unspecified because this Core.Base
    // door has no implicit zone service; ParseExact's explicit-zone overload is the kind-aware
    // conversion surface.
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
    // what the general parser still does NOT do: culture patterns, month names and a
    // two-digit year. A custom format run wider than seven fractional digits is now rejected
    // explicitly rather than silently rendered as an unrelated tick string.
    EXPECT_FALSE(DateTime::TryParse("June 15 2024 1:2:3", out));   // culture/wider grammar
    EXPECT_FALSE(DateTime::TryParse("24-06-15", out));             // two-digit year
    EXPECT_THROW(DateTime(1'234'567).ToString("ffffffff"), System::FormatException);
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
// General DateTime Parse still consumes but does not apply the offset, returning Unspecified:
// Core.Base has no implicit zone service. This test is about which strings are accepted;
// DateTimeOffsetTests2 pins captured offset values, while ParseExact pins explicit-zone
// DateTime conversion.
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
// #1941 — DateTimeKind storage and the post-phase-1 existing-member ripple audit
// ===========================================================================
//
// The original phase-1 approval was narrow: encode
// Unspecified/Utc/Local plus the reserved ambiguous-local marker in the high bits of an unsigned
// 64-bit payload, PRESERVING the measured layouts; add a Kind accessor, kind-taking construction
// and SpecifyKind; keep Ticks pure; range-check before packing; and audit arithmetic, comparison,
// hashing, formatting and serialization so previously kindless results stay unchanged.
//
// That historical boundary is not the current contract. Phase 2 added explicit zone-taking
// conversion and #1942 added exact-parse styles. The post-#1941 ripple audit also corrected the
// existing static values and arithmetic to .NET's contract. Only the NO-ARGUMENT conversion forms
// remain absent; the declaration pin below names exactly those.

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
        << "SpecifyKind must convert NOTHING; callers use the explicit conversion members when "
           "they need the tick value shifted";
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
    // Value 3 is the RESERVED LocalAmbiguousDst encoding, and it remains unreachable through the
    // public constructor, exactly as in .NET. Current conversion also cannot produce it because
    // the available zone abstraction has no ambiguity result. The fold in getKindProperty() is
    // therefore still unreachable and a mutation removing it remains observationally equivalent;
    // arithmetic nevertheless preserves both raw flag bits for the future path that can set it.

    // THE RANGE CHECK RUNS BEFORE PACKING, which the approval requires explicitly. Without it an
    // out-of-range tick count would collide with the flag bits and silently report a kind the
    // caller never asked for -- so this is a correctness row, not a validation nicety.
    EXPECT_THROW((void)DateTime(DateTime::MaxTicks + 1, System::DateTimeKind::Utc),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)DateTime(-1, System::DateTimeKind::Utc),
                 System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW((void)DateTime(DateTime::MaxTicks, System::DateTimeKind::Utc));
}

TEST(DateTimeKindStorageTests, Fix1941_IdentityIgnoresKindButArithmeticPreservesIt) {
    // Kind does not participate in identity, comparison, hashing, formatting, component access,
    // or DateTime-DateTime subtraction. DateTime-returning arithmetic is different: .NET ORs
    // InternalKind into every result, including its hidden ambiguous-local marker.
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

    // Pin EVERY DateTime-returning arithmetic door, not just the three implementation roots.
    // Add/Add{units}/AddYears and the operators delegate today, but a later refactor must not be
    // allowed to make just one public spelling lose the kind again.
    for (const auto kind : {System::DateTimeKind::Utc, System::DateTimeKind::Local}) {
        const DateTime source(638540436301234567LL, kind);
        const auto expectKind = [&](const char* operation, const DateTime& result) {
            SCOPED_TRACE(operation);
            EXPECT_EQ(result.getKindProperty(), kind);
        };

        expectKind("Add", source.Add(TimeSpan::FromTicks(7)));
        expectKind("AddDays", source.AddDays(1));
        expectKind("AddHours", source.AddHours(1));
        expectKind("AddMinutes", source.AddMinutes(1));
        expectKind("AddSeconds", source.AddSeconds(1));
        expectKind("AddMilliseconds", source.AddMilliseconds(1));
        expectKind("AddTicks", source.AddTicks(1));
        expectKind("AddMonths", source.AddMonths(1));
        expectKind("AddYears", source.AddYears(1));
        expectKind("Subtract(TimeSpan)", source.Subtract(TimeSpan::FromTicks(7)));
        expectKind("operator+", source + TimeSpan::FromTicks(7));
        expectKind("operator-", source - TimeSpan::FromTicks(7));
    }
}

namespace {
// The detection idiom must take a DEPENDENT parameter. gcc evaluates a non-dependent `requires`
// eagerly and reports "has no member named ToLocalTime" as a hard error instead of yielding
// false, which is the trap #2299 recorded for the same shape.
template <typename T> concept HasToLocalTime     = requires(T d) { d.ToLocalTime(); };
template <typename T> concept HasToUniversalTime = requires(T d) { d.ToUniversalTime(); };
} // namespace

TEST(DateTimeKindStorageTests, Decl1941_TheNoArgumentConversionSurfaceIsStillAbsent) {
    // These concepts name only the NO-ARGUMENT .NET forms. Phase 2 deliberately added overloads
    // that take an ILocalTimeZone; Core.Base still cannot reach a zone on its own, so an argument-
    // free overload would move SA-15.1's recorded dependency boundary.
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

TEST(DateTimeProvider1940Tests, AStatefulProviderIsResolvedExactlyOncePerFormatCall) {
    using System::Globalization::DateTimeFormatInfo;
    DateTimeFormatInfo first;
    DateTimeFormatInfo second;
    first.setMonthNamesProperty(
        {"FirstJan", "FirstFeb", "FirstMar", "FirstApr", "FirstMay", "FirstJun",
         "FirstJul", "FirstAug", "FirstSep", "FirstOct", "FirstNov", "FirstDec", ""});
    second.setMonthNamesProperty(
        {"SecondJan", "SecondFeb", "SecondMar", "SecondApr", "SecondMay", "SecondJun",
         "SecondJul", "SecondAug", "SecondSep", "SecondOct", "SecondNov", "SecondDec", ""});

    struct AlternatingProvider final : System::IFormatProvider {
        DateTimeFormatInfo* first;
        DateTimeFormatInfo* second;
        mutable int calls = 0;

        AlternatingProvider(DateTimeFormatInfo* firstInfo, DateTimeFormatInfo* secondInfo)
            : first(firstInfo), second(secondInfo) {}

        [[nodiscard]] void* GetFormat(const std::type_info& type) const override {
            if (type != typeid(DateTimeFormatInfo)) return nullptr;
            ++calls;
            return calls == 1 ? first : second;
        }
    } provider(&first, &second);

    EXPECT_EQ(DateTime(2024, 6, 15).ToString("MMMM", &provider), "FirstJun");
    EXPECT_EQ(provider.calls, 1);
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

TEST(DateTimeKindPhase2Tests, ExtremeProviderOffsetsClampBeforeSignedArithmetic) {
    class ExtremeZone final : public System::ILocalTimeZone {
    public:
        explicit ExtremeZone(bool maximum) : maximum_(maximum) {}
        [[nodiscard]] System::TimeSpan GetUtcOffset(const System::DateTime&) const override {
            return maximum_ ? System::TimeSpan::MaxValue : System::TimeSpan::MinValue;
        }
        [[nodiscard]] bool IsDaylightSavingTime(const System::DateTime&) const override {
            return false;
        }
    private:
        bool maximum_;
    } maximum(true), minimum(false);

    const DateTime utcMiddle(DateTime::MaxTicks / 2, System::DateTimeKind::Utc);
    EXPECT_EQ(utcMiddle.ToLocalTime(maximum).getTicksProperty(), DateTime::MaxTicks);
    EXPECT_EQ(utcMiddle.ToLocalTime(minimum).getTicksProperty(), 0);

    const DateTime localMiddle(DateTime::MaxTicks / 2, System::DateTimeKind::Local);
    EXPECT_EQ(localMiddle.ToUniversalTime(maximum).getTicksProperty(), 0);
    EXPECT_EQ(localMiddle.ToUniversalTime(minimum).getTicksProperty(), DateTime::MaxTicks);
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

// ===========================================================================
// #2414 -- DateTime::ParseExact, the grammar-and-provider half
// ===========================================================================
//
// WHAT MADE THIS IMPOSSIBLE BEFORE, AND IT WAS THE SCANNER RATHER THAN THE TYPE.
// `detail::MatchExactFormat` took a `bool forDate` and ran ONE of two blocks: the date block
// handled `y`/`M`/`d` and REJECTED every time token as unsupported, and the time block did the
// mirror. So a format naming both families could not be matched by any spelling, which is why
// `DateTime` had no `ParseExact` at all -- not an omission but a consequence. The two families
// are disjoint (`M` is a month and `m` a minute; both languages are case-sensitive here), so
// admitting both is a matter of NOT REJECTING the other family, and the mutation that restores
// either rejection is caught below.

TEST(DateTimeParseExact2414Tests, DateAndTimeTokensMatchInOneFormat) {
    // The property the whole ticket exists for: one format, both families.
    const auto dt = System::DateTime::ParseExact("2024-06-15 13:45:30", "yyyy-MM-dd HH:mm:ss");
    EXPECT_EQ(dt.getYearProperty(), 2024);
    EXPECT_EQ(dt.getMonthProperty(), 6);
    EXPECT_EQ(dt.getDayProperty(), 15);
    EXPECT_EQ(dt.getHourProperty(), 13);
    EXPECT_EQ(dt.getMinuteProperty(), 45);
    EXPECT_EQ(dt.getSecondProperty(), 30);

    // `M` and `m` in ONE format is the case a shared gate would get wrong, and it is the reason
    // the two families can share a pass at all: the month is 06 and the minute 45, so a scanner
    // that folded case -- or that let one block consume the other's token -- gives a DIFFERENT
    // ANSWER here rather than merely failing, which is the shape that survives a careless repair.
    System::DateTime again = System::DateTime::MinValue;
    ASSERT_TRUE(System::DateTime::TryParseExact("06/15/2024 45", "MM/dd/yyyy mm", again));
    EXPECT_EQ(again.getMonthProperty(), 6);
    EXPECT_EQ(again.getMinuteProperty(), 45);
    EXPECT_EQ(again.getHourProperty(), 0);
}

TEST(DateTimeParseExact2414Tests, TimeComponentsDefaultToMidnightButTheDateIsMandatory) {
    // .NET's own rule: a date with no time is midnight, and this is a valid call rather than a
    // convenience -- `DateTime.ParseExact("2024-06-15", "yyyy-MM-dd")` returns 00:00:00.
    const auto dateOnly = System::DateTime::ParseExact("2024-06-15", "yyyy-MM-dd");
    EXPECT_EQ(dateOnly.getHourProperty(), 0);
    EXPECT_EQ(dateOnly.getMinuteProperty(), 0);
    EXPECT_EQ(dateOnly.getSecondProperty(), 0);

    // The reverse is NOT symmetric, and the asymmetry is deliberate: a format binding only a
    // time has no date to attach it to, and `NoCurrentDateDefault` -- the style that decides what
    // happens then -- is #1942's. Refusing it is what stops an invented default from shipping
    // under a ticket that has not been asked about it.
    System::DateTime unused = System::DateTime::MinValue;
    EXPECT_FALSE(System::DateTime::TryParseExact("13:45:30", "HH:mm:ss", unused));
    EXPECT_THROW(System::DateTime::ParseExact("13:45:30", "HH:mm:ss"), System::FormatException);
}

// INVERTED BY #1942 (SA-16.1), NOT DELETED -- this case was asserting the boundary that ticket
// exists to move. #2414 recorded that "this port's exact grammar carries no zone token at all,
// which is also why #1942's RoundtripKind would have nothing to preserve"; the grammar now has
// one, admitted at the doors that can carry a kind and still refused at the doors that cannot.
TEST(DateTimeParseExact2414Tests, ZoneTokensAreAdmittedForDateTimeAndStillRefusedElsewhere) {
    using System::Globalization::DateTimeStyles;

    // `g` (the era) is STILL rejected in every mode -- this port has no era table, and that is a
    // different absence from the zone one. Asserting them together is what kept the old case
    // honest and keeps this one honest.
    System::DateTime unused = System::DateTime::MinValue;
    EXPECT_FALSE(System::DateTime::TryParseExact("2024-06-15 A.D.", "yyyy-MM-dd g", unused));

    // A zone token is now MATCHED for DateTime. It needs a zone to convert against, so the
    // zone-less door reports that rather than silently failing to parse -- see the case below.
    System::DateTime dt = System::DateTime::MinValue;
    ASSERT_TRUE(System::DateTime::TryParseExact("2024-06-15T13:45:30Z", "yyyy-MM-ddTHH:mm:ssK",
                                                nullptr, DateTimeStyles::RoundtripKind, dt));
    EXPECT_EQ(dt.getKindProperty(), System::DateTimeKind::Utc);
    EXPECT_EQ(dt, System::DateTime(2024, 6, 15, 13, 45, 30));

    // ...and it is still refused for `DateOnly` and `TimeOnly`, which have no kind and no offset
    // to carry. Their doors leave `allowZoneToken` off, so the widening did not reach them.
    System::DateOnly d = System::DateOnly::MinValue;
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024-06-15Z", "yyyy-MM-ddK", d));
    System::TimeOnly t(0, 0);
    EXPECT_FALSE(System::TimeOnly::TryParseExact("13:45Z", "HH:mmK", t));
}

// The zone-less doors CANNOT CONVERT, so a converting input reports that rather than failing to
// parse. This is a diagnostic where #2414 had a silent mismatch, and it is the visible cost of
// SA-16.1's one deviation: the zone is a parameter, so a door without one says so.
TEST(DateTimeParseExact2414Tests, AConvertingInputThroughAZonelessDoorNamesTheMissingZone) {
    using System::Globalization::DateTimeStyles;
    System::DateTime unused = System::DateTime::MinValue;

    EXPECT_THROW(System::DateTime::TryParseExact("2024-06-15T13:45:30Z", "yyyy-MM-ddTHH:mm:ssK",
                                                 unused),
                 System::ArgumentNullException);
    try {
        System::DateTime::ParseExact("2024-06-15T13:45:30Z", "yyyy-MM-ddTHH:mm:ssK");
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "zone");
        // The message must tell the caller what to do, not merely that something is null.
        EXPECT_NE(std::string(e.what()).find("CurrentTimeZone"), std::string::npos) << e.what();
    }

    // RoundtripKind needs NO zone, because it stamps rather than converts -- so the same input and
    // the same door succeed once a non-converting style is named. That is what shows the throw is
    // about the conversion rather than about the token.
    EXPECT_NO_THROW(System::DateTime::ParseExact("2024-06-15T13:45:30Z", "yyyy-MM-ddTHH:mm:ssK",
                                                 nullptr, DateTimeStyles::RoundtripKind));
}

TEST(DateTimeParseExact2414Tests, StandardFormatsAreTheDateTimeTableNotTheTwoHalvesConcatenated) {
    // `s` uses a T separator and `u` a space, so the two cannot come from one rule -- which is
    // why this table is separate from the date-only and time-only one rather than derived.
    EXPECT_EQ(System::DateTime::ParseExact("2024-06-15T13:45:30", "s"),
              System::DateTime(2024, 6, 15, 13, 45, 30));
    EXPECT_EQ(System::DateTime::ParseExact("2024-06-15 13:45:30Z", "u"),
              System::DateTime(2024, 6, 15, 13, 45, 30));
    // `u`'s Z is a LITERAL, so it is required and it sets no kind.
    System::DateTime unused = System::DateTime::MinValue;
    EXPECT_FALSE(System::DateTime::TryParseExact("2024-06-15 13:45:30", "u", unused));

    // `R` ends in a literal GMT the date-only `R` does not have, and it VALIDATES the weekday:
    // 2024-06-15 is a Saturday, so naming Monday must fail rather than be ignored.
    EXPECT_EQ(System::DateTime::ParseExact("Sat, 15 Jun 2024 13:45:30 GMT", "R"),
              System::DateTime(2024, 6, 15, 13, 45, 30));
    EXPECT_FALSE(System::DateTime::TryParseExact("Mon, 15 Jun 2024 13:45:30 GMT", "R", unused));

    // `o` GOT ITS `K` BACK IN #1942. #2414 had to drop it -- the grammar carried no zone token,
    // so the pattern would not have compiled -- and its header said a later ticket adding one must
    // revisit this row. This is that row: `o` is now .NET's pattern in full, so `K`'s empty form
    // still reads an unspecified value AND the `Z` form is accepted, which #2414 pinned refused.
    EXPECT_EQ(System::DateTime::ParseExact("2024-06-15T13:45:30.1234567", "o").getTicksProperty(),
              System::DateTime(2024, 6, 15, 13, 45, 30).getTicksProperty() + 1234567);
    EXPECT_EQ(System::DateTime::ParseExact("2024-06-15T13:45:30.1234567", "o").getKindProperty(),
              System::DateTimeKind::Unspecified);
    ASSERT_TRUE(System::DateTime::TryParseExact(
        "2024-06-15T13:45:30.1234567Z", "o", nullptr,
        System::Globalization::DateTimeStyles::RoundtripKind, unused));
    EXPECT_EQ(unused.getKindProperty(), System::DateTimeKind::Utc);

    // A one-character format that is NOT standard stays refused rather than being read as a
    // custom single specifier -- .NET requires "%H" for that, and the rule is #1939's.
    EXPECT_FALSE(System::DateTime::TryParseExact("13", "H", unused));
    ASSERT_TRUE(System::DateTime::TryParseExact("2024-06-15", "yyyy-MM-dd", unused));
}

TEST(DateTimeParseExact2414Tests, TheProviderSuppliesTheNamesAndNullMeansInvariant) {
    using System::Globalization::DateTimeFormatInfo;

    // A null provider is the invariant culture, which is what .NET's own null means.
    EXPECT_EQ(System::DateTime::ParseExact("15 June 2024 13:45", "dd MMMM yyyy HH:mm", nullptr),
              System::DateTime(2024, 6, 15, 13, 45, 0));

    // A provider's month names are REACHED, which is the half that could not have been tested
    // before #2412 made the scanner take them: the invariant names must now fail and the
    // provider's must succeed, so the assertion cannot pass by the provider being ignored.
    DateTimeFormatInfo czech;
    czech.setMonthNamesProperty({"leden", "unor", "brezen", "duben", "kveten", "cerven",
                                 "cervenec", "srpen", "zari", "rijen", "listopad", "prosinec"});
    System::DateTime result = System::DateTime::MinValue;
    ASSERT_TRUE(System::DateTime::TryParseExact("15 cerven 2024 13:45", "dd MMMM yyyy HH:mm",
                                                &czech, result));
    EXPECT_EQ(result, System::DateTime(2024, 6, 15, 13, 45, 0));
    EXPECT_FALSE(System::DateTime::TryParseExact("15 June 2024 13:45", "dd MMMM yyyy HH:mm",
                                                 &czech, result));

    // A provider that knows nothing about dates falls back to the current info rather than
    // crashing -- #1940's rule, asserted here because this is a second door onto it.
    struct KnowsNothing : System::IFormatProvider {
        [[nodiscard]] void* GetFormat(const std::type_info&) const override { return nullptr; }
    } knowsNothing;
    EXPECT_EQ(System::DateTime::ParseExact("15 June 2024 13:45", "dd MMMM yyyy HH:mm",
                                           &knowsNothing),
              System::DateTime(2024, 6, 15, 13, 45, 0));
}

TEST(DateTimeParseExact2414Tests, TwelveHourClockIsNeitherPlusTwelveNorANoOp) {
    // 12 AM is hour 0 and 12 PM is hour 12, so BOTH ends are special cases and a body written as
    // a single `+ 12` gets one of them wrong while passing every ordinary row.
    EXPECT_EQ(System::DateTime::ParseExact("2024-06-15 12:00:00 AM", "yyyy-MM-dd hh:mm:ss tt")
                  .getHourProperty(), 0);
    EXPECT_EQ(System::DateTime::ParseExact("2024-06-15 12:00:00 PM", "yyyy-MM-dd hh:mm:ss tt")
                  .getHourProperty(), 12);
    EXPECT_EQ(System::DateTime::ParseExact("2024-06-15 01:00:00 PM", "yyyy-MM-dd hh:mm:ss tt")
                  .getHourProperty(), 13);
    EXPECT_EQ(System::DateTime::ParseExact("2024-06-15 01:00:00 AM", "yyyy-MM-dd hh:mm:ss tt")
                  .getHourProperty(), 1);

    // And an hour outside 1..12 is refused on the twelve-hour clock rather than wrapped.
    System::DateTime unused = System::DateTime::MinValue;
    EXPECT_FALSE(System::DateTime::TryParseExact("2024-06-15 13:00:00 PM",
                                                 "yyyy-MM-dd hh:mm:ss tt", unused));
}

// The single-family modes must be UNCHANGED by the widening, which is the regression the
// parameterisation could most plausibly have caused: `DateOnly` must still reject a time token
// and `TimeOnly` must still reject a date token, because for them the other family is not
// admitted at all -- the `!allowTime` / `!allowDate` guards are what preserve that.
TEST(DateTimeParseExact2414Tests, WideningDidNotOpenTheSingleFamilyModes) {
    System::DateOnly d = System::DateOnly::MinValue;
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024-06-15 13:45", "yyyy-MM-dd HH:mm", d));
    ASSERT_TRUE(System::DateOnly::TryParseExact("2024-06-15", "yyyy-MM-dd", d));

    System::TimeOnly t(0, 0);
    EXPECT_FALSE(System::TimeOnly::TryParseExact("2024-06-15 13:45", "yyyy-MM-dd HH:mm", t));
    ASSERT_TRUE(System::TimeOnly::TryParseExact("13:45", "HH:mm", t));
}

// ===========================================================================
// #1942 (SA-16.1) -- the exact-parsing DateTimeStyles contract for DateTime
// ===========================================================================
//
// A FIXED ZONE RATHER THAN THE MACHINE'S, on purpose. Asserting against
// `TimeZone::CurrentTimeZone()` would make every row depend on the container's tzdata -- the
// mistake #2351 repaired -- and a zone that is always +02:00 makes each conversion's DIRECTION
// and MAGNITUDE visible, which the process zone cannot do when it happens to be UTC.
//
// It REUSES #1941 phase 2's `FixedZone` rather than declaring a second one: two zone doubles in
// one file is the shape that lets two suites drift apart on what "fixed" means.

TEST(DateTimeStyles1942Tests, ValidateStylesHasThreeRulesAndThreeDifferentMessages) {
    using System::Globalization::DateTimeStyles;
    const std::string in = "2024-06-15", fmt = "yyyy-MM-dd";

    // Rule 1: an undefined bit.
    EXPECT_THROW(System::DateTime::ParseExact(in, fmt, nullptr,
                                              static_cast<DateTimeStyles>(0x4000)),
                 System::ArgumentException);
    // Rule 2: AssumeLocal and AssumeUniversal together.
    EXPECT_THROW(System::DateTime::ParseExact(
                     in, fmt, nullptr,
                     static_cast<DateTimeStyles>(static_cast<int>(DateTimeStyles::AssumeLocal) |
                                                 static_cast<int>(DateTimeStyles::AssumeUniversal))),
                 System::ArgumentException);
    // Rule 3: RoundtripKind with any of the other three. .NET writes this as an INTEGER
    // comparison on the masked value, which does not look like what it means, so all three
    // partners are asserted rather than one standing for the set.
    for (auto partner : {DateTimeStyles::AssumeLocal, DateTimeStyles::AssumeUniversal,
                         DateTimeStyles::AdjustToUniversal}) {
        EXPECT_THROW(System::DateTime::ParseExact(
                         in, fmt, nullptr,
                         static_cast<DateTimeStyles>(
                             static_cast<int>(DateTimeStyles::RoundtripKind) |
                             static_cast<int>(partner))),
                     System::ArgumentException)
            << static_cast<int>(partner);
    }

    // THE THREE MESSAGES DIFFER, which is what makes them three rules rather than one. A single
    // "invalid styles" text would satisfy every EXPECT_THROW above.
    auto messageFor = [&](DateTimeStyles s) {
        try { System::DateTime::ParseExact(in, fmt, nullptr, s); }
        catch (const System::ArgumentException& e) { return std::string(e.what()); }
        return std::string("<no throw>");
    };
    const auto m1 = messageFor(static_cast<DateTimeStyles>(0x4000));
    const auto m2 = messageFor(static_cast<DateTimeStyles>(
        static_cast<int>(DateTimeStyles::AssumeLocal) |
        static_cast<int>(DateTimeStyles::AssumeUniversal)));
    const auto m3 = messageFor(static_cast<DateTimeStyles>(
        static_cast<int>(DateTimeStyles::RoundtripKind) |
        static_cast<int>(DateTimeStyles::AdjustToUniversal)));
    EXPECT_NE(m1, m2);
    EXPECT_NE(m2, m3);
    EXPECT_NE(m1, m3);
}

TEST(DateTimeStyles1942Tests, AnIllegalStyleThrowsAndLeavesTheOutParameterAlone) {
    using System::Globalization::DateTimeStyles;
    System::DateTime sentinel(1999, 1, 1);

    EXPECT_THROW(System::DateTime::TryParseExact("2024-06-15", "yyyy-MM-dd", nullptr,
                                                 static_cast<DateTimeStyles>(0x4000), sentinel),
                 System::ArgumentException);
    // Two claims -- it throws, AND it did not write -- so the second needs its own assertion.
    EXPECT_EQ(sentinel, System::DateTime(1999, 1, 1));
}

// THE NO-ZONE CASE. Four of its five outcomes return WITHOUT converting anything, and the fifth
// is the row a reader most expects to be different: .NET's AssumeUniversal WITHOUT
// AdjustToUniversal falls through to the local adjustment, so the result is LOCAL, not Utc.
TEST(DateTimeStyles1942Tests, TheAssumeStylesStampFourWaysAndConvertOnce) {
    using System::Globalization::DateTimeStyles;
    using System::DateTimeKind;
    const FixedZone plusTwo(2);
    const std::string in = "2024-06-15T12:00:00", fmt = "yyyy-MM-ddTHH:mm:ss";
    const System::DateTime naive(2024, 6, 15, 12, 0, 0);

    // Neither Assume* style: Unspecified, ticks untouched, and no zone needed.
    const auto none = System::DateTime::ParseExact(in, fmt, nullptr, DateTimeStyles::None);
    EXPECT_EQ(none.getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(none.getTicksProperty(), naive.getTicksProperty());

    // AssumeLocal alone STAMPS and returns -- .NET returns early here, so the ticks do not move.
    const auto asLocal = System::DateTime::ParseExact(in, fmt, nullptr,
                                                      DateTimeStyles::AssumeLocal);
    EXPECT_EQ(asLocal.getKindProperty(), DateTimeKind::Local);
    EXPECT_EQ(asLocal.getTicksProperty(), naive.getTicksProperty());

    // AssumeUniversal WITH AdjustToUniversal also stamps and returns.
    const auto utcAdjusted = System::DateTime::ParseExact(
        in, fmt, nullptr,
        static_cast<DateTimeStyles>(static_cast<int>(DateTimeStyles::AssumeUniversal) |
                                    static_cast<int>(DateTimeStyles::AdjustToUniversal)));
    EXPECT_EQ(utcAdjusted.getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(utcAdjusted.getTicksProperty(), naive.getTicksProperty());

    // AssumeLocal WITH AdjustToUniversal CONVERTS: 12:00 local at +02:00 is 10:00 UTC.
    const auto toUtc = System::DateTime::ParseExact(
        in, fmt, nullptr,
        static_cast<DateTimeStyles>(static_cast<int>(DateTimeStyles::AssumeLocal) |
                                    static_cast<int>(DateTimeStyles::AdjustToUniversal)),
        &plusTwo);
    EXPECT_EQ(toUtc.getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(toUtc, System::DateTime(2024, 6, 15, 10, 0, 0));

    // AssumeUniversal ALONE is the surprising row and it is .NET's: the value is read as UTC and
    // then adjusted to LOCAL, so 12:00 becomes 14:00 and the kind is Local rather than Utc.
    const auto assumedUtc = System::DateTime::ParseExact(
        in, fmt, nullptr, DateTimeStyles::AssumeUniversal, &plusTwo);
    EXPECT_EQ(assumedUtc.getKindProperty(), DateTimeKind::Local);
    EXPECT_EQ(assumedUtc, System::DateTime(2024, 6, 15, 14, 0, 0));
}

// THE ZONE-CARRYING CASE, where the Assume* styles do NOT apply at all -- .NET says so in its own
// comment. Three outcomes, and the first turns on a distinction no value-based test can see.
TEST(DateTimeStyles1942Tests, RoundtripKindFiresOnlyForALiteralZ) {
    using System::Globalization::DateTimeStyles;
    using System::DateTimeKind;
    const FixedZone plusTwo(2);
    const std::string fmt = "yyyy-MM-ddTHH:mm:ssK";

    // A literal `Z` roundtrips: kind Utc, ticks untouched, no zone needed.
    const auto z = System::DateTime::ParseExact("2024-06-15T12:00:00Z", fmt, nullptr,
                                                DateTimeStyles::RoundtripKind);
    EXPECT_EQ(z.getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(z, System::DateTime(2024, 6, 15, 12, 0, 0));

    // `+00:00` NAMES THE SAME INSTANT AND IS NOT THE SAME THING. .NET's RoundtripKind tests
    // ParseFlags.TimeZoneUtc, which only a literal `Z` sets -- so a numeric zero offset is
    // converted rather than stamped, and comes back as LOCAL. No assertion about the VALUE could
    // separate these two, because both name 12:00 UTC.
    const auto zeroOffset = System::DateTime::ParseExact("2024-06-15T12:00:00+00:00", fmt, nullptr,
                                                         DateTimeStyles::RoundtripKind, &plusTwo);
    EXPECT_EQ(zeroOffset.getKindProperty(), DateTimeKind::Local);
    EXPECT_EQ(zeroOffset, System::DateTime(2024, 6, 15, 14, 0, 0));

    // AdjustToUniversal on a zone-qualified input removes the offset and stops there.
    const auto toUtc = System::DateTime::ParseExact("2024-06-15T12:00:00+05:00", fmt, nullptr,
                                                    DateTimeStyles::AdjustToUniversal);
    EXPECT_EQ(toUtc.getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(toUtc, System::DateTime(2024, 6, 15, 7, 0, 0));

    // ...and with no adjusting style it goes to LOCAL: 12:00+05:00 is 07:00 UTC is 09:00 at +02:00.
    const auto toLocal = System::DateTime::ParseExact("2024-06-15T12:00:00+05:00", fmt, nullptr,
                                                      DateTimeStyles::None, &plusTwo);
    EXPECT_EQ(toLocal.getKindProperty(), DateTimeKind::Local);
    EXPECT_EQ(toLocal, System::DateTime(2024, 6, 15, 9, 0, 0));
}

TEST(DateTimeStyles1942Tests, TheZoneTokenWidthsDifferAndAnOutOfRangeOffsetFails) {
    using System::Globalization::DateTimeStyles;
    const FixedZone utcZone(0);
    System::DateTime out = System::DateTime::MinValue;

    // `zzz` and `K` carry `:mm`; `z` and `zz` do not. Collapsing them into one arm is the easy
    // mistake and it changes which inputs parse.
    EXPECT_TRUE(System::DateTime::TryParseExact("2024-06-15 +05:30", "yyyy-MM-dd zzz", nullptr,
                                                DateTimeStyles::None, out, &utcZone));
    EXPECT_FALSE(System::DateTime::TryParseExact("2024-06-15 +05:30", "yyyy-MM-dd zz", nullptr,
                                                 DateTimeStyles::None, out, &utcZone));
    EXPECT_TRUE(System::DateTime::TryParseExact("2024-06-15 +05", "yyyy-MM-dd zz", nullptr,
                                                DateTimeStyles::None, out, &utcZone));
    EXPECT_TRUE(System::DateTime::TryParseExact("2024-06-15 +5", "yyyy-MM-dd %z", nullptr,
                                                DateTimeStyles::None, out, &utcZone));

    // An offset outside +-14:00 is a FAILURE rather than a clamp (DateTimeParse.cs:2776-2781).
    EXPECT_TRUE(System::DateTime::TryParseExact("2024-06-15 +14:00", "yyyy-MM-dd zzz", nullptr,
                                                DateTimeStyles::None, out, &utcZone));
    EXPECT_FALSE(System::DateTime::TryParseExact("2024-06-15 +15:00", "yyyy-MM-dd zzz", nullptr,
                                                 DateTimeStyles::None, out, &utcZone));

    // `+14:59` IS THE ROW THAT SEPARATES THE TWO GUARDS, and mutation M7 found that the case
    // above could not: the scanner's own `hours > 14` already refuses `+15:00`, so removing the
    // +-14:00 bound at the DateTime level changed nothing there. Fourteen hours and fifty-nine
    // minutes passes the coarse hour check and exceeds the exact bound, so only the second guard
    // can refuse it -- and the negative direction is asserted too, because a bound written on the
    // magnitude and a bound written on the signed value differ exactly at one end.
    EXPECT_FALSE(System::DateTime::TryParseExact("2024-06-15 +14:59", "yyyy-MM-dd zzz", nullptr,
                                                 DateTimeStyles::None, out, &utcZone));
    EXPECT_FALSE(System::DateTime::TryParseExact("2024-06-15 -14:59", "yyyy-MM-dd zzz", nullptr,
                                                 DateTimeStyles::None, out, &utcZone));
    EXPECT_TRUE(System::DateTime::TryParseExact("2024-06-15 -14:00", "yyyy-MM-dd zzz", nullptr,
                                                DateTimeStyles::None, out, &utcZone));

    // A zone token may bind at most once.
    EXPECT_FALSE(System::DateTime::TryParseExact("2024-06-15 +05:00 +05:00",
                                                 "yyyy-MM-dd zzz zzz", nullptr,
                                                 DateTimeStyles::None, out, &utcZone));
}

TEST(DateTimeStyles1942Tests, TryParseExactReturnsFalseWhenOffsetMovesPastDateTimeRange) {
    using System::Globalization::DateTimeStyles;
    const std::string format = "yyyy-MM-ddTHH:mm:ssK";
    System::DateTime result(2024, 6, 15);

    // Each wall clock is representable and each offset is within +/-14h; only the resulting UTC
    // instant is outside DateTime's range. A Try* door must not leak the checked arithmetic
    // exception used by ordinary DateTime.AddMinutes.
    EXPECT_FALSE(System::DateTime::TryParseExact(
        "0001-01-01T00:00:00+05:00", format, nullptr,
        DateTimeStyles::AdjustToUniversal, result));
    EXPECT_EQ(result, System::DateTime::MinValue);
    EXPECT_FALSE(System::DateTime::TryParseExact(
        "9999-12-31T23:59:59-05:00", format, nullptr,
        DateTimeStyles::AdjustToUniversal, result));
    EXPECT_EQ(result, System::DateTime::MinValue);

    EXPECT_THROW((void)System::DateTime::ParseExact(
                     "0001-01-01T00:00:00+05:00", format, nullptr,
                     DateTimeStyles::AdjustToUniversal),
                 System::FormatException);
    EXPECT_THROW((void)System::DateTime::ParseExact(
                     "9999-12-31T23:59:59-05:00", format, nullptr,
                     DateTimeStyles::AdjustToUniversal),
                 System::FormatException);
}

// ===========================================================================
// #2416 -- DateTime::ToString had no standard-format table at all
// ===========================================================================
//
// MEASURED BEFORE THE REPAIR: `ToString("o")` emitted the LITERAL "o", `ToString("s")` returned
// "0" -- reading `s` as SECONDS -- and `"%d"` rendered "%15". A one-character format was silently
// treated as a CUSTOM specifier where .NET treats it as a STANDARD one.
//
// THE TABLE ALREADY EXISTED AND WAS SIMPLY NOT CALLED. `DateTimeFormatInfo::
// GetAllDateTimePatterns(char)` carries all nineteen specifiers, culture-aware, so this was a
// WIRING repair rather than a new table -- and what it closed is the two halves of one type
// disagreeing: #2414 and #1942 gave the PARSE side its table, so `ParseExact(x, "o")` read .NET's
// roundtrip pattern while `ToString("o")` emitted the letter.

TEST(DateTimeToStringStandard2416Tests, EveryStandardSpecifierResolvesRatherThanEmittingItself) {
    const System::DateTime d(2024, 6, 15, 12, 0, 0);

    EXPECT_EQ(d.ToString("s"), "2024-06-15T12:00:00");
    EXPECT_EQ(d.ToString("R"), "Sat, 15 Jun 2024 12:00:00 GMT");
    EXPECT_EQ(d.ToString("r"), d.ToString("R"));
    EXPECT_EQ(d.ToString("u"), "2024-06-15 12:00:00Z");
    EXPECT_EQ(d.ToString("d"), "06/15/2024");
    EXPECT_EQ(d.ToString("D"), "Saturday, 15 June 2024");
    EXPECT_EQ(d.ToString("t"), "12:00");
    EXPECT_EQ(d.ToString("T"), "12:00:00");
    EXPECT_THROW(d.ToString("U"), System::FormatException)
        << "Core.Base cannot perform U's required implicit local-to-UTC conversion";

    // NONE of these may be the specifier letter itself, which is what the defect looked like and
    // is the assertion a table-shaped test would omit.
    for (const char* f : {"d", "D", "f", "F", "g", "G", "m", "M", "o", "O",
                          "r", "R", "s", "t", "T", "u", "y", "Y"}) {
        EXPECT_NE(d.ToString(f), std::string(f)) << f;
    }
}

// THE ROW THE WHOLE TICKET IS ABOUT: the two halves of one type must agree on what `o` means.
// Asserting the ROUND TRIP rather than the two tables separately is what makes that a property
// rather than two literals that happen to match today.
TEST(DateTimeToStringStandard2416Tests, TheFormatAndParseSidesAgreeOnTheRoundtripPattern) {
    using System::Globalization::DateTimeStyles;
    const auto utc = System::DateTime::SpecifyKind(System::DateTime(2024, 6, 15, 12, 0, 0),
                                                   System::DateTimeKind::Utc);

    const std::string written = utc.ToString("o");
    EXPECT_EQ(written, "2024-06-15T12:00:00.0000000Z");

    const auto back = System::DateTime::ParseExact(written, "o", nullptr,
                                                   DateTimeStyles::RoundtripKind);
    EXPECT_EQ(back, utc);
    EXPECT_EQ(back.getKindProperty(), System::DateTimeKind::Utc);

    // An UNSPECIFIED value writes no marker and reads back unspecified -- `K` emitting nothing is
    // the same rule as `K` matching the empty string on the parse side (#1942). One rule, two
    // halves, and this asserts them together.
    const System::DateTime naive(2024, 6, 15, 12, 0, 0);
    EXPECT_EQ(naive.ToString("o"), "2024-06-15T12:00:00.0000000");
    EXPECT_EQ(System::DateTime::ParseExact(naive.ToString("o"), "o").getKindProperty(),
              System::DateTimeKind::Unspecified);
}

// `%d` IS .NET'S SPELLING FOR A SINGLE CUSTOM SPECIFIER, and it exists precisely because a bare
// `d` is the short-date pattern -- without it there is no way to ask for an unpadded day at all.
// It used to render "%15", so the percent was emitted as a literal AND the day was read wrong.
TEST(DateTimeToStringStandard2416Tests, ThePercentEscapeSelectsASingleCustomSpecifier) {
    const System::DateTime d(2024, 6, 15, 12, 0, 0);

    EXPECT_EQ(d.ToString("%d"), "15");
    EXPECT_EQ(d.ToString("%y"), "24");
    EXPECT_EQ(d.ToString("%H"), "12");
    EXPECT_EQ(d.ToString("yyyy %d"), "2024 15");
    // ...and the bare letter is the STANDARD reading, so the pair must differ. Asserting only the
    // escape would pass against a body that ignored the `%` and got `d` right by accident.
    EXPECT_NE(d.ToString("%d"), d.ToString("d"));

    // `%%` is an error in .NET, and so is any unrecognised single character. Emitting it as a
    // literal -- which is what this did -- is neither the FormatException .NET raises nor the
    // ArgumentException GetAllDateTimePatterns raises: two members, two contracts.
    EXPECT_THROW(d.ToString("%%"), System::FormatException);
    EXPECT_THROW(d.ToString("yyyy %"), System::FormatException);
    EXPECT_THROW(d.ToString("yyyy %%"), System::FormatException);
    EXPECT_THROW(d.ToString("q"), System::FormatException);
    EXPECT_THROW(d.ToString("Z"), System::FormatException);
}

TEST(DateTimeToStringStandard2416Tests, TheCustomFormatterGainedTheTwoTokensItWasMissing) {
    // `tt` -- the parse side has read it since #1939 while the format side emitted a literal.
    EXPECT_EQ(System::DateTime(2024, 6, 15, 12, 0, 0).ToString("hh:mm tt"), "12:00 PM");
    EXPECT_EQ(System::DateTime(2024, 6, 15, 0, 30, 0).ToString("hh:mm tt"), "12:30 AM");
    EXPECT_EQ(System::DateTime(2024, 6, 15, 9, 5, 0).ToString("h:mm t"), "9:05 A");

    // `K` -- Utc writes `Z`, Unspecified writes NOTHING. A LOCAL value would need this process's
    // zone, which `Core.Base` cannot name -- SA-16.1's boundary, here with no parameter to carry
    // one -- so it is emitted as empty rather than guessed, and that is pinned rather than left
    // to be discovered.
    //
    // ...AND THE SPELLING IS `%K`, NOT `K`. A first cut of this case wrote the bare letter and
    // threw, because a one-character format is the STANDARD reading and `K` is not one of the
    // nineteen -- which is the very distinction this ticket exists to introduce. The test tripped
    // over its own subject, and the row is kept as evidence that the rule bites.
    const System::DateTime naive(2024, 6, 15, 12, 0, 0);
    EXPECT_THROW(naive.ToString("K"), System::FormatException);
    EXPECT_EQ(System::DateTime::SpecifyKind(naive, System::DateTimeKind::Utc).ToString("%K"), "Z");
    EXPECT_EQ(naive.ToString("%K"), "");
    EXPECT_EQ(System::DateTime::SpecifyKind(naive, System::DateTimeKind::Local).ToString("%K"), "");

    EXPECT_THROW(naive.ToString("zzz"), System::FormatException)
        << "Core.Base has no implicit zone with which to evaluate DateTime's offset token";
}

// A MULTI-CHARACTER format is still custom, so the widening did not reach the spelling that
// already worked -- the regression this repair could most plausibly have caused.
TEST(DateTimeToStringStandard2416Tests, MultiCharacterFormatsAreStillCustom) {
    const System::DateTime d(2024, 6, 15, 12, 0, 0);
    EXPECT_EQ(d.ToString("yyyy-MM-dd"), "2024-06-15");
    EXPECT_EQ(d.ToString("dd/MM/yyyy HH:mm:ss"), "15/06/2024 12:00:00");
    EXPECT_EQ(d.ToString("MMMM"), "June");
    EXPECT_EQ(d.ToString("yyyy \"MM\" 'dd'"), "2024 MM dd");
    EXPECT_THROW(d.ToString("yyyy 'unterminated"), System::FormatException);
    EXPECT_THROW(d.ToString("HH:mm:ss.ffffffff"), System::FormatException);
    // ...and the no-argument overload is untouched, keeping its space separator.
    EXPECT_EQ(d.ToString(), "2024-06-15 12:00:00");
}

// ===========================================================================
// #1944 -- multi-format ParseExact, across all five exact-parsing types
// ===========================================================================
//
// ONE LOOP FOR FIVE TYPES (`detail::MatchFirstOfManyFormats`), because the taxonomy is identical
// and five copies of one rule is how five doors come to disagree about what an empty element
// means. .NET writes it twice -- `TryParseExactMultiple` and `TryParseExactMultipleTimeSpan` --
// and the two agree, so sharing is faithful rather than a shortcut.

TEST(MultiFormatParseExact1944Tests, FirstSuccessWinsAndTheORDERIsWhatDecides) {
    using System::DateTime;
    const std::vector<std::string> formats = {"yyyy-MM-dd", "MM/dd/yyyy", "dd.MM.yyyy"};

    EXPECT_EQ(DateTime::ParseExact("2024-06-15", formats), DateTime(2024, 6, 15));
    EXPECT_EQ(DateTime::ParseExact("06/15/2024", formats), DateTime(2024, 6, 15));
    EXPECT_EQ(DateTime::ParseExact("15.06.2024", formats), DateTime(2024, 6, 15));

    // THE ROW THAT PROVES IT IS ORDERED rather than "whichever matches": an input two formats
    // both accept must be read by the FIRST of them. `01/02/2024` is 2 January under `MM/dd` and
    // 1 February under `dd/MM`, so the two orders give DIFFERENT DATES -- not merely a different
    // code path -- and a set-based implementation would be visibly wrong here.
    const std::vector<std::string> usFirst = {"MM/dd/yyyy", "dd/MM/yyyy"};
    const std::vector<std::string> euFirst = {"dd/MM/yyyy", "MM/dd/yyyy"};
    EXPECT_EQ(DateTime::ParseExact("01/02/2024", usFirst), DateTime(2024, 1, 2));
    EXPECT_EQ(DateTime::ParseExact("01/02/2024", euFirst), DateTime(2024, 2, 1));
}

// THE SUBTLE RULE, and the one a plausible implementation gets wrong: an EMPTY ELEMENT aborts the
// whole loop rather than being skipped. .NET returns SetBadFormatSpecifierFailure immediately.
TEST(MultiFormatParseExact1944Tests, AnEmptyElementAbortsRatherThanBeingSkipped) {
    using System::DateTime;
    DateTime out = DateTime::MinValue;

    // The empty element sits BEFORE a format that would have matched, so "skip it and carry on"
    // succeeds here and the correct rule fails. Putting it last would make the two agree and the
    // case would assert nothing.
    EXPECT_FALSE(DateTime::TryParseExact("2024-06-15", {"", "yyyy-MM-dd"}, nullptr,
                                         System::Globalization::DateTimeStyles::None, out));
    EXPECT_THROW(DateTime::ParseExact("2024-06-15", {"", "yyyy-MM-dd"}),
                 System::FormatException);

    // ...and with no empty element the same list works, so what fails is the element rather than
    // the shape of the call.
    EXPECT_TRUE(DateTime::TryParseExact("2024-06-15", {"dd.MM.yyyy", "yyyy-MM-dd"}, nullptr,
                                        System::Globalization::DateTimeStyles::None, out));
}

TEST(MultiFormatParseExact1944Tests, AnEmptyCollectionIsAFormatFailureNotAnArgumentOne) {
    using System::DateTime;
    DateTime out = DateTime::MinValue;

    // .NET's Format_NoFormatSpecifier -- easy to get wrong in the direction of ArgumentException,
    // which is what "no formats were supplied" sounds like.
    EXPECT_FALSE(DateTime::TryParseExact("2024-06-15", std::vector<std::string>{}, nullptr,
                                         System::Globalization::DateTimeStyles::None, out));
    EXPECT_THROW(DateTime::ParseExact("2024-06-15", std::vector<std::string>{}),
                 System::FormatException);

    // THE TWO FAILURE KINDS CARRY .NET'S TWO MESSAGES, and that is the only thing distinguishing
    // them -- both are FormatException and both make Try* return false. Measured while writing
    // this: with the empty-collection guard simply REMOVED, the loop body never runs and the
    // fall-through gives the same answer, so the guard would be a proven equivalence and #1944's
    // mutation M2 uncaught. Carrying .NET's own two messages makes it load-bearing, and it is
    // also the right diagnosis: telling a caller who supplied no formats that their INPUT was
    // unrecognised is wrong.
    auto messageFor = [](const std::vector<std::string>& formats) {
        try { (void)DateTime::ParseExact("2024-06-15", formats); }
        catch (const System::FormatException& e) { return std::string(e.what()); }
        return std::string("<no throw>");
    };
    EXPECT_NE(messageFor({}).find("No format specifiers were provided."), std::string::npos)
        << messageFor({});
    EXPECT_NE(messageFor({"dd.MM.yyyy"}).find("was not recognized"), std::string::npos)
        << messageFor({"dd.MM.yyyy"});
    EXPECT_NE(messageFor({}), messageFor({"dd.MM.yyyy"}));

    // An EMPTY ELEMENT is the same KIND as an empty collection -- the caller's formats are wrong,
    // not their input -- so it gets the format-specifier message rather than the parse one.
    EXPECT_NE(messageFor({"", "yyyy-MM-dd"}).find("No format specifiers were provided."),
              std::string::npos)
        << messageFor({"", "yyyy-MM-dd"});

    // An empty INPUT is an ordinary parse failure too, and the two are different rules that
    // happen to agree on the outcome -- asserted together so neither is mistaken for the other.
    EXPECT_FALSE(DateTime::TryParseExact("", {"yyyy-MM-dd"}, nullptr,
                                         System::Globalization::DateTimeStyles::None, out));
}

// THE STYLE IS VALIDATED ONCE, BEFORE THE LOOP, so an illegal style raises whatever the formats
// are -- INCLUDING an empty collection, where no single-format call would ever run. Validating
// inside the loop would make the exception depend on the format list, which is the shape a
// caller cannot reason about.
TEST(MultiFormatParseExact1944Tests, TheStyleIsValidatedBeforeTheLoopRuns) {
    using System::DateTime;
    using System::Globalization::DateTimeStyles;
    DateTime out = DateTime::MinValue;
    const auto bogus = static_cast<DateTimeStyles>(0x4000);

    EXPECT_THROW(DateTime::TryParseExact("2024-06-15", {"yyyy-MM-dd"}, nullptr, bogus, out),
                 System::ArgumentException);
    // The empty-collection case is the one that discriminates: with validation inside the loop
    // this would return false instead of raising, because the loop body never executes.
    EXPECT_THROW(DateTime::TryParseExact("2024-06-15", std::vector<std::string>{}, nullptr,
                                         bogus, out),
                 System::ArgumentException);
    EXPECT_THROW(DateTime::ParseExact("2024-06-15", std::vector<std::string>{}, nullptr,
                                      bogus),
                 System::ArgumentException)
        << "ParseExact validates styles before diagnosing an empty format collection too";
    EXPECT_EQ(out, DateTime::MinValue);
}

TEST(DateTimeKindRippleTests, ExactTwoDigitYearUsesTheCanonical2049Window) {
    EXPECT_EQ(DateTime::ParseExact("49-06-15", "yy-MM-dd"), DateTime(2049, 6, 15));
    EXPECT_EQ(DateTime::ParseExact("50-06-15", "yy-MM-dd"), DateTime(1950, 6, 15));
}

// A FAILED MULTI-FORMAT PARSE MUST NOT LEAVE A PARTIAL RESULT from a format that matched part of
// the way. The loop writes into a candidate and commits only on success, which is the same
// discipline every single-format door here already follows.
TEST(MultiFormatParseExact1944Tests, AFailedParseLeavesTheOutParameterAtItsDefault) {
    using System::DateTime;
    DateTime out(1999, 1, 1);
    EXPECT_FALSE(DateTime::TryParseExact("not a date", {"yyyy-MM-dd", "MM/dd/yyyy"}, nullptr,
                                         System::Globalization::DateTimeStyles::None, out));
    EXPECT_EQ(out, DateTime::MinValue);
}

// ALL FIVE TYPES SHARE THE LOOP, so all five must show it. A test covering only DateTime would
// pass against four copies that had drifted.
TEST(MultiFormatParseExact1944Tests, EveryExactParsingTypeHasTheSameTaxonomy) {
    using System::Globalization::DateTimeStyles;
    using System::Globalization::TimeSpanStyles;

    EXPECT_EQ(System::DateOnly::ParseExact("2024-06-15", {"dd.MM.yyyy", "yyyy-MM-dd"}),
              System::DateOnly(2024, 6, 15));
    EXPECT_EQ(System::TimeOnly::ParseExact("13:45", {"HH-mm", "HH:mm"}), System::TimeOnly(13, 45));
    EXPECT_EQ(System::TimeSpan::ParseExact("01:30", {"hh-mm", "hh':'mm"}),
              System::TimeSpan::FromMinutes(90));
    EXPECT_EQ(System::DateTimeOffset::ParseExact(
                  "2024-06-15T12:00:00Z", {"dd.MM.yyyy", "yyyy-MM-ddTHH:mm:ssK"})
                  .getOffsetProperty(),
              System::TimeSpan(static_cast<SharpRuntime::longcs>(0)));

    // ...and the empty-element rule holds on every one of them, which is the property the shared
    // loop exists to guarantee rather than to be assumed.
    System::DateOnly d = System::DateOnly::MinValue;
    System::TimeOnly t(0, 0);
    System::TimeSpan ts;
    System::DateTimeOffset dto;
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024-06-15", {"", "yyyy-MM-dd"}, nullptr,
                                                 DateTimeStyles::None, d));
    EXPECT_FALSE(System::TimeOnly::TryParseExact("13:45", {"", "HH:mm"}, nullptr,
                                                 DateTimeStyles::None, t));
    EXPECT_FALSE(System::TimeSpan::TryParseExact("01:30", {"", "hh':'mm"}, nullptr,
                                                 TimeSpanStyles::None, ts));
    EXPECT_FALSE(System::DateTimeOffset::TryParseExact("2024-06-15T12:00:00Z",
                                                       {"", "yyyy-MM-ddTHH:mm:ssK"}, nullptr,
                                                       DateTimeStyles::None, dto, nullptr));
}

// THE OVERLOAD-RESOLUTION HAZARD #1944's acceptance criteria anticipated ("compile ambiguity
// fixtures"), measured rather than reasoned about.
//
// Without the `std::initializer_list` overloads, `ParseExact(s, {"a", "b"})` is AMBIGUOUS: two
// `const char*` in braces match `std::basic_string(InputIt first, InputIt last)` over two
// UNRELATED pointers, so the single-format overload is a candidate -- and if it had ever won, the
// result would be undefined behaviour rather than a wrong answer. `{"one"}` was ambiguous too;
// three or more elements were not, and an explicit `std::vector<std::string>{...}` never was.
//
// A braced list binds to an `initializer_list` parameter by a LIST-INITIALIZATION SEQUENCE, which
// outranks any user-defined conversion, so the dangerous candidate can no longer win. These rows
// are compile-time evidence: each spelling below simply has to compile and give the right answer.
TEST(MultiFormatParseExact1944Tests, EveryBracedSpellingResolvesToTheMultiFormatReading) {
    using System::DateTime;
    const DateTime expected(2024, 6, 15);

    EXPECT_EQ(DateTime::ParseExact("2024-06-15", {"yyyy-MM-dd"}), expected);
    EXPECT_EQ(DateTime::ParseExact("2024-06-15", {"dd.MM.yyyy", "yyyy-MM-dd"}), expected);
    EXPECT_EQ(DateTime::ParseExact("2024-06-15", {"a.b", "c/d", "yyyy-MM-dd"}), expected);
    EXPECT_EQ(DateTime::ParseExact("2024-06-15", std::vector<std::string>{"yyyy-MM-dd"}), expected);
    EXPECT_EQ(DateTime::ParseExact("2024-06-15", {std::string("yyyy-MM-dd")}), expected);

    // ...and the SINGLE-format overload is still reachable, unbraced, which is the half a fix
    // aimed only at the braced spelling could have broken.
    EXPECT_EQ(DateTime::ParseExact("2024-06-15", "yyyy-MM-dd"), expected);
    EXPECT_EQ(DateTime::ParseExact("2024-06-15", std::string("yyyy-MM-dd")), expected);
}
