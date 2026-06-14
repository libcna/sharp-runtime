// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Task 40: Span, Half, Int128, UInt128, DateTimeOffset, TimeOnly, DBNull,
// FormattableString, OperatingSystem, BFloat16, DivisionRounding, StringComparer,
// Progress, UnicodeRange/Ranges, CancellationTokenRegistration,
// KeyNotFoundException, ReferenceEqualityComparer, ReadonlyProperty.
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include <vector>

#include "System/Span.hpp"
#include "System/Half.hpp"
#include "System/Int128.hpp"
#include "System/UInt128.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/TimeOnly.hpp"
#include "System/DBNull.hpp"
#include "System/FormattableString.hpp"
#include "System/OperatingSystem.hpp"
#include "System/Numerics/BFloat16.hpp"
#include "System/Numerics/DivisionRounding.hpp"
#include "System/StringComparer.hpp"
#include "System/Progress.hpp"
#include "System/Text/Unicode/UnicodeRange.hpp"
#include "System/Text/Unicode/UnicodeRanges.hpp"
#include "System/Threading/CancellationTokenRegistration.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "System/Collections/Generic/ReferenceEqualityComparer.hpp"
#include "SharpRuntime/Experimental/ReadonlyProperty.hpp"

// ===========================================================================
// Span<T>
// ===========================================================================

using System::Span;
using System::ReadOnlySpan;

TEST(SpanTests, DefaultCtor_IsEmpty) {
    Span<int> s;
    EXPECT_EQ(s.getLengthProperty(), 0);
    EXPECT_TRUE(s.getIsEmptyProperty());
}

TEST(SpanTests, PtrLen_Ctor_LengthCorrect) {
    int arr[] = {1, 2, 3};
    Span<int> s(arr, 3);
    EXPECT_EQ(s.getLengthProperty(), 3);
    EXPECT_FALSE(s.getIsEmptyProperty());
}

TEST(SpanTests, IndexOperator_ReadsAndWrites) {
    int arr[] = {10, 20, 30};
    Span<int> s(arr, 3);
    EXPECT_EQ(s[0], 10);
    s[1] = 99;
    EXPECT_EQ(arr[1], 99);
}

TEST(SpanTests, IndexOperator_OutOfRange_Throws) {
    int arr[] = {1};
    Span<int> s(arr, 1);
    EXPECT_THROW(s[1], std::out_of_range);
}

TEST(SpanTests, Slice_SubRange) {
    int arr[] = {1, 2, 3, 4, 5};
    Span<int> s(arr, 5);
    auto sl = s.Slice(1, 3);
    EXPECT_EQ(sl.getLengthProperty(), 3);
    EXPECT_EQ(sl[0], 2);
    EXPECT_EQ(sl[2], 4);
}

TEST(SpanTests, FromVector_Ctor) {
    std::vector<int> v = {5, 6, 7};
    Span<int> s(v);
    EXPECT_EQ(s.getLengthProperty(), 3);
    EXPECT_EQ(s[2], 7);
}

TEST(SpanTests, RangeFor_IteratesAll) {
    int arr[] = {1, 2, 3};
    Span<int> s(arr, 3);
    int sum = 0;
    for (int x : s) sum += x;
    EXPECT_EQ(sum, 6);
}

TEST(ReadOnlySpanTests, Ctor_LengthAndAccess) {
    const int arr[] = {7, 8, 9};
    ReadOnlySpan<int> rs(arr, 3);
    EXPECT_EQ(rs.getLengthProperty(), 3);
    EXPECT_EQ(rs[0], 7);
}

TEST(ReadOnlySpanTests, Slice_Works) {
    const int arr[] = {10, 20, 30, 40};
    ReadOnlySpan<int> rs(arr, 4);
    auto sl = rs.Slice(2, 2);
    EXPECT_EQ(sl[0], 30);
    EXPECT_EQ(sl[1], 40);
}

// ===========================================================================
// Half
// ===========================================================================

TEST(HalfTests, FromSingle_Zero_IsZero) {
    System::Half h = System::Half::FromSingle(0.0f);
    EXPECT_NEAR(h.ToSingle(), 0.0f, 1e-6f);
}

