// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <limits>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/DateOnly.hpp"
#include "System/Globalization/DateTimeFormatInfo.hpp"
#include "System/Globalization/DateTimeStyles.hpp"
#include "System/ArgumentException.hpp"
#include "System/DateTime.hpp"
#include "System/FormatException.hpp"
#include "System/TimeOnly.hpp"
#include "System/TimeSpan.hpp"

using System::DateOnly;
using System::DateTime;
using System::TimeOnly;
using System::TimeSpan;

// ===========================================================================
// DateOnly — AddDays / AddMonths / AddYears
// ===========================================================================

TEST(DateOnlyTests, Constructor_InvalidDayForMonth_Throws) {
    EXPECT_THROW(DateOnly(2023, 2, 30), System::ArgumentOutOfRangeException);
}

TEST(DateOnlyTests, Constructor_Feb29_NonLeapYear_Throws) {
    EXPECT_THROW(DateOnly(2023, 2, 29), System::ArgumentOutOfRangeException);
}

TEST(DateOnlyTests, Constructor_Feb29_LeapYear_Succeeds) {
    EXPECT_NO_THROW(DateOnly(2024, 2, 29));
}

TEST(DateOnlyTests, Constructor_MonthOutOfRange_Throws) {
    EXPECT_THROW(DateOnly(2023, 13, 1), System::ArgumentOutOfRangeException);
}

TEST(DateOnlyTests, TryParse_InvalidDayForMonth_ReturnsFalse) {
    DateOnly d;
    EXPECT_FALSE(DateOnly::TryParse("2023-02-30", d));
    EXPECT_FALSE(DateOnly::TryParse("2023-02-29", d)); // non-leap year
}

TEST(DateOnlyTests, AddDays_Positive) {
    DateOnly d(2024, 1, 30);
    DateOnly r = d.AddDays(3);
    EXPECT_EQ(r.getYearProperty(),  2024);
    EXPECT_EQ(r.getMonthProperty(), 2);
    EXPECT_EQ(r.getDayProperty(),   2);
}

TEST(DateOnlyTests, AddDays_Negative) {
    DateOnly d(2024, 3, 1);
    DateOnly r = d.AddDays(-1);
    EXPECT_EQ(r.getMonthProperty(), 2);
    EXPECT_EQ(r.getDayProperty(),   29); // 2024 is leap year
}

TEST(DateOnlyTests, AddDays_CrossYear) {
    DateOnly d(2023, 12, 31);
    DateOnly r = d.AddDays(1);
    EXPECT_EQ(r.getYearProperty(),  2024);
    EXPECT_EQ(r.getMonthProperty(), 1);
    EXPECT_EQ(r.getDayProperty(),   1);
}

TEST(DateOnlyTests, AddMonths_Positive) {
    DateOnly d(2024, 11, 15);
    DateOnly r = d.AddMonths(3);
    EXPECT_EQ(r.getYearProperty(),  2025);
    EXPECT_EQ(r.getMonthProperty(), 2);
    EXPECT_EQ(r.getDayProperty(),   15);
}

TEST(DateOnlyTests, AddMonths_ClampDay) {
    DateOnly d(2024, 1, 31);
    DateOnly r = d.AddMonths(1); // Feb has 29 days in 2024
    EXPECT_EQ(r.getMonthProperty(), 2);
    EXPECT_EQ(r.getDayProperty(),   29);
}

TEST(DateOnlyTests, AddMonths_Negative) {
    DateOnly d(2024, 3, 15);
    DateOnly r = d.AddMonths(-2);
    EXPECT_EQ(r.getMonthProperty(), 1);
}

TEST(DateOnlyTests, AddYears_Positive) {
    DateOnly d(2020, 2, 29); // leap day
    DateOnly r = d.AddYears(1);
    EXPECT_EQ(r.getYearProperty(),  2021);
    EXPECT_EQ(r.getMonthProperty(), 2);
    EXPECT_EQ(r.getDayProperty(),   28); // clamped
}

// ===========================================================================
// DateOnly — FromDateTime
// ===========================================================================

TEST(DateOnlyTests, FromDateTime_ExtractsDate) {
    DateTime dt(2025, 6, 13, 14, 30, 0);
    DateOnly d = DateOnly::FromDateTime(dt);
    EXPECT_EQ(d.getYearProperty(),  2025);
    EXPECT_EQ(d.getMonthProperty(), 6);
    EXPECT_EQ(d.getDayProperty(),   13);
}

// ===========================================================================
// DateOnly — Parse / TryParse
// ===========================================================================

TEST(DateOnlyTests, Parse_ValidIso) {
    DateOnly d = DateOnly::Parse("2025-06-13");
    EXPECT_EQ(d.getYearProperty(),  2025);
    EXPECT_EQ(d.getMonthProperty(), 6);
    EXPECT_EQ(d.getDayProperty(),   13);
}

TEST(DateOnlyTests, TryParse_Valid) {
    DateOnly d;
    EXPECT_TRUE(DateOnly::TryParse("2000-01-01", d));
    EXPECT_EQ(d.getYearProperty(), 2000);
}

TEST(DateOnlyTests, TryParse_Invalid_ReturnsFalse) {
    DateOnly d;
    EXPECT_FALSE(DateOnly::TryParse("not-a-date", d));
    EXPECT_FALSE(DateOnly::TryParse("20250613", d)); // missing dashes
}

TEST(DateOnlyTests, Parse_InvalidThrows) {
    EXPECT_THROW(DateOnly::Parse("bad"), System::FormatException);
}

// ===========================================================================
// DateOnly — ToString(format)
// ===========================================================================

TEST(DateOnlyTests, ToStringFormat_Iso) {
    DateOnly d(2025, 6, 3);
    EXPECT_EQ(d.ToString("yyyy-MM-dd"), "2025-06-03");
}

TEST(DateOnlyTests, ToStringFormat_ShortYear) {
    DateOnly d(2025, 1, 1);
    EXPECT_EQ(d.ToString("yy/M/d"), "25/1/1");
}

TEST(DateOnlyTests, ToStringFormat_Literal) {
    DateOnly d(2025, 12, 25);
    EXPECT_EQ(d.ToString("yyyy'.'MM'.'dd"), "2025.12.25");
}

