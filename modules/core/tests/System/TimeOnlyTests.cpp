// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/TimeOnly.hpp"
#include "System/TimeSpan.hpp"
#include "System/DateTime.hpp"
#include "System/DateOnly.hpp"
#include "System/FormatException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

using System::TimeOnly;
using System::TimeSpan;
using System::ArgumentOutOfRangeException;

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, DefaultCtor_IsMidnight) {
    TimeOnly t;
    EXPECT_EQ(t.getHourProperty(), 0);
    EXPECT_EQ(t.getMinuteProperty(), 0);
    EXPECT_EQ(t.getSecondProperty(), 0);
    EXPECT_EQ(t.getMillisecondProperty(), 0);
}

TEST(TimeOnlyTests, HourMinuteCtor) {
    TimeOnly t(14, 30);
    EXPECT_EQ(t.getHourProperty(), 14);
    EXPECT_EQ(t.getMinuteProperty(), 30);
    EXPECT_EQ(t.getSecondProperty(), 0);
}

TEST(TimeOnlyTests, HourMinuteSecondCtor) {
    TimeOnly t(9, 5, 45);
    EXPECT_EQ(t.getHourProperty(), 9);
    EXPECT_EQ(t.getMinuteProperty(), 5);
    EXPECT_EQ(t.getSecondProperty(), 45);
}

TEST(TimeOnlyTests, FullCtor) {
    TimeOnly t(23, 59, 59, 999);
    EXPECT_EQ(t.getHourProperty(), 23);
    EXPECT_EQ(t.getMinuteProperty(), 59);
    EXPECT_EQ(t.getSecondProperty(), 59);
    EXPECT_EQ(t.getMillisecondProperty(), 999);
}

TEST(TimeOnlyTests, TicksCtor_Midnight) {
    TimeOnly t(0LL);
    EXPECT_EQ(t.getHourProperty(), 0);
    EXPECT_EQ(t.getMinuteProperty(), 0);
    EXPECT_EQ(t.getSecondProperty(), 0);
}

TEST(TimeOnlyTests, TicksCtor_1Hour) {
    // 1 hour = 36,000,000,000 ticks
    TimeOnly t(36000000000LL);
    EXPECT_EQ(t.getHourProperty(), 1);
    EXPECT_EQ(t.getMinuteProperty(), 0);
}

TEST(TimeOnlyTests, TicksCtor_RoundTrip) {
    TimeOnly orig(12, 34, 56, 789);
    TimeOnly rt(orig.getTicksProperty());
    EXPECT_EQ(rt.getHourProperty(),        orig.getHourProperty());
    EXPECT_EQ(rt.getMinuteProperty(),      orig.getMinuteProperty());
    EXPECT_EQ(rt.getSecondProperty(),      orig.getSecondProperty());
    EXPECT_EQ(rt.getMillisecondProperty(), orig.getMillisecondProperty());
}

TEST(TimeOnlyTests, TicksCtor_Negative_Throws) {
    EXPECT_THROW(TimeOnly(-1LL), ArgumentOutOfRangeException);
}

TEST(TimeOnlyTests, TicksCtor_TooLarge_Throws) {
    EXPECT_THROW(TimeOnly(864000000000LL), ArgumentOutOfRangeException); // == TicksPerDay
}

TEST(TimeOnlyTests, HourMinuteCtor_HourOutOfRange_Throws) {
    EXPECT_THROW(TimeOnly(24, 0), ArgumentOutOfRangeException);
}

TEST(TimeOnlyTests, HourMinuteCtor_MinuteOutOfRange_Throws) {
    EXPECT_THROW(TimeOnly(0, 60), ArgumentOutOfRangeException);
}

TEST(TimeOnlyTests, HourMinuteSecondCtor_SecondOutOfRange_Throws) {
    EXPECT_THROW(TimeOnly(0, 0, 60), ArgumentOutOfRangeException);
}

