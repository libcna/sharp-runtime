// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/DateTime.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/FormatException.hpp"
#include "System/DayOfWeek.hpp"
#include "System/TimeSpan.hpp"

using System::ArgumentException;
using System::ArgumentOutOfRangeException;
using System::DateTime;
using System::DateTimeOffset;
using System::DayOfWeek;
using System::TimeSpan;

// Note: the historical DateTimeOffsetTests suite lives in tests/Task40Tests.cpp.
// This file uses a Tests2 suffix per NEXT.md's documented duplicate-suite-name policy.

// ---------------------------------------------------------------------------
// Static fields: MinValue / MaxValue / UnixEpoch
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, MinValue_IsZeroTicksZeroOffset) {
    EXPECT_EQ(DateTimeOffset::MinValue.getTicksProperty(), 0LL);
    EXPECT_EQ(DateTimeOffset::MinValue.getUtcTicksProperty(), 0LL);
    EXPECT_TRUE(DateTimeOffset::MinValue.getOffsetProperty() == TimeSpan::Zero);
}

TEST(DateTimeOffsetTests2, MaxValue_IsMaxTicksZeroOffset) {
    EXPECT_EQ(DateTimeOffset::MaxValue.getTicksProperty(), DateTime::MaxTicks);
    EXPECT_EQ(DateTimeOffset::MaxValue.getUtcTicksProperty(), DateTime::MaxTicks);
}

TEST(DateTimeOffsetTests2, UnixEpoch_MatchesDateTimeUnixEpoch) {
    EXPECT_EQ(DateTimeOffset::UnixEpoch.getUtcTicksProperty(), DateTime::UnixEpochTicks);
    EXPECT_EQ(DateTimeOffset::UnixEpoch.ToUnixTimeSeconds(), 0LL);
}

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, CtorFromTicksAndOffset) {
    DateTime dt(2020, 6, 15, 10, 30, 0);
    DateTimeOffset dto(dt.getTicksProperty(), TimeSpan::FromHours(2));
    EXPECT_EQ(dto.getTicksProperty(), dt.getTicksProperty());
    EXPECT_NEAR(dto.getOffsetProperty().getTotalHoursProperty(), 2.0, 1e-9);
}

TEST(DateTimeOffsetTests2, CtorFromDateTime_AttachesZeroOffset) {
    DateTime dt(2020, 6, 15, 10, 30, 0);
    DateTimeOffset dto(dt);
    EXPECT_TRUE(dto.getOffsetProperty() == TimeSpan::Zero);
    EXPECT_EQ(dto.getUtcTicksProperty(), dt.getTicksProperty());
}

TEST(DateTimeOffsetTests2, CtorFromDateTime_ImplicitConversion) {
    DateTime dt(2020, 6, 15, 10, 30, 0);
    DateTimeOffset dto = dt; // implicit conversion, mirrors .NET's implicit operator
    EXPECT_EQ(dto.getUtcTicksProperty(), dt.getTicksProperty());
}

TEST(DateTimeOffsetTests2, CtorFromComponentsAndOffset) {
    DateTimeOffset dto(2020, 6, 15, 10, 30, 45, TimeSpan::FromHours(-5));
    EXPECT_EQ(dto.getYearProperty(), 2020);
    EXPECT_EQ(dto.getMonthProperty(), 6);
    EXPECT_EQ(dto.getDayProperty(), 15);
    EXPECT_EQ(dto.getHourProperty(), 10);
    EXPECT_EQ(dto.getMinuteProperty(), 30);
    EXPECT_EQ(dto.getSecondProperty(), 45);
    EXPECT_NEAR(dto.getOffsetProperty().getTotalHoursProperty(), -5.0, 1e-9);
}

TEST(DateTimeOffsetTests2, CtorFromComponentsMillisecondAndOffset) {
    DateTimeOffset dto(2020, 6, 15, 10, 30, 45, 123, TimeSpan::FromHours(1));
    EXPECT_EQ(dto.getMillisecondProperty(), 123);
}