TEST(DateOnlyTests, MinValue_Is_0001_01_01) {
    EXPECT_EQ(System::DateOnly::MinValue.getYearProperty(),  1);
    EXPECT_EQ(System::DateOnly::MinValue.getMonthProperty(), 1);
    EXPECT_EQ(System::DateOnly::MinValue.getDayProperty(),   1);
}

TEST(DateOnlyTests, MaxValue_Is_9999_12_31) {
    EXPECT_EQ(System::DateOnly::MaxValue.getYearProperty(),  9999);
    EXPECT_EQ(System::DateOnly::MaxValue.getMonthProperty(), 12);
    EXPECT_EQ(System::DateOnly::MaxValue.getDayProperty(),   31);
}

TEST(DateOnlyTests, DayNumber_MinValue_IsZero) {
    EXPECT_EQ(System::DateOnly::MinValue.getDayNumberProperty(), 0);
}

TEST(DateOnlyTests, FromDayNumber_RoundTrip) {
    System::DateOnly d(2025, 6, 14);
    int dn = d.getDayNumberProperty();
    EXPECT_EQ(System::DateOnly::FromDayNumber(dn), d);
}

TEST(DateOnlyTests, DayOfWeek_Known) {
    // 2025-06-14 is a Saturday
    System::DateOnly d(2025, 6, 14);
    EXPECT_EQ(d.getDayOfWeekProperty(), System::DayOfWeek::Saturday);
}

TEST(DateOnlyTests, DayOfYear_Jan1_IsOne) {
    EXPECT_EQ(System::DateOnly(2025, 1, 1).getDayOfYearProperty(), 1);
}

TEST(DateOnlyTests, DayOfYear_Dec31_NonLeap) {
    EXPECT_EQ(System::DateOnly(2025, 12, 31).getDayOfYearProperty(), 365);
}

TEST(DateOnlyTests, DayOfYear_Dec31_Leap) {
    EXPECT_EQ(System::DateOnly(2024, 12, 31).getDayOfYearProperty(), 366);
}

TEST(DateOnlyTests, CompareTo_Less) {
    System::DateOnly a(2020, 1, 1), b(2021, 1, 1);
    EXPECT_LT(a.CompareTo(b), 0);
}

TEST(DateOnlyTests, CompareTo_Equal) {
    System::DateOnly a(2020, 6, 15), b(2020, 6, 15);
    EXPECT_EQ(a.CompareTo(b), 0);
}

TEST(DateOnlyTests, CompareTo_Greater) {
    System::DateOnly a(2022, 1, 1), b(2021, 1, 1);
    EXPECT_GT(a.CompareTo(b), 0);
}

TEST(DateOnlyTests, Equals_SameDate) {
    System::DateOnly a(2025, 3, 1), b(2025, 3, 1);
    EXPECT_TRUE(a.Equals(b));
}

TEST(DateOnlyTests, Equals_DifferentDate) {
    System::DateOnly a(2025, 3, 1), b(2025, 3, 2);
    EXPECT_FALSE(a.Equals(b));
}

TEST(DateOnlyTests, Deconstruct) {
    System::DateOnly d(2025, 11, 7);
    System::intcs y, m, day;
    d.Deconstruct(y, m, day);
    EXPECT_EQ(y, 2025); EXPECT_EQ(m, 11); EXPECT_EQ(day, 7);
}

TEST(DateOnlyTests, GetHashCode_MatchesDayNumber) {
    System::DateOnly d(2025, 6, 14);
    EXPECT_EQ(d.GetHashCode(), d.getDayNumberProperty());
}

// GetHashCode_DifferentDatesDiffer was removed by #2284: GetHashCode_MatchesDayNumber directly
// above pins the hash to the day number exactly, which is strictly stronger than any statement
// that two particular dates do not collide (docs/HashAssertionContractRule.md R2/R3).

TEST(DateOnlyTests, ToDateTime_CombinesDateAndTime) {
    DateOnly d(2025, 6, 14);
    TimeOnly t(14, 30, 45, 500);
    DateTime dt = d.ToDateTime(t);
    EXPECT_EQ(dt.getYearProperty(),        2025);
    EXPECT_EQ(dt.getMonthProperty(),       6);
    EXPECT_EQ(dt.getDayProperty(),         14);
    EXPECT_EQ(dt.getHourProperty(),        14);
    EXPECT_EQ(dt.getMinuteProperty(),      30);
    EXPECT_EQ(dt.getSecondProperty(),      45);
    EXPECT_EQ(dt.getMillisecondProperty(), 500);
}

// ===========================================================================
// TimeOnly — AddHours / AddMinutes
// ===========================================================================

TEST(TimeOnlyTests, AddHours_Simple) {
    TimeOnly t(10, 0, 0);
    TimeOnly r = t.AddHours(3);
    EXPECT_EQ(r.getHourProperty(), 13);
}

TEST(TimeOnlyTests, AddHours_WrapAround) {
    TimeOnly t(23, 0, 0);
    TimeOnly r = t.AddHours(2);
    EXPECT_EQ(r.getHourProperty(), 1);
}

TEST(TimeOnlyTests, AddHours_Negative) {
    TimeOnly t(1, 0, 0);
    TimeOnly r = t.AddHours(-3);
    EXPECT_EQ(r.getHourProperty(), 22);
}

TEST(TimeOnlyTests, AddMinutes_Simple) {
    TimeOnly t(12, 45, 0);
    TimeOnly r = t.AddMinutes(20);
    EXPECT_EQ(r.getHourProperty(),   13);
    EXPECT_EQ(r.getMinuteProperty(),  5);
}

// ===========================================================================
// TimeOnly — FromTimeSpan / ToTimeSpan
// ===========================================================================

TEST(TimeOnlyTests, FromTimeSpan_TwoHours) {
    TimeOnly t = TimeOnly::FromTimeSpan(TimeSpan::FromHours(2));
    EXPECT_EQ(t.getHourProperty(),   2);
    EXPECT_EQ(t.getMinuteProperty(), 0);
}

TEST(TimeOnlyTests, ToTimeSpan_RoundTrip) {
    TimeOnly t(8, 30, 15, 0);
    TimeSpan ts = t.ToTimeSpan();
    EXPECT_EQ(static_cast<int>(ts.getTotalMinutesProperty()), 510);
}

// ===========================================================================
// TimeOnly — FromDateTime
// ===========================================================================