TEST(TimeOnlyTests, FullCtor_MillisecondOutOfRange_Throws) {
    EXPECT_THROW(TimeOnly(0, 0, 0, 1000), ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// MinValue / MaxValue
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, MinValue_IsMidnight) {
    auto mn = TimeOnly::getMinValueProperty();
    EXPECT_EQ(mn.getHourProperty(),   0);
    EXPECT_EQ(mn.getMinuteProperty(), 0);
    EXPECT_EQ(mn.getSecondProperty(), 0);
    EXPECT_EQ(mn.getMillisecondProperty(), 0);
}

TEST(TimeOnlyTests, MaxValue_Is235959999) {
    auto mx = TimeOnly::getMaxValueProperty();
    EXPECT_EQ(mx.getHourProperty(),   23);
    EXPECT_EQ(mx.getMinuteProperty(), 59);
    EXPECT_EQ(mx.getSecondProperty(), 59);
    EXPECT_EQ(mx.getMillisecondProperty(), 999);
}

// ---------------------------------------------------------------------------
// Ticks property
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, GetTicks_Midnight_Zero) {
    EXPECT_EQ(TimeOnly(0, 0, 0, 0).getTicksProperty(), 0LL);
}

TEST(TimeOnlyTests, GetTicks_OneHour) {
    EXPECT_EQ(TimeOnly(1, 0, 0, 0).getTicksProperty(), 36000000000LL);
}

TEST(TimeOnlyTests, GetTicks_WithMs) {
    // 1 ms = 10000 ticks
    EXPECT_EQ(TimeOnly(0, 0, 0, 1).getTicksProperty(), 10000LL);
}

// ---------------------------------------------------------------------------
// MicrosecondProperty / NanosecondProperty
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, MicrosecondProperty_AlwaysZero) {
    EXPECT_EQ(TimeOnly(12, 30, 45, 500).getMicrosecondProperty(), 0);
}

TEST(TimeOnlyTests, NanosecondProperty_AlwaysZero) {
    EXPECT_EQ(TimeOnly(12, 30, 45, 500).getNanosecondProperty(), 0);
}

// ---------------------------------------------------------------------------
// AddHours / AddMinutes
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, AddHours_ThreeHours) {
    TimeOnly t(10, 0);
    auto r = t.AddHours(3);
    EXPECT_EQ(r.getHourProperty(), 13);
}

TEST(TimeOnlyTests, AddHours_WrapsMidnight) {
    TimeOnly t(23, 0);
    auto r = t.AddHours(2);
    EXPECT_EQ(r.getHourProperty(), 1);
}

TEST(TimeOnlyTests, AddHours_NegativeWrap) {
    TimeOnly t(2, 0);
    auto r = t.AddHours(-3);
    EXPECT_EQ(r.getHourProperty(), 23);
}

TEST(TimeOnlyTests, AddMinutes_TwentyMinutes) {
    TimeOnly t(10, 30);
    auto r = t.AddMinutes(20);
    EXPECT_EQ(r.getMinuteProperty(), 50);
}

TEST(TimeOnlyTests, AddMinutes_WrapsMidnight) {
    TimeOnly t(23, 50);
    auto r = t.AddMinutes(20);
    EXPECT_EQ(r.getHourProperty(), 0);
    EXPECT_EQ(r.getMinuteProperty(), 10);
}

// ---------------------------------------------------------------------------
// Add(TimeSpan)
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, Add_TimeSpan_OneHour) {
    TimeOnly t(10, 0);
    auto r = t.Add(TimeSpan(1, 0, 0)); // hours=1, min=0, sec=0
    EXPECT_EQ(r.getHourProperty(), 11);
}

TEST(TimeOnlyTests, Add_TimeSpan_WrapsMidnight) {
    TimeOnly t(23, 30);
    auto r = t.Add(TimeSpan(1, 0, 0)); // +1 hour → 00:30
    EXPECT_EQ(r.getHourProperty(), 0);
    EXPECT_EQ(r.getMinuteProperty(), 30);
}