// ---------------------------------------------------------------------------
// Offset validation (.NET parity: ArgumentException / ArgumentOutOfRangeException)
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, Ctor_OffsetNotWholeMinutes_Throws) {
    DateTime dt(2020, 6, 15, 10, 30, 0);
    TimeSpan badOffset(TimeSpan::TicksPerMinute / 2); // half a minute
    EXPECT_THROW(DateTimeOffset(dt, badOffset), ArgumentException);
}

TEST(DateTimeOffsetTests2, Ctor_OffsetBeyond14Hours_Throws) {
    DateTime dt(2020, 6, 15, 10, 30, 0);
    EXPECT_THROW(DateTimeOffset(dt, TimeSpan::FromHours(15)), ArgumentOutOfRangeException);
    EXPECT_THROW(DateTimeOffset(dt, TimeSpan::FromHours(-15)), ArgumentOutOfRangeException);
}

TEST(DateTimeOffsetTests2, Ctor_Exactly14Hours_DoesNotThrow) {
    DateTime dt(2020, 6, 15, 10, 30, 0);
    EXPECT_NO_THROW(DateTimeOffset(dt, TimeSpan::FromHours(14)));
    EXPECT_NO_THROW(DateTimeOffset(dt, TimeSpan::FromHours(-14)));
}

TEST(DateTimeOffsetTests2, Ctor_UtcTicksBelowZero_Throws) {
    DateTime dt = DateTime::MinValue;
    EXPECT_THROW(DateTimeOffset(dt, TimeSpan::FromHours(1)), ArgumentOutOfRangeException);
}

TEST(DateTimeOffsetTests2, Ctor_UtcTicksAboveMax_Throws) {
    DateTime dt = DateTime::MaxValue;
    EXPECT_THROW(DateTimeOffset(dt, TimeSpan::FromHours(-1)), ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// New component accessors
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, DayOfWeekAndDayOfYear) {
    DateTimeOffset dto(2020, 1, 15, 0, 0, 0, TimeSpan::Zero); // Wednesday, day 15
    EXPECT_EQ(dto.getDayOfWeekProperty(), DayOfWeek::Wednesday);
    EXPECT_EQ(dto.getDayOfYearProperty(), 15);
}

TEST(DateTimeOffsetTests2, TicksProperty_IsClockTicks) {
    DateTime dt(2020, 6, 15, 10, 30, 0);
    DateTimeOffset dto(dt, TimeSpan::FromHours(2));
    EXPECT_EQ(dto.getTicksProperty(), dt.getTicksProperty());
}

TEST(DateTimeOffsetTests2, TimeOfDayProperty) {
    DateTimeOffset dto(2020, 6, 15, 10, 30, 45, TimeSpan::Zero);
    TimeSpan tod = dto.getTimeOfDayProperty();
    EXPECT_EQ(tod.getHoursProperty(), 10);
    EXPECT_EQ(tod.getMinutesProperty(), 30);
    EXPECT_EQ(tod.getSecondsProperty(), 45);
}

TEST(DateTimeOffsetTests2, TotalOffsetMinutesProperty) {
    DateTimeOffset dto(2020, 6, 15, 10, 30, 0, TimeSpan::FromHours(-5));
    EXPECT_EQ(dto.getTotalOffsetMinutesProperty(), -300);
}

TEST(DateTimeOffsetTests2, LocalDateTimeProperty_MatchesUtcPlusLocalOffset) {
    DateTimeOffset dto = DateTimeOffset::getUtcNowProperty();
    DateTime local = dto.getLocalDateTimeProperty();
    DateTimeOffset localOffset = dto.ToLocalTime();
    EXPECT_EQ(local.getTicksProperty(), localOffset.getDateTimeProperty().getTicksProperty());
}

// ---------------------------------------------------------------------------
// ToOffset
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, ToOffset_PreservesUtcInstant) {
    DateTimeOffset dto(2020, 6, 15, 10, 30, 0, TimeSpan::FromHours(2));
    DateTimeOffset converted = dto.ToOffset(TimeSpan::FromHours(-3));
    EXPECT_EQ(converted.getUtcTicksProperty(), dto.getUtcTicksProperty());
    EXPECT_NEAR(converted.getOffsetProperty().getTotalHoursProperty(), -3.0, 1e-9);
}