TEST(TimeOnlyTests, FromDateTime_ExtractsTime) {
    DateTime dt(2025, 6, 13, 14, 30, 45);
    TimeOnly t = TimeOnly::FromDateTime(dt);
    EXPECT_EQ(t.getHourProperty(),   14);
    EXPECT_EQ(t.getMinuteProperty(), 30);
    EXPECT_EQ(t.getSecondProperty(), 45);
}

// ===========================================================================
// TimeOnly — Parse / TryParse
// ===========================================================================

TEST(TimeOnlyTests, Parse_Valid) {
    TimeOnly t = TimeOnly::Parse("14:30:00");
    EXPECT_EQ(t.getHourProperty(),   14);
    EXPECT_EQ(t.getMinuteProperty(), 30);
    EXPECT_EQ(t.getSecondProperty(),  0);
}

TEST(TimeOnlyTests, Parse_WithMilliseconds) {
    TimeOnly t = TimeOnly::Parse("08:05:03.750");
    EXPECT_EQ(t.getMillisecondProperty(), 750);
}

TEST(TimeOnlyTests, TryParse_Valid) {
    TimeOnly t;
    EXPECT_TRUE(TimeOnly::TryParse("00:00:00", t));
}

TEST(TimeOnlyTests, TryParse_Invalid_ReturnsFalse) {
    TimeOnly t;
    EXPECT_FALSE(TimeOnly::TryParse("not-a-time", t));
}

TEST(TimeOnlyTests, Parse_InvalidThrows) {
    EXPECT_THROW(TimeOnly::Parse("bad"), System::FormatException);
}

// ===========================================================================
// TimeOnly — ToString(format)
// ===========================================================================

TEST(TimeOnlyTests, ToStringFormat_HHmmss) {
    TimeOnly t(8, 5, 3);
    EXPECT_EQ(t.ToString("HH:mm:ss"), "08:05:03");
}

TEST(TimeOnlyTests, ToStringFormat_12Hour) {
    TimeOnly t(14, 30, 0);
    EXPECT_EQ(t.ToString("hh':'mm"), "02:30");
}

TEST(TimeOnlyTests, ToStringFormat_WithMilliseconds) {
    TimeOnly t(12, 0, 0, 123);
    EXPECT_EQ(t.ToString("HH:mm:ss'.'fff"), "12:00:00.123");
}

// ===========================================================================
// DateOnly — CCF-004 class C defined arithmetic (SR-AUD-060, ticket #1837)
// ===========================================================================
//
// docs/DefinedArithmeticBoundaryPlan.md classes SR-AUD-060 as class C: seven signed-overflow
// sites across four public entry points (FromDayNumber :65, AddDays :76, AddMonths :81,
// AddYears :92) plus a three-multiplication cascade in jdnToDate (:35/:37/:39) reached when a
// wrapped or wildly out-of-range value flows into it. The repair uses defined arithmetic and
// range-checks each entry point *before* computing, so jdnToDate is only ever reached with an
// in-range day number. Two of the rejected inputs were silent WRONG ANSWERS rather than UB.
// Evidence: build-probe/1837_dateonly_surface.cpp, build-probe/1837_prefix.log vs _postfix.log.
//
// The pre-repair values pinned below were measured (case 9 of the surface probe), so
// "every valid result is unchanged" is proven rather than assumed.

// --- valid results must be byte-identical to before the repair ------------------------------

TEST(DateOnlyTests, DefinedArith_ValidResultsUnchanged_1837) {
    EXPECT_EQ(DateOnly::MaxValue.getDayNumberProperty(), 3652058); // the domain's upper bound
    EXPECT_EQ(DateOnly::MinValue.getDayNumberProperty(), 0);
    EXPECT_EQ(DateOnly::FromDayNumber(0), DateOnly(1, 1, 1));
    EXPECT_EQ(DateOnly::FromDayNumber(3652058), DateOnly(9999, 12, 31));
    EXPECT_EQ(DateOnly(2024, 1, 30).AddDays(3), DateOnly(2024, 2, 2));
    EXPECT_EQ(DateOnly(2024, 3, 1).AddDays(-1), DateOnly(2024, 2, 29)); // 2024 leap
    EXPECT_EQ(DateOnly(2024, 1, 31).AddMonths(1), DateOnly(2024, 2, 29)); // day clamped
    EXPECT_EQ(DateOnly(2024, 3, 15).AddMonths(-2), DateOnly(2024, 1, 15));
    EXPECT_EQ(DateOnly(2020, 2, 29).AddYears(1), DateOnly(2021, 2, 28)); // 29 Feb -> 28 Feb
    // The widest legal spans in both directions -- the guards must not reject these.
    EXPECT_EQ(DateOnly::MinValue.AddDays(3652058), DateOnly(9999, 12, 31));
    EXPECT_EQ(DateOnly::MaxValue.AddDays(-3652058), DateOnly(1, 1, 1));
    EXPECT_EQ(DateOnly::MinValue.AddDays(0), DateOnly::MinValue);
    EXPECT_EQ(DateOnly::MaxValue.AddDays(0), DateOnly::MaxValue);
}

// --- FromDayNumber: one unsigned compare rejects both directions ----------------------------