TEST(TimeOnlyTests, Add_TimeSpan_NearMaxValue_DoesNotOverflow) {
    // Adding this instance's (bounded, < 1 day) ticks directly to a TimeSpan near
    // TimeSpan::MaxValue's ticks (~Int64::MaxValue) is signed-integer-overflow UB in C++ if
    // done before reducing modulo TicksPerDay -- must reduce first, matching real .NET's
    // AddTicks algorithm.
    TimeOnly t(12, 0);
    EXPECT_NO_THROW({ auto r = t.Add(TimeSpan::MaxValue); (void)r; });
    EXPECT_NO_THROW({ auto r = t.Add(TimeSpan::MinValue); (void)r; });
}

// ---------------------------------------------------------------------------
// Equals / CompareTo / GetHashCode
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, Equals_SameTime_True) {
    TimeOnly a(12, 30, 0);
    TimeOnly b(12, 30, 0);
    EXPECT_TRUE(a.Equals(b));
}

TEST(TimeOnlyTests, Equals_DifferentTime_False) {
    EXPECT_FALSE(TimeOnly(12, 0).Equals(TimeOnly(13, 0)));
}

TEST(TimeOnlyTests, CompareTo_Earlier_Negative) {
    EXPECT_LT(TimeOnly(8, 0).CompareTo(TimeOnly(9, 0)), 0);
}

TEST(TimeOnlyTests, CompareTo_Equal_Zero) {
    EXPECT_EQ(TimeOnly(8, 0).CompareTo(TimeOnly(8, 0)), 0);
}

TEST(TimeOnlyTests, CompareTo_Later_Positive) {
    EXPECT_GT(TimeOnly(10, 0).CompareTo(TimeOnly(9, 0)), 0);
}

TEST(TimeOnlyTests, GetHashCode_SameTime_SameHash) {
    EXPECT_EQ(TimeOnly(12, 30, 45, 500).GetHashCode(),
              TimeOnly(12, 30, 45, 500).GetHashCode());
}

// Replaces GetHashCode_DifferentTime_DifferentHash: TimeOnly::GetHashCode folds 64 tick bits
// into 32 (TimeOnly.hpp:267), so unequal times may legally collide
// (docs/HashAssertionContractRule.md R2). GetHashCode_SameTime_SameHash above owns the contract
// direction; the value inequality the removed assertion stood in for is asserted here.
TEST(TimeOnlyTests, DifferentTime_AreUnequal) {
    EXPECT_FALSE(TimeOnly(12, 0).Equals(TimeOnly(13, 0)));
    EXPECT_NE(TimeOnly(12, 0), TimeOnly(13, 0));
}

// ---------------------------------------------------------------------------
// IsBetween
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, IsBetween_InsideRange_True) {
    TimeOnly t(12, 0);
    EXPECT_TRUE(t.IsBetween(TimeOnly(10, 0), TimeOnly(14, 0)));
}

TEST(TimeOnlyTests, IsBetween_OutsideRange_False) {
    TimeOnly t(9, 0);
    EXPECT_FALSE(t.IsBetween(TimeOnly(10, 0), TimeOnly(14, 0)));
}

TEST(TimeOnlyTests, IsBetween_AtStart_True) {
    TimeOnly t(10, 0);
    EXPECT_TRUE(t.IsBetween(TimeOnly(10, 0), TimeOnly(14, 0)));
}

TEST(TimeOnlyTests, IsBetween_AtEnd_False) {
    // .NET's IsBetween treats the end as exclusive.
    TimeOnly t(14, 0);
    EXPECT_FALSE(t.IsBetween(TimeOnly(10, 0), TimeOnly(14, 0)));
}

TEST(TimeOnlyTests, IsBetween_EqualStartAndEnd_AlwaysFalse) {
    EXPECT_FALSE(TimeOnly(10, 0).IsBetween(TimeOnly(10, 0), TimeOnly(10, 0)));
    EXPECT_FALSE(TimeOnly(12, 0).IsBetween(TimeOnly(10, 0), TimeOnly(10, 0)));
}