TEST(HalfTests, FromSingle_One_RoundTrip) {
    System::Half h = System::Half::FromSingle(1.0f);
    EXPECT_NEAR(h.ToSingle(), 1.0f, 1e-3f);
}

TEST(HalfTests, StaticZero_IsZero) {
    EXPECT_NEAR(System::Half::Zero.ToSingle(), 0.0f, 1e-6f);
}

TEST(HalfTests, StaticPositiveInfinity_IsInf) {
    EXPECT_TRUE(std::isinf(System::Half::PositiveInfinity.ToSingle()));
    EXPECT_GT(System::Half::PositiveInfinity.ToSingle(), 0.0f);
}

TEST(HalfTests, StaticNegativeInfinity_IsNegInf) {
    EXPECT_TRUE(std::isinf(System::Half::NegativeInfinity.ToSingle()));
    EXPECT_LT(System::Half::NegativeInfinity.ToSingle(), 0.0f);
}

TEST(HalfTests, StaticNaN_IsNaN) {
    EXPECT_TRUE(std::isnan(System::Half::NaN.ToSingle()));
}

TEST(HalfTests, Comparison_LessThan) {
    auto h1 = System::Half::FromSingle(1.0f);
    auto h2 = System::Half::FromSingle(2.0f);
    EXPECT_TRUE(h1 < h2);
    EXPECT_FALSE(h2 < h1);
}

TEST(HalfTests, Equality) {
    auto h1 = System::Half::FromSingle(3.0f);
    auto h2 = System::Half::FromSingle(3.0f);
    EXPECT_EQ(h1, h2);
}

TEST(HalfTests, ExplicitConversion_ToFloat) {
    auto h = System::Half::FromSingle(5.0f);
    float f = static_cast<float>(h);
    EXPECT_NEAR(f, 5.0f, 1e-2f);
}

// ===========================================================================
// Int128
// ===========================================================================

TEST(Int128Tests, DefaultCtor_IsZero) {
    System::Int128 v;
    EXPECT_EQ(v.getLowerProperty(), uint64_t(0));
    EXPECT_EQ(v.getUpperProperty(), int64_t(0));
}

TEST(Int128Tests, Addition) {
    System::Int128 a(1), b(2);
    System::Int128 c = a + b;
    EXPECT_EQ(static_cast<long long>(c), 3LL);
}

TEST(Int128Tests, Subtraction) {
    System::Int128 a(10), b(3);
    EXPECT_EQ(static_cast<long long>(a - b), 7LL);
}

TEST(Int128Tests, Multiplication) {
    System::Int128 a(6), b(7);
    EXPECT_EQ(static_cast<long long>(a * b), 42LL);
}

TEST(Int128Tests, Negation) {
    System::Int128 a(5);
    EXPECT_EQ(static_cast<long long>(-a), -5LL);
}

TEST(Int128Tests, ToString_Zero) {
    EXPECT_EQ(System::Int128().ToString(), "0");
}

TEST(Int128Tests, ToString_Positive) {
    System::Int128 v(123);
    EXPECT_EQ(v.ToString(), "123");
}

TEST(Int128Tests, Comparison) {
    System::Int128 a(1), b(2);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
    EXPECT_TRUE(a != b);
}

TEST(Int128Tests, StaticZero_One) {
    EXPECT_EQ(static_cast<long long>(System::Int128::Zero()), 0LL);
    EXPECT_EQ(static_cast<long long>(System::Int128::One()), 1LL);
}

// ===========================================================================
// UInt128
// ===========================================================================

TEST(UInt128Tests, DefaultCtor_IsZero) {
    System::UInt128 v;
    EXPECT_EQ(v.getLowerProperty(), uint64_t(0));
    EXPECT_EQ(v.getUpperProperty(), uint64_t(0));
}

TEST(UInt128Tests, Addition) {
    System::UInt128 a(1), b(2);
    EXPECT_EQ(static_cast<unsigned long long>(a + b), 3ULL);
}