TEST(DateOnlyTests, FromDayNumber_OutOfRange_ThrowsArgumentOutOfRange_1837) {
    // Before the repair, FromDayNumber(INTCS_MAX) overflowed :65 and FromDayNumber(INTCS_MIN)
    // drove jdnToDate's :35/:37/:39 cascade; both happened to end in a "year" exception after
    // undefined behaviour. They now throw a clean "dayNumber" exception with no UB.
    try {
        (void)DateOnly::FromDayNumber(std::numeric_limits<System::intcs>::max());
        FAIL() << "Expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "dayNumber");
    }
    EXPECT_THROW(DateOnly::FromDayNumber(std::numeric_limits<System::intcs>::min()),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateOnly::FromDayNumber(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateOnly::FromDayNumber(3652059), System::ArgumentOutOfRangeException); // max+1
}

TEST(DateOnlyTests, FromDayNumber_BoundaryValuesSucceed_1837) {
    EXPECT_EQ(DateOnly::FromDayNumber(0), DateOnly::MinValue);
    EXPECT_EQ(DateOnly::FromDayNumber(3652058), DateOnly::MaxValue);
}

// --- AddDays: defined unsigned wrap + single compare ----------------------------------------

TEST(DateOnlyTests, AddDays_OutOfRange_ThrowsArgumentOutOfRange_1837) {
    // MaxValue.AddDays(INTCS_MAX) overflowed :76 and cascaded; MinValue.AddDays(INTCS_MIN)
    // drove the cascade directly. paramName is "value", matching .NET DateOnly.AddDays.
    try {
        (void)DateOnly::MaxValue.AddDays(1);
        FAIL() << "Expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "value");
    }
    EXPECT_THROW(DateOnly::MinValue.AddDays(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateOnly::MaxValue.AddDays(std::numeric_limits<System::intcs>::max()),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateOnly::MinValue.AddDays(std::numeric_limits<System::intcs>::min()),
                 System::ArgumentOutOfRangeException);
}

// --- AddMonths: bounded (no loop), rejects the input bound and the result -------------------

TEST(DateOnlyTests, AddMonths_DeltaBeyondBound_ThrowsMonths_1837) {
    // |value| > 120000 is rejected before any arithmetic -- this both removed the :81 overflow
    // (AddMonths(INTCS_MAX)) and bounded the normalisation that used to loop ~179M times
    // (AddMonths(INTCS_MIN)). Neither INTCS_MAX nor INTCS_MIN is a hang now.
    try {
        (void)DateOnly(1, 1, 1).AddMonths(std::numeric_limits<System::intcs>::max());
        FAIL() << "Expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "months");
    }
    EXPECT_THROW(DateOnly(1, 1, 1).AddMonths(std::numeric_limits<System::intcs>::min()),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateOnly(1, 1, 1).AddMonths(120001), System::ArgumentOutOfRangeException);
}

TEST(DateOnlyTests, AddMonths_InBoundButUnrepresentableResult_ThrowsMonths_1837) {
    // A delta inside +/-120000 whose result leaves [0001,9999] is rejected with "months".
    EXPECT_THROW(DateOnly(9999, 12, 1).AddMonths(1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateOnly(1, 1, 1).AddMonths(-1), System::ArgumentOutOfRangeException);
    // A delta inside the bound whose result stays valid still works.
    EXPECT_EQ(DateOnly(1, 1, 1).AddMonths(119987), DateOnly(9999, 12, 1));
}

// --- AddYears: the silent wrong answer, and the "value" paramName ---------------------------

TEST(DateOnlyTests, AddYears_MinValue_NoLongerSilentlyReturnsSameDate_1837) {
    // THE regression: DateOnly(1,1,1).AddYears(INTCS_MIN) computed INTCS_MIN*12 == -6*2^32,
    // which wrapped to ZERO and returned 0001-01-01 unchanged -- asking for a date 2.1 billion
    // years earlier and getting the same date back with no error. It must now throw.
    try {
        (void)DateOnly(1, 1, 1).AddYears(std::numeric_limits<System::intcs>::min());
        FAIL() << "Expected ArgumentOutOfRangeException (was silently 0001-01-01)";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "value");
    }
    EXPECT_THROW(DateOnly(1, 1, 1).AddYears(std::numeric_limits<System::intcs>::max()),
                 System::ArgumentOutOfRangeException);
}

TEST(DateOnlyTests, AddYears_BoundAndResultRejections_1837) {
    // |value| > 10000 rejected by the input bound; an in-bound value whose year leaves the
    // domain rejected by the resulting-year check. Both name "value".
    EXPECT_THROW(DateOnly(5000, 6, 15).AddYears(10001), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DateOnly(5000, 6, 15).AddYears(-10001), System::ArgumentOutOfRangeException);
    try {
        (void)DateOnly(9999, 6, 15).AddYears(1); // year 10000, in bound, unrepresentable
        FAIL() << "Expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "value");
    }
    // The widest in-domain year spans still work.
    EXPECT_EQ(DateOnly(1, 6, 15).AddYears(9998), DateOnly(9999, 6, 15));
    EXPECT_EQ(DateOnly(9999, 6, 15).AddYears(-9998), DateOnly(1, 6, 15));
}

// ---------------------------------------------------------------------------
// CCF-002 class D (SR-AUD-061, ticket #1879, approved 2026-07-31): DateOnly's
// parser consumes the whole string or fails. The worst row is
// "2024-06-15 10:20:30" -- a full timestamp silently truncated to its date.
// ---------------------------------------------------------------------------

TEST(DateOnlyTests, Ccf002d_TrailingTextIsRejected) {
    System::DateOnly d;
    EXPECT_FALSE(System::DateOnly::TryParse("2024-06-15junk", d));
    EXPECT_FALSE(System::DateOnly::TryParse("2024-06-15 10:20:30", d));
    ASSERT_TRUE(System::DateOnly::TryParse("2024-06-15 ", d));
    EXPECT_EQ(d, System::DateOnly(2024, 6, 15));
    EXPECT_THROW(System::DateOnly::Parse("2024-06-15junk"), System::FormatException);
    EXPECT_THROW(System::DateOnly::Parse("2024-06-15 10:20:30"), System::FormatException);
}

TEST(DateOnlyTests, Ccf002d_SscanfLeniencyShapesAreGone) {
    System::DateOnly d;
    EXPECT_FALSE(System::DateOnly::TryParse(" 024-06-15", d));
    EXPECT_FALSE(System::DateOnly::TryParse("+024-06-15", d));
}

TEST(DateOnlyTests, Ccf002d_WellFormedDatesKeepTheirExactValues) {
    System::DateOnly d;
    ASSERT_TRUE(System::DateOnly::TryParse("2024-06-15", d));
    EXPECT_EQ(d.getYearProperty(), 2024);
    EXPECT_EQ(d.getMonthProperty(), 6);
    EXPECT_EQ(d.getDayProperty(), 15);
    ASSERT_TRUE(System::DateOnly::TryParse("0001-01-01", d));
    EXPECT_EQ(d.getYearProperty(), 1);
    ASSERT_TRUE(System::DateOnly::TryParse("9999-12-31", d));
    EXPECT_EQ(d.getDayProperty(), 31);
    // A trailing UTC designator parsed before this ticket and still does.
    ASSERT_TRUE(System::DateOnly::TryParse("2024-06-15Z", d));
    EXPECT_EQ(d.getDayProperty(), 15);
    // Previously-rejected shapes stay rejected.
    EXPECT_FALSE(System::DateOnly::TryParse("2024-02-30", d));
    EXPECT_FALSE(System::DateOnly::TryParse("2024-6-15garbage", d));
}

// #1929 row 1 (decided 2026-08-18). DateOnly takes the same date widening as the
// other two doors -- it must, or the port would disagree with itself about what a
// date is.
TEST(DateOnlyTests, Decided1929_SingleDigitMonthAndDayParseToTheSameValue) {
    const char* spellings[] = {"2024-06-05", "2024-6-05", "2024-06-5", "2024-6-5"};
    for (const char* text : spellings) {
        System::DateOnly out(1, 1, 1);
        ASSERT_TRUE(System::DateOnly::TryParse(text, out)) << text;
        EXPECT_EQ(out, System::DateOnly(2024, 6, 5)) << text;
        EXPECT_EQ(System::DateOnly::Parse(text), out) << text;
    }
    // The trailing UTC designator still rides along, and is still ignored.
    System::DateOnly z(1, 1, 1);
    ASSERT_TRUE(System::DateOnly::TryParse("2024-6-5Z", z));
    EXPECT_EQ(z, System::DateOnly(2024, 6, 5));

    // Range validation is unaffected by the width of the text that carries it.
    System::DateOnly out(1, 1, 1);
    EXPECT_FALSE(System::DateOnly::TryParse("2024-2-30", out));
    EXPECT_FALSE(System::DateOnly::TryParse("2024-13-1", out));
    EXPECT_FALSE(System::DateOnly::TryParse("2024-0-1", out));
    EXPECT_FALSE(System::DateOnly::TryParse("2024-1-0", out));
}

TEST(DateOnlyTests, Approved1929_OuterWhitespaceAndUnapprovedDateWidths) {
    const char* accepted[] = {
        " 2024-06-15 ", "\t2024-06-15Z\r\n", "\f\v2024-06-15z\v"
    };
    for (const char* text : accepted) {
        System::DateOnly out(1, 1, 1);
        ASSERT_TRUE(System::DateOnly::TryParse(text, out)) << text;
        EXPECT_EQ(out, System::DateOnly(2024, 6, 15)) << text;
        EXPECT_EQ(System::DateOnly::Parse(text), out) << text;
    }

    System::DateOnly out(1, 1, 1);
    const char* rejected[] = {
        "2024 -06-15", "2024- 06-15", "2024-06 -15", "2024-06- 15",
        " \t\r\n ", "2024-06-15 Z", "2024-06-15 garbage",
        // #1929 row 1 moved "2024-6-15" and "2024-06-5" out of this list on
        // 2026-08-18. Internal whitespace is still grammar, and a year is still
        // exactly four digits.
        "24-06-15", "2024-006-15", "2024-06-015"
    };
    for (const char* text : rejected) EXPECT_FALSE(System::DateOnly::TryParse(text, out)) << text;

    try {
        (void)System::DateOnly::Parse("2024-06-15 garbage");
        FAIL();
    } catch (const System::FormatException& e) {
        EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u));
        EXPECT_EQ(std::string(e.what()),
                  "String was not recognized as a valid DateOnly: 2024-06-15 garbage");
    }
}