TEST(TimeOnlyTests, IsBetween_MidnightWrapping_Inside) {
    // Range 22:00–02:00 wraps midnight
    TimeOnly t(23, 0);
    EXPECT_TRUE(t.IsBetween(TimeOnly(22, 0), TimeOnly(2, 0)));
}

TEST(TimeOnlyTests, IsBetween_MidnightWrapping_Outside) {
    TimeOnly t(12, 0);
    EXPECT_FALSE(t.IsBetween(TimeOnly(22, 0), TimeOnly(2, 0)));
}

// ---------------------------------------------------------------------------
// FromTimeSpan / ToTimeSpan
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, FromTimeSpan_RoundTrip) {
    TimeOnly orig(14, 30, 25, 100);
    TimeSpan ts = orig.ToTimeSpan();
    TimeOnly rt  = TimeOnly::FromTimeSpan(ts);
    EXPECT_EQ(rt.getHourProperty(),        14);
    EXPECT_EQ(rt.getMinuteProperty(),      30);
    EXPECT_EQ(rt.getSecondProperty(),      25);
    EXPECT_EQ(rt.getMillisecondProperty(), 100);
}

TEST(TimeOnlyTests, ToTimeSpan_Midnight_ZeroSpan) {
    TimeOnly t;
    TimeSpan ts = t.ToTimeSpan();
    EXPECT_EQ(ts.getHoursProperty(),   0);
    EXPECT_EQ(ts.getMinutesProperty(), 0);
    EXPECT_EQ(ts.getSecondsProperty(), 0);
}

// ---------------------------------------------------------------------------
// operator-
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, OperatorMinus_SameTime_ZeroSpan) {
    TimeOnly t(12, 0);
    TimeSpan diff = t - t;
    EXPECT_EQ(diff.getHoursProperty(),   0);
    EXPECT_EQ(diff.getMinutesProperty(), 0);
}

TEST(TimeOnlyTests, OperatorMinus_OneHourApart) {
    TimeOnly t1(13, 0), t2(12, 0);
    TimeSpan diff = t1 - t2;
    EXPECT_DOUBLE_EQ(diff.getTotalHoursProperty(), 1.0);
}

TEST(TimeOnlyTests, OperatorMinus_WrapsAroundMidnight_StaysNonNegative) {
    // .NET "circular clock" semantics: t1 earlier than t2 wraps to a
    // positive elapsed time through midnight, never negative.
    TimeOnly t1(1, 0), t2(23, 0);
    TimeSpan diff = t1 - t2;
    EXPECT_DOUBLE_EQ(diff.getTotalHoursProperty(), 2.0);
}

// ---------------------------------------------------------------------------
// Comparison operators
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, Operators_EqualTimes) {
    TimeOnly a(10, 0), b(10, 0);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a <  b);
    EXPECT_TRUE(a <= b);
    EXPECT_FALSE(a >  b);
    EXPECT_TRUE(a >= b);
}

TEST(TimeOnlyTests, Operators_EarlierLater) {
    TimeOnly a(9, 0), b(10, 0);
    EXPECT_TRUE(a  < b);
    EXPECT_TRUE(a <= b);
    EXPECT_FALSE(a >  b);
    EXPECT_TRUE(b  > a);
}

// ---------------------------------------------------------------------------
// ToString / ToShortTimeString / ToLongTimeString
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, ToString_PadsZeros) {
    TimeOnly t(9, 5, 3);
    EXPECT_EQ(t.ToString(), "09:05:03");
}

TEST(TimeOnlyTests, ToString_Noon) {
    EXPECT_EQ(TimeOnly(12, 0, 0).ToString(), "12:00:00");
}

TEST(TimeOnlyTests, ToShortTimeString_SameAsToString) {
    TimeOnly t(14, 30, 0);
    EXPECT_EQ(t.ToShortTimeString(), t.ToString());
}