TEST(UInt128Tests, Multiplication) {
    System::UInt128 a(9), b(9);
    EXPECT_EQ(static_cast<unsigned long long>(a * b), 81ULL);
}

TEST(UInt128Tests, ToString_Zero) {
    EXPECT_EQ(System::UInt128().ToString(), "0");
}

TEST(UInt128Tests, ToString_Value) {
    System::UInt128 v(999);
    EXPECT_EQ(v.ToString(), "999");
}

TEST(UInt128Tests, Comparison) {
    System::UInt128 a(10), b(20);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
    EXPECT_EQ(a, a);
}

TEST(UInt128Tests, StaticZero_One) {
    EXPECT_EQ(static_cast<unsigned long long>(System::UInt128::Zero()), 0ULL);
    EXPECT_EQ(static_cast<unsigned long long>(System::UInt128::One()), 1ULL);
}

// ===========================================================================
// DateTimeOffset
// ===========================================================================

using System::DateTimeOffset;
using System::DateTime;
using System::TimeSpan;

TEST(DateTimeOffsetTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(DateTimeOffset dto);
}

TEST(DateTimeOffsetTests, CtorWithDateTime_StoresDateTime) {
    DateTime dt;
    DateTimeOffset dto(dt, TimeSpan());
    EXPECT_EQ(dto.getDateTimeProperty(), dt);
}

TEST(DateTimeOffsetTests, CtorWithOffset_StoresOffset) {
    DateTime dt;
    TimeSpan offset = TimeSpan::FromHours(2);
    DateTimeOffset dto(dt, offset);
    EXPECT_NEAR(dto.getOffsetProperty().getTotalHoursProperty(), 2.0, 1e-9);
}

TEST(DateTimeOffsetTests, Equality_SameValues) {
    DateTime dt;
    DateTimeOffset a(dt, TimeSpan()), b(dt, TimeSpan());
    EXPECT_EQ(a, b);
}

TEST(DateTimeOffsetTests, Inequality_DifferentOffset) {
    DateTime dt;
    DateTimeOffset a(dt, TimeSpan()), b(dt, TimeSpan::FromHours(1));
    EXPECT_NE(a, b);
}