TEST(DateOnlyTests, Ticket1880_TryParseFailureAlwaysAssignsMinValue) {
    System::DateOnly out(1999, 9, 9);
    const auto expectFailure = [&out](const char* text) {
        out = System::DateOnly(1999, 9, 9);
        EXPECT_FALSE(System::DateOnly::TryParse(text, out)) << text;
        EXPECT_EQ(out, System::DateOnly::MinValue) << text;
    };

    expectFailure("");                         // initial scan / empty
    expectFailure(" \t\r\n ");               // whitespace-only
    expectFailure("not-a-date");               // malformed
    expectFailure("2024-06-15junk");           // trailing content
    expectFailure("0000-01-01");               // early range rejection
    expectFailure("10000-01-01");              // fixed-width range boundary
    expectFailure("2024-02-30");               // constructor rejection

    ASSERT_TRUE(System::DateOnly::TryParse("0001-01-01", out));
    EXPECT_EQ(out, System::DateOnly::MinValue);
    ASSERT_TRUE(System::DateOnly::TryParse("9999-12-31", out));
    EXPECT_EQ(out, System::DateOnly::MaxValue);
    ASSERT_TRUE(System::DateOnly::TryParse("2024-06-15", out));
    EXPECT_EQ(out, System::DateOnly(2024, 6, 15));

    try {
        (void)System::DateOnly::Parse("not-a-date");
        FAIL();
    } catch (const System::FormatException& e) {
        EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u));
        EXPECT_EQ(std::string(e.what()),
                  "String was not recognized as a valid DateOnly: not-a-date");
    }
}

// ===========================================================================
// #1939 — invariant single-format ParseExact (#1929 row 4A)
// ===========================================================================
//
// The approval is deliberately smaller than .NET and does not pretend to be culture-aware: four
// string-only providerless declarations, the invariant standard O/o and R/r, and a named custom
// subset. Providers, styles, kind, multi-format and span APIs are other tickets by name.

TEST(DateOnlyTests, Fix1939_TheApprovalsOwnWorkedExamples) {
    // Quoted from docs/DateTimeExactParsingAndKindDesign.md's 4A approval.
    EXPECT_EQ(System::DateOnly::ParseExact("2024-06-15", "O").getDayNumberProperty(), 739051);
    EXPECT_EQ(System::TimeOnly::ParseExact("10:20:30.1234567", "O").getTicksProperty(),
              372301234567LL);
    EXPECT_EQ(System::DateOnly::ParseExact("2024-6-5", "yyyy-M-d"), System::DateOnly(2024, 6, 5));

    System::TimeOnly out(1, 1);
    EXPECT_FALSE(System::TimeOnly::TryParseExact("10:20:30.12345678", "HH:mm:ss.ffffffff", out));
    EXPECT_EQ(out, System::TimeOnly::getMinValueProperty());

    // ONE OF THE APPROVAL'S EXAMPLES IS NOW STALE, and correcting it is part of the work rather
    // than a footnote: it says "the same unpadded text still fails general DateOnly::Parse".
    // #1929 landed the day before and widened the general grammar to accept a one- or two-digit
    // month and day, so it succeeds now. The example's POINT -- that exact and general parsing are
    // separate grammars -- is unaffected, and is asserted below with text the general parser
    // really does reject.
    System::DateOnly general(1, 1, 1);
    EXPECT_TRUE(System::DateOnly::TryParse("2024-6-5", general));   // #1929
    EXPECT_FALSE(System::DateOnly::TryParse("15/06/2024", general));
    EXPECT_EQ(System::DateOnly::ParseExact("15/06/2024", "dd/MM/yyyy"),
              System::DateOnly(2024, 6, 15));
}