TEST(TimeOnlyTests, ToLongTimeString_IncludesMs) {
    TimeOnly t(9, 5, 3, 7);
    std::string s = t.ToLongTimeString();
    EXPECT_NE(s.find("007"), std::string::npos);
    EXPECT_NE(s.find("09:05:03"), std::string::npos);
}

TEST(TimeOnlyTests, ToString_Format_HH_mm_ss) {
    TimeOnly t(9, 5, 3);
    EXPECT_EQ(t.ToString("HH:mm:ss"), "09:05:03");
}

TEST(TimeOnlyTests, ToString_Format_12Hour) {
    TimeOnly t(13, 30, 0);
    std::string s = t.ToString("hh:mm");
    EXPECT_EQ(s, "01:30");
}

TEST(TimeOnlyTests, ToString_Format_Milliseconds) {
    TimeOnly t(0, 0, 0, 123);
    EXPECT_EQ(t.ToString("fff"), "123");
}

// ---------------------------------------------------------------------------
// Parse / TryParse
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, Parse_HHMMSS) {
    TimeOnly t = TimeOnly::Parse("14:30:45");
    EXPECT_EQ(t.getHourProperty(),   14);
    EXPECT_EQ(t.getMinuteProperty(), 30);
    EXPECT_EQ(t.getSecondProperty(), 45);
}

TEST(TimeOnlyTests, Parse_WithMs) {
    TimeOnly t = TimeOnly::Parse("09:05:03.123");
    EXPECT_EQ(t.getMillisecondProperty(), 123);
}

TEST(TimeOnlyTests, Parse_InvalidFormat_Throws) {
    EXPECT_THROW(TimeOnly::Parse("not-a-time"), System::FormatException);
}

TEST(TimeOnlyTests, TryParse_ValidString_True) {
    TimeOnly t;
    EXPECT_TRUE(TimeOnly::TryParse("10:20:30", t));
    EXPECT_EQ(t.getHourProperty(), 10);
}

TEST(TimeOnlyTests, TryParse_InvalidString_False) {
    TimeOnly t;
    EXPECT_FALSE(TimeOnly::TryParse("garbage", t));
}

TEST(TimeOnlyTests, TryParse_WithMs_True) {
    TimeOnly t;
    EXPECT_TRUE(TimeOnly::TryParse("01:02:03.456", t));
    EXPECT_EQ(t.getMillisecondProperty(), 456);
}

// ---------------------------------------------------------------------------
// CCF-002 (ticket #1877). TimeOnly is the family's counter-example: its
// constructors ALREADY carry exactly the rule DateTime was missing, with .NET's
// verbatim message and paramName. These cases pin that, because DateTime's
// repair now copies these strings and a silent drift would make the two types
// disagree about the same error.
//
// Its parser's grammar defects (SR-AUD-009) are deliberately NOT repaired here;
// the currently-accepted spellings are pinned as *current* behaviour so ticket
// #1879 cannot change them without a failing test making it visible.
// ---------------------------------------------------------------------------

TEST(TimeOnlyTests, Ccf002_ConstructorExceptionIdentityIsTheReferenceText) {
    try {
        (void)TimeOnly(24, 0, 0);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "");
        EXPECT_EQ(e.getMessageProperty(),
                  "Hour, Minute, and Second parameters describe an un-representable DateTime.");
    }

    try {
        (void)TimeOnly(0, 0, 0, 1000);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "millisecond");
        EXPECT_EQ(e.getMessageProperty(),
                  "Valid values are between 0 and 999, inclusive. (Parameter 'millisecond')");
    }

    EXPECT_THROW(TimeOnly(0, 60, 0),  ArgumentOutOfRangeException);
    EXPECT_THROW(TimeOnly(0, 0, 60),  ArgumentOutOfRangeException);
    EXPECT_THROW(TimeOnly(-1, 0, 0),  ArgumentOutOfRangeException);
    EXPECT_THROW(TimeOnly(0, -1, 0),  ArgumentOutOfRangeException);
    EXPECT_THROW(TimeOnly(0, 0, -1),  ArgumentOutOfRangeException);
    EXPECT_THROW(TimeOnly(0, 0, 0, -1), ArgumentOutOfRangeException);
}