TEST(DateTimeOffsetTests, ToString_NonEmpty) {
    DateTimeOffset dto;
    EXPECT_FALSE(dto.ToString().empty());
}
TEST(DateTimeOffsetTests, UtcNow_UtcTicks_Positive) {
    auto dto = DateTimeOffset::getUtcNowProperty();
    EXPECT_GT(dto.getUtcTicksProperty(), 0LL);
    EXPECT_EQ(dto.getOffsetProperty(), TimeSpan::Zero);
}
TEST(DateTimeOffsetTests, Now_HasOffset) {
    auto dto = DateTimeOffset::getNowProperty();
    EXPECT_GT(dto.getUtcTicksProperty(), 0LL);
}
TEST(DateTimeOffsetTests, ComponentAccessors) {
    DateTime dt(2024, 6, 15, 10, 30, 45, 0);
    TimeSpan off = TimeSpan::FromHours(2);
    DateTimeOffset dto(dt, off);
    EXPECT_EQ(dto.getYearProperty(),  2024);
    EXPECT_EQ(dto.getMonthProperty(), 6);
    EXPECT_EQ(dto.getDayProperty(),   15);
    EXPECT_EQ(dto.getHourProperty(),  10);
    EXPECT_EQ(dto.getMinuteProperty(),30);
    EXPECT_EQ(dto.getSecondProperty(),45);
}
TEST(DateTimeOffsetTests, AddHours_ShiftsTime) {
    DateTime dt(2024, 1, 1, 0, 0, 0, 0);
    DateTimeOffset dto(dt, TimeSpan::Zero);
    auto dto2 = dto.AddHours(3);
    EXPECT_EQ(dto2.getHourProperty(), 3);
}
TEST(DateTimeOffsetTests, AddDays_ShiftsDate) {
    DateTime dt(2024, 1, 10, 0, 0, 0, 0);
    DateTimeOffset dto(dt, TimeSpan::Zero);
    auto dto2 = dto.AddDays(5);
    EXPECT_EQ(dto2.getDayProperty(), 15);
}
TEST(DateTimeOffsetTests, Subtract_TwoDates_GivesTimeSpan) {
    DateTime dt1(2024, 1, 1, 0, 0, 0, 0);
    DateTime dt2(2024, 1, 2, 0, 0, 0, 0);
    DateTimeOffset a(dt1, TimeSpan::Zero), b(dt2, TimeSpan::Zero);
    TimeSpan diff = b - a;
    EXPECT_NEAR(diff.getTotalHoursProperty(), 24.0, 0.001);
}
TEST(DateTimeOffsetTests, Operators_Plus_Minus) {
    DateTime dt(2024, 6, 1, 12, 0, 0, 0);
    DateTimeOffset dto(dt, TimeSpan::Zero);
    auto dto2 = dto + TimeSpan::FromHours(1);
    EXPECT_EQ(dto2.getHourProperty(), 13);
    auto dto3 = dto2 - TimeSpan::FromHours(1);
    EXPECT_EQ(dto3.getHourProperty(), 12);
}
TEST(DateTimeOffsetTests, ComparisonOperators) {
    DateTime dt1(2024, 1, 1, 0, 0, 0, 0), dt2(2024, 1, 2, 0, 0, 0, 0);
    DateTimeOffset a(dt1, TimeSpan::Zero), b(dt2, TimeSpan::Zero);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(a <= a);
    EXPECT_TRUE(b >= b);
}
TEST(DateTimeOffsetTests, CompareTo) {
    DateTime dt1(2024, 1, 1, 0, 0, 0, 0), dt2(2024, 1, 2, 0, 0, 0, 0);
    DateTimeOffset a(dt1, TimeSpan::Zero), b(dt2, TimeSpan::Zero);
    EXPECT_LT(a.CompareTo(b), 0);
    EXPECT_GT(b.CompareTo(a), 0);
    EXPECT_EQ(a.CompareTo(a), 0);
}
TEST(DateTimeOffsetTests, Parse_ISO8601_WithOffset) {
    DateTimeOffset dto = DateTimeOffset::Parse("2024-06-15T10:30:00+02:00");
    EXPECT_EQ(dto.getYearProperty(),  2024);
    EXPECT_EQ(dto.getHourProperty(),  10);
    EXPECT_NEAR(dto.getOffsetProperty().getTotalHoursProperty(), 2.0, 0.001);
}
TEST(DateTimeOffsetTests, Parse_ISO8601_Z) {
    DateTimeOffset dto = DateTimeOffset::Parse("2024-06-15T10:30:00Z");
    EXPECT_EQ(dto.getOffsetProperty(), TimeSpan::Zero);
}
TEST(DateTimeOffsetTests, TryParse_Invalid_ReturnsFalse) {
    DateTimeOffset dto;
    EXPECT_FALSE(DateTimeOffset::TryParse("not-a-date", dto));
}
TEST(DateTimeOffsetTests, AddMonths) {
    DateTime dt(2024, 1, 31, 0, 0, 0, 0);
    DateTimeOffset dto(dt, TimeSpan::Zero);
    auto dto2 = dto.AddMonths(1);
    EXPECT_EQ(dto2.getMonthProperty(), 2);
}
TEST(DateTimeOffsetTests, AddYears) {
    DateTime dt(2024, 6, 1, 0, 0, 0, 0);
    DateTimeOffset dto(dt, TimeSpan::Zero);
    auto dto2 = dto.AddYears(2);
    EXPECT_EQ(dto2.getYearProperty(), 2026);
}
TEST(DateTimeOffsetTests, ToUniversalTime_ZeroOffset) {
    DateTime dt(2024, 6, 1, 12, 0, 0, 0);
    TimeSpan off = TimeSpan::FromHours(2);
    DateTimeOffset dto(dt, off);
    auto utc = dto.ToUniversalTime();
    EXPECT_EQ(utc.getOffsetProperty(), TimeSpan::Zero);
    EXPECT_EQ(utc.getHourProperty(), 10); // 12 - 2 = 10
}
TEST(DateTimeOffsetTests, ToString_WithFormat_O) {
    DateTime dt(2024, 6, 15, 10, 30, 0, 0);
    DateTimeOffset dto(dt, TimeSpan::FromHours(2));
    std::string s = dto.ToString(std::string("O"));
    EXPECT_NE(s.find("2024"), std::string::npos);
    EXPECT_NE(s.find("+02:00"), std::string::npos);
}