TEST(DateOnlyTests, Fix1939_StandardFormatsAndWeekdayAgreement) {
    EXPECT_EQ(System::DateOnly::ParseExact("2024-06-15", "o"), System::DateOnly(2024, 6, 15));
    EXPECT_EQ(System::DateOnly::ParseExact("Sat, 15 Jun 2024", "R"), System::DateOnly(2024, 6, 15));
    EXPECT_EQ(System::DateOnly::ParseExact("Sat, 15 Jun 2024", "r"), System::DateOnly(2024, 6, 15));

    // AGREEMENT is what makes R a validating format rather than a shape: 2024-06-15 is a
    // Saturday, so naming any other weekday must fail.
    System::DateOnly out(1, 1, 1);
    EXPECT_FALSE(System::DateOnly::TryParseExact("Mon, 15 Jun 2024", "R", out));
    EXPECT_EQ(out, System::DateOnly::MinValue);

    // A one-character format that is not standard is NOT silently custom -- .NET requires "%d",
    // because a bare "d" is the standard short-date pattern this port does not implement.
    EXPECT_FALSE(System::DateOnly::TryParseExact("15", "d", out));
    EXPECT_TRUE(System::DateOnly::TryParseExact("2024-06-15", "yyyy-MM-dd", out));
}

TEST(DateOnlyTests, Fix1939_TheDigitWidthRuleIsDotNetsNotTheSpecifierCount) {
    // ParseDigits(str, 1) reads ONE OR TWO digits; ParseDigits(str, n>1) reads exactly n. A
    // scanner written as "count the specifiers, read that many digits" gets the single-specifier
    // case wrong in both directions.
    System::DateOnly out(1, 1, 1);
    EXPECT_TRUE(System::DateOnly::TryParseExact("2024-6-5", "yyyy-M-d", out));
    EXPECT_TRUE(System::DateOnly::TryParseExact("2024-06-15", "yyyy-M-d", out))
        << "one specifier accepts two digits too";
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024-6-5", "yyyy-MM-dd", out))
        << "two specifiers accept exactly two";

    // yy uses the invariant calendar's fixed TwoDigitYearMax of 2029, which is a property of the
    // invariant culture rather than culture state -- so it is available here even though #1929
    // declined a two-digit year in the GENERAL parser, where nothing says what was meant.
    EXPECT_EQ(System::DateOnly::ParseExact("24-06-15", "yy-MM-dd"), System::DateOnly(2024, 6, 15));
    EXPECT_EQ(System::DateOnly::ParseExact("30-06-15", "yy-MM-dd"), System::DateOnly(1930, 6, 15));
}

TEST(DateOnlyTests, Fix1939_NamesLiteralsAndEscapes) {
    EXPECT_EQ(System::DateOnly::ParseExact("15 June 2024", "dd MMMM yyyy"),
              System::DateOnly(2024, 6, 15));
    EXPECT_EQ(System::DateOnly::ParseExact("15 Jun 2024", "dd MMM yyyy"),
              System::DateOnly(2024, 6, 15));
    // "June" must not be consumed as "Jun" with a stray 'e' left over, which is why the name
    // match tries the longest candidate first.
    System::DateOnly out(1, 1, 1);
    EXPECT_FALSE(System::DateOnly::TryParseExact("15 June 2024", "dd MMM yyyy", out));

    // The three literal mechanisms. Each still needs a complete year/month/day around it -- my
    // first cut of this row wrote `"'on' yyyy"` and then asserted success, which the "one
    // complete date" rule refuses and rightly.
    EXPECT_EQ(System::DateOnly::ParseExact("on 2024-06-15", "'on' yyyy-MM-dd"),
              System::DateOnly(2024, 6, 15));
    EXPECT_EQ(System::DateOnly::ParseExact("on 2024-06-15", "\"on\" yyyy-MM-dd"),
              System::DateOnly(2024, 6, 15));
    EXPECT_EQ(System::DateOnly::ParseExact("d2024-06-15", "\\dyyyy-MM-dd"),
              System::DateOnly(2024, 6, 15))
        << "a backslash escape, so the 'd' is a literal rather than the day field";
    EXPECT_EQ(System::DateOnly::ParseExact("2024-06-5", "yyyy-MM-%d"),
              System::DateOnly(2024, 6, 5))
        << "%% makes a single specifier custom rather than standard";
}

TEST(DateOnlyTests, Fix1939_WhatIsRejected) {
    System::DateOnly out(1, 1, 1);
    // A missing date component. There is no current-date default here: that is
    // NoCurrentDateDefault, a STYLE, and styles are ticket #1942.
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024-06", "yyyy-MM", out));
    EXPECT_FALSE(System::DateOnly::TryParseExact("06-15", "MM-dd", out));
    // Time, zone and era tokens in a date format.
    for (const char* bad : {"yyyy-MM-dd HH:mm", "yyyy-MM-ddzzz", "yyyy-MM-ddK", "gyyyy-MM-dd"}) {
        EXPECT_FALSE(System::DateOnly::TryParseExact("2024-06-15", bad, out)) << bad;
    }
    // Whitespace is never free: it is a literal like any other, and there is no AllowLeadingWhite.
    EXPECT_FALSE(System::DateOnly::TryParseExact(" 2024-06-15", "yyyy-MM-dd", out));
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024-06-15 ", "yyyy-MM-dd", out));
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024 -06-15", "yyyy-MM-dd", out));
    // Trailing input, an unmatched literal, an unterminated quote, a duplicate field.
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024-06-15x", "yyyy-MM-dd", out));
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024/06/15", "yyyy-MM-dd", out));
    EXPECT_FALSE(System::DateOnly::TryParseExact("x2024-06-15", "'x yyyy-MM-dd", out));
    EXPECT_FALSE(System::DateOnly::TryParseExact("2024-06-15-15", "yyyy-MM-dd-dd", out));
    // An impossible calendar date.
    EXPECT_FALSE(System::DateOnly::TryParseExact("2023-02-29", "yyyy-MM-dd", out));
    EXPECT_TRUE(System::DateOnly::TryParseExact("2024-02-29", "yyyy-MM-dd", out));
    // Every failure writes MinValue and Parse throws the type's FormatException family.
    EXPECT_EQ(out.getDayNumberProperty(), System::DateOnly(2024, 2, 29).getDayNumberProperty());
    EXPECT_THROW((void)System::DateOnly::ParseExact("nope", "yyyy-MM-dd"), System::FormatException);
    try {
        (void)System::DateOnly::ParseExact("nope", "yyyy-MM-dd");
        FAIL();
    } catch (const System::FormatException& e) {
        EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u));
    }
}