TEST(TimeOnlyTests, Ccf002_ParserAlreadyRejectsOutOfRangeComponents) {
    TimeOnly t(7, 7, 7, 7);
    EXPECT_FALSE(TimeOnly::TryParse("24:00:00", t));
    EXPECT_FALSE(TimeOnly::TryParse("10:60:00", t));
    EXPECT_FALSE(TimeOnly::TryParse("10:20:60", t));
    EXPECT_FALSE(TimeOnly::TryParse("-1:00:00", t));
    EXPECT_FALSE(TimeOnly::TryParse("10:20", t));
    EXPECT_FALSE(TimeOnly::TryParse("", t));
    EXPECT_FALSE(TimeOnly::TryParse("99999999999999:00:00", t));
}

TEST(TimeOnlyTests, Ccf002_FractionalScalingIsCorrectAndPinned) {
    TimeOnly t;
    ASSERT_TRUE(TimeOnly::TryParse("10:20:30.1", t));
    EXPECT_EQ(t.getMillisecondProperty(), 100);
    ASSERT_TRUE(TimeOnly::TryParse("10:20:30.12", t));
    EXPECT_EQ(t.getMillisecondProperty(), 120);
    ASSERT_TRUE(TimeOnly::TryParse("10:20:30.123", t));
    EXPECT_EQ(t.getMillisecondProperty(), 123);
}

// FLIPPED by #1879 (approved 2026-07-31), which is exactly what this test said
// would happen to it.
TEST(TimeOnlyTests, Ccf002_MalformedGrammarIsNowRejected) {
    TimeOnly t;
    EXPECT_FALSE(TimeOnly::TryParse("10:20:30.abc", t));
    EXPECT_FALSE(TimeOnly::TryParse("10:20:30junk", t));
    EXPECT_FALSE(TimeOnly::TryParse("10:20:30.", t));
    EXPECT_FALSE(TimeOnly::TryParse("10:20:30.12345678", t));
    EXPECT_THROW(TimeOnly::Parse("10:20:30junk"), System::FormatException);
}

TEST(TimeOnlyTests, Ccf002_WellFormedTimesKeepTheirExactValues) {
    TimeOnly t;
    ASSERT_TRUE(TimeOnly::TryParse("10:20:30", t));
    EXPECT_EQ(t.getHourProperty(), 10);
    EXPECT_EQ(t.getMinuteProperty(), 20);
    EXPECT_EQ(t.getSecondProperty(), 30);
    EXPECT_EQ(t.getMillisecondProperty(), 0);
    ASSERT_TRUE(TimeOnly::TryParse("10:20:30.5", t));
    EXPECT_EQ(t.getMillisecondProperty(), 500);
    ASSERT_TRUE(TimeOnly::TryParse("10:20:30.12", t));
    EXPECT_EQ(t.getMillisecondProperty(), 120);
    ASSERT_TRUE(TimeOnly::TryParse("10:20:30.123", t));
    EXPECT_EQ(t.getMillisecondProperty(), 123);
    ASSERT_TRUE(TimeOnly::TryParse("00:00:00", t));
    EXPECT_EQ(t.getHourProperty(), 0);
    ASSERT_TRUE(TimeOnly::TryParse("23:59:59", t));
    EXPECT_EQ(t.getSecondProperty(), 59);
}