// ---------------------------------------------------------------------------
// AddTicks
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, AddTicks_ShiftsClockAndUtc) {
    DateTimeOffset dto(2020, 6, 15, 10, 30, 0, TimeSpan::Zero);
    DateTimeOffset shifted = dto.AddTicks(DateTime::TicksPerSecond * 5);
    EXPECT_EQ(shifted.getSecondProperty(), 5);
}

// ---------------------------------------------------------------------------
// AddMonths / AddYears -- overflow safety
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, AddMonths_LargeValue_ThrowsInsteadOfOverflowing) {
    // `getMonthProperty() + months` computed directly with no upfront bounds check (the
    // previous, from-scratch reimplementation of this method) is real signed-integer-
    // overflow UB in C++ for a merely large (not extreme) `months` argument. Real .NET
    // delegates entirely to DateTime.AddMonths, which already validates months is within
    // [-120000, 120000] before doing any arithmetic; this must throw the same way.
    DateTimeOffset dto(2020, 6, 15, 10, 30, 0, TimeSpan::Zero);
    EXPECT_THROW(dto.AddMonths(1000000000), System::ArgumentOutOfRangeException);
    EXPECT_THROW(dto.AddMonths(-1000000000), System::ArgumentOutOfRangeException);
}

TEST(DateTimeOffsetTests2, AddYears_LargeValue_ThrowsInsteadOfOverflowing) {
    // The previous AddYears computed `years * 12` with no upfront bounds check --
    // confirmed via UBSan to overflow int32 for e.g. years == 200000000.
    DateTimeOffset dto(2020, 6, 15, 10, 30, 0, TimeSpan::Zero);
    EXPECT_THROW(dto.AddYears(200000000), System::ArgumentOutOfRangeException);
    EXPECT_THROW(dto.AddYears(-200000000), System::ArgumentOutOfRangeException);
}

TEST(DateTimeOffsetTests2, AddMonths_PreservesOffset) {
    DateTimeOffset dto(2020, 6, 15, 10, 30, 0, TimeSpan::FromHours(5.0));
    DateTimeOffset shifted = dto.AddMonths(3);
    EXPECT_EQ(shifted.getMonthProperty(), 9);
    EXPECT_EQ(shifted.getOffsetProperty(), TimeSpan::FromHours(5.0));
}

// ---------------------------------------------------------------------------
// ToLocalTime / ToUniversalTime round trip
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, ToLocalTime_PreservesUtcInstant) {
    DateTimeOffset utcNow = DateTimeOffset::getUtcNowProperty();
    DateTimeOffset local = utcNow.ToLocalTime();
    EXPECT_EQ(local.getUtcTicksProperty(), utcNow.getUtcTicksProperty());
}

// ---------------------------------------------------------------------------
// Unix time conversions
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, FromUnixTimeSeconds_Zero_IsEpoch) {
    DateTimeOffset dto = DateTimeOffset::FromUnixTimeSeconds(0);
    EXPECT_EQ(dto.getUtcTicksProperty(), DateTime::UnixEpochTicks);
    EXPECT_TRUE(dto.getOffsetProperty() == TimeSpan::Zero);
}

TEST(DateTimeOffsetTests2, FromUnixTimeSeconds_KnownValue) {
    // 2000-01-01T00:00:00Z = 946684800 Unix seconds
    DateTimeOffset dto = DateTimeOffset::FromUnixTimeSeconds(946684800LL);
    EXPECT_EQ(dto.getYearProperty(), 2000);
    EXPECT_EQ(dto.getMonthProperty(), 1);
    EXPECT_EQ(dto.getDayProperty(), 1);
}