// ===========================================================================
// TimeOnly
// ===========================================================================

using System::TimeOnly;

TEST(TimeOnlyTests, DefaultCtor_AllZero) {
    TimeOnly t;
    EXPECT_EQ(t.getHourProperty(), 0);
    EXPECT_EQ(t.getMinuteProperty(), 0);
    EXPECT_EQ(t.getSecondProperty(), 0);
    EXPECT_EQ(t.getMillisecondProperty(), 0);
}

TEST(TimeOnlyTests, HourMinute_Ctor) {
    TimeOnly t(13, 45);
    EXPECT_EQ(t.getHourProperty(), 13);
    EXPECT_EQ(t.getMinuteProperty(), 45);
}

TEST(TimeOnlyTests, HourMinuteSecond_Ctor) {
    TimeOnly t(9, 30, 15);
    EXPECT_EQ(t.getSecondProperty(), 15);
}

TEST(TimeOnlyTests, ToString_Format) {
    TimeOnly t(8, 5, 3);
    EXPECT_EQ(t.ToString(), "08:05:03");
}

TEST(TimeOnlyTests, Comparison_Earlier_Less) {
    TimeOnly t1(9, 0), t2(10, 0);
    EXPECT_TRUE(t1 < t2);
    EXPECT_FALSE(t2 < t1);
}

TEST(TimeOnlyTests, Equality) {
    TimeOnly a(12, 30, 0), b(12, 30, 0);
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a != b);
}

// ===========================================================================
// DBNull
// ===========================================================================

TEST(DBNullTests, Value_ReturnsSingleton) {
    auto& a = System::DBNull::Value();
    auto& b = System::DBNull::Value();
    EXPECT_EQ(&a, &b);
}

TEST(DBNullTests, ToString_ReturnsEmptyString) {
    EXPECT_EQ(System::DBNull::Value().ToString(), "");
}

// ===========================================================================
// FormattableString
// ===========================================================================

TEST(FormattableStringTests, Format_StoredCorrectly) {
    System::FormattableString fs("{0} and {1}", {"hello", "world"});
    EXPECT_EQ(fs.getFormatProperty(), "{0} and {1}");
}

TEST(FormattableStringTests, ArgumentCount_Correct) {
    System::FormattableString fs("{0}", {"a"});
    EXPECT_EQ(fs.getArgumentCountProperty(), 1);
}

TEST(FormattableStringTests, GetArgument_ByIndex) {
    System::FormattableString fs("{0} {1}", {"foo", "bar"});
    EXPECT_EQ(fs.GetArgument(0), "foo");
    EXPECT_EQ(fs.GetArgument(1), "bar");
}

TEST(FormattableStringTests, ToString_SubstitutesPlaceholders) {
    System::FormattableString fs("Hello, {0}!", {"World"});
    EXPECT_EQ(fs.ToString(), "Hello, World!");
}

TEST(FormattableStringTests, Invariant_ReturnsToString) {
    System::FormattableString fs("{0}+{1}={2}", {"1", "2", "3"});
    EXPECT_EQ(System::FormattableString::Invariant(fs), "1+2=3");
}

// ===========================================================================
// OperatingSystem
// ===========================================================================

using System::OperatingSystem;
using System::PlatformID;
using System::Version;

TEST(OperatingSystemTests, Platform_StoredCorrectly) {
    OperatingSystem os(PlatformID::Unix, Version(5, 0));
    EXPECT_EQ(os.getPlatformProperty(), PlatformID::Unix);
}

TEST(OperatingSystemTests, Version_StoredCorrectly) {
    OperatingSystem os(PlatformID::Unix, Version(5, 4));
    EXPECT_EQ(os.getVersionProperty().Major, 5);
    EXPECT_EQ(os.getVersionProperty().Minor, 4);
}