// The unpadded "H:m:s" form is DELIBERATELY still accepted. It parses today, and
// .NET's TimeOnly.Parse accepts it too, so requiring zero padding here would
// diverge from .NET rather than converge on it. Contrast the DateTimeOffset
// offset fields, which #1879 does require to be two digits.
TEST(TimeOnlyTests, Ccf002_UnpaddedFieldsAreStillAccepted) {
    TimeOnly t;
    ASSERT_TRUE(TimeOnly::TryParse("1:2:3", t));
    EXPECT_EQ(t.getHourProperty(), 1);
    EXPECT_EQ(t.getMinuteProperty(), 2);
    EXPECT_EQ(t.getSecondProperty(), 3);
}

// std::sscanf's "%d" skipped leading whitespace and accepted an explicit sign,
// so these parsed before #1879 replaced it with a digits-only scanner. None of
// them is part of any documented grammar.
TEST(TimeOnlyTests, Ccf002_SscanfLeniencyShapesAreGone) {
    TimeOnly t;
    EXPECT_TRUE(TimeOnly::TryParse(" 10:20:30", t));
    EXPECT_FALSE(TimeOnly::TryParse("+10:20:30", t));
    EXPECT_FALSE(TimeOnly::TryParse("10:20:30.-1", t));
    EXPECT_FALSE(TimeOnly::TryParse("10:20:30Z", t));
    EXPECT_TRUE(TimeOnly::TryParse("10:20:30 ", t));
    EXPECT_FALSE(TimeOnly::TryParse("99999999999999:20:30", t));
}

// #1929 rows 5-6 (approved 2026-08-01). TimeOnly's existing four-int object
// representation now retains ticks-within-the-second rather than truncating to
// milliseconds, preserving size/alignment while making all related doors agree.
TEST(TimeOnlyTests, Approved1929_OneThroughSevenFractionDigitsRetainExactTicks) {
    const long long wholeSecond = TimeOnly(10, 20, 30).getTicksProperty();
    const char* fractions[] = {"1", "12", "123", "1234", "12345", "123456", "1234567"};
    const long long expected[] = {1000000, 1200000, 1230000, 1234000, 1234500, 1234560, 1234567};
    for (int i = 0; i < 7; ++i) {
        const std::string text = std::string("10:20:30.") + fractions[i];
        TimeOnly out;
        ASSERT_TRUE(TimeOnly::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), wholeSecond + expected[i]) << text;
        EXPECT_EQ(TimeOnly::Parse(text).getTicksProperty(), out.getTicksProperty()) << text;
    }

    TimeOnly out(7, 7, 7, 7);
    const char* rejected[] = {
        "10:20:30.", "10:20:30.abc", "10:20:30.12345678",
        "10:20:30..1", "10:20:30,1", "10:20:30.1x"
    };
    for (const char* text : rejected) EXPECT_FALSE(TimeOnly::TryParse(text, out)) << text;
}