TEST(DateTimeOffsetTests2, FromUnixTimeMilliseconds_KnownValue) {
    DateTimeOffset dto = DateTimeOffset::FromUnixTimeMilliseconds(946684800000LL);
    EXPECT_EQ(dto.getYearProperty(), 2000);
}

TEST(DateTimeOffsetTests2, ToUnixTimeSeconds_RoundTrips) {
    DateTimeOffset dto = DateTimeOffset::FromUnixTimeSeconds(1234567890LL);
    EXPECT_EQ(dto.ToUnixTimeSeconds(), 1234567890LL);
}

TEST(DateTimeOffsetTests2, ToUnixTimeMilliseconds_RoundTrips) {
    DateTimeOffset dto = DateTimeOffset::FromUnixTimeMilliseconds(1234567890123LL);
    EXPECT_EQ(dto.ToUnixTimeMilliseconds(), 1234567890123LL);
}

TEST(DateTimeOffsetTests2, FromUnixTimeSeconds_OutOfRange_Throws) {
    EXPECT_THROW(DateTimeOffset::FromUnixTimeSeconds(-70000000000LL), ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// Compare / Equals / EqualsExact / GetHashCode
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, StaticCompare_MatchesInstanceCompareTo) {
    DateTimeOffset a(2020, 1, 1, 0, 0, 0, TimeSpan::Zero);
    DateTimeOffset b(2021, 1, 1, 0, 0, 0, TimeSpan::Zero);
    EXPECT_LT(DateTimeOffset::Compare(a, b), 0);
    EXPECT_GT(DateTimeOffset::Compare(b, a), 0);
    EXPECT_EQ(DateTimeOffset::Compare(a, a), 0);
}

TEST(DateTimeOffsetTests2, StaticEquals_ComparesUtcInstant) {
    DateTimeOffset a(2020, 1, 1, 12, 0, 0, TimeSpan::FromHours(2));
    DateTimeOffset b(2020, 1, 1, 10, 0, 0, TimeSpan::Zero); // same UTC instant
    EXPECT_TRUE(DateTimeOffset::Equals(a, b));
    EXPECT_TRUE(a.Equals(b));
}

TEST(DateTimeOffsetTests2, EqualsExact_RequiresSameOffset) {
    DateTimeOffset a(2020, 1, 1, 12, 0, 0, TimeSpan::FromHours(2));
    DateTimeOffset b(2020, 1, 1, 10, 0, 0, TimeSpan::Zero); // same UTC instant, different offset
    EXPECT_TRUE(a.Equals(b));
    EXPECT_FALSE(a.EqualsExact(b));
    EXPECT_TRUE(a.EqualsExact(a));
}

TEST(DateTimeOffsetTests2, GetHashCode_EqualForSameUtcInstant) {
    DateTimeOffset a(2020, 1, 1, 12, 0, 0, TimeSpan::FromHours(2));
    DateTimeOffset b(2020, 1, 1, 10, 0, 0, TimeSpan::Zero);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

// ---------------------------------------------------------------------------
// CCF-002 classes A and B (SR-AUD-006, ticket #1877).
//
// Class A: both component constructors delegated to DateTime's, so they shared
// its missing time-component validation. Class B: they also validated the offset
// LAST, because the clock DateTime was produced in a mem-initialiser. Real .NET
// evaluates ValidateOffset(offset) FIRST (DateTimeOffset.cs), so the reference
// order is offset-shape -> offset-range -> date -> time -> millisecond ->
// UTC-range.
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, Ccf002_InvalidTimeComponentsAreRejected) {
    const TimeSpan utc = TimeSpan::Zero;
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, 24, 0, 0, utc),       ArgumentOutOfRangeException);
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, 0, 60, 0, utc),       ArgumentOutOfRangeException);
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, 0, 0, 60, utc),       ArgumentOutOfRangeException);
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, 0, 0, 0, 1000, utc),  ArgumentOutOfRangeException);
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, -1, 0, 0, utc),       ArgumentOutOfRangeException);
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, 0, 0, 0, -1, utc),    ArgumentOutOfRangeException);
    // Previously succeeded as 2024-01-02 01:00 +02:00.
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, 25, 0, 0, TimeSpan::FromHours(2)),
                 ArgumentOutOfRangeException);
}