TEST(OperatingSystemTests, IsLinux_TrueOnLinux) {
#ifdef __linux__
    EXPECT_TRUE(OperatingSystem::IsLinux());
#else
    EXPECT_FALSE(OperatingSystem::IsLinux());
#endif
}

TEST(OperatingSystemTests, IsWindows_FalseOnLinux) {
#ifdef __linux__
    EXPECT_FALSE(OperatingSystem::IsWindows());
#endif
}

TEST(OperatingSystemTests, IsAndroid_AlwaysFalse) {
    EXPECT_FALSE(OperatingSystem::IsAndroid());
}

TEST(OperatingSystemTests, VersionString_ContainsPlatformName) {
    OperatingSystem os(PlatformID::Unix, Version(1, 0));
    EXPECT_NE(os.getVersionStringProperty().find("Unix"), std::string::npos);
}

// ===========================================================================
// BFloat16
// ===========================================================================

using System::Numerics::BFloat16;

TEST(BFloat16Tests, Zero_IsZeroFloat) {
    EXPECT_NEAR(static_cast<float>(BFloat16::Zero()), 0.0f, 1e-6f);
}

TEST(BFloat16Tests, One_IsOneFloat) {
    EXPECT_NEAR(static_cast<float>(BFloat16::One()), 1.0f, 1e-2f);
}

TEST(BFloat16Tests, NegativeOne_IsNegOne) {
    EXPECT_NEAR(static_cast<float>(BFloat16::NegativeOne()), -1.0f, 1e-2f);
}

TEST(BFloat16Tests, FromFloat_RoundTrip) {
    BFloat16 v(2.0f);
    EXPECT_NEAR(static_cast<float>(v), 2.0f, 0.1f);
}

TEST(BFloat16Tests, Addition) {
    BFloat16 a(1.0f), b(2.0f);
    float result = static_cast<float>(a + b);
    EXPECT_NEAR(result, 3.0f, 0.1f);
}

TEST(BFloat16Tests, IsNaN_NaN) {
    EXPECT_TRUE(BFloat16::IsNaN(BFloat16::NaN()));
}

TEST(BFloat16Tests, IsNaN_NonNaN) {
    EXPECT_FALSE(BFloat16::IsNaN(BFloat16::One()));
}

TEST(BFloat16Tests, IsInfinity_PosInf) {
    EXPECT_TRUE(BFloat16::IsPositiveInfinity(BFloat16::PositiveInfinity()));
}

TEST(BFloat16Tests, IsNegativeInfinity) {
    EXPECT_TRUE(BFloat16::IsNegativeInfinity(BFloat16::NegativeInfinity()));
}

TEST(BFloat16Tests, Comparison_LessThan) {
    EXPECT_TRUE(BFloat16(1.0f) < BFloat16(2.0f));
    EXPECT_FALSE(BFloat16(2.0f) < BFloat16(1.0f));
}

TEST(BFloat16Tests, Negate) {
    BFloat16 v(3.0f);
    EXPECT_NEAR(static_cast<float>(-v), -3.0f, 0.2f);
}

// ===========================================================================
// DivisionRounding
// ===========================================================================

using System::Numerics::DivisionRounding;

TEST(DivisionRoundingTests, Values) {
    EXPECT_EQ(static_cast<int>(DivisionRounding::Truncate), 0);
    EXPECT_EQ(static_cast<int>(DivisionRounding::Floor),    1);
    EXPECT_EQ(static_cast<int>(DivisionRounding::Ceiling),  2);
}

// ===========================================================================
// StringComparer
// ===========================================================================

TEST(StringComparerTests, Ordinal_Compare_Less) {
    auto cmp = System::StringComparer::Ordinal();
    EXPECT_LT(cmp->Compare("abc", "abd"), 0);
}

TEST(StringComparerTests, Ordinal_Compare_Equal) {
    auto cmp = System::StringComparer::Ordinal();
    EXPECT_EQ(cmp->Compare("abc", "abc"), 0);
}