TEST(TimeOnlyTests, Approved1929_TickPrecisionSurvivesTheStructuralSurface) {
    const TimeOnly value(36'000'000'000LL + 2'000'000LL + 345LL);
    EXPECT_EQ(value.getTicksProperty(), 36'002'000'345LL);
    EXPECT_EQ(value.getMillisecondProperty(), 200);
    EXPECT_EQ(value.getMicrosecondProperty(), 34);
    EXPECT_EQ(value.getNanosecondProperty(), 500);
    EXPECT_EQ(value.ToString("HH:mm:ss.fffffff"), "01:00:00.2000345");
    EXPECT_EQ(value.ToString("ffffffff"), "200"); // preserve unsupported >7-run behavior
    EXPECT_EQ(TimeOnly::Parse(value.ToString("HH:mm:ss.fffffff")).getTicksProperty(),
              value.getTicksProperty());
    EXPECT_EQ(value.ToTimeSpan().getTicksProperty(), value.getTicksProperty());
    EXPECT_EQ(TimeOnly::FromTimeSpan(value.ToTimeSpan()).getTicksProperty(), value.getTicksProperty());

    const System::DateTime dt = System::DateTime(2024, 6, 15).AddTicks(value.getTicksProperty());
    EXPECT_EQ(TimeOnly::FromDateTime(dt).getTicksProperty(), value.getTicksProperty());
    EXPECT_EQ(System::DateOnly(2024, 6, 15).ToDateTime(value).getTicksProperty(),
              dt.getTicksProperty());

    const TimeOnly oneTick(1);
    EXPECT_NE(oneTick, TimeOnly(0));
    EXPECT_LT(TimeOnly(0), oneTick);
    EXPECT_EQ((oneTick - TimeOnly(0)).getTicksProperty(), 1);
    EXPECT_TRUE(oneTick.IsBetween(TimeOnly(0), TimeOnly(2)));
    EXPECT_EQ(TimeOnly::getMaxValueProperty().getTicksProperty(), 864'000'000'000LL - 1);
}

TEST(TimeOnlyTests, Approved1929_OuterWhitespaceOnlyAndExactFailureIdentity) {
    const char* accepted[] = {
        " 1:2:3.1234567 ", "\t1:2:3.1234567\r\n", "\f\v1:2:3.1234567\v"
    };
    for (const char* text : accepted) {
        TimeOnly out;
        ASSERT_TRUE(TimeOnly::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), TimeOnly::Parse(text).getTicksProperty()) << text;
    }

    TimeOnly out;
    const char* rejected[] = {
        "1 :2:3", "1: 2:3", "1:2: 3", "1:2:3 .1", " \t\r\n ",
        "+1:2:3", "1:-2:3", "1:2:+3", "1:2:3Z", "1:2:3 garbage"
    };
    for (const char* text : rejected) EXPECT_FALSE(TimeOnly::TryParse(text, out)) << text;
    try {
        (void)TimeOnly::Parse("1:2:3 garbage");
        FAIL();
    } catch (const System::FormatException& e) {
        EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u));
        EXPECT_EQ(std::string(e.what()),
                  "String was not recognized as a valid TimeOnly: 1:2:3 garbage");
    }
}

TEST(TimeOnlyTests, Ticket1880_TryParseFailureAlwaysAssignsMinValue) {
    TimeOnly out(7, 7, 7, 7);
    const auto expectFailure = [&out](const char* text) {
        out = TimeOnly(7, 7, 7, 7);
        EXPECT_FALSE(TimeOnly::TryParse(text, out)) << text;
        EXPECT_EQ(out.getTicksProperty(), TimeOnly::getMinValueProperty().getTicksProperty())
            << text;
    };

    expectFailure("");                         // initial scan / empty
    expectFailure(" \t\r\n ");               // whitespace-only
    expectFailure("not-a-time");               // malformed
    expectFailure("10:20");                    // incomplete fields
    expectFailure("10:20:30.");                // malformed fraction
    expectFailure("10:20:30junk");             // trailing content
    expectFailure("24:00:00");                 // hour boundary
    expectFailure("10:60:00");                 // minute boundary
    expectFailure("10:20:60");                 // second boundary
    expectFailure("10:20:30.12345678");        // precision boundary

    ASSERT_TRUE(TimeOnly::TryParse("0:0:0", out));
    EXPECT_EQ(out.getTicksProperty(), TimeOnly::getMinValueProperty().getTicksProperty());
    ASSERT_TRUE(TimeOnly::TryParse("23:59:59.9999999", out));
    EXPECT_EQ(out.getTicksProperty(), TimeOnly::getMaxValueProperty().getTicksProperty());
    ASSERT_TRUE(TimeOnly::TryParse("10:20:30.1234567", out));
    EXPECT_NE(out.getTicksProperty(), TimeOnly::getMinValueProperty().getTicksProperty());

    try {
        (void)TimeOnly::Parse("not-a-time");
        FAIL();
    } catch (const System::FormatException& e) {
        EXPECT_EQ(e.getHResultProperty(), static_cast<int>(0x80131537u));
        EXPECT_EQ(std::string(e.what()),
                  "String was not recognized as a valid TimeOnly: not-a-time");
    }
}