TEST(TimeOnlyTests, Fix1939_TimeOnlyExactContract) {
    EXPECT_EQ(System::TimeOnly::ParseExact("10:20:30", "R"), System::TimeOnly(10, 20, 30));
    EXPECT_EQ(System::TimeOnly::ParseExact("10:20:30.1234567", "o").getTicksProperty(),
              372301234567LL);

    // Seconds are optional and default to zero; hour and minute are not.
    EXPECT_EQ(System::TimeOnly::ParseExact("10:20", "HH:mm"), System::TimeOnly(10, 20));
    System::TimeOnly out(1, 1);
    EXPECT_FALSE(System::TimeOnly::TryParseExact("20", "mm", out));
    EXPECT_FALSE(System::TimeOnly::TryParseExact("10", "HH", out));

    // A 12-HOUR FORM REQUIRES t, and a 24-hour form must not carry one. Without the designator
    // there is no way to say half past one in the AFTERNOON, so accepting "hh:mm" alone would
    // silently answer half past one in the morning.
    EXPECT_EQ(System::TimeOnly::ParseExact("01:30 PM", "hh:mm tt"), System::TimeOnly(13, 30));
    EXPECT_EQ(System::TimeOnly::ParseExact("12:30 AM", "hh:mm tt"), System::TimeOnly(0, 30));
    EXPECT_EQ(System::TimeOnly::ParseExact("12:30 PM", "hh:mm tt"), System::TimeOnly(12, 30));
    EXPECT_FALSE(System::TimeOnly::TryParseExact("01:30", "hh:mm", out));
    EXPECT_FALSE(System::TimeOnly::TryParseExact("01:30 PM", "HH:mm tt", out));
    EXPECT_FALSE(System::TimeOnly::TryParseExact("13:30 PM", "hh:mm tt", out))
        << "13 is not a 12-hour clock reading";

    // f needs the exact digit count; F permits omitted low digits.
    EXPECT_EQ(System::TimeOnly::ParseExact("10:20:30.123", "HH:mm:ss.fff").getTicksProperty(),
              System::TimeOnly(10, 20, 30, 123).getTicksProperty());
    EXPECT_FALSE(System::TimeOnly::TryParseExact("10:20:30.12", "HH:mm:ss.fff", out));
    EXPECT_TRUE(System::TimeOnly::TryParseExact("10:20:30.12", "HH:mm:ss.FFF", out));
    EXPECT_TRUE(System::TimeOnly::TryParseExact("10:20:30.", "HH:mm:ss.FFF", out));

    // Date, zone and over-wide fractional tokens are rejected, and so is out-of-range input.
    for (const char* bad : {"yyyy HH:mm", "HH:mmzzz", "HH:mm:ss.ffffffff", "HHH:mm"}) {
        EXPECT_FALSE(System::TimeOnly::TryParseExact("10:20", bad, out)) << bad;
    }
    EXPECT_FALSE(System::TimeOnly::TryParseExact("24:00", "HH:mm", out));
    EXPECT_FALSE(System::TimeOnly::TryParseExact("10:60", "HH:mm", out));
    EXPECT_THROW((void)System::TimeOnly::ParseExact("nope", "HH:mm"), System::FormatException);
}

// ===========================================================================================
// #2412 -- DateOnly/TimeOnly ParseExact take a provider and a DateTimeStyles
//
// This is the half of #1942 and #1943 that needs NO timezone contract, and it exists because
// their recorded dependencies formed a CYCLE: #1942 waited for "the relevant exact overload"
// (an overload taking a DateTimeStyles -- measured, nothing in modules/ accepted one), and
// #1943 waited for "#1940-#1942", which includes #1942.
//
// What separates cleanly is that DateOnly and TimeOnly HAVE NO DateTimeKind, so every
// kind-affecting style has nothing to act on and .NET rejects them outright. The styles that
// would need a timezone contract are exactly the styles that are illegal here.
// ===========================================================================================

TEST(DateOnlyStyles2412Tests, OnlyTheWhitespaceStylesAreLegal) {
    using System::Globalization::DateTimeStyles;
    // DateOnly.cs:317-320 -- the whole contract, and the message is .NET's own.
    for (const auto illegal : {DateTimeStyles::AdjustToUniversal, DateTimeStyles::AssumeLocal,
                               DateTimeStyles::AssumeUniversal, DateTimeStyles::RoundtripKind,
                               DateTimeStyles::NoCurrentDateDefault}) {
        EXPECT_THROW((void)DateOnly::ParseExact("2024-06-15", "yyyy-MM-dd", nullptr, illegal),
                     System::ArgumentException)
            << "style bit " << static_cast<int>(illegal);
    }
    try {
        (void)DateOnly::ParseExact("2024-06-15", "yyyy-MM-dd", nullptr,
                                    DateTimeStyles::AssumeUniversal);
        FAIL() << "an illegal style was accepted";
    } catch (const System::ArgumentException& e) {
        // ArgumentException appends " (Parameter 'style')", which is .NET's own shape, so the
        // assertion is on the text this port actually produces rather than on the raw resource.
        EXPECT_EQ(std::string(e.getMessageProperty()),
                  "The only allowed values for the styles are AllowWhiteSpaces, "
                  "AllowTrailingWhite, AllowLeadingWhite, and AllowInnerWhite. "
                  "(Parameter 'style')");
        EXPECT_EQ(e.getParamNameProperty(), "style");
    }

    // ...and all four whitespace bits, alone and combined, are accepted.
    for (const auto legal : {DateTimeStyles::None, DateTimeStyles::AllowLeadingWhite,
                             DateTimeStyles::AllowTrailingWhite, DateTimeStyles::AllowInnerWhite,
                             DateTimeStyles::AllowWhiteSpaces}) {
        EXPECT_NO_THROW((void)DateOnly::ParseExact("2024-06-15", "yyyy-MM-dd", nullptr, legal));
    }
}