TEST(DateTimeOffsetTests2, Ccf002_ExtremeIntegerHourIsRejectedInsteadOfOverflowing) {
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, 2000000000, 0, 0, TimeSpan::Zero),
                 ArgumentOutOfRangeException);
    EXPECT_THROW(DateTimeOffset(2024, 1, 1, SharpRuntime::INTCS_MIN, 0, 0, 0, TimeSpan::Zero),
                 ArgumentOutOfRangeException);
}

TEST(DateTimeOffsetTests2, Ccf002_OffsetIsValidatedBeforeTheClockDateTime) {
    // Both of these threw the offset error before the repair too -- but only because
    // hour 24 was silently accepted. Validating the offset first is what keeps them
    // reporting the offset rather than the hour, and it is what the reference does.
    try {
        (void)DateTimeOffset(2024, 1, 1, 24, 0, 0, TimeSpan::FromHours(15));
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "offset");
    }

    try {
        (void)DateTimeOffset(2024, 1, 1, 24, 0, 0, TimeSpan(90LL));
        FAIL() << "expected ArgumentException";
    } catch (const ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "offset");
    }

    // The one case whose reported parameter moved: 'year' before, 'offset' after.
    // ArgumentOutOfRangeException in both, and 'offset' is what .NET reports.
    try {
        (void)DateTimeOffset(2024, 13, 1, 0, 0, 0, TimeSpan::FromHours(15));
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "offset");
    }
}

TEST(DateTimeOffsetTests2, Ccf002_WithAValidOffsetTheDateStillWinsOverTheTime) {
    try {
        (void)DateTimeOffset(2024, 13, 1, 24, 0, 0, TimeSpan::Zero);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "year");
    }
}

TEST(DateTimeOffsetTests2, Ccf002_UtcRangeGuardStillRunsLast) {
    // Every component and the offset are individually valid; only the UTC instant
    // the offset produces is unrepresentable.
    try {
        (void)DateTimeOffset(9999, 12, 31, 23, 59, 59, 999, TimeSpan::FromHours(-14));
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "offset");
        EXPECT_NE(e.getMessageProperty().find("UTC time represented"), std::string::npos);
    }
}

TEST(DateTimeOffsetTests2, Ccf002_TickConstructorValidatesTheOffsetFirst) {
    try {
        (void)DateTimeOffset(-1LL, TimeSpan::FromHours(15));
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "offset");
    }
    // A bad tick count with a good offset still reports 'ticks'.
    try {
        (void)DateTimeOffset(-1LL, TimeSpan::Zero);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "ticks");
    }
}

TEST(DateTimeOffsetTests2, Ccf002_ValidComponentsAreUnchanged) {
    const DateTimeOffset v(2024, 6, 15, 10, 30, 0, 999, TimeSpan::FromHours(2));
    EXPECT_EQ(v.getTicksProperty(), 638540442009990000LL);
    EXPECT_EQ(v.getTotalOffsetMinutesProperty(), 120);
    EXPECT_NO_THROW(DateTimeOffset(1, 1, 1, 0, 0, 0, 0, TimeSpan::Zero));
    EXPECT_NO_THROW(DateTimeOffset(9999, 12, 31, 23, 59, 59, 999, TimeSpan::Zero));
    EXPECT_NO_THROW(DateTimeOffset(2024, 2, 29, 23, 59, 59, 999, TimeSpan::FromHours(14)));
    EXPECT_NO_THROW(DateTimeOffset(2024, 2, 29, 23, 59, 59, 999, TimeSpan::FromHours(-14)));
}

TEST(DateTimeOffsetTests2, Ccf002_ParserRejectsOutOfRangeClockComponents) {
    DateTimeOffset out;
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T25:00:00+02:00", out));
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:60:00+02:00", out));
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T23:59:59+02:00", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 120);
}

