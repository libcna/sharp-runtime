// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include "System/Globalization/DateTimeStyles.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ILocalTimeZone.hpp"
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <cstdlib>
#include <ctime>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/DateTime.hpp"
#include "System/DateTimeKind.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/FormatException.hpp"
#include "System/DayOfWeek.hpp"
#include "System/TimeSpan.hpp"
#include "System/detail/ProcessTimeZoneState.hpp"

using System::ArgumentException;
using System::ArgumentOutOfRangeException;
using System::DateTime;
using System::DateTimeKind;
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

TEST(DateTimeOffsetTests2, CtorFromUtcDateTime_AttachesZeroOffset) {
    DateTime dt(DateTime(2020, 6, 15, 10, 30, 0).getTicksProperty(), DateTimeKind::Utc);
    DateTimeOffset dto(dt);
    EXPECT_TRUE(dto.getOffsetProperty() == TimeSpan::Zero);
    EXPECT_EQ(dto.getUtcTicksProperty(), dt.getTicksProperty());
    EXPECT_EQ(dto.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
}

TEST(DateTimeOffsetTests2, CtorFromDateTime_ImplicitConversion) {
    DateTime dt(DateTime(2020, 6, 15, 10, 30, 0).getTicksProperty(), DateTimeKind::Utc);
    DateTimeOffset dto = dt; // implicit conversion, mirrors .NET's implicit operator
    EXPECT_EQ(dto.getUtcTicksProperty(), dt.getTicksProperty());
}

// ===========================================================================================
// Post-#1941 DateTimeKind ripple: DateTimeOffset constructors, properties, and factories
//
// Core.Base cannot name TimeZone::CurrentTimeZone() without making the TimeZone -> Core.Base
// dependency cyclic. DateTimeOffset already had a private platform reader and documents the
// accepted practical-subset boundary: every local operation uses the process's CURRENT offset,
// not historical/future rules for the input date. A fixed POSIX TZ string makes the supported
// contract deterministic without depending on the host's configured zone or installed tzdata.
// ===========================================================================================

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
namespace {

class ScopedDateTimeOffsetTz final {
    std::string saved_;
    bool wasSet_;

public:
    explicit ScopedDateTimeOffsetTz(const char* zone) {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        const char* current = ::getenv("TZ");
        wasSet_ = current != nullptr;
        if (wasSet_) saved_ = current;
        ::setenv("TZ", zone, 1);
        ::tzset();
    }

    ScopedDateTimeOffsetTz(const ScopedDateTimeOffsetTz&) = delete;
    ScopedDateTimeOffsetTz& operator=(const ScopedDateTimeOffsetTz&) = delete;

    ~ScopedDateTimeOffsetTz() {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        if (wasSet_) ::setenv("TZ", saved_.c_str(), 1);
        else         ::unsetenv("TZ");
        ::tzset();
    }
};

SharpRuntime::longcs tickDistance(SharpRuntime::longcs left, SharpRuntime::longcs right) {
    return left >= right ? left - right : right - left;
}

} // namespace

TEST(DateTimeOffsetKindRippleTests, Post1941_OneArgumentConstructorChoosesOffsetByKind) {
    ScopedDateTimeOffsetTz tz("UTC-02"); // POSIX sign convention: a fixed UTC+02:00 zone
    const DateTime clock(2024, 6, 15, 12, 0, 0);
    const DateTime utc(clock.getTicksProperty(), DateTimeKind::Utc);
    const DateTime local(clock.getTicksProperty(), DateTimeKind::Local);

    const DateTimeOffset fromUtc(utc);
    const DateTimeOffset fromLocal(local);
    const DateTimeOffset fromUnspecified(clock);

    EXPECT_EQ(fromUtc.getOffsetProperty(), TimeSpan::Zero);
    EXPECT_EQ(fromLocal.getOffsetProperty(), TimeSpan::FromHours(2));
    EXPECT_EQ(fromUnspecified.getOffsetProperty(), TimeSpan::FromHours(2));

    for (const DateTimeOffset* value : {&fromUtc, &fromLocal, &fromUnspecified}) {
        EXPECT_EQ(value->getDateTimeProperty().getTicksProperty(), clock.getTicksProperty());
        EXPECT_EQ(value->getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
    }
}

TEST(DateTimeOffsetKindRippleTests, Post1941_GeneralParseDefaultsAnOffsetlessInputToLocal) {
    ScopedDateTimeOffsetTz tz("UTC-02"); // fixed UTC+02:00
    DateTimeOffset parsed;

    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T12:34:56", parsed));
    EXPECT_EQ(parsed.getDateTimeProperty(), DateTime(2024, 6, 15, 12, 34, 56));
    EXPECT_EQ(parsed.getOffsetProperty(), TimeSpan::FromHours(2));
    EXPECT_EQ(DateTimeOffset::Parse("2024-06-15T12:34:56").getOffsetProperty(),
              TimeSpan::FromHours(2));

    // An explicit designator still wins over the local default.
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T12:34:56Z", parsed));
    EXPECT_EQ(parsed.getOffsetProperty(), TimeSpan::Zero);
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T12:34:56-05:30", parsed));
    EXPECT_EQ(parsed.getOffsetProperty(), TimeSpan::FromMinutes(-330));
}

TEST(DateTimeOffsetKindRippleTests, Post1941_OffsetlessParseStillEnforcesTheUtcRange) {
    ScopedDateTimeOffsetTz tz("UTC-14"); // fixed UTC+14:00
    const DateTimeOffset sentinel(DateTime(2024, 6, 15), TimeSpan::Zero);
    DateTimeOffset parsed = sentinel;

    // The visible clock fits, but applying the default local offset would place the represented
    // UTC instant before DateTime.MinValue. TryParse must fail and reset its out value; Parse must
    // translate the same condition to FormatException.
    EXPECT_FALSE(DateTimeOffset::TryParse("0001-01-01T00:00:00", parsed));
    EXPECT_EQ(parsed, DateTimeOffset::MinValue);
    EXPECT_THROW((void)DateTimeOffset::Parse("0001-01-01T00:00:00"),
                 System::FormatException);
}

TEST(DateTimeOffsetKindRippleTests, Post1941_ExplicitOffsetMustAgreeWithUtcAndLocalKind) {
    ScopedDateTimeOffsetTz tz("UTC-02");
    const DateTime clock(2024, 6, 15, 12, 0, 0);
    const DateTime utc(clock.getTicksProperty(), DateTimeKind::Utc);
    const DateTime local(clock.getTicksProperty(), DateTimeKind::Local);

    DateTimeOffset utcValue(utc, TimeSpan::Zero);
    DateTimeOffset localValue(local, TimeSpan::FromHours(2));
    DateTimeOffset arbitrary(clock, TimeSpan::FromHours(-5));
    EXPECT_EQ(utcValue.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(localValue.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(arbitrary.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);

    try {
        (void)DateTimeOffset(utc, TimeSpan::FromHours(15));
        FAIL() << "expected Utc/offset mismatch";
    } catch (const ArgumentException& e) {
        // Kind consistency runs before the ordinary +/-14h bound, as in DateTimeOffset.cs.
        EXPECT_EQ(e.getParamNameProperty(), "offset");
        EXPECT_EQ(e.getMessageProperty(),
                  "The UTC Offset for Utc DateTime instances must be 0. (Parameter 'offset')");
    }

    try {
        (void)DateTimeOffset(local, TimeSpan(TimeSpan::TicksPerMinute / 2));
        FAIL() << "expected Local/offset mismatch";
    } catch (const ArgumentException& e) {
        // This is the Local mismatch, not the later whole-minute diagnostic.
        EXPECT_EQ(e.getParamNameProperty(), "offset");
        EXPECT_EQ(e.getMessageProperty(),
                  "The UTC Offset of the local dateTime parameter does not match the offset "
                  "argument. (Parameter 'offset')");
    }

    EXPECT_NO_THROW((void)DateTimeOffset(clock, TimeSpan::FromHours(13)));
    EXPECT_THROW((void)DateTimeOffset(clock, TimeSpan(TimeSpan::TicksPerMinute / 2)),
                 ArgumentException);
}

TEST(DateTimeOffsetKindRippleTests, Post1941_DateTimePropertiesPublishTheReferenceKinds) {
    ScopedDateTimeOffsetTz tz("UTC-02");
    const DateTimeOffset value(2024, 6, 15, 12, 0, 0, TimeSpan::FromHours(5));

    const DateTime clock = value.getDateTimeProperty();
    const DateTime utc = value.getUtcDateTimeProperty();
    const DateTime local = value.getLocalDateTimeProperty();

    EXPECT_EQ(clock.getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(clock.getHourProperty(), 12);
    EXPECT_EQ(utc.getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(utc.getHourProperty(), 7);
    EXPECT_EQ(local.getKindProperty(), DateTimeKind::Local);
    EXPECT_EQ(local.getHourProperty(), 9);

    // Internal clock construction must stay Unspecified even when it starts from UtcDateTime.
    const DateTimeOffset shifted = value.ToOffset(TimeSpan::FromHours(-3));
    EXPECT_EQ(shifted.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(shifted.getUtcTicksProperty(), value.getUtcTicksProperty());
    const DateTimeOffset asLocal = value.ToLocalTime();
    EXPECT_EQ(asLocal.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
}

TEST(DateTimeOffsetKindRippleTests, Post1941_NowFactoriesShareOneUtcInstantAndCorrectOffsets) {
    ScopedDateTimeOffsetTz tz("UTC-02");
    const DateTimeOffset utcBefore = DateTimeOffset::getUtcNowProperty();
    const DateTimeOffset localNow = DateTimeOffset::getNowProperty();
    const DateTimeOffset utcAfter = DateTimeOffset::getUtcNowProperty();

    EXPECT_EQ(utcBefore.getOffsetProperty(), TimeSpan::Zero);
    EXPECT_EQ(localNow.getOffsetProperty(), TimeSpan::FromHours(2));
    EXPECT_EQ(utcAfter.getOffsetProperty(), TimeSpan::Zero);
    EXPECT_EQ(utcBefore.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(localNow.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(utcBefore.getUtcDateTimeProperty().getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(localNow.getLocalDateTimeProperty().getKindProperty(), DateTimeKind::Local);

    constexpr SharpRuntime::longcs generousClockTolerance = 5 * DateTime::TicksPerSecond;
    EXPECT_LT(tickDistance(localNow.getUtcTicksProperty(), utcBefore.getUtcTicksProperty()),
              generousClockTolerance);
    EXPECT_LT(tickDistance(localNow.getUtcTicksProperty(), utcAfter.getUtcTicksProperty()),
              generousClockTolerance);
    EXPECT_EQ(localNow.getTicksProperty() - localNow.getUtcTicksProperty(),
              TimeSpan::FromHours(2).getTicksProperty());
}

TEST(DateTimeOffsetKindRippleTests, Post1941_LocalConversionClampsAtBothDateTimeBounds) {
    {
        ScopedDateTimeOffsetTz tz("UTC-14"); // POSIX spelling for fixed UTC+14:00
        const DateTimeOffset local = DateTimeOffset::MaxValue.ToLocalTime();
        EXPECT_EQ(local.getOffsetProperty(), TimeSpan::FromHours(14));
        EXPECT_EQ(local.getTicksProperty(), DateTime::MaxTicks);
        EXPECT_EQ(local.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);

        const DateTime property = DateTimeOffset::MaxValue.getLocalDateTimeProperty();
        EXPECT_EQ(property.getTicksProperty(), DateTime::MaxTicks);
        EXPECT_EQ(property.getKindProperty(), DateTimeKind::Local);
    }

    {
        ScopedDateTimeOffsetTz tz("UTC+14"); // POSIX spelling for fixed UTC-14:00
        const DateTimeOffset local = DateTimeOffset::MinValue.ToLocalTime();
        EXPECT_EQ(local.getOffsetProperty(), TimeSpan::FromHours(-14));
        EXPECT_EQ(local.getTicksProperty(), 0);
        EXPECT_EQ(local.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);

        const DateTime property = DateTimeOffset::MinValue.getLocalDateTimeProperty();
        EXPECT_EQ(property.getTicksProperty(), 0);
        EXPECT_EQ(property.getKindProperty(), DateTimeKind::Local);
    }
}
#endif

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
// DateTimeOffset formatting uses DateTime's one custom grammar with an offset
// ---------------------------------------------------------------------------

TEST(DateTimeOffsetFormatKindRippleTests, CustomClockFormatsDoNotAcquireAnOffsetSuffix) {
    const DateTimeOffset value(2024, 6, 15, 10, 30, 45, TimeSpan::FromHours(5.5));

    EXPECT_EQ(value.ToString("yyyy"), "2024");
    EXPECT_EQ(value.ToString("yyyy-MM-dd HH:mm:ss"), "2024-06-15 10:30:45");
    EXPECT_EQ(value.ToString(""), value.ToString());
}

TEST(DateTimeOffsetFormatKindRippleTests, CustomOffsetTokensUseTheStoredOffsetAndTheirOwnWidths) {
    const DateTimeOffset positive(2024, 6, 15, 10, 30, 45, TimeSpan::FromMinutes(330));
    EXPECT_EQ(positive.ToString("%z"), "+5");
    EXPECT_EQ(positive.ToString("zz"), "+05");
    EXPECT_EQ(positive.ToString("zzz"), "+05:30");
    EXPECT_EQ(positive.ToString("zzzz"), "+05:30");
    EXPECT_EQ(positive.ToString("%K"), "+05:30");
    EXPECT_EQ(positive.ToString("KK"), "+05:30+05:30");
    EXPECT_EQ(positive.ToString("yyyy %z"), "2024 +5");

    const DateTimeOffset negative(2024, 6, 15, 10, 30, 45, TimeSpan::FromMinutes(-30));
    EXPECT_EQ(negative.ToString("%z"), "-0");
    EXPECT_EQ(negative.ToString("zz"), "-00");
    EXPECT_EQ(negative.ToString("zzz"), "-00:30");

    // One character is first interpreted as a standard specifier. `%` is therefore required
    // for the one-character custom forms, just as DateTime requires `%d` for a custom day.
    EXPECT_THROW(positive.ToString("z"), System::FormatException);
    EXPECT_THROW(positive.ToString("K"), System::FormatException);
}

TEST(DateTimeOffsetFormatKindRippleTests, QuotedOffsetLettersStayLiteral) {
    const DateTimeOffset value(2024, 6, 15, 10, 30, 45, TimeSpan::FromMinutes(330));
    EXPECT_EQ(value.ToString("yyyy 'z' 'K' zzz"), "2024 z K +05:30");
    EXPECT_EQ(value.ToString("yyyy \"z\" \"K\" zzz"), "2024 z K +05:30");
}

TEST(DateTimeOffsetFormatKindRippleTests, StandardClockPatternsDoNotAppendAnOffset) {
    const DateTimeOffset value(2024, 6, 15, 10, 30, 45, TimeSpan::FromMinutes(330));
    EXPECT_EQ(value.ToString("s"), "2024-06-15T10:30:45");
    EXPECT_EQ(value.ToString("G"), "06/15/2024 10:30:45");
}

TEST(DateTimeOffsetFormatKindRippleTests, RoundtripHasSevenDigitsAndUniversalFullIsRejected) {
    const DateTime clock = DateTime(2024, 6, 15, 10, 30, 45, 123).AddTicks(4567);
    const DateTimeOffset value(clock, TimeSpan::FromMinutes(330));

    EXPECT_EQ(value.ToString("O"), "2024-06-15T10:30:45.1234567+05:30");
    EXPECT_EQ(value.ToString("o"), value.ToString("O"));
    EXPECT_THROW(value.ToString("U"), System::FormatException);
}

TEST(DateTimeOffsetFormatKindRippleTests, OptionalFractionsTrimZerosAndADanglingPoint) {
    const DateTime partial = DateTime(2024, 6, 15, 10, 30, 45, 123).AddTicks(4000);
    const DateTimeOffset withFraction(partial, TimeSpan::FromMinutes(330));
    const DateTimeOffset whole(2024, 6, 15, 10, 30, 45, TimeSpan::FromMinutes(330));

    EXPECT_EQ(withFraction.ToString("yyyy-MM-dd'T'HH:mm:ss.FFFFFFFzzz"),
              "2024-06-15T10:30:45.1234+05:30");
    EXPECT_EQ(whole.ToString("yyyy-MM-dd'T'HH:mm:ss.FFFFFFFzzz"),
              "2024-06-15T10:30:45+05:30");
    EXPECT_THROW(withFraction.ToString("yyyy-MM-ddTHH:mm:ss.ffffffffzzz"),
                 System::FormatException);
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

TEST(DateTimeOffsetTests2, ToOffset_ExtremeTimeSpanThrowsWithoutSignedOverflow) {
    // ToOffset accepts a full TimeSpan, not merely an already-validated timezone offset. The
    // checked DateTime addition must reject these values before the DateTimeOffset offset guard;
    // spelling this as raw signed addition is UBSan-confirmed overflow at the upper boundary.
    EXPECT_THROW(DateTimeOffset::MaxValue.ToOffset(TimeSpan(SharpRuntime::LONGCS_MAX)),
                 ArgumentOutOfRangeException);
    EXPECT_THROW(DateTimeOffset::MinValue.ToOffset(TimeSpan(SharpRuntime::LONGCS_MIN)),
                 ArgumentOutOfRangeException);
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

// FLIPPED TWICE. #1879 (approved 2026-07-31) made the offset text consume in
// full AND demanded two digits in each field; #1929 row 3 (decided 2026-08-18)
// kept the first half and dropped the second, which is the half .NET never had.
TEST(DateTimeOffsetTests2, Ccf002_OffsetGrammarIsNowStrict) {
    DateTimeOffset out;
    EXPECT_FALSE(DateTimeOffset::TryParse("2024-06-15T10:30:00+02:00junk", out));
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00+2:5", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 125);
    EXPECT_NO_THROW((void)DateTimeOffset::Parse("2024-06-15T10:30:00+2:5"));
    // Every well-formed offset still parses to exactly its previous value.
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00+02:00", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 120);
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00-05:30", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), -330);
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00+14:00", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 840);
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00Z", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 0);
}

// PREMISE CORRECTION, and then its consequence. The decision packet §C.4 called
// "+2:5" a "wrong answer that survives round-tripping" because it yields 125
// minutes. Measured against the reference: .NET's ParseTimeZone
// (Globalization/DateTimeParse.cs:530-548) accepts a one- OR two-digit hour and
// a one- or two-digit minute and builds `new TimeSpan(hourOffset, minuteOffset,
// 0)`, so .NET reads "+2:5" as 2h05m -- the very value the port already
// produced. The port and .NET AGREED on this input, and #1879 rejecting it was
// a deliberate narrowing of an already-narrow subset, not the removal of a
// wrong answer. That is what this test recorded while the narrowing stood.
//
// #1929 row 3 (decided 2026-08-18) removed the narrowing. The test now pins the
// agreement itself, which is what it was always about.
TEST(DateTimeOffsetTests2, Ccf002_UnpaddedOffsetRejectionIsANarrowingNotAParityFix) {
    DateTimeOffset out;
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00+2:5", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 125);
    // The padded spelling of the same instant means the same thing, which is the
    // whole point: neither spelling was ever the wrong answer.
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T10:30:00+02:05", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 125);
}

TEST(DateTimeOffsetTests2, Approved1929_WhitespaceClockWidthsAndFractionTicks) {
    const long long expected = DateTime(2024, 6, 15, 1, 2, 3).AddTicks(1'234'567).getTicksProperty();
    const char* accepted[] = {
        " 2024-06-15T1:2:3.1234567+02:05 ",
        "\t2024-06-15T1:2:3.1234567+02:05\r\n",
        "\f\v2024-06-15T1:2:3.1234567+02:05\v"
    };
    for (const char* text : accepted) {
        DateTimeOffset out;
        ASSERT_TRUE(DateTimeOffset::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), expected) << text;
        EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 125) << text;
        EXPECT_EQ(DateTimeOffset::Parse(text).getTicksProperty(), expected) << text;
    }

    const char* fractions[] = {"1", "12", "123", "1234", "12345", "123456", "1234567"};
    const long long fractionTicks[] = {1000000, 1200000, 1230000, 1234000, 1234500, 1234560, 1234567};
    const long long wholeSecond = DateTime(2024, 6, 15, 1, 2, 3).getTicksProperty();
    for (int i = 0; i < 7; ++i) {
        const std::string text = std::string("2024-06-15T1:2:3.") + fractions[i] + "Z";
        DateTimeOffset out;
        ASSERT_TRUE(DateTimeOffset::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), wholeSecond + fractionTicks[i]) << text;
    }
}

TEST(DateTimeOffsetTests2, Approved1929_MalformedAndUnapprovedFormsRemainRejected) {
    DateTimeOffset out;
    const char* rejected[] = {
        "2024-06-15T1:2:3.12345678Z", "2024-06-15T1:2:3.Z",
        "2024-06-15T1:2:3.abcZ", "2024-06-15T1:2:3.1garbageZ",
        "2024-06-15T1 :2:3Z", "2024-06-15T1: 2:3Z", "2024-06-15T1:2: 3Z",
        " \t\r\n ",
        // #1929 rows 1 and 3 moved "2024-6-15T1:2:3Z", "2024-06-5T1:2:3Z",
        // "+2:5", "+02:5" and "+2:05" out of this list on 2026-08-18. What
        // replaces them is what the widened grammar still refuses.
        "2024-06-15T1:2:3+2:60",     // minute field at 60
        "2024-06-15T1:2:3+02:005",   // three-digit minute
        "2024-06-15T1:2:3+12345",    // five-digit offset run
        "2024-06-15T1:2:3+",         // sign with no digits
        "2024-06-15T1:2:3+2:",       // colon with no minute
        "24-06-15T1:2:3Z"            // two-digit year
    };
    for (const char* text : rejected) EXPECT_FALSE(DateTimeOffset::TryParse(text, out)) << text;

    try {
        (void)DateTimeOffset::Parse("2024-06-15T1:2:3.12345678Z");
        FAIL();
    } catch (const System::FormatException& e) {
        EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u));
        EXPECT_EQ(std::string(e.what()), "String was not recognized as a valid DateTimeOffset.");
    }
}

// #1929 rows 1 and 3 (decided 2026-08-18). This is the door where the offset is
// KEPT, so this is where its value is pinned. The three-or-four-digit split is
// ParseTimeZone's `hourOffset = value / 100; minuteOffset = value % 100`
// (DateTimeParse.cs:552-556) -- which is why "+800" and "+0800" agree.
TEST(DateTimeOffsetTests2, Decided1929_OffsetSpellingsCarryTheirDotNetValues) {
    struct Case { const char* suffix; int minutes; };
    const Case cases[] = {
        {"Z", 0},        {"z", 0},
        {"+8", 480},     {"+08", 480},    {"+800", 480},   {"+0800", 480},
        {"-8", -480},    {"-0800", -480},
        {"+2:5", 125},   {"+02:05", 125}, {"+2:05", 125},  {"+02:5", 125},
        {"+205", 125},   {"+0205", 125},
        {"-5:30", -330}, {"-0530", -330},
        {"+14:00", 840}, {"+1400", 840},  {"-1400", -840},
        {"+0", 0},       {"+0000", 0},    {"-0", 0},
    };
    for (const Case& c : cases) {
        const std::string text = std::string("2024-06-15T10:30:00") + c.suffix;
        DateTimeOffset out;
        ASSERT_TRUE(DateTimeOffset::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTotalOffsetMinutesProperty(), c.minutes) << text;
        EXPECT_EQ(out.getTicksProperty(), 638540442000000000LL) << text;
    }
}

// The +/-14h bound belongs HERE and not to the shared grammar, because that is
// where .NET applies it: ParseTimeZone permits an hour up to 99 and the
// MinOffset/MaxOffset test runs later, at the two sites that store an offset
// (DateTimeParse.cs:2777,2875). DateTime, which parses an offset and discards
// it, must not inherit the check -- and DateTimeTests pins that it does not.
TEST(DateTimeOffsetTests2, Decided1929_TheFourteenHourBoundIsThisDoorsNotTheGrammars) {
    DateTimeOffset out;
    const char* rejected[] = {"+15", "+1500", "+14:01", "+1401", "-1401", "+99", "+9959"};
    for (const char* suffix : rejected) {
        const std::string text = std::string("2024-06-15T10:30:00") + suffix;
        EXPECT_FALSE(DateTimeOffset::TryParse(text, out)) << text;
    }
    // The very same strings are accepted by DateTime, which ignores the value.
    DateTime ignored;
    for (const char* suffix : rejected) {
        const std::string text = std::string("2024-06-15T10:30:00") + suffix;
        EXPECT_TRUE(DateTime::TryParse(text, ignored)) << text;
    }
}

// The old split searched for a sign from CHARACTER 10 and handed the prefix to
// DateTime::TryParse. A one-digit month or day makes the date part shorter than
// ten characters, so the search would have started inside the time -- or, for a
// bare date, past the end. This is the case that forced the rewrite.
TEST(DateTimeOffsetTests2, Decided1929_ShortDatePartStillFindsItsOffset) {
    DateTimeOffset out;
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-6-5T1:2:3-05:00", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), -300);
    EXPECT_EQ(out.getTicksProperty(), DateTime(2024, 6, 5, 1, 2, 3).getTicksProperty());

    // A bare short date is eight characters; the removed `size() < 10` precheck
    // rejected it before the grammar ever saw it.
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-6-5Z", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 0);
    EXPECT_EQ(out.getTicksProperty(), DateTime(2024, 6, 5).getTicksProperty());
}

TEST(DateTimeOffsetTests2, Ticket1880_TryParseFailureAlwaysAssignsMinValue) {
    const DateTimeOffset sentinel(DateTime(638'540'436'301'234'567LL),
                                  TimeSpan::FromHours(2));
    DateTimeOffset out = sentinel;
    const auto expectFailure = [&out, &sentinel](const char* text) {
        out = sentinel;
        EXPECT_FALSE(DateTimeOffset::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), DateTimeOffset::MinValue.getTicksProperty()) << text;
        EXPECT_EQ(out.getUtcTicksProperty(), DateTimeOffset::MinValue.getUtcTicksProperty()) << text;
        EXPECT_EQ(out.getOffsetProperty().getTicksProperty(), 0) << text;
    };

    expectFailure("");                                      // too short / empty
    expectFailure(" \t\r\n ");                            // whitespace-only
    expectFailure("2024-06-15T1:2:3+02:60");                // offset range
    expectFailure("2024-06-15T1:2:3+2:60");                 // offset grammar
    expectFailure("2024-02-30T1:2:3Z");                     // DateTime failure
    expectFailure("0001-01-01T0:0:0+14:00");                // UTC below MinValue
    expectFailure("9999-12-31T23:59:59-14:00");             // UTC above MaxValue
    expectFailure("2024-06-15T1:2:3+02:00junk");            // trailing content

    ASSERT_TRUE(DateTimeOffset::TryParse("0001-01-01T0:0:0Z", out));
    EXPECT_EQ(out.getUtcTicksProperty(), DateTimeOffset::MinValue.getUtcTicksProperty());
    ASSERT_TRUE(DateTimeOffset::TryParse("9999-12-31T23:59:59.9999999Z", out));
    EXPECT_EQ(out.getUtcTicksProperty(), DateTime::MaxTicks);
    ASSERT_TRUE(DateTimeOffset::TryParse("2024-06-15T1:2:3.1234567+02:05", out));
    EXPECT_EQ(out.getTotalOffsetMinutesProperty(), 125);

    try {
        (void)DateTimeOffset::Parse("not-an-offset");
        FAIL();
    } catch (const System::FormatException& e) {
        EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u));
        EXPECT_EQ(std::string(e.what()),
                  "String was not recognized as a valid DateTimeOffset.");
    }
}

// ===========================================================================
// #1943 (SA-16.2) -- DateTimeOffset::ParseExact
// ===========================================================================

namespace {

/// Reused shape rather than a third zone double: a fixed whole-hour offset, so every assertion is
/// about this type's rules rather than about the container's tzdata (#2351's lesson).
class Dto1943FixedZone final : public System::ILocalTimeZone {
public:
    explicit Dto1943FixedZone(SharpRuntime::longcs hours) : hours_(hours) {}
    [[nodiscard]] System::TimeSpan GetUtcOffset(const System::DateTime&) const override {
        return System::TimeSpan::FromHours(static_cast<double>(hours_));
    }
    [[nodiscard]] bool IsDaylightSavingTime(const System::DateTime&) const override {
        return false;
    }
private:
    SharpRuntime::longcs hours_;
};

} // namespace

// AN OFFSET IS NOT A TIME ZONE, which is the measurement that let this land at all: a format
// carrying an explicit offset needs no zone database whatever.
TEST(DateTimeOffsetParseExact1943Tests, AnExplicitOffsetNeedsNoZoneAtAll) {
    using System::DateTimeOffset;

    const auto parsed = DateTimeOffset::ParseExact("2024-06-15T12:00:00+05:30",
                                                   "yyyy-MM-ddTHH:mm:sszzz");
    EXPECT_EQ(parsed.getDateTimeProperty(), System::DateTime(2024, 6, 15, 12, 0, 0));
    EXPECT_EQ(parsed.getOffsetProperty(), System::TimeSpan::FromMinutes(330));

    // THE VALUE IS CAPTURED, NOT ADJUSTED -- this is not the DateTime matrix with a different
    // result type. The wall-clock time stays exactly as written and only the offset is chosen,
    // which is what makes a DateTimeOffset a DateTimeOffset.
    EXPECT_EQ(parsed.getDateTimeProperty().getHourProperty(), 12);

    // `K` carries an offset too, and a literal `Z` through it is offset zero.
    const auto z = DateTimeOffset::ParseExact("2024-06-15T12:00:00Z", "yyyy-MM-ddTHH:mm:ssK");
    EXPECT_EQ(z.getOffsetProperty(), System::TimeSpan(static_cast<SharpRuntime::longcs>(0)));
    EXPECT_EQ(z.getDateTimeProperty(), System::DateTime(2024, 6, 15, 12, 0, 0));
}

// THE COST SA-16.6 ACCEPTED KNOWINGLY, and it is larger here than for DateTime: there only a few
// styles convert, here EVERY format without an offset token needs a zone, so the most ordinary
// call raises unless one is supplied.
TEST(DateTimeOffsetParseExact1943Tests, ANoOffsetFormatNamesTheMissingZoneAndSaysWhatToPass) {
    using System::DateTimeOffset;
    using System::Globalization::DateTimeStyles;
    const Dto1943FixedZone plusTwo(2);

    EXPECT_THROW(DateTimeOffset::ParseExact("2024-06-15", "yyyy-MM-dd"),
                 System::ArgumentNullException);
    try {
        DateTimeOffset::ParseExact("2024-06-15", "yyyy-MM-dd");
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "zone");
        const std::string what = e.what();
        // The message must name ALL THREE routes out, because a caller hitting this has three
        // genuinely different fixes and no way to guess them from "zone was null".
        EXPECT_NE(what.find("CurrentTimeZone"), std::string::npos) << what;
        EXPECT_NE(what.find("zzz"), std::string::npos) << what;
        EXPECT_NE(what.find("AssumeUniversal"), std::string::npos) << what;
    }

    // Route 1: supply the zone. The offset becomes the zone's, and the value is still not adjusted.
    const auto withZone = DateTimeOffset::ParseExact("2024-06-15", "yyyy-MM-dd", nullptr,
                                                     DateTimeStyles::None, &plusTwo);
    EXPECT_EQ(withZone.getOffsetProperty(), System::TimeSpan::FromHours(2));
    EXPECT_EQ(withZone.getDateTimeProperty(), System::DateTime(2024, 6, 15, 0, 0, 0));

    // Route 2: AssumeUniversal makes the offset ZERO and needs no zone -- .NET says so in its own
    // comment, and it is the one defaulted-offset route that is zone-free.
    const auto assumed = DateTimeOffset::ParseExact("2024-06-15", "yyyy-MM-dd", nullptr,
                                                    DateTimeStyles::AssumeUniversal);
    EXPECT_EQ(assumed.getOffsetProperty(), System::TimeSpan(static_cast<SharpRuntime::longcs>(0)));

    // Route 3 is the offset token, asserted in the case above.
}

TEST(DateTimeOffsetParseExact1943Tests, StylesAreValidatedAndTheOutParameterIsLeftAlone) {
    using System::DateTimeOffset;
    using System::Globalization::DateTimeStyles;

    DateTimeOffset sentinel = DateTimeOffset::ParseExact("2001-02-03T04:05:06Z",
                                                         "yyyy-MM-ddTHH:mm:ssK");
    EXPECT_THROW(DateTimeOffset::TryParseExact("2024-06-15", "yyyy-MM-dd", nullptr,
                                               static_cast<DateTimeStyles>(0x4000), sentinel),
                 System::ArgumentException);
    EXPECT_EQ(sentinel.getDateTimeProperty(), System::DateTime(2001, 2, 3, 4, 5, 6));

    // DateTimeOffset has a narrower validator than DateTime: NoCurrentDateDefault is invalid,
    // and Try* still validates it before touching the out parameter.
    EXPECT_THROW(DateTimeOffset::TryParseExact(
                     "2024-06-15", "yyyy-MM-dd", nullptr,
                     DateTimeStyles::NoCurrentDateDefault, sentinel),
                 System::ArgumentException);
    EXPECT_EQ(sentinel.getDateTimeProperty(), System::DateTime(2001, 2, 3, 4, 5, 6));

    // The parameter name is `styles` here where DateTime's is `style`. .NET VARIES IT BY OVERLOAD
    // rather than using one name, and both are transcribed as they are rather than harmonised.
    try {
        DateTimeOffset::ParseExact("2024-06-15", "yyyy-MM-dd", nullptr,
                                   static_cast<DateTimeStyles>(0x4000));
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "styles");
    }

    try {
        (void)DateTimeOffset::ParseExact("2024-06-15", "yyyy-MM-dd", nullptr,
                                         DateTimeStyles::NoCurrentDateDefault);
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "styles");
    }

    EXPECT_THROW(DateTimeOffset::ParseExact(
                     "2024-06-15", std::vector<std::string>{}, nullptr,
                     static_cast<DateTimeStyles>(0x4000)),
                 System::ArgumentException)
        << "styles are validated before an empty format collection is diagnosed";
}

TEST(DateTimeOffsetParseExact1943Tests, AdjustToUniversalNormalizesClockAndOffset) {
    using System::DateTimeOffset;
    using System::Globalization::DateTimeStyles;
    const Dto1943FixedZone plusTwo(2);

    const auto explicitOffset = DateTimeOffset::ParseExact(
        "2024-06-15T12:00:00+02:00", "yyyy-MM-ddTHH:mm:sszzz", nullptr,
        DateTimeStyles::AdjustToUniversal);
    EXPECT_EQ(explicitOffset.getDateTimeProperty(), System::DateTime(2024, 6, 15, 10, 0, 0));
    EXPECT_EQ(explicitOffset.getOffsetProperty(), System::TimeSpan::Zero);

    const auto defaultedLocal = DateTimeOffset::ParseExact(
        "2024-06-15T12:00:00", "yyyy-MM-ddTHH:mm:ss", nullptr,
        DateTimeStyles::AdjustToUniversal, &plusTwo);
    EXPECT_EQ(defaultedLocal.getDateTimeProperty(), System::DateTime(2024, 6, 15, 10, 0, 0));
    EXPECT_EQ(defaultedLocal.getOffsetProperty(), System::TimeSpan::Zero);

    // DateTimeOffset accepts and ignores RoundtripKind for compatibility, unlike DateTime's
    // generic validation which rejects this combination. The remaining Adjust flag still acts.
    const auto roundtripIgnored = DateTimeOffset::ParseExact(
        "2024-06-15T12:00:00+02:00", "yyyy-MM-ddTHH:mm:sszzz", nullptr,
        DateTimeStyles::RoundtripKind | DateTimeStyles::AdjustToUniversal);
    EXPECT_EQ(roundtripIgnored.getDateTimeProperty(), System::DateTime(2024, 6, 15, 10, 0, 0));
    EXPECT_EQ(roundtripIgnored.getOffsetProperty(), System::TimeSpan::Zero);
}

TEST(DateTimeOffsetParseExact1943Tests, TheOffsetBoundAndTheUtcRangeAreBothEnforced) {
    using System::DateTimeOffset;
    DateTimeOffset out;

    EXPECT_TRUE(DateTimeOffset::TryParseExact("2024-06-15 +14:00", "yyyy-MM-dd zzz", out));
    EXPECT_FALSE(DateTimeOffset::TryParseExact("2024-06-15 +14:59", "yyyy-MM-dd zzz", out));

    // .NET requires BOTH the parsed time and its UTC equivalent to fit a DateTime. A value at the
    // very start of the range with a positive offset has a UTC equivalent BEFORE MinValue, so
    // only the second check can refuse it -- the first passes.
    EXPECT_FALSE(DateTimeOffset::TryParseExact("0001-01-01T00:00:00+05:00",
                                               "yyyy-MM-ddTHH:mm:sszzz", out));
    EXPECT_TRUE(DateTimeOffset::TryParseExact("0001-01-01T00:00:00-05:00",
                                              "yyyy-MM-ddTHH:mm:sszzz", out));
}

TEST(DateTimeOffsetParseExact1943Tests, AnInvalidZoneOffsetFailsWithoutSignedOverflow) {
    class InvalidZone final : public System::ILocalTimeZone {
    public:
        explicit InvalidZone(System::TimeSpan offset) : offset_(offset) {}
        [[nodiscard]] System::TimeSpan GetUtcOffset(const System::DateTime&) const override {
            return offset_;
        }
        [[nodiscard]] bool IsDaylightSavingTime(const System::DateTime&) const override {
            return false;
        }
    private:
        System::TimeSpan offset_;
    };

    const DateTimeOffset sentinel(DateTime(2001, 2, 3), TimeSpan::Zero);
    for (const TimeSpan& invalid : {TimeSpan::MinValue, TimeSpan::MaxValue, TimeSpan(1)}) {
        const InvalidZone zone(invalid);
        DateTimeOffset out = sentinel;
        EXPECT_FALSE(DateTimeOffset::TryParseExact(
            "2024-06-15", "yyyy-MM-dd", nullptr,
            System::Globalization::DateTimeStyles::None, out, &zone));
        EXPECT_EQ(out, DateTimeOffset::MinValue);
        EXPECT_THROW((void)DateTimeOffset::ParseExact(
                         "2024-06-15", "yyyy-MM-dd", nullptr,
                         System::Globalization::DateTimeStyles::None, &zone),
                     System::FormatException);
    }
}