TEST(StringComparerTests, Ordinal_Equals_True) {
    auto cmp = System::StringComparer::Ordinal();
    EXPECT_TRUE(cmp->Equals("hello", "hello"));
}

TEST(StringComparerTests, Ordinal_Equals_False) {
    auto cmp = System::StringComparer::Ordinal();
    EXPECT_FALSE(cmp->Equals("Hello", "hello"));
}

TEST(StringComparerTests, OrdinalIgnoreCase_Equals_CaseInsensitive) {
    auto cmp = System::StringComparer::OrdinalIgnoreCase();
    EXPECT_TRUE(cmp->Equals("ABC", "abc"));
}

TEST(StringComparerTests, OrdinalIgnoreCase_Compare_Equal) {
    auto cmp = System::StringComparer::OrdinalIgnoreCase();
    EXPECT_EQ(cmp->Compare("XYZ", "xyz"), 0);
}

TEST(StringComparerTests, GetHashCode_SameInput_SameHash) {
    auto cmp = System::StringComparer::Ordinal();
    EXPECT_EQ(cmp->GetHashCode("test"), cmp->GetHashCode("test"));
}

TEST(StringComparerTests, InvariantCulture_IsSameAsOrdinal) {
    auto a = System::StringComparer::InvariantCulture();
    EXPECT_TRUE(a->Equals("foo", "foo"));
}

// ===========================================================================
// Progress<T>
// ===========================================================================

TEST(ProgressTests, Report_InvokesHandler) {
    int received = -1;
    System::Progress<int> p([&received](int v) { received = v; });
    p.Report(42);
    EXPECT_EQ(received, 42);
}

TEST(ProgressTests, DefaultCtor_Report_NoThrow) {
    System::Progress<int> p;
    EXPECT_NO_THROW(p.Report(1));
}

TEST(ProgressTests, AddHandler_BothCalled) {
    int count = 0;
    System::Progress<int> p([&count](int) { ++count; });
    p.addProgressChangedHandler([&count](int) { ++count; });
    p.Report(0);
    EXPECT_EQ(count, 2);
}
TEST(ProgressTests, MultipleEventHandlers_AllCalled) {
    int sum = 0;
    System::Progress<int> p;
    p.addProgressChangedHandler([&sum](int v) { sum += v; });
    p.addProgressChangedHandler([&sum](int v) { sum += v * 2; });
    p.Report(3);
    EXPECT_EQ(sum, 9); // 3 + 6
}
TEST(ProgressTests, IProgress_Polymorphic) {
    int received = -1;
    System::Progress<int> p([&received](int v) { received = v; });
    System::IProgress<int>* ip = &p;
    ip->Report(77);
    EXPECT_EQ(received, 77);
}
TEST(ProgressTests, OnReport_Override_Called) {
    struct TrackingProgress : System::Progress<int> {
        int onReportCalls = 0;
    protected:
        void OnReport(const int& value) override {
            ++onReportCalls;
            System::Progress<int>::OnReport(value);
        }
    };
    int received = -1;
    TrackingProgress p;
    p.addProgressChangedHandler([&received](int v) { received = v; });
    p.Report(5);
    EXPECT_EQ(p.onReportCalls, 1);
    EXPECT_EQ(received, 5);
}
TEST(ProgressTests, Report_StringType) {
    std::string last;
    System::Progress<std::string> p([&last](std::string s) { last = std::move(s); });
    p.Report("hello");
    EXPECT_EQ(last, "hello");
}

// ===========================================================================
// UnicodeRange / UnicodeRanges
// ===========================================================================

using System::Text::Unicode::UnicodeRange;
using System::Text::Unicode::UnicodeRanges;

TEST(UnicodeRangeTests, Ctor_StoresValues) {
    UnicodeRange r(0x0041, 26);
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0041);
    EXPECT_EQ(r.getLengthProperty(), 26);
}

TEST(UnicodeRangeTests, Create_AtoZ) {
    auto r = UnicodeRange::Create(u'A', u'Z');
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0041);
    EXPECT_EQ(r.getLengthProperty(), 26);
}