// A Try* METHOD THAT THROWS. DateOnly.cs:519-522 raises for an illegal style rather than
// returning false, which is exactly what a reader assumes away. A parse failure still returns
// false -- both halves in one case, so neither can be satisfied alone.
TEST(DateOnlyStyles2412Tests, TryParseExactThrowsForAnIllegalStyleButNotForABadParse) {
    using System::Globalization::DateTimeStyles;
    DateOnly result = DateOnly::MaxValue;
    EXPECT_THROW((void)DateOnly::TryParseExact("2024-06-15", "yyyy-MM-dd", nullptr,
                                                DateTimeStyles::AssumeLocal, result),
                 System::ArgumentException);
    EXPECT_EQ(result, DateOnly::MaxValue)
        << "a rejected style must leave the caller's variable untouched";

    EXPECT_FALSE(DateOnly::TryParseExact("not a date", "yyyy-MM-dd", nullptr,
                                          DateTimeStyles::None, result));
}

TEST(DateOnlyStyles2412Tests, TheWhitespaceStylesHaveTheirEffect) {
    using System::Globalization::DateTimeStyles;
    DateOnly result = DateOnly::MinValue;

    // Without the style the surrounding space is a mismatch; with it, it is ignored.
    EXPECT_FALSE(DateOnly::TryParseExact("  2024-06-15", "yyyy-MM-dd", result));
    EXPECT_TRUE(DateOnly::TryParseExact("  2024-06-15", "yyyy-MM-dd", nullptr,
                                         DateTimeStyles::AllowLeadingWhite, result));
    EXPECT_EQ(result, DateOnly(2024, 6, 15));

    EXPECT_FALSE(DateOnly::TryParseExact("2024-06-15  ", "yyyy-MM-dd", result));
    EXPECT_TRUE(DateOnly::TryParseExact("2024-06-15  ", "yyyy-MM-dd", nullptr,
                                         DateTimeStyles::AllowTrailingWhite, result));
    EXPECT_EQ(result, DateOnly(2024, 6, 15));

    // Inner whitespace is permitted only where the format expects a separator -- not between the
    // digits of one field, which is the row that separates "skip whitespace anywhere" from .NET's
    // rule.
    EXPECT_TRUE(DateOnly::TryParseExact("2024- 06- 15", "yyyy-MM-dd", nullptr,
                                         DateTimeStyles::AllowInnerWhite, result));
    EXPECT_EQ(result, DateOnly(2024, 6, 15));
    EXPECT_FALSE(DateOnly::TryParseExact("20 24-06-15", "yyyy-MM-dd", nullptr,
                                          DateTimeStyles::AllowInnerWhite, result))
        << "AllowInnerWhite must not split a numeric field";

    // Each bit is separate: leading does not buy trailing.
    EXPECT_FALSE(DateOnly::TryParseExact("2024-06-15 ", "yyyy-MM-dd", nullptr,
                                          DateTimeStyles::AllowLeadingWhite, result));
    EXPECT_TRUE(DateOnly::TryParseExact(" 2024-06-15 ", "yyyy-MM-dd", nullptr,
                                         DateTimeStyles::AllowWhiteSpaces, result));
}

// The provider is HONOURED rather than accepted and ignored -- #1942's own criterion, and the
// reason #1940 refused to add an overload it could only ignore.
TEST(DateOnlyStyles2412Tests, TheProviderSuppliesTheMonthAndDayNames) {
    using System::Globalization::DateTimeStyles;
    System::Globalization::DateTimeFormatInfo custom;
    custom.setAbbreviatedMonthNamesProperty(
        {"jan", "fev", "mar", "avr", "mai", "jun", "jul", "aou", "sep", "oct", "nov", "dec", ""});

    DateOnly result = DateOnly::MinValue;
    EXPECT_TRUE(DateOnly::TryParseExact("15 jun 2024", "dd MMM yyyy", &custom,
                                         DateTimeStyles::None, result));
    EXPECT_EQ(result, DateOnly(2024, 6, 15));

    // The invariant answer is unchanged, and the custom name is NOT accepted by it.
    EXPECT_TRUE(DateOnly::TryParseExact("15 Jun 2024", "dd MMM yyyy", result));
    EXPECT_EQ(result, DateOnly(2024, 6, 15));
    EXPECT_FALSE(DateOnly::TryParseExact("15 fev 2024", "dd MMM yyyy", result))
        << "a provider's names must not leak into the invariant parse";
}

// #1939 recorded that the scanner's longest-first name matching was "defensive rather than
// load-bearing", because no INVARIANT name is a prefix of another and a first-match mutation went
// uncaught. A provider's names carry no such guarantee, so #2412 makes the rule decide the answer
// -- and pins it with exactly that shape.
TEST(DateOnlyStyles2412Tests, LongestFirstNameMatchingIsNowLoadBearing) {
    using System::Globalization::DateTimeStyles;
    System::Globalization::DateTimeFormatInfo prefixes;
    // "Ma" is a prefix of "March": first-match would take "Ma" (month 1) and then choke on "rch".
    prefixes.setMonthNamesProperty({"Ma", "Feb", "March", "April", "May", "June", "July", "August",
                                     "September", "October", "November", "December", ""});

    DateOnly result = DateOnly::MinValue;
    ASSERT_TRUE(DateOnly::TryParseExact("15 March 2024", "dd MMMM yyyy", &prefixes,
                                         DateTimeStyles::None, result));
    EXPECT_EQ(result.getMonthProperty(), 3)
        << "the longer name must win, or 'March' is read as the month named 'Ma'";

    // ...and the shorter one still resolves to its own month.
    ASSERT_TRUE(DateOnly::TryParseExact("15 Ma 2024", "dd MMMM yyyy", &prefixes,
                                         DateTimeStyles::None, result));
    EXPECT_EQ(result.getMonthProperty(), 1);
}