// ---------------------------------------------------------------------------
// CCF-002 class C (SR-AUD-007a, ticket #1878) -- offset-field range validation
// in TryParse.
//
// The two numeric fields of a textual offset were read with sscanf and used
// unchecked, so an impossible minute value was absorbed into the TimeSpan before
// the whole-minute and +/-14h guards could see it, a negative field inverted the
// sign the caller wrote, and a large hour field made TryParse itself THROW.
//
// This is a range check on numeric component values, not a grammar change: the
// same character sequences still match. The remaining grammar defects of this
// parser (SR-AUD-007b) are ticket #1879 and are pinned below as *current*
// behaviour.
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetTests2, Ccf002_ImpossibleOffsetMinutesAreRejected) {
    DateTimeOffset out;
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+02:60", out)); // meant +03:00
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+02:75", out)); // meant +03:15
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+02:99", out)); // meant +03:39
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00-02:75", out)); // meant -03:15
    EXPECT_THROW((void)DateTimeOffset::Parse("2024-06-15T10:30:00+02:75"),
                 System::FormatException);
}

TEST(DateTimeOffsetTests2, Ccf002_NegativeOffsetFieldsCannotInvertTheSign) {
    DateTimeOffset out;
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+02:-30", out)); // meant +01:30
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+-05:00", out)); // meant -05:00
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00--05:00", out)); // meant +05:00
}

TEST(DateTimeOffsetTests2, Ccf002_TryParseNeverThrowsForAnOversizedOffsetField) {
    // TimeSpan::FromSeconds threw OverflowException from OUTSIDE TryParse's
    // try/catch, so a Try-style method escaped as an exception.
    DateTimeOffset out;
    EXPECT_NO_THROW({
        EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+999999999999:00", out));
    });
    EXPECT_NO_THROW({
        EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+2147483647:00", out));
    });
    EXPECT_NO_THROW({
        EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00-2147483647:00", out));
    });
    // Parse now reports the documented FormatException instead of OverflowException.
    EXPECT_THROW((void)DateTimeOffset::Parse("2024-06-15T10:30:00+2147483647:00"),
                 System::FormatException);
}

TEST(DateTimeOffsetTests2, Ccf002_EveryValidOffsetStillParsesToTheSameValue) {
    struct Case { const char* text; int minutes; };
    const Case cases[] = {
        {"2024-06-15T10:30:00+00:00",   0},
        {"2024-06-15T10:30:00+00:59",  59},
        {"2024-06-15T10:30:00+02:00", 120},
        {"2024-06-15T10:30:00-05:30", -330},
        {"2024-06-15T10:30:00+14:00", 840},
        {"2024-06-15T10:30:00-14:00", -840},
    };
    for (const Case& c : cases) {
        DateTimeOffset out;
        ASSERT_TRUE(DateTimeOffset::TryParse(c.text, out)) << c.text;
        EXPECT_EQ(out.getTotalOffsetMinutesProperty(), c.minutes) << c.text;
        EXPECT_EQ(out.getTicksProperty(), 638540442000000000LL) << c.text;
    }
    DateTimeOffset z;
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00Z", z));
    EXPECT_EQ(z.getTotalOffsetMinutesProperty(), 0);
}

TEST(DateTimeOffsetTests2, Ccf002_OutOfRangeOffsetHoursStillFailTheSameWay) {
    // "+15:00" was already rejected -- by the +/-14h guard, after the arithmetic.
    // It is now rejected before it, with the identical observable result.
    DateTimeOffset out;
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+15:00", out));
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00-15:00", out));
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+14:01", out));
}

TEST(DateTimeOffsetTests2, Ccf002_CurrentlyAcceptedOffsetGrammarIsPinnedUnchanged) {
    // SR-AUD-007b is NOT repaired by ticket #1878. Both inputs are accepted today
    // and must stay accepted until ticket #1879 obtains explicit approval for the
    // accepted-grammar change; then these expectations flip.
    DateTimeOffset out;
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00+02:00junk", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 120);
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00+2:5", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 125);
}