TEST(UnicodeRangeTests, Create_InvalidOrder_Throws) {
    EXPECT_THROW(UnicodeRange::Create(u'Z', u'A'), std::invalid_argument);
}

TEST(UnicodeRangesTests, All_Covers_BMP) {
    auto r = UnicodeRanges::All();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0000);
    EXPECT_EQ(r.getLengthProperty(), 0x10000);
}

TEST(UnicodeRangesTests, BasicLatin_Starts0) {
    auto r = UnicodeRanges::BasicLatin();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0000);
    EXPECT_EQ(r.getLengthProperty(), 128);
}

TEST(UnicodeRangesTests, Cyrillic_StartsAt0x0400) {
    auto r = UnicodeRanges::Cyrillic();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0400);
}

TEST(UnicodeRangesTests, None_LengthZero) {
    auto r = UnicodeRanges::None();
    EXPECT_EQ(r.getLengthProperty(), 0);
}

// ===========================================================================
// CancellationTokenRegistration
// ===========================================================================

using System::Threading::CancellationTokenRegistration;

TEST(CancellationTokenRegistrationTests, DefaultCtor_IsActive) {
    CancellationTokenRegistration reg;
    EXPECT_TRUE(reg.getIsActiveProperty());
}

TEST(CancellationTokenRegistrationTests, Dispose_BecomesInactive) {
    CancellationTokenRegistration reg;
    reg.Dispose();
    EXPECT_FALSE(reg.getIsActiveProperty());
}

TEST(CancellationTokenRegistrationTests, Unregister_BecomesInactive) {
    CancellationTokenRegistration reg;
    reg.Unregister();
    EXPECT_FALSE(reg.getIsActiveProperty());
}

// ===========================================================================
// KeyNotFoundException
// ===========================================================================

using System::Collections::Generic::KeyNotFoundException;

TEST(KeyNotFoundExceptionTests, DefaultCtor_WhatNonEmpty) {
    KeyNotFoundException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}

TEST(KeyNotFoundExceptionTests, MessageCtor_WhatContainsMessage) {
    KeyNotFoundException ex("key 'foo' not found");
    EXPECT_NE(std::string(ex.what()).find("foo"), std::string::npos);
}

TEST(KeyNotFoundExceptionTests, IsA_SystemException) {
    EXPECT_THROW(throw KeyNotFoundException(), System::SystemException);
}

// ===========================================================================
// ReferenceEqualityComparer<T>
// ===========================================================================

using System::Collections::Generic::ReferenceEqualityComparer;

TEST(ReferenceEqualityComparerTests, SamePointer_Equals_True) {
    int x = 5;
    ReferenceEqualityComparer<int> cmp;
    EXPECT_TRUE(cmp.Equals(&x, &x));
}

TEST(ReferenceEqualityComparerTests, DifferentPointer_Equals_False) {
    int a = 1, b = 1;
    ReferenceEqualityComparer<int> cmp;
    EXPECT_FALSE(cmp.Equals(&a, &b));
}

TEST(ReferenceEqualityComparerTests, GetHashCode_SamePtr_SameHash) {
    int x = 7;
    ReferenceEqualityComparer<int> cmp;
    EXPECT_EQ(cmp.GetHashCode(&x), cmp.GetHashCode(&x));
}

TEST(ReferenceEqualityComparerTests, Instance_ReturnsSingleton) {
    auto& a = ReferenceEqualityComparer<int>::Instance();
    auto& b = ReferenceEqualityComparer<int>::Instance();
    EXPECT_EQ(&a, &b);
}

// ===========================================================================
// ReadonlyProperty (SharpRuntime::Experimental)
// ===========================================================================

using SharpRuntime::Experimental::ReadOnlyProperty;

TEST(ReadOnlyPropertyTests, Getter_ReturnsValue) {
    ReadOnlyProperty<int> p([](){ return 42; });
    EXPECT_EQ(p.get(), 42);
}

TEST(ReadOnlyPropertyTests, ImplicitConversion_Works) {
    ReadOnlyProperty<std::string> p([](){ return std::string("hi"); });
    std::string v = p;
    EXPECT_EQ(v, "hi");
}
