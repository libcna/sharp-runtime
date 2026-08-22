// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/InvalidTimeZoneException.hpp"
#include <filesystem>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/TimeZoneInfo.hpp"
#include "System/TimeZoneNotFoundException.hpp"
#include "System/detail/ProcessTimeZoneState.hpp"

using System::TimeZoneInfo;
using System::TimeSpan;
using System::DateTime;
using System::DateTimeKind;
using System::TimeZoneNotFoundException;

// ---------------------------------------------------------------------------
// Utc() static zone
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, Utc_Id_IsUTC) {
    EXPECT_EQ(TimeZoneInfo::Utc().getIdProperty(), "UTC");
}

TEST(TimeZoneInfoTests, Utc_DisplayName_NonEmpty) {
    EXPECT_FALSE(TimeZoneInfo::Utc().getDisplayNameProperty().empty());
}

TEST(TimeZoneInfoTests, Utc_StandardName_NonEmpty) {
    EXPECT_FALSE(TimeZoneInfo::Utc().getStandardNameProperty().empty());
}

TEST(TimeZoneInfoTests, Utc_DaylightName_NonEmpty) {
    EXPECT_FALSE(TimeZoneInfo::Utc().getDaylightNameProperty().empty());
}

TEST(TimeZoneInfoTests, Utc_BaseUtcOffset_IsZero) {
    EXPECT_TRUE(TimeZoneInfo::Utc().getBaseUtcOffsetProperty() == TimeSpan::Zero);
}

TEST(TimeZoneInfoTests, Utc_SupportsDst_IsFalse) {
    EXPECT_FALSE(TimeZoneInfo::Utc().getSupportsDaylightSavingTimeProperty());
}

// ---------------------------------------------------------------------------
// Local() static zone
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, Local_Id_IsLocal) {
    EXPECT_EQ(TimeZoneInfo::Local().getIdProperty(), "Local");
}

TEST(TimeZoneInfoTests, Local_DisplayName_NonEmpty) {
    EXPECT_FALSE(TimeZoneInfo::Local().getDisplayNameProperty().empty());
}

TEST(TimeZoneInfoTests, Local_BaseUtcOffset_InValidRange) {
    double hours = TimeZoneInfo::Local().getBaseUtcOffsetProperty().getTotalHoursProperty();
    EXPECT_GE(hours, -14.0);
    EXPECT_LE(hours,  14.0);
}

// ---------------------------------------------------------------------------
// Instance methods (UTC zone has zero offset and no DST)
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, Utc_IsDaylightSavingTime_ReturnsFalse) {
    DateTime dt;
    EXPECT_FALSE(TimeZoneInfo::Utc().IsDaylightSavingTime(dt));
}

TEST(TimeZoneInfoTests, Utc_GetUtcOffset_IsZero) {
    DateTime dt;
    EXPECT_TRUE(TimeZoneInfo::Utc().GetUtcOffset(dt) == TimeSpan::Zero);
}

TEST(TimeZoneInfoTests, Utc_IsAmbiguousTime_ReturnsFalse) {
    DateTime dt;
    EXPECT_FALSE(TimeZoneInfo::Utc().IsAmbiguousTime(dt));
}

TEST(TimeZoneInfoTests, Utc_IsInvalidTime_ReturnsFalse) {
    DateTime dt;
    EXPECT_FALSE(TimeZoneInfo::Utc().IsInvalidTime(dt));
}

TEST(TimeZoneInfoTests, Utc_ConvertTimeToUtc_ZeroOffsetPreservesRawValue) {
    // UTC zone has zero offset, so the clock value is unchanged, but the result is explicitly UTC.
    DateTime dt;
    DateTime result = TimeZoneInfo::Utc().ConvertTimeToUtc(dt);
    EXPECT_EQ(result.getTicksProperty(), dt.getTicksProperty());
    EXPECT_EQ(result.getKindProperty(), DateTimeKind::Utc);
}

// ---------------------------------------------------------------------------
// FindSystemTimeZoneById
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_UTC_ReturnsNonNull) {
    auto tz = TimeZoneInfo::FindSystemTimeZoneById("UTC");
    EXPECT_NE(tz, nullptr);
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_UTC_HasCorrectId) {
    auto tz = TimeZoneInfo::FindSystemTimeZoneById("UTC");
    EXPECT_EQ(tz->getIdProperty(), "UTC");
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_Unknown_ThrowsInvalidArgument) {
    EXPECT_THROW(TimeZoneInfo::FindSystemTimeZoneById("America/Unknown"),
                 TimeZoneNotFoundException);
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_PathTraversal_Throws) {
    EXPECT_THROW(TimeZoneInfo::FindSystemTimeZoneById("../../etc/passwd"),
                 TimeZoneNotFoundException);
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_EuropePrague_OffsetInRange) {
    auto tz = TimeZoneInfo::FindSystemTimeZoneById("Europe/Prague");
    double hours = tz->getBaseUtcOffsetProperty().getTotalHoursProperty();
    // CET=+1, CEST=+2
    EXPECT_GE(hours, 1.0);
    EXPECT_LE(hours, 2.0);
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_AmericaNewYork_NegativeOffset) {
    auto tz = TimeZoneInfo::FindSystemTimeZoneById("America/New_York");
    double hours = tz->getBaseUtcOffsetProperty().getTotalHoursProperty();
    EXPECT_LT(hours, 0.0);
}

// ---------------------------------------------------------------------------
// GetSystemTimeZones
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, GetSystemTimeZones_ReturnsAtLeastTwoZones) {
    auto zones = TimeZoneInfo::GetSystemTimeZones();
    EXPECT_GE(zones.size(), 2u);
}

TEST(TimeZoneInfoTests, GetSystemTimeZones_AllNonNull) {
    for (const auto& tz : TimeZoneInfo::GetSystemTimeZones()) {
        EXPECT_NE(tz, nullptr);
    }
}

// ---------------------------------------------------------------------------
// CreateCustomTimeZone
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, CreateCustomTimeZone_HasCorrectId) {
    auto tz = TimeZoneInfo::CreateCustomTimeZone("MyZone", TimeSpan::Zero, "My Zone", "My Standard");
    EXPECT_EQ(tz->getIdProperty(), "MyZone");
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_HasCorrectDisplayName) {
    auto tz = TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::Zero, "Display Name", "Standard");
    EXPECT_EQ(tz->getDisplayNameProperty(), "Display Name");
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_HasCorrectStandardName) {
    auto tz = TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::Zero, "D", "StdName");
    EXPECT_EQ(tz->getStandardNameProperty(), "StdName");
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_NoSupportsDst) {
    auto tz = TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::Zero, "D", "S");
    EXPECT_FALSE(tz->getSupportsDaylightSavingTimeProperty());
}

TEST(TimeZoneInfoTests, SupportsDaylightSavingTime_ReflectsWholeYearNotJustNow) {
    // America/New_York observes DST every year; this must be true regardless of
    // which calendar month the test happens to run in.
    auto ny = TimeZoneInfo::FindSystemTimeZoneById("America/New_York");
    EXPECT_TRUE(ny->getSupportsDaylightSavingTimeProperty());
    // America/Phoenix (Arizona) never observes DST, in any month.
    auto phoenix = TimeZoneInfo::FindSystemTimeZoneById("America/Phoenix");
    EXPECT_FALSE(phoenix->getSupportsDaylightSavingTimeProperty());
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_PositiveOffsetStoredCorrectly) {
    TimeSpan offset = TimeSpan::FromHours(2);
    auto tz = TimeZoneInfo::CreateCustomTimeZone("CET", offset, "Central European Time", "CET");
    EXPECT_TRUE(tz->getBaseUtcOffsetProperty() == offset);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_NegativeOffsetStoredCorrectly) {
    TimeSpan offset = TimeSpan::FromHours(-5);
    auto tz = TimeZoneInfo::CreateCustomTimeZone("EST", offset, "Eastern Standard Time", "EST");
    EXPECT_TRUE(tz->getBaseUtcOffsetProperty() == offset);
}

// ---------------------------------------------------------------------------
// ConvertTimeBySystemTimeZoneId
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, ConvertTimeBySystemTimeZoneId_UTC_DoesNotThrow) {
    DateTime dt;
    EXPECT_NO_THROW(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(dt, "UTC"));
}

TEST(TimeZoneInfoTests, ConvertTimeBySystemTimeZoneId_Unknown_Throws) {
    DateTime dt;
    EXPECT_THROW(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(dt, "Mars/Olympus"),
                 TimeZoneNotFoundException);
}

// ---------------------------------------------------------------------------
// ConvertTime / ConvertTimeFromUtc
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, ConvertTime_DestZone_AddsOffset) {
    auto utcPlus2 = TimeZoneInfo::CreateCustomTimeZone("+02", TimeSpan::FromHours(2), "+2", "+2");
    DateTime utc = DateTime::SpecifyKind(DateTime(2025, 1, 1, 12, 0, 0), DateTimeKind::Utc);
    DateTime local = TimeZoneInfo::ConvertTime(utc, *utcPlus2);
    EXPECT_EQ(local.getHourProperty(), 14);
    EXPECT_EQ(local.getKindProperty(), DateTimeKind::Unspecified);
}

TEST(TimeZoneInfoTests, ConvertTime_SrcDst_Correct) {
    auto src = TimeZoneInfo::CreateCustomTimeZone("src", TimeSpan::FromHours(2), "s", "s");
    auto dst = TimeZoneInfo::CreateCustomTimeZone("dst", TimeSpan::FromHours(5), "d", "d");
    DateTime srcTime(2025, 1, 1, 10, 0, 0);
    DateTime dstTime = TimeZoneInfo::ConvertTime(srcTime, *src, *dst);
    EXPECT_EQ(dstTime.getHourProperty(), 13); // +3h difference
}

TEST(TimeZoneInfoTests, ConvertTimeFromUtc_AddsOffset) {
    auto utcPlus3 = TimeZoneInfo::CreateCustomTimeZone("+03", TimeSpan::FromHours(3), "+3", "+3");
    DateTime utc = DateTime::SpecifyKind(DateTime(2025, 6, 1, 9, 0, 0), DateTimeKind::Utc);
    DateTime local = TimeZoneInfo::ConvertTimeFromUtc(utc, *utcPlus3);
    EXPECT_EQ(local.getHourProperty(), 12);
    EXPECT_EQ(local.getKindProperty(), DateTimeKind::Unspecified);
}

// ---------------------------------------------------------------------------
// TryFindSystemTimeZoneById
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, TryFind_Utc_ReturnsTrue) {
    std::shared_ptr<TimeZoneInfo> tz;
    bool ok = TimeZoneInfo::TryFindSystemTimeZoneById("UTC", tz);
    EXPECT_TRUE(ok);
    ASSERT_NE(tz, nullptr);
    EXPECT_EQ(tz->getIdProperty(), "UTC");
}

TEST(TimeZoneInfoTests, TryFind_Unknown_ReturnsFalse) {
    std::shared_ptr<TimeZoneInfo> tz;
    bool ok = TimeZoneInfo::TryFindSystemTimeZoneById("Mars/Olympus", tz);
    EXPECT_FALSE(ok);
}

// ---------------------------------------------------------------------------
// Equals / HasSameRules / operators
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, Equals_SameId_True) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::FromHours(1), "X", "X");
    auto b = TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::FromHours(1), "X", "X");
    EXPECT_TRUE(a->Equals(*b));
}

TEST(TimeZoneInfoTests, Equals_DiffId_False) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("A", TimeSpan::FromHours(1), "A", "A");
    auto b = TimeZoneInfo::CreateCustomTimeZone("B", TimeSpan::FromHours(1), "B", "B");
    EXPECT_FALSE(a->Equals(*b));
}

TEST(TimeZoneInfoTests, HasSameRules_SameOffset_True) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("A", TimeSpan::FromHours(2), "A", "A");
    auto b = TimeZoneInfo::CreateCustomTimeZone("B", TimeSpan::FromHours(2), "B", "B");
    EXPECT_TRUE(a->HasSameRules(*b));
}

TEST(TimeZoneInfoTests, Decl2185_HasSameRulesIsOneDirectionallyPermissiveAndPermanentlySo) {
    // PERMANENT DEVIATION, decided 2026-08-19 (docs/StandingApprovals.md SA-13). .NET's
    // HasSameRules compares the two zones' adjustment-rule arrays element by element; this port
    // compares the base offset and the DST flag, because it stores no rules.
    //
    // Closing it needs tzdata's OWN rule structures -- the TZif POSIX-TZ footer, or the full
    // transition table with its per-era type records -- which is out of scope for this port. It is
    // NOT closable by sampling libc at any granularity: two zones can agree on every sampled
    // instant and still differ in rule.
    //
    // This test asserts the deviation itself so it is a DECLARATION, and it is written to fail the
    // day a TZif reader lands -- which is exactly when the declaration must be withdrawn.
    auto ny = TimeZoneInfo::FindSystemTimeZoneById("America/New_York");
    auto havana = TimeZoneInfo::FindSystemTimeZoneById("America/Havana");
    ASSERT_NE(ny, nullptr);
    ASSERT_NE(havana, nullptr);

    // The premise: these two really do share a base offset and a DST flag. If tzdata ever changed
    // that, this case would stop being about SR-AUD-228 at all, so it is asserted rather than
    // assumed -- the #2351 lesson about tests that encode an environment fact.
    ASSERT_EQ(ny->getBaseUtcOffsetProperty(), havana->getBaseUtcOffsetProperty());
    ASSERT_EQ(ny->getSupportsDaylightSavingTimeProperty(),
              havana->getSupportsDaylightSavingTimeProperty());

    // .NET returns FALSE here -- their transition dates differ. This port returns true.
    EXPECT_TRUE(ny->HasSameRules(*havana))
        << "#2185: declared permanent. If this ever returns false, a TZif reader landed and the "
           "permanent-deviation note on HasSameRules must be withdrawn.";
}

TEST(TimeZoneInfoTests, Decl2185_TheFailureIsOneDirectional) {
    // The half that makes the deviation safe to declare rather than merely tolerate: this method
    // can only be too PERMISSIVE. It never returns false for a pair .NET calls the same, because
    // the two things it does compare -- base offset and DST flag -- are both things .NET's rule
    // comparison also distinguishes. A caller using it as a NECESSARY condition is correct; one
    // using it as a SUFFICIENT condition is not.
    const TimeZoneInfo& utc = TimeZoneInfo::Utc();
    auto ny = TimeZoneInfo::FindSystemTimeZoneById("America/New_York");
    ASSERT_NE(ny, nullptr);

    // Different base offsets => false here, and false in .NET too. No false negative exists.
    EXPECT_NE(utc.getBaseUtcOffsetProperty(), ny->getBaseUtcOffsetProperty());
    EXPECT_FALSE(utc.HasSameRules(*ny));

    // Reflexivity holds unconditionally, which is the one guarantee a permissive comparison keeps.
    EXPECT_TRUE(ny->HasSameRules(*ny));
    EXPECT_TRUE(utc.HasSameRules(utc));
}

TEST(TimeZoneInfoTests, HasSameRules_DiffOffset_False) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("A", TimeSpan::FromHours(1), "A", "A");
    auto b = TimeZoneInfo::CreateCustomTimeZone("B", TimeSpan::FromHours(2), "B", "B");
    EXPECT_FALSE(a->HasSameRules(*b));
}

TEST(TimeZoneInfoTests, OperatorEqual_SameId) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("Z", TimeSpan::Zero, "Z", "Z");
    auto b = TimeZoneInfo::CreateCustomTimeZone("Z", TimeSpan::Zero, "Z", "Z");
    EXPECT_TRUE(*a == *b);
    EXPECT_FALSE(*a != *b);
}

// ---------------------------------------------------------------------------
// New API: HasIanaId, GetHashCode, ToString
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, HasIanaId_WithSlash_True) {
    auto tz = TimeZoneInfo::FindSystemTimeZoneById("Europe/Prague");
    EXPECT_TRUE(tz->getHasIanaIdProperty());
}

TEST(TimeZoneInfoTests, HasIanaId_UTC_False) {
    EXPECT_FALSE(TimeZoneInfo::Utc().getHasIanaIdProperty());
}

TEST(TimeZoneInfoTests, GetHashCode_SameId_SameHash) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("TZ1", TimeSpan::Zero, "X", "X");
    auto b = TimeZoneInfo::CreateCustomTimeZone("TZ1", TimeSpan::Zero, "Y", "Y");
    EXPECT_EQ(a->GetHashCode(), b->GetHashCode());
}

TEST(TimeZoneInfoTests, ToString_ReturnsDisplayName) {
    auto tz = TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::Zero, "My Display", "X");
    EXPECT_EQ(tz->ToString(), "My Display");
}

// ---------------------------------------------------------------------------
// GetAmbiguousTimeOffsets / GetAdjustmentRules
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, GetAmbiguousTimeOffsets_DateTime_ReturnsEmpty) {
    DateTime dt;
    auto offsets = TimeZoneInfo::Utc().GetAmbiguousTimeOffsets(dt);
    EXPECT_TRUE(offsets.empty());
}

TEST(TimeZoneInfoTests, GetAdjustmentRules_ReturnsEmpty) {
    auto rules = TimeZoneInfo::Utc().GetAdjustmentRules();
    EXPECT_TRUE(rules.empty());
}

// ---------------------------------------------------------------------------
// GetSystemTimeZones(bool) overload
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, GetSystemTimeZones_SkipSorting_SameCount) {
    auto a = TimeZoneInfo::GetSystemTimeZones(false);
    auto b = TimeZoneInfo::GetSystemTimeZones(true);
    EXPECT_EQ(a.size(), b.size());
}

// ---------------------------------------------------------------------------
// Static ConvertTimeToUtc(DateTime, TimeZoneInfo)
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, ConvertTimeToUtc_Static_SubtractsOffset) {
    auto tz = TimeZoneInfo::CreateCustomTimeZone("+02", TimeSpan::FromHours(2), "+2", "+2");
    DateTime local(2025, 6, 1, 14, 0, 0);
    DateTime utc = TimeZoneInfo::ConvertTimeToUtc(local, *tz);
    EXPECT_EQ(utc.getHourProperty(), 12);
    EXPECT_EQ(utc.getKindProperty(), DateTimeKind::Utc);
}

// ---------------------------------------------------------------------------
// ClearCachedData — no-op, must not throw
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, ClearCachedData_DoesNotThrow) {
    EXPECT_NO_THROW(TimeZoneInfo::ClearCachedData());
}

// ---------------------------------------------------------------------------
// TryConvertIanaIdToWindowsId / TryConvertWindowsIdToIanaId
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, TryConvertIanaToWindows_KnownId_ReturnsTrue) {
    std::string win;
    bool ok = TimeZoneInfo::TryConvertIanaIdToWindowsId("Europe/Prague", win);
    EXPECT_TRUE(ok);
    EXPECT_FALSE(win.empty());
}

TEST(TimeZoneInfoTests, TryConvertIanaToWindows_KnownId_CorrectMapping) {
    std::string win;
    TimeZoneInfo::TryConvertIanaIdToWindowsId("America/New_York", win);
    EXPECT_EQ(win, "Eastern Standard Time");
}

TEST(TimeZoneInfoTests, TryConvertIanaToWindows_Unknown_ReturnsFalse) {
    std::string win;
    bool ok = TimeZoneInfo::TryConvertIanaIdToWindowsId("Mars/Olympus", win);
    EXPECT_FALSE(ok);
}

TEST(TimeZoneInfoTests, TryConvertWindowsToIana_KnownId_ReturnsTrue) {
    std::string iana;
    bool ok = TimeZoneInfo::TryConvertWindowsIdToIanaId("Eastern Standard Time", iana);
    EXPECT_TRUE(ok);
    EXPECT_FALSE(iana.empty());
}

TEST(TimeZoneInfoTests, TryConvertWindowsToIana_Unknown_ReturnsFalse) {
    std::string iana;
    bool ok = TimeZoneInfo::TryConvertWindowsIdToIanaId("Fake Standard Time", iana);
    EXPECT_FALSE(ok);
}

TEST(TimeZoneInfoTests, TryConvertRoundtrip_IanaToWindowsToIana) {
    std::string win, iana2;
    ASSERT_TRUE(TimeZoneInfo::TryConvertIanaIdToWindowsId("Asia/Tokyo", win));
    ASSERT_TRUE(TimeZoneInfo::TryConvertWindowsIdToIanaId(win, iana2));
    EXPECT_FALSE(iana2.empty());
}

// ---------------------------------------------------------------------------
// ConvertTimeBySystemTimeZoneId (DateTimeOffset variant)
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, ConvertTimeBySystemTimeZoneId_DateTimeOffset_UTC_NoThrow) {
    System::DateTimeOffset dto;
    EXPECT_NO_THROW(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(dto, "UTC"));
}

TEST(TimeZoneInfoTests, ConvertTimeBySystemTimeZoneId_DateTimeOffset_Unknown_Throws) {
    System::DateTimeOffset dto;
    EXPECT_THROW(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(dto, "Mars/Olympus"),
                 TimeZoneNotFoundException);
}

// ---------------------------------------------------------------------------
// TransitionTime::GetHashCode
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, TransitionTime_GetHashCode_FixedRule) {
    System::DateTime tod;
    auto t = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 14);
    // Fixed rules initialise week_ = 1, so hash = month ^ (week << 8) = 3 ^ (1 << 8)
    EXPECT_EQ(t.GetHashCode(), 3 ^ (1 << 8));
}

TEST(TimeZoneInfoTests, TransitionTime_GetHashCode_SameInputSameHash) {
    System::DateTime tod;
    auto a = TimeZoneInfo::TransitionTime::CreateFloatingDateRule(tod, 10, 4, System::DayOfWeek::Sunday);
    auto b = TimeZoneInfo::TransitionTime::CreateFloatingDateRule(tod, 10, 4, System::DayOfWeek::Sunday);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

// ---------------------------------------------------------------------------
// AdjustmentRule::GetHashCode / Equals / operators
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, AdjustmentRule_GetHashCode_Consistent) {
    System::DateTime start(2020, 1, 1), end(2020, 12, 31);
    System::DateTime tod;
    auto ttStart = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 14);
    auto ttEnd = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 10, 7);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, System::TimeSpan::Zero, ttStart, ttEnd);
    EXPECT_EQ(r->GetHashCode(), r->GetHashCode());
}

TEST(TimeZoneInfoTests, AdjustmentRule_Equals_SameRules_True) {
    System::DateTime start(2020, 1, 1), end(2020, 12, 31);
    System::DateTime tod;
    auto ttStart = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 14);
    auto ttEnd = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 10, 7);
    auto a = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, System::TimeSpan::Zero, ttStart, ttEnd);
    auto b = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, System::TimeSpan::Zero, ttStart, ttEnd);
    EXPECT_TRUE(a->Equals(*b));
    EXPECT_TRUE(*a == *b);
    EXPECT_FALSE(*a != *b);
}

TEST(TimeZoneInfoTests, AdjustmentRule_Equals_DiffEnd_False) {
    System::DateTime start(2020, 1, 1);
    System::DateTime end1(2020, 12, 31), end2(2021, 12, 31);
    System::DateTime tod;
    auto ttStart = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 14);
    auto ttEnd = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 10, 7);
    auto a = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end1, System::TimeSpan::Zero, ttStart, ttEnd);
    auto b = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end2, System::TimeSpan::Zero, ttStart, ttEnd);
    EXPECT_FALSE(a->Equals(*b));
    EXPECT_TRUE(*a != *b);
}

// ---------------------------------------------------------------------------
// TransitionTime
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, TransitionTime_FixedDateRule) {
    DateTime tod;
    auto t = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 14);
    EXPECT_TRUE(t.getIsFixedDateRuleProperty());
    EXPECT_EQ(t.getMonthProperty(), 3);
    EXPECT_EQ(t.getDayProperty(), 14);
}

TEST(TimeZoneInfoTests, TransitionTime_FloatingDateRule) {
    DateTime tod;
    auto t = TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 10, 4, System::DayOfWeek::Sunday);
    EXPECT_FALSE(t.getIsFixedDateRuleProperty());
    EXPECT_EQ(t.getMonthProperty(), 10);
    EXPECT_EQ(t.getWeekProperty(), 4);
    EXPECT_EQ(t.getDayOfWeekProperty(), System::DayOfWeek::Sunday);
}

TEST(TimeZoneInfoTests, TransitionTime_EqualityOperators) {
    DateTime tod;
    auto a = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 25);
    auto b = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 25);
    auto c = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 4, 1);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
}

TEST(TimeZoneInfoTests, TransitionTime_Equals_SameFixed_True) {
    DateTime tod;
    auto a = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 6, 15);
    auto b = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 6, 15);
    EXPECT_TRUE(a.Equals(b));
}

TEST(TimeZoneInfoTests, TransitionTime_Equals_DiffDay_False) {
    DateTime tod;
    auto a = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 6, 10);
    auto b = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 6, 20);
    EXPECT_FALSE(a.Equals(b));
}

TEST(TimeZoneInfoTests, TransitionTime_Equals_FixedVsFloating_False) {
    DateTime tod;
    auto fixed    = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 14);
    auto floating = TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 3, 2, System::DayOfWeek::Sunday);
    EXPECT_FALSE(fixed.Equals(floating));
}

TEST(TimeZoneInfoTests, TransitionTime_Equals_FloatingSameFields_True) {
    DateTime tod;
    auto a = TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 10, 5, System::DayOfWeek::Saturday);
    auto b = TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 10, 5, System::DayOfWeek::Saturday);
    EXPECT_TRUE(a.Equals(b));
}

TEST(TimeZoneInfoTests, TransitionTime_CreateFixed_InvalidMonth_Throws) {
    DateTime tod;
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 0, 1),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 13, 1),
                 System::ArgumentOutOfRangeException);
}

TEST(TimeZoneInfoTests, TransitionTime_CreateFixed_InvalidDay_Throws) {
    DateTime tod;
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 1, 0),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 1, 32),
                 System::ArgumentOutOfRangeException);
}

TEST(TimeZoneInfoTests, TransitionTime_CreateFloating_InvalidMonth_Throws) {
    DateTime tod;
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 13, 1, System::DayOfWeek::Sunday), System::ArgumentOutOfRangeException);
}

TEST(TimeZoneInfoTests, TransitionTime_CreateFloating_InvalidWeek_Throws) {
    DateTime tod;
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 3, 6, System::DayOfWeek::Sunday), System::ArgumentOutOfRangeException);
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 3, 0, System::DayOfWeek::Sunday), System::ArgumentOutOfRangeException);
}

// Regression tests for a code-audit finding (ticket 243): CreateFixedDateRule/
// CreateFloatingDateRule previously did not validate timeOfDay at all. Verified against real
// .NET's TimeZoneInfo.TransitionTime.ValidateTransitionTime (TimeZoneInfo.TransitionTime.cs),
// which throws ArgumentException when timeOfDay.Ticks >= TimeSpan.TicksPerDay (i.e. it has a
// date component beyond DateTime.MinValue's implicit date) -- a full DateTime like year
// 2024/January/1 with a time-of-day fell straight through with no validation before this fix.
TEST(TimeZoneInfoTests, TransitionTime_CreateFixed_TimeOfDayWithDateComponent_Throws) {
    DateTime withDate(2024, 1, 1, 2, 0, 0);
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFixedDateRule(withDate, 3, 14),
                 System::ArgumentException);
}

TEST(TimeZoneInfoTests, TransitionTime_CreateFloating_TimeOfDayWithDateComponent_Throws) {
    DateTime withDate(2024, 1, 1, 2, 0, 0);
    EXPECT_THROW(TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        withDate, 10, 4, System::DayOfWeek::Sunday), System::ArgumentException);
}

TEST(TimeZoneInfoTests, TransitionTime_CreateFixed_ValidTimeOfDay_DoesNotThrow) {
    // A pure time-of-day (year/month/day fixed at DateTime.MinValue's 1/1/1) at
    // millisecond granularity must still be accepted.
    DateTime validTod(1, 1, 1, 2, 30, 0);
    EXPECT_NO_THROW(TimeZoneInfo::TransitionTime::CreateFixedDateRule(validTod, 3, 14));
}

TEST(TimeZoneInfoTests, TransitionTime_DayProperty_StoredByFixedRule) {
    DateTime tod;
    auto t = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 7, 4);
    EXPECT_EQ(t.getDayProperty(), 4);
    EXPECT_EQ(t.getMonthProperty(), 7);
    EXPECT_TRUE(t.getIsFixedDateRuleProperty());
}

TEST(TimeZoneInfoTests, TransitionTime_WeekProperty_StoredByFloatingRule) {
    DateTime tod;
    auto t = TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 11, 3, System::DayOfWeek::Thursday);
    EXPECT_EQ(t.getWeekProperty(), 3);
    EXPECT_EQ(t.getDayOfWeekProperty(), System::DayOfWeek::Thursday);
    EXPECT_FALSE(t.getIsFixedDateRuleProperty());
}

TEST(TimeZoneInfoTests, TransitionTime_GetHashCode_FloatingRule) {
    DateTime tod;
    auto t = TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        tod, 10, 4, System::DayOfWeek::Sunday);
    EXPECT_EQ(t.GetHashCode(), 10 ^ (4 << 8));
}

// ---------------------------------------------------------------------------
// Ticket #2177 (SR-AUD-224): a failed TryFindSystemTimeZoneById must clear the
// caller's out parameter. Before this fix the catch-all returned false without
// touching `result`, so a caller reusing one variable across several lookups was
// handed the previous zone by a lookup that had failed. Measured before the fix
// (build-probe/2176_probe1_surface.log): TryFind("Mars/Olympus", out) returned
// false with out still holding "UTC". Real .NET assigns null to the out parameter.
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, TryFind_Failure_ClearsPreviouslyPopulatedOutParameter) {
    std::shared_ptr<TimeZoneInfo> tz = TimeZoneInfo::FindSystemTimeZoneById("UTC");
    ASSERT_NE(tz, nullptr);
    ASSERT_EQ(tz->getIdProperty(), "UTC");
    EXPECT_FALSE(TimeZoneInfo::TryFindSystemTimeZoneById("Mars/Olympus", tz));
    EXPECT_EQ(tz, nullptr);
}

TEST(TimeZoneInfoTests, TryFind_Failure_EmptyId_ClearsOutParameter) {
    std::shared_ptr<TimeZoneInfo> tz = TimeZoneInfo::FindSystemTimeZoneById("UTC");
    ASSERT_NE(tz, nullptr);
    EXPECT_FALSE(TimeZoneInfo::TryFindSystemTimeZoneById("", tz));
    EXPECT_EQ(tz, nullptr);
}

TEST(TimeZoneInfoTests, TryFind_Failure_PathTraversal_ClearsOutParameter) {
    std::shared_ptr<TimeZoneInfo> tz = TimeZoneInfo::FindSystemTimeZoneById("UTC");
    ASSERT_NE(tz, nullptr);
    EXPECT_FALSE(TimeZoneInfo::TryFindSystemTimeZoneById("../../etc/passwd", tz));
    EXPECT_EQ(tz, nullptr);
}

TEST(TimeZoneInfoTests, TryFind_Failure_AfterFailureLeavesNullNotStale) {
    // Two failures in a row must both leave the parameter null, not resurrect anything.
    std::shared_ptr<TimeZoneInfo> tz;
    EXPECT_FALSE(TimeZoneInfo::TryFindSystemTimeZoneById("Mars/Olympus", tz));
    EXPECT_EQ(tz, nullptr);
    EXPECT_FALSE(TimeZoneInfo::TryFindSystemTimeZoneById("Venus/Maxwell", tz));
    EXPECT_EQ(tz, nullptr);
}

TEST(TimeZoneInfoTests, TryFind_Success_OverwritesAPreviousZone) {
    // The success path must still replace whatever was there.
    std::shared_ptr<TimeZoneInfo> tz = TimeZoneInfo::FindSystemTimeZoneById("Europe/Prague");
    ASSERT_NE(tz, nullptr);
    ASSERT_TRUE(TimeZoneInfo::TryFindSystemTimeZoneById("UTC", tz));
    ASSERT_NE(tz, nullptr);
    EXPECT_EQ(tz->getIdProperty(), "UTC");
}

TEST(TimeZoneInfoTests, TryFind_SuccessThenFailure_DoesNotKeepTheSuccess) {
    std::shared_ptr<TimeZoneInfo> tz;
    ASSERT_TRUE(TimeZoneInfo::TryFindSystemTimeZoneById("Europe/Prague", tz));
    ASSERT_NE(tz, nullptr);
    EXPECT_FALSE(TimeZoneInfo::TryFindSystemTimeZoneById("Europe/Nowhere", tz));
    EXPECT_EQ(tz, nullptr);
}

// ---------------------------------------------------------------------------
// Ticket #2178 (SR-AUD-225): CreateCustomTimeZone copied every input without
// validation, so it minted zones .NET cannot represent. Measured before the fix
// (build-probe/2176_probe1_surface.log): id="" accepted; +15h, -15h, a 90-second
// offset and TimeSpan::MinValue all accepted. The audit's current-.NET probe
// records ArgumentException for the empty id and ArgumentOutOfRangeException for
// the out-of-range offset. The whole-minute rule is the same class -- a value the
// managed counterpart cannot represent -- and is listed in #2186 for text-level
// verification against the reference.
//
// The offset bound also closes a reachable arithmetic door: a zone built with
// TimeSpan::MinValue made ConvertTimeToUtc evaluate -baseUtcOffset_ and report
// "Negating the minimum value of a twos complement number is invalid." from a
// public conversion rather than rejecting the argument where it was supplied.
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, CreateCustomTimeZone_EmptyId_ThrowsArgumentException) {
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone("", TimeSpan::Zero, "D", "S"),
                 System::ArgumentException);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_SingleCharacterId_IsStillAccepted) {
    auto tz = TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::Zero, "D", "S");
    ASSERT_NE(tz, nullptr);
    EXPECT_EQ(tz->getIdProperty(), "X");
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_OffsetAbovePlus14Hours_Throws) {
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::FromHours(15), "D", "S"),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone(
                     "X", TimeSpan(TimeZoneInfo::MaxUtcOffsetTicks + TimeSpan::TicksPerMinute),
                     "D", "S"),
                 System::ArgumentOutOfRangeException);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_OffsetBelowMinus14Hours_Throws) {
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::FromHours(-15), "D", "S"),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone(
                     "X", TimeSpan(-TimeZoneInfo::MaxUtcOffsetTicks - TimeSpan::TicksPerMinute),
                     "D", "S"),
                 System::ArgumentOutOfRangeException);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_Exactly14Hours_IsAcceptedInBothDirections) {
    // +/-14 hours is the inclusive bound, matching Etc/GMT-14 which really exists.
    auto plus = TimeZoneInfo::CreateCustomTimeZone("+14", TimeSpan::FromHours(14), "D", "S");
    auto minus = TimeZoneInfo::CreateCustomTimeZone("-14", TimeSpan::FromHours(-14), "D", "S");
    EXPECT_EQ(plus->getBaseUtcOffsetProperty().getTicksProperty(),
              TimeZoneInfo::MaxUtcOffsetTicks);
    EXPECT_EQ(minus->getBaseUtcOffsetProperty().getTicksProperty(),
              -TimeZoneInfo::MaxUtcOffsetTicks);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_SubMinuteOffset_ThrowsArgumentException) {
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::FromSeconds(90), "D", "S"),
                 System::ArgumentException);
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan(1), "D", "S"),
                 System::ArgumentException);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_WholeMinuteOffsets_AreAccepted) {
    // 45-minute and 30-minute zones are real (Asia/Kathmandu, Asia/Kolkata).
    auto k = TimeZoneInfo::CreateCustomTimeZone("+0545", TimeSpan::FromMinutes(345), "D", "S");
    EXPECT_EQ(k->getBaseUtcOffsetProperty().getTicksProperty(), 345 * TimeSpan::TicksPerMinute);
    auto neg = TimeZoneInfo::CreateCustomTimeZone("-0330", TimeSpan::FromMinutes(-210), "D", "S");
    EXPECT_EQ(neg->getBaseUtcOffsetProperty().getTicksProperty(), -210 * TimeSpan::TicksPerMinute);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_TimeSpanExtremes_AreRejectedAtTheFactory) {
    // Before this fix TimeSpan::MinValue produced a zone whose ConvertTimeToUtc reported a
    // two's-complement negation diagnostic. The argument is now rejected where it is supplied.
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::MinValue, "D", "S"),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::MaxValue, "D", "S"),
                 System::ArgumentOutOfRangeException);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_ValidationOrder_IdBeforeOffset) {
    // Both inputs invalid: the id diagnostic wins, so the caller is told about the first
    // argument rather than the second.
    EXPECT_THROW(TimeZoneInfo::CreateCustomTimeZone("", TimeSpan::FromHours(15), "D", "S"),
                 System::ArgumentException);
    bool sawOutOfRange = false;
    try {
        TimeZoneInfo::CreateCustomTimeZone("", TimeSpan::FromHours(15), "D", "S");
    } catch (const System::ArgumentOutOfRangeException&) {
        sawOutOfRange = true;
    } catch (const System::ArgumentException&) {
    }
    EXPECT_FALSE(sawOutOfRange);
}

TEST(TimeZoneInfoTests, CreateCustomTimeZone_RejectionMessages) {
    // The exception types are what the audit's managed probe establishes; the exact .NET
    // resource text is unverifiable in this container and is pinned here so it cannot drift
    // silently (#2186 carries the verification question).
    try {
        TimeZoneInfo::CreateCustomTimeZone("", TimeSpan::Zero, "D", "S");
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find("not a valid time zone ID"), std::string::npos);
    }
    try {
        TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::FromHours(15), "D", "S");
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_NE(std::string(e.what()).find("plus or minus 14.0 hours"), std::string::npos);
    }
    try {
        TimeZoneInfo::CreateCustomTimeZone("X", TimeSpan::FromSeconds(90), "D", "S");
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find("whole minutes"), std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// Ticket #2180 (SR-AUD-227): Equals compared id_ byte-for-byte while GetHashCode
// lowercased it, so two zones could compare unequal and still hash equal.
// Measured before the fix (build-probe/2176_probe1_surface.log):
// Equals("Zone","zone") = 0 with equal hashes. The audit's current-.NET probe
// records that custom "Zone"/"zone" compare true, because .NET compares ids with
// StringComparer.OrdinalIgnoreCase.
//
// The old fold also ran through std::tolower, whose result depends on the process
// LC_CTYPE, so the hash of a non-ASCII id was a function of a global the caller
// may change at any time. Both sides now share one ordinal ASCII fold.
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, Equals_CaseVariantIds_AreEqual) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("Zone", TimeSpan::Zero, "d", "s");
    auto b = TimeZoneInfo::CreateCustomTimeZone("zone", TimeSpan::Zero, "d", "s");
    auto c = TimeZoneInfo::CreateCustomTimeZone("ZONE", TimeSpan::Zero, "d", "s");
    EXPECT_TRUE(a->Equals(*b));
    EXPECT_TRUE(a->Equals(*c));
    EXPECT_TRUE(b->Equals(*c));
    EXPECT_TRUE(*a == *b);
    EXPECT_FALSE(*a != *b);
}

TEST(TimeZoneInfoTests, Equals_AndHash_AgreeForCaseVariantIds) {
    // The contract the old pair broke: equal implies equal hash.
    auto a = TimeZoneInfo::CreateCustomTimeZone("Europe/Prague", TimeSpan::Zero, "d", "s");
    auto b = TimeZoneInfo::CreateCustomTimeZone("europe/PRAGUE", TimeSpan::Zero, "d", "s");
    ASSERT_TRUE(a->Equals(*b));
    EXPECT_EQ(a->GetHashCode(), b->GetHashCode());
}

TEST(TimeZoneInfoTests, Equals_GenuinelyDifferentIds_StillUnequal) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("Zone", TimeSpan::Zero, "d", "s");
    auto b = TimeZoneInfo::CreateCustomTimeZone("Zones", TimeSpan::Zero, "d", "s");
    auto c = TimeZoneInfo::CreateCustomTimeZone("Zon", TimeSpan::Zero, "d", "s");
    EXPECT_FALSE(a->Equals(*b));
    EXPECT_FALSE(a->Equals(*c));
    EXPECT_TRUE(*a != *b);
}

TEST(TimeZoneInfoTests, Equals_IsReflexiveSymmetricAndTransitive) {
    auto a = TimeZoneInfo::CreateCustomTimeZone("Tz", TimeSpan::Zero, "d", "s");
    auto b = TimeZoneInfo::CreateCustomTimeZone("tZ", TimeSpan::Zero, "d", "s");
    auto c = TimeZoneInfo::CreateCustomTimeZone("TZ", TimeSpan::Zero, "d", "s");
    EXPECT_TRUE(a->Equals(*a));
    EXPECT_EQ(a->Equals(*b), b->Equals(*a));
    EXPECT_TRUE(a->Equals(*b) && b->Equals(*c) && a->Equals(*c));
}

TEST(TimeZoneInfoTests, GetHashCode_FoldIsOrdinalNotLocaleDependent) {
    // Non-ASCII bytes must be left exactly as they are, whatever LC_CTYPE says. Two ids
    // that differ only in a high byte must stay distinct, and an id whose only difference
    // is ASCII case must stay equal, under any locale the platform happens to be in.
    auto highA = TimeZoneInfo::CreateCustomTimeZone(std::string("Zone\xC3\xA9"), TimeSpan::Zero,
                                                    "d", "s");
    auto highB = TimeZoneInfo::CreateCustomTimeZone(std::string("zone\xC3\xA9"), TimeSpan::Zero,
                                                    "d", "s");
    auto highC = TimeZoneInfo::CreateCustomTimeZone(std::string("Zone\xC3\x89"), TimeSpan::Zero,
                                                    "d", "s");
    EXPECT_TRUE(highA->Equals(*highB));
    EXPECT_EQ(highA->GetHashCode(), highB->GetHashCode());
    EXPECT_FALSE(highA->Equals(*highC));
}

TEST(TimeZoneInfoTests, Equals_BytesBetweenTheCaseRanges_AreNotFolded) {
    // '[' (0x5B) sits immediately after 'Z' and '@' (0x40) immediately before 'A'; a fold
    // written with an off-by-one range would map them into the letters.
    auto a = TimeZoneInfo::CreateCustomTimeZone("[", TimeSpan::Zero, "d", "s");
    auto b = TimeZoneInfo::CreateCustomTimeZone("{", TimeSpan::Zero, "d", "s");
    auto c = TimeZoneInfo::CreateCustomTimeZone("@", TimeSpan::Zero, "d", "s");
    auto d = TimeZoneInfo::CreateCustomTimeZone("`", TimeSpan::Zero, "d", "s");
    EXPECT_FALSE(a->Equals(*b));
    EXPECT_FALSE(c->Equals(*d));
}

TEST(TimeZoneInfoTests, Equals_SystemZoneComparesCaseInsensitivelyToACustomOne) {
    auto sys = TimeZoneInfo::FindSystemTimeZoneById("Europe/Prague");
    auto custom = TimeZoneInfo::CreateCustomTimeZone("EUROPE/prague", TimeSpan::FromHours(1),
                                                     "d", "s");
    EXPECT_TRUE(sys->Equals(*custom));
    EXPECT_EQ(sys->GetHashCode(), custom->GetHashCode());
}

// ---------------------------------------------------------------------------
// Ticket #2181 (SR-AUD-229): the POSIX lookup recorded tm_gmtoff and tm_zone at
// time(nullptr), so BaseUtcOffset, StandardName and DaylightName all depended on
// the month the object happened to be created in -- contradicting the header's
// own "this is always the standard offset" and the Windows branch, which reads
// Bias (standard) and DaylightBias separately.
//
// Scale, measured before the fix over every installed zone
// (build-probe/2176_probe4_allzones.log): 499 TZif zones, 158 observe DST, and
// 141 reported the wrong BaseUtcOffset on 2026-08-10 alone; the remaining 17 are
// southern-hemisphere zones, which were correct that day and wrong in January.
//
// The values below are properties of the zones themselves, not of the day the
// suite runs, which is the whole point: a test that asserted "whatever the
// current offset is" could not fail.
// ---------------------------------------------------------------------------

namespace {

// A zone the suite needs may be missing on a host with a trimmed tzdata; skip rather
// than fail, the way the pre-existing Prague and Phoenix assertions already assume.
std::shared_ptr<TimeZoneInfo> zoneOrNull(const std::string& id) {
    std::shared_ptr<TimeZoneInfo> tz;
    return TimeZoneInfo::TryFindSystemTimeZoneById(id, tz) ? tz : nullptr;
}

SharpRuntime::longcs offsetMinutes(const TimeZoneInfo& tz) {
    return tz.getBaseUtcOffsetProperty().getTicksProperty() / TimeSpan::TicksPerMinute;
}

/// What .NET would report as this zone's base UTC offset, computed from the SAME tzdata the
/// port reads, through an independent oracle: glibc's own TZ handling.
///
/// TimeZoneInfo.Unix.cs:81-93 walks the zone's transitions up to `DateTime.UtcNow` and keeps
/// the offset of the last one whose type is NOT daylight; the daylight name comes from the
/// last one that is. This reproduces that rule by sampling the trailing year monthly and
/// taking the most recent sample with `tm_isdst == 0`.
///
/// Deriving the expectation rather than writing a literal is what ticket #2351 changed, and
/// it matters for exactly one class of zone: NEGATIVE-DST zones, where the standard offset is
/// the LARGER one. Under tzdata 2018a and later, Europe/Dublin marks IST (+1) as standard and
/// GMT (+0) as daylight, and Africa/Casablanca marks +01 as standard and its Ramadan
/// reversion to +00 as daylight. A literal "Dublin's standard offset is 0" was correct when
/// it was written and became a tzdata-version assertion afterwards.
struct ZoneOracle {
    bool     available = false;
    long     standardOffsetMinutes = 0;
    long     daylightOffsetMinutes = 0;
    bool     observesDaylight = false;
    std::string standardAbbrev;
    std::string daylightAbbrev;
};

ZoneOracle oracleFor(const std::string& id) {
    // The oracle intentionally manipulates the same process-global C-library state as production
    // lookups. Keep save/select/all localtime reads/restore in one critical section so an
    // unrelated DateTime::Now or TimeZone query cannot observe the temporary zone.
    std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
    ZoneOracle out;
    const char* previous = ::getenv("TZ");
    const std::string saved = previous != nullptr ? std::string(previous) : std::string();
    const bool hadTz = previous != nullptr;

    ::setenv("TZ", id.c_str(), 1);
    ::tzset();

    const std::time_t now = std::time(nullptr);
    bool sawStandard = false;
    // Monthly samples over the trailing year, oldest first, so the LAST standard sample seen
    // is the most recent one -- the same "last non-DST transition at or before now" rule.
    for (int monthsBack = 12; monthsBack >= 0; --monthsBack) {
        const std::time_t sample = now - static_cast<std::time_t>(monthsBack) * 30 * 24 * 3600;
        std::tm local{};
        if (::localtime_r(&sample, &local) == nullptr) continue;
        const long offset = local.tm_gmtoff / 60;
        const std::string abbrev = local.tm_zone != nullptr ? std::string(local.tm_zone) : std::string();
        if (local.tm_isdst == 0) {
            out.standardOffsetMinutes = offset;
            out.standardAbbrev = abbrev;
            sawStandard = true;
        } else if (local.tm_isdst > 0) {
            out.daylightOffsetMinutes = offset;
            out.daylightAbbrev = abbrev;
            out.observesDaylight = true;
        }
    }

    if (hadTz) ::setenv("TZ", saved.c_str(), 1); else ::unsetenv("TZ");
    ::tzset();

    out.available = sawStandard;
    return out;
}

} // namespace

TEST(TimeZoneInfoTests, BaseUtcOffset_NewYork_IsStandardMinusFive_NotTheCurrentOffset) {
    auto ny = zoneOrNull("America/New_York");
    if (!ny) GTEST_SKIP() << "America/New_York is not installed";
    EXPECT_EQ(offsetMinutes(*ny), -300);
    EXPECT_EQ(ny->getStandardNameProperty(), "EST");
    EXPECT_EQ(ny->getDaylightNameProperty(), "EDT");
    EXPECT_TRUE(ny->getSupportsDaylightSavingTimeProperty());
}

TEST(TimeZoneInfoTests, BaseUtcOffset_Prague_IsStandardPlusOne) {
    auto tz = zoneOrNull("Europe/Prague");
    if (!tz) GTEST_SKIP() << "Europe/Prague is not installed";
    EXPECT_EQ(offsetMinutes(*tz), 60);
    EXPECT_EQ(tz->getStandardNameProperty(), "CET");
    EXPECT_EQ(tz->getDaylightNameProperty(), "CEST");
}

// Dublin is a NEGATIVE-DST zone in tzdata 2018a and later: IST (+1) is marked standard and
// GMT (+0) is marked daylight, which is the reverse of the intuitive reading. The offsets are
// therefore derived from the installed tzdata rather than written as literals -- see
// oracleFor(). Ticket #2351; before it, this case asserted "standard is 0, daylight is +1",
// which was a tzdata-version assertion in disguise and failed on tzdata 2026b.
//
// The invariant being tested is unchanged and is the one #2181 introduced: BaseUtcOffset is
// the STANDARD offset, whichever of the two that is, and it does not move with the season.
TEST(TimeZoneInfoTests, BaseUtcOffset_Dublin_IsTheStandardOffsetEvenWhenDaylightIsNegative) {
    auto tz = zoneOrNull("Europe/Dublin");
    if (!tz) GTEST_SKIP() << "Europe/Dublin is not installed";
    const ZoneOracle oracle = oracleFor("Europe/Dublin");
    if (!oracle.available) GTEST_SKIP() << "no standard-time sample for Europe/Dublin";

    EXPECT_EQ(offsetMinutes(*tz), oracle.standardOffsetMinutes)
        << "BaseUtcOffset must be the standard offset the installed tzdata declares";
    EXPECT_EQ(tz->getStandardNameProperty(), oracle.standardAbbrev);
    EXPECT_TRUE(tz->getSupportsDaylightSavingTimeProperty());
    if (oracle.observesDaylight) {
        EXPECT_EQ(tz->getDaylightNameProperty(), oracle.daylightAbbrev);
        EXPECT_NE(tz->getStandardNameProperty(), tz->getDaylightNameProperty());
        // The whole point of the zone: the two offsets differ by an hour in one direction or
        // the other, and the port must not silently pick the seasonal one.
        EXPECT_EQ(std::abs(oracle.standardOffsetMinutes - oracle.daylightOffsetMinutes), 60);
        // Season-independent anti-regression assertion, and the one that keeps #2181's defect
        // caught here whatever month the suite runs in: whichever of the two offsets is
        // current today, BaseUtcOffset must not be the DAYLIGHT one.
        EXPECT_NE(offsetMinutes(*tz), oracle.daylightOffsetMinutes);
    }
}

TEST(TimeZoneInfoTests, BaseUtcOffset_SouthernHemisphere_IsTheSameRuleAsNorthern) {
    // Sydney's standard offset is the *smaller* one (+10 AEST, +11 AEDT). A repair that
    // simply took "the offset in January" would get every southern zone backwards.
    auto tz = zoneOrNull("Australia/Sydney");
    if (!tz) GTEST_SKIP() << "Australia/Sydney is not installed";
    EXPECT_EQ(offsetMinutes(*tz), 600);
    EXPECT_EQ(tz->getStandardNameProperty(), "AEST");
    EXPECT_EQ(tz->getDaylightNameProperty(), "AEDT");
}

TEST(TimeZoneInfoTests, BaseUtcOffset_HalfHourDaylightDelta) {
    // Lord Howe shifts by 30 minutes, not 60.
    auto tz = zoneOrNull("Australia/Lord_Howe");
    if (!tz) GTEST_SKIP() << "Australia/Lord_Howe is not installed";
    EXPECT_EQ(offsetMinutes(*tz), 630);
    EXPECT_TRUE(tz->getSupportsDaylightSavingTimeProperty());
    EXPECT_NE(tz->getStandardNameProperty(), tz->getDaylightNameProperty());
}

TEST(TimeZoneInfoTests, BaseUtcOffset_QuarterHourZone) {
    auto tz = zoneOrNull("Pacific/Chatham");
    if (!tz) GTEST_SKIP() << "Pacific/Chatham is not installed";
    EXPECT_EQ(offsetMinutes(*tz), 765);   // +12:45 standard
}

TEST(TimeZoneInfoTests, BaseUtcOffset_NonWholeHourZonesWithoutDaylight) {
    auto kolkata = zoneOrNull("Asia/Kolkata");
    if (kolkata) {
        EXPECT_EQ(offsetMinutes(*kolkata), 330);
        EXPECT_FALSE(kolkata->getSupportsDaylightSavingTimeProperty());
        // No daylight time means both names describe the same thing.
        EXPECT_EQ(kolkata->getStandardNameProperty(), kolkata->getDaylightNameProperty());
    }
    auto kathmandu = zoneOrNull("Asia/Kathmandu");
    if (kathmandu) { EXPECT_EQ(offsetMinutes(*kathmandu), 345); }
    auto tehran = zoneOrNull("Asia/Tehran");
    if (tehran) { EXPECT_EQ(offsetMinutes(*tehran), 210); }
}

// Africa/Casablanca is the other negative-DST zone here: it is on the same offset in both
// January and July, so the two-sample probe this case originally replaced could not see the
// Ramadan reversion at all. Which of the two offsets tzdata calls "standard" has since moved
// -- current data marks +01 standard and the reversion daylight -- so the expectation is
// derived, not written. Ticket #2351.
TEST(TimeZoneInfoTests, BaseUtcOffset_AllYearDaylightZoneUsesItsStandardReversion) {
    auto tz = zoneOrNull("Africa/Casablanca");
    if (!tz) GTEST_SKIP() << "Africa/Casablanca is not installed";
    const ZoneOracle oracle = oracleFor("Africa/Casablanca");
    if (!oracle.available) GTEST_SKIP() << "no standard-time sample for Africa/Casablanca";

    EXPECT_EQ(offsetMinutes(*tz), oracle.standardOffsetMinutes);
    EXPECT_EQ(tz->getStandardNameProperty(), oracle.standardAbbrev);
    EXPECT_TRUE(tz->getSupportsDaylightSavingTimeProperty());
    if (oracle.observesDaylight) {
        EXPECT_NE(offsetMinutes(*tz), oracle.daylightOffsetMinutes);
    }
}

TEST(TimeZoneInfoTests, BaseUtcOffset_FixedOffsetZonesAreUnchanged) {
    auto minus5 = zoneOrNull("Etc/GMT+5");
    if (minus5) {
        EXPECT_EQ(offsetMinutes(*minus5), -300);
        EXPECT_FALSE(minus5->getSupportsDaylightSavingTimeProperty());
    }
    auto plus14 = zoneOrNull("Etc/GMT-14");
    if (plus14) { EXPECT_EQ(offsetMinutes(*plus14), 840); }
    auto utc = zoneOrNull("Etc/UTC");
    if (utc) {
        EXPECT_EQ(offsetMinutes(*utc), 0);
        EXPECT_FALSE(utc->getSupportsDaylightSavingTimeProperty());
    }
}

TEST(TimeZoneInfoTests, BaseUtcOffset_PhoenixNeverObservesDaylightTime) {
    auto tz = zoneOrNull("America/Phoenix");
    if (!tz) GTEST_SKIP() << "America/Phoenix is not installed";
    EXPECT_EQ(offsetMinutes(*tz), -420);
    EXPECT_FALSE(tz->getSupportsDaylightSavingTimeProperty());
}

TEST(TimeZoneInfoTests, BaseUtcOffset_RepeatedLookupsAgree) {
    // The value must be a property of the zone, so two lookups in the same process must give
    // the same answer -- and so must a lookup of an alias that resolves to the same rules.
    auto a = zoneOrNull("America/New_York");
    auto b = zoneOrNull("America/New_York");
    if (!a || !b) GTEST_SKIP() << "America/New_York is not installed";
    EXPECT_EQ(offsetMinutes(*a), offsetMinutes(*b));
    EXPECT_EQ(a->getStandardNameProperty(), b->getStandardNameProperty());
    auto est = zoneOrNull("EST5EDT");
    if (est) { EXPECT_EQ(offsetMinutes(*est), offsetMinutes(*a)); }
}

TEST(TimeZoneInfoTests, StandardAndDaylightNames_DifferExactlyWhenTheZoneObservesDaylightTime) {
    for (const char* id : {"America/New_York", "Europe/Prague", "Australia/Sydney",
                           "Europe/Dublin", "Asia/Kolkata", "America/Phoenix", "Etc/UTC"}) {
        auto tz = zoneOrNull(id);
        if (!tz) continue;
        const bool namesDiffer =
            tz->getStandardNameProperty() != tz->getDaylightNameProperty();
        EXPECT_EQ(namesDiffer, tz->getSupportsDaylightSavingTimeProperty())
            << "zone " << id << " std=" << tz->getStandardNameProperty()
            << " dst=" << tz->getDaylightNameProperty();
    }
}

// ---------------------------------------------------------------------------
// Ticket #2183 (post-audit, no SR-AUD identifier): the lookup door validated a
// file it did not necessarily read. zoneFileExists checked only S_ISREG under
// /usr/share/zoneinfo and then handed the identifier to setenv("TZ", ...), which
// glibc parses as a POSIX rule string when it is not a path. Measured before the
// fix (build-probe/2176_probe2_newdefects.log): all seven non-TZif regular files
// shipped inside /usr/share/zoneinfo were accepted as zones with offset 0 and a
// name taken from the filename; "America//New_York", "America/./New_York" and
// "./America/New_York" were accepted; and an embedded NUL truncated the
// filesystem check while the full 21-byte string was stored as the zone's Id.
//
// The six shipped data files are used as malformed-input fixtures precisely so
// that nothing has to be written into system zoneinfo.
// ---------------------------------------------------------------------------

// #2186, question 4, answered 2026-08-18. #2183 kept TimeZoneNotFoundException for a file that
// exists but is not zone data, "rather than guessing InvalidTimeZoneException". The guess is
// unnecessary now: .NET raises InvalidTimeZoneException there
// (TimeZoneInfo.Unix.cs:697, SR.InvalidTimeZone_NoTTInfoStructures) and reserves
// TimeZoneNotFoundException for an id that names nothing. Those are different answers to
// different questions -- "there is no such zone" versus "that is not a zone" -- and a caller
// catching only the first used to swallow the second.
TEST(TimeZoneInfoTests, Fix2186_NotZoneDataIsInvalidNotNotFound) {
    // The six data files shipped inside /usr/share/zoneinfo exist and are not TZif.
    int checked = 0;
    for (const char* id : {"zone.tab", "zone1970.tab", "iso3166.tab", "tzdata.zi",
                           "leapseconds", "leap-seconds.list"}) {
        if (!std::filesystem::exists(std::string("/usr/share/zoneinfo/") + id)) continue;
        ++checked;
        EXPECT_THROW((void)TimeZoneInfo::FindSystemTimeZoneById(id),
                     System::InvalidTimeZoneException) << id;
    }
    if (checked == 0) GTEST_SKIP() << "no non-TZif data files are installed here";

    // An id that names NOTHING is still not-found, which is what makes the split meaningful.
    EXPECT_THROW((void)TimeZoneInfo::FindSystemTimeZoneById("No/Such/Zone/Anywhere"),
                 System::TimeZoneNotFoundException);
    // ...and so is a malformed one, which never reaches the file at all.
    EXPECT_THROW((void)TimeZoneInfo::FindSystemTimeZoneById("America//New_York"),
                 System::TimeZoneNotFoundException);

    // TryFindSystemTimeZoneById returns FALSE for every failure and throws none of them, which is
    // .NET's other half of this question: its Try form discards the exception outright
    // (TimeZoneInfo.cs:526-527).
    std::shared_ptr<TimeZoneInfo> tz;
    EXPECT_FALSE(TimeZoneInfo::TryFindSystemTimeZoneById("zone.tab", tz));
    EXPECT_EQ(tz, nullptr);
    EXPECT_FALSE(TimeZoneInfo::TryFindSystemTimeZoneById("No/Such/Zone/Anywhere", tz));
    EXPECT_EQ(tz, nullptr);
    EXPECT_TRUE(TimeZoneInfo::TryFindSystemTimeZoneById("UTC", tz));
    EXPECT_NE(tz, nullptr);
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_NonTzifDataFiles_AreRejected) {
    int checked = 0;
    for (const char* id : {"zone.tab", "zone1970.tab", "iso3166.tab", "tzdata.zi",
                           "leapseconds", "leap-seconds.list"}) {
        std::shared_ptr<TimeZoneInfo> tz;
        if (TimeZoneInfo::TryFindSystemTimeZoneById(id, tz)) {
            ADD_FAILURE() << "'" << id << "' is not zone data but was accepted as a zone";
        }
        ++checked;
    }
    EXPECT_EQ(checked, 6);
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_MalformedPathShapes_AreRejected) {
    for (const char* id : {"America//New_York", "America///New_York", "./America/New_York",
                           "America/./New_York", "America/New_York/", "/America/New_York",
                           "/etc/passwd", "..", "../../etc/passwd", "", "America/../America/New_York"}) {
        EXPECT_THROW(TimeZoneInfo::FindSystemTimeZoneById(id), TimeZoneNotFoundException)
            << "identifier '" << id << "' should not resolve";
    }
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_EmbeddedNul_IsRejectedNotTruncated) {
    // The C string stops at the NUL, so the old code stat()ed "America/New_York", succeeded,
    // and stored all 21 bytes as the zone's Id -- an Id that can never round-trip.
    std::string truncating("America/New_York\0junk", 21);
    ASSERT_EQ(truncating.size(), 21u);
    EXPECT_THROW(TimeZoneInfo::FindSystemTimeZoneById(truncating), TimeZoneNotFoundException);
    std::string afterUtc("UTC\0x", 5);
    EXPECT_THROW(TimeZoneInfo::FindSystemTimeZoneById(afterUtc), TimeZoneNotFoundException);
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_RealZonesStillResolve) {
    // The guard must not narrow the door: every shape a real identifier takes still works,
    // including a multi-segment id, a single-segment id, and a '+' in the last segment.
    for (const char* id : {"UTC", "Local", "Etc/UTC", "Etc/GMT+5", "America/New_York",
                           "America/Argentina/Buenos_Aires", "Europe/Prague", "EST5EDT"}) {
        std::shared_ptr<TimeZoneInfo> tz;
        if (!TimeZoneInfo::TryFindSystemTimeZoneById(id, tz)) continue;  // trimmed tzdata
        EXPECT_NE(tz, nullptr) << id;
        EXPECT_EQ(tz->getIdProperty(), id);
    }
    // At minimum the two synthetic ids must always work.
    EXPECT_NE(TimeZoneInfo::FindSystemTimeZoneById("UTC"), nullptr);
    EXPECT_NE(TimeZoneInfo::FindSystemTimeZoneById("Local"), nullptr);
}

TEST(TimeZoneInfoTests, FindSystemTimeZoneById_ADirectoryIsNotAZone) {
    EXPECT_THROW(TimeZoneInfo::FindSystemTimeZoneById("America"), TimeZoneNotFoundException);
    EXPECT_THROW(TimeZoneInfo::FindSystemTimeZoneById("Etc"), TimeZoneNotFoundException);
}

// ---------------------------------------------------------------------------
// Ticket #2185 (SR-AUD-228): HasSameRules reduces every rule set to a base offset
// plus a bool, so it cannot return false where .NET does. America/New_York and
// America/Havana share a standard offset (-05:00) and a daylight flag but not
// their transition rules; .NET reports them as different, this port reports them
// as the same.
//
// The complete repair needs stored adjustment rules, which is a measured object
// layout change -- sizeof(TimeZoneInfo) 160 -> 184 on LP64
// (build-probe/2185_layout.log), with no member ordering that avoids it. That
// needs an explicit decision, so #2185's implementation half is gated.
//
// These tests PIN the current behaviour so the question cannot be answered
// silently: if a later change makes HasSameRules rule-aware, they fail and force
// the author to come back to the gate rather than slipping a layout change
// through. They assert what IS, not what OUGHT to be.
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, PIN_HasSameRules_CannotDistinguishNewYorkFromHavana) {
    auto ny = zoneOrNull("America/New_York");
    auto havana = zoneOrNull("America/Havana");
    if (!ny || !havana) GTEST_SKIP() << "America/New_York or America/Havana is not installed";
    // Both are -05:00 standard with daylight time; their transition dates differ.
    ASSERT_EQ(offsetMinutes(*ny), offsetMinutes(*havana));
    ASSERT_TRUE(ny->getSupportsDaylightSavingTimeProperty());
    ASSERT_TRUE(havana->getSupportsDaylightSavingTimeProperty());
    EXPECT_TRUE(ny->HasSameRules(*havana))
        << "PIN (#2185, SR-AUD-228): this port cannot tell these two zones apart. .NET returns "
           "false. If this now fails, the rule-aware repair has been implemented -- that is an "
           "object layout change (160 -> 184) and needs the gate in #2185 to be cleared first.";
}

TEST(TimeZoneInfoTests, PIN_HasSameRules_OnlyDistinguishesOffsetAndTheDaylightFlag) {
    auto ny = zoneOrNull("America/New_York");
    auto phoenix = zoneOrNull("America/Phoenix");
    if (!ny || !phoenix) GTEST_SKIP() << "a required zone is not installed";
    // A different offset is the one thing it can see.
    EXPECT_FALSE(ny->HasSameRules(*phoenix));
    // And a matching offset plus a matching flag is all it needs to say "same".
    auto fake = TimeZoneInfo::CreateCustomTimeZone("Invented/Zone", TimeSpan::FromMinutes(-300),
                                                   "d", "s");
    EXPECT_FALSE(ny->HasSameRules(*fake))   // custom zones report SupportsDaylightSavingTime false
        << "PIN: the daylight flag is the second and last thing HasSameRules compares.";
    auto twin = TimeZoneInfo::CreateCustomTimeZone("Another/Zone", TimeSpan::FromMinutes(-420),
                                                   "d", "s");
    EXPECT_TRUE(phoenix->HasSameRules(*twin))
        << "PIN: an invented zone with Phoenix's offset and no daylight time is 'same rules'.";
}

TEST(TimeZoneInfoTests, PIN_GetAdjustmentRules_IsEmptyForEverySystemZone) {
    // The reason HasSameRules cannot do better: there is nothing to compare.
    for (const char* id : {"America/New_York", "America/Havana", "Europe/Prague", "UTC"}) {
        auto tz = zoneOrNull(id);
        if (!tz) continue;
        EXPECT_TRUE(tz->GetAdjustmentRules().empty())
            << "PIN (#2185): " << id << " would need stored rules for HasSameRules to improve.";
    }
}

TEST(TimeZoneInfoTests, PIN_TimeZoneInfoObjectLayoutIsUnchanged) {
    // #2177-#2184 add no member. The gate in #2185 is precisely a change to this number, so it
    // is asserted rather than described. Measured on LP64: 4 x std::string (32) + TimeSpan (24)
    // + bool (1) + 7 bytes tail padding.
    static_assert(sizeof(TimeZoneInfo) == 160,
                  "sizeof(TimeZoneInfo) changed; if this is the SR-AUD-228 rule storage, ticket "
                  "#2185's approval gate must be cleared first");
    static_assert(alignof(TimeZoneInfo) == 8, "alignof(TimeZoneInfo) changed");
    static_assert(sizeof(TimeZoneInfo::TransitionTime) == 40,
                  "sizeof(TimeZoneInfo::TransitionTime) changed");
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Ticket #2186 (deferred verification): behaviours this review measured but will
// not change without a reference answer. Pinned so that answering the question
// later is a deliberate act with a failing test attached, not a silent drift.
// ---------------------------------------------------------------------------

// FLIPPED by #2186 (2026-08-18), question 1. The pin recorded that .NET "appears to clamp" and
// that it was "unverifiable here". It clamps, and the mechanism is one line:
//
//     private static DateTime SafeCreateDateTimeFromTicks(long ticks, DateTimeKind kind = …)
//         => (ulong)ticks <= DateTime.MaxTicks ? new DateTime(ticks, kind)
//                                              : (ticks < 0 ? DateTime.MinValue : DateTime.MaxValue);
//                                                        -- TimeZoneInfo.Cache.cs:340-342
//
// with ConvertTime building its result through it (TimeZoneInfo.cs:685).
TEST(TimeZoneInfoTests, Fix2186_ConversionOverflowClampsRatherThanThrowing) {
    auto plus14  = TimeZoneInfo::CreateCustomTimeZone("+14", TimeSpan::FromHours(14), "d", "s");
    auto minus14 = TimeZoneInfo::CreateCustomTimeZone("-14", TimeSpan::FromHours(-14), "d", "s");

    EXPECT_EQ(TimeZoneInfo::ConvertTimeFromUtc(DateTime::MaxValue, *plus14), DateTime::MaxValue);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeFromUtc(DateTime::MinValue, *minus14), DateTime::MinValue);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeToUtc(DateTime::MinValue, *plus14), DateTime::MinValue);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeToUtc(DateTime::MaxValue, *minus14), DateTime::MaxValue);

    // BOTH ENDS, and the sign of the excess is what picks the bound -- clamping the wrong way
    // would still "not throw" and would still pass a test that only asserted that.
    EXPECT_EQ(TimeZoneInfo::ConvertTimeFromUtc(DateTime::MinValue, *plus14),
              DateTime::MinValue.AddHours(14)) << "an in-range result is not clamped at all";
    EXPECT_EQ(TimeZoneInfo::ConvertTimeFromUtc(DateTime::MaxValue, *minus14),
              DateTime::MaxValue.AddHours(-14));

    // The three-argument ConvertTime clamps ONCE, at the end. .NET computes the result "from raw
    // ticks to avoid precision loss from double-clamping" (TimeZoneInfo.cs:683-685), so an
    // intermediate UTC value that leaves the range must not drag the final answer to a bound when
    // the final answer is representable.
    EXPECT_EQ(TimeZoneInfo::ConvertTime(DateTime::MaxValue, *minus14, *minus14),
              DateTime::MaxValue)
        << "the intermediate UTC ticks overflow; the final local ticks do not";
    EXPECT_EQ(TimeZoneInfo::ConvertTime(DateTime::MinValue, *plus14, *plus14),
              DateTime::MinValue);

    // The ordinary rows, unmoved.
    const DateTime noon(2025, 6, 15, 12, 0, 0);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeFromUtc(noon, *plus14), noon.AddHours(14));
    EXPECT_EQ(TimeZoneInfo::ConvertTimeToUtc(noon, *plus14), noon.AddHours(-14));
}

TEST(TimeZoneInfoTests, PIN_ClearCachedDataIsInertAndGetSystemTimeZonesReturnsTwo) {
    // Both are documented feature gaps rather than defects; sizing them needs .NET's caching
    // contract, so they are recorded in #2186 and pinned here.
    auto before = TimeZoneInfo::Local().getBaseUtcOffsetProperty().getTicksProperty();
    EXPECT_NO_THROW(TimeZoneInfo::ClearCachedData());
    EXPECT_EQ(TimeZoneInfo::Local().getBaseUtcOffsetProperty().getTicksProperty(), before)
        << "PIN (#2186): ClearCachedData() is a no-op; Local() uses a block-scope static.";
    auto zones = TimeZoneInfo::GetSystemTimeZones();
    EXPECT_EQ(zones.size(), 2u)
        << "PIN (#2186): the database holds hundreds of zones; this returns UTC and Local.";
}

// FLIPPED by #2186 (2026-08-18), questions 2 and 3. #2179 measured all three as accepted and
// declined to repair them, because "inventing three more rejections on a recollection of the .NET
// source is exactly what this review declines to do". That was the right call, and the reference
// now supplies all three -- AND CORRECTS THE TICKET'S OWN STATEMENT OF TWO OF THEM.
TEST(TimeZoneInfoTests, Fix2186_AdjacentAdjustmentRuleValidationsArePresent) {
    DateTime tod;
    auto tt  = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 3, 14);
    auto tt2 = TimeZoneInfo::TransitionTime::CreateFixedDateRule(tod, 10, 7);

    // A dateStart or dateEnd carrying a time-of-day (AdjustmentRule.cs:216-223).
    EXPECT_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 1, 1, 5, 30, 0), DateTime(2025, 12, 31), TimeSpan::Zero, tt, tt2),
                 System::ArgumentException);
    EXPECT_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 1, 1), DateTime(2025, 12, 31, 5, 30, 0), TimeSpan::Zero, tt, tt2),
                 System::ArgumentException);
    // ...but MinValue and MaxValue are EXEMPT, which is how a rule spanning all time is spelled.
    // Both carry a zero time-of-day anyway; the exemption matters for the reference's Kind
    // conjunct, and reproducing the shape keeps this a transcription.
    EXPECT_NO_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        DateTime::MinValue, DateTime::MaxValue, TimeSpan::Zero, tt, tt2));

    // THE RANGE IS NOT +/-14 HOURS, which is what the ticket said. .NET's check is
    // `daylightDelta.TotalHours < -23.0 || > 14.0` (:206-208), and it explains why in a comment:
    // Samoa moved across the International Date Line, so describing its delta needs -23. The
    // MESSAGE still says "plus or minus 14.0 hours" because it is shared with UtcOffsetOutOfRange
    // -- that inconsistency is .NET's, and it is transcribed rather than tidied.
    EXPECT_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 1, 1), DateTime(2025, 12, 31), TimeSpan::FromHours(15), tt, tt2),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 1, 1), DateTime(2025, 12, 31), TimeSpan::FromHours(-24), tt, tt2),
                 System::ArgumentOutOfRangeException);
    // -23 is accepted and -14 is not the boundary. A symmetric +/-14 check would reject this row.
    EXPECT_NO_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        DateTime(2025, 1, 1), DateTime(2025, 12, 31), TimeSpan::FromHours(-23), tt, tt2));
    EXPECT_NO_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        DateTime(2025, 1, 1), DateTime(2025, 12, 31), TimeSpan::FromHours(14), tt, tt2));

    // THE SECONDS CHECK IS NOT "SUB-MINUTE" EITHER. It is "not a whole number of minutes"
    // (:211-214), so a large delta with a stray 30 seconds fails as surely as 30 seconds does.
    EXPECT_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 1, 1), DateTime(2025, 12, 31), TimeSpan::FromSeconds(30), tt, tt2),
                 System::ArgumentException);
    EXPECT_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 1, 1), DateTime(2025, 12, 31),
                     TimeSpan::FromHours(1) + TimeSpan::FromSeconds(30), tt, tt2),
                 System::ArgumentException);
    EXPECT_NO_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        DateTime(2025, 1, 1), DateTime(2025, 12, 31), TimeSpan::FromMinutes(90), tt, tt2));

    // The exact texts, which is question 3: the exception TYPES came from the audit probe and the
    // TEXT did not, so the text is what this asserts.
    try {
        (void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
            DateTime(2025, 1, 1), DateTime(2025, 12, 31), TimeSpan::FromSeconds(30), tt, tt2);
        FAIL();
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "The TimeSpan parameter cannot be specified more precisely than whole "
                      "minutes."),
                  std::string::npos) << e.what();
    }
    try {
        (void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
            DateTime(2025, 1, 1, 5, 30, 0), DateTime(2025, 12, 31), TimeSpan::Zero, tt, tt2);
        FAIL();
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "The supplied DateTime includes a TimeOfDay setting.   This is not "
                      "supported."),
                  std::string::npos) << e.what();
    }

    // The pre-existing reversed-range rejection is unmoved.
    EXPECT_THROW((void)TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 12, 31), DateTime(2025, 1, 1), TimeSpan::Zero, tt, tt2),
                 System::ArgumentException);
}

TEST(TimeZoneInfoTests, PIN_TimeZoneInfoItselfModelsNoDaylightTransitions) {
    // The documented limitation, asserted so that making TimeZoneInfo date-sensitive is a
    // deliberate act. The legacy TimeZone adapter deliberately differs; see TimeZoneTests.cpp.
    auto ny = zoneOrNull("America/New_York");
    if (!ny) GTEST_SKIP() << "America/New_York is not installed";
    const DateTime january(2025, 1, 15, 12, 0, 0), july(2025, 7, 15, 12, 0, 0);
    EXPECT_EQ(ny->GetUtcOffset(january).getTicksProperty(),
              ny->GetUtcOffset(july).getTicksProperty());
    EXPECT_FALSE(ny->IsDaylightSavingTime(july));
    EXPECT_FALSE(ny->IsAmbiguousTime(DateTime(2025, 11, 2, 1, 30, 0)));
    EXPECT_FALSE(ny->IsInvalidTime(DateTime(2025, 3, 9, 2, 30, 0)));
    EXPECT_TRUE(ny->GetAmbiguousTimeOffsets(DateTime(2025, 11, 2, 1, 30, 0)).empty());
}

// ---------------------------------------------------------------------------
// Post-#1941 ripple audit: TimeZoneInfo must consume and produce DateTimeKind.
// ---------------------------------------------------------------------------

TEST(TimeZoneInfoTests, Post1941_TransitionTimeRequiresAnUnspecifiedClockAtBothFactories) {
    const SharpRuntime::longcs clockTicks = 2 * TimeSpan::TicksPerHour;
    for (DateTimeKind kind : {DateTimeKind::Utc, DateTimeKind::Local}) {
        const DateTime clock(clockTicks, kind);
        EXPECT_THROW((void)TimeZoneInfo::TransitionTime::CreateFixedDateRule(clock, 3, 14),
                     System::ArgumentException)
            << static_cast<int>(kind);
        EXPECT_THROW((void)TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
                         clock, 10, 4, System::DayOfWeek::Sunday),
                     System::ArgumentException)
            << static_cast<int>(kind);
    }

    const DateTime unspecified(clockTicks, DateTimeKind::Unspecified);
    EXPECT_NO_THROW((void)TimeZoneInfo::TransitionTime::CreateFixedDateRule(
        unspecified, 3, 14));

    try {
        // Kind is the first reference validation, ahead of even an invalid month/day pair.
        (void)TimeZoneInfo::TransitionTime::CreateFixedDateRule(
            DateTime(clockTicks, DateTimeKind::Utc), 0, 0);
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "timeOfDay");
        EXPECT_NE(std::string(e.what()).find("DateTimeKind.Unspecified"), std::string::npos);
    }
}

TEST(TimeZoneInfoTests, Post1941_ConvertTimeKindFollowsCanonicalDestinationIdentity) {
    const DateTime utc = DateTime::SpecifyKind(DateTime(2025, 6, 15, 12, 0, 0),
                                                DateTimeKind::Utc);
    auto other = TimeZoneInfo::CreateCustomTimeZone(
        "Other", TimeSpan::FromHours(2), "Other", "Other");
    auto utcLookalike = TimeZoneInfo::CreateCustomTimeZone(
        "UTC", TimeSpan::Zero, "Coordinated Universal Time", "Coordinated Universal Time");

    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, TimeZoneInfo::Utc(), TimeZoneInfo::Utc())
                  .getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, TimeZoneInfo::Utc(), TimeZoneInfo::Local())
                  .getKindProperty(),
              DateTimeKind::Local);
    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, TimeZoneInfo::Utc(), *other).getKindProperty(),
              DateTimeKind::Unspecified);

    // The mapping is intentionally identity-based, like .NET's. Equal id/offset data does not
    // make an arbitrary custom zone the canonical UTC singleton.
    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, TimeZoneInfo::Utc(), *utcLookalike)
                  .getKindProperty(),
              DateTimeKind::Unspecified);
}

TEST(TimeZoneInfoTests, Post1941_EveryDateTimeConversionDoorUsesTheDestinationKindMatrix) {
    const DateTime utc = DateTime::SpecifyKind(DateTime(2025, 6, 15, 12, 0, 0),
                                                DateTimeKind::Utc);
    const DateTime local = DateTime::SpecifyKind(DateTime(2025, 6, 15, 12, 0, 0),
                                                  DateTimeKind::Local);
    auto other = TimeZoneInfo::CreateCustomTimeZone(
        "Other", TimeSpan::FromHours(2), "Other", "Other");

    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, TimeZoneInfo::Utc()).getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(TimeZoneInfo::ConvertTime(local, TimeZoneInfo::Local()).getKindProperty(),
              DateTimeKind::Local);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeFromUtc(utc, *other).getKindProperty(),
              DateTimeKind::Unspecified);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(utc, "UTC").getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(utc, "UTC", "Local")
                  .getKindProperty(),
              DateTimeKind::Unspecified);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(local, "Local", "UTC")
                  .getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(local, "lOcAl", "UTC")
                  .getKindProperty(),
              DateTimeKind::Utc);
}

TEST(TimeZoneInfoTests, Post1941_UtcLookupIsCanonicalButLocalLookupIsDeliberatelyACopy) {
    const DateTime utc = DateTime::SpecifyKind(DateTime(2025, 6, 15, 12, 0, 0),
                                                DateTimeKind::Utc);
    const auto foundUtc = TimeZoneInfo::FindSystemTimeZoneById("UTC");
    const auto foundMixedCaseUtc = TimeZoneInfo::FindSystemTimeZoneById("uTc");
    const auto foundLocal = TimeZoneInfo::FindSystemTimeZoneById("Local");

    ASSERT_NE(foundUtc, nullptr);
    ASSERT_NE(foundMixedCaseUtc, nullptr);
    ASSERT_NE(foundLocal, nullptr);
    EXPECT_EQ(foundUtc.get(), &TimeZoneInfo::Utc());
    EXPECT_EQ(foundMixedCaseUtc.get(), &TimeZoneInfo::Utc());
    EXPECT_NE(foundLocal.get(), &TimeZoneInfo::Local());

    // .NET's UTC lookup returns its canonical singleton. Local lookup intentionally returns the
    // equivalent database object, not TimeZoneInfo.Local, so the two destinations have different
    // result Kinds despite both being built-in ids in this port.
    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, *foundUtc).getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, *foundMixedCaseUtc).getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(utc, "uTc").getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, *foundLocal).getKindProperty(),
              DateTimeKind::Unspecified);
    EXPECT_EQ(TimeZoneInfo::ConvertTime(utc, TimeZoneInfo::Local()).getKindProperty(),
              DateTimeKind::Local);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(utc, "Local").getKindProperty(),
              DateTimeKind::Unspecified);

    const auto zones = TimeZoneInfo::GetSystemTimeZones();
    ASSERT_GE(zones.size(), 2u);
    EXPECT_EQ(zones.front().get(), &TimeZoneInfo::Utc());
    EXPECT_NE(zones.back().get(), &TimeZoneInfo::Local());
}

#if defined(__EMSCRIPTEN__)
TEST(TimeZoneInfoTests, Post1941_EmscriptenLocalIsDistinctFromUtcEvenAtZeroOffset) {
    EXPECT_EQ(TimeZoneInfo::Local().getIdProperty(), "Local");
    EXPECT_EQ(TimeZoneInfo::Utc().getIdProperty(), "UTC");
    EXPECT_NE(&TimeZoneInfo::Local(), &TimeZoneInfo::Utc());
    EXPECT_FALSE(TimeZoneInfo::Local().Equals(TimeZoneInfo::Utc()));

    const DateTime local = DateTime::SpecifyKind(DateTime(2025, 6, 15, 12, 0, 0),
                                                  DateTimeKind::Local);
    EXPECT_EQ(TimeZoneInfo::ConvertTimeBySystemTimeZoneId(local, "local", "UTC")
                  .getKindProperty(),
              DateTimeKind::Utc);
}
#endif

TEST(TimeZoneInfoTests, Post1941_CanonicalUtcAliasCannotMutateTheSingleton) {
    // FindSystemTimeZoneById("UTC") deliberately aliases TimeZoneInfo::Utc so destination Kind
    // is selected by the same reference identity as .NET. The pointee type must therefore not be
    // assignable: otherwise `*found = *custom` would overwrite the process-wide singleton and
    // corrupt every later conversion. Copy construction remains available for the deliberately
    // copied Local lookup and for ordinary value snapshots.
    static_assert(std::is_copy_constructible_v<TimeZoneInfo>);
    static_assert(!std::is_move_constructible_v<TimeZoneInfo>);
    static_assert(!std::is_copy_assignable_v<TimeZoneInfo>);
    static_assert(!std::is_move_assignable_v<TimeZoneInfo>);

    const auto foundUtc = TimeZoneInfo::FindSystemTimeZoneById("UTC");
    ASSERT_NE(foundUtc, nullptr);
    EXPECT_EQ(foundUtc.get(), &TimeZoneInfo::Utc());
    EXPECT_EQ(TimeZoneInfo::Utc().getIdProperty(), "UTC");
    EXPECT_EQ(TimeZoneInfo::Utc().getBaseUtcOffsetProperty(), TimeSpan::Zero);
}

TEST(TimeZoneInfoTests, Post1941_ConvertTimeRejectsAKindThatConflictsWithTheSourceZone) {
    const DateTime utc = DateTime::SpecifyKind(DateTime(2025, 6, 15, 12, 0, 0),
                                                DateTimeKind::Utc);
    const DateTime local = DateTime::SpecifyKind(DateTime(2025, 6, 15, 12, 0, 0),
                                                  DateTimeKind::Local);
    const DateTime unspecified(2025, 6, 15, 12, 0, 0);
    auto other = TimeZoneInfo::CreateCustomTimeZone(
        "Other", TimeSpan::FromHours(2), "Other", "Other");

    EXPECT_THROW((void)TimeZoneInfo::ConvertTime(utc, *other, TimeZoneInfo::Utc()),
                 System::ArgumentException);
    EXPECT_THROW((void)TimeZoneInfo::ConvertTime(local, TimeZoneInfo::Utc(), *other),
                 System::ArgumentException);
    EXPECT_THROW((void)TimeZoneInfo::ConvertTimeFromUtc(local, *other),
                 System::ArgumentException);
    EXPECT_NO_THROW((void)TimeZoneInfo::ConvertTime(
        unspecified, *other, TimeZoneInfo::Utc()));

    try {
        (void)TimeZoneInfo::ConvertTime(utc, *other, TimeZoneInfo::Utc());
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "sourceTimeZone");
        EXPECT_NE(std::string(e.what()).find("Kind property set correctly"), std::string::npos);
    }
}

TEST(TimeZoneInfoTests, Post1941_ConvertTimeToUtcStampsUtcEvenWhenTheClockClamps) {
    auto plus14 = TimeZoneInfo::CreateCustomTimeZone(
        "+14", TimeSpan::FromHours(14), "+14", "+14");
    const DateTime ordinary(2025, 6, 15, 12, 0, 0);

    EXPECT_EQ(TimeZoneInfo::ConvertTimeToUtc(ordinary, *plus14).getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(plus14->ConvertTimeToUtc(ordinary).getKindProperty(), DateTimeKind::Utc);

    const DateTime clamped = TimeZoneInfo::ConvertTimeToUtc(DateTime::MinValue, *plus14);
    EXPECT_EQ(clamped.getTicksProperty(), DateTime::MinValue.getTicksProperty());
    EXPECT_EQ(clamped.getKindProperty(), DateTimeKind::Utc);
}

TEST(TimeZoneInfoTests, Post1941_SafeFromTicksCarriesKindOnlyForAnInRangeConstruction) {
    EXPECT_EQ(TimeZoneInfo::safeFromTicks(42, DateTimeKind::Utc).getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(TimeZoneInfo::safeFromTicks(-1, DateTimeKind::Utc).getKindProperty(),
              DateTimeKind::Unspecified);
    EXPECT_EQ(TimeZoneInfo::safeFromTicks(DateTime::MaxTicks + 1, DateTimeKind::Local)
                  .getKindProperty(),
              DateTimeKind::Unspecified);
}

TEST(TimeZoneInfoTests, Post1941_DateTimeOffsetConversionBuildsAnUnspecifiedDestinationClock) {
    const DateTime utc = DateTime::SpecifyKind(DateTime(2025, 6, 15, 12, 0, 0),
                                                DateTimeKind::Utc);
    const System::DateTimeOffset input(utc, TimeSpan::Zero);
    auto plus2 = TimeZoneInfo::CreateCustomTimeZone(
        "+02", TimeSpan::FromHours(2), "+02", "+02");

    System::DateTimeOffset converted;
    EXPECT_NO_THROW(converted = TimeZoneInfo::ConvertTime(input, *plus2));
    EXPECT_EQ(converted.getOffsetProperty(), TimeSpan::FromHours(2));
    EXPECT_EQ(converted.getHourProperty(), 14);
    EXPECT_EQ(converted.getDateTimeProperty().getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(converted.getUtcDateTimeProperty().getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(converted.getUtcTicksProperty(), input.getUtcTicksProperty());
}

TEST(TimeZoneInfoTests, Post1941_DateTimeOffsetConversionClampsAtBothRangeEnds) {
    auto plus14 = TimeZoneInfo::CreateCustomTimeZone(
        "+14", TimeSpan::FromHours(14), "+14", "+14");
    auto minus14 = TimeZoneInfo::CreateCustomTimeZone(
        "-14", TimeSpan::FromHours(-14), "-14", "-14");

    System::DateTimeOffset upper;
    System::DateTimeOffset lower;
    EXPECT_NO_THROW(upper = TimeZoneInfo::ConvertTime(System::DateTimeOffset::MaxValue,
                                                       *plus14));
    EXPECT_NO_THROW(lower = TimeZoneInfo::ConvertTime(System::DateTimeOffset::MinValue,
                                                       *minus14));
    EXPECT_EQ(upper.getTicksProperty(), System::DateTimeOffset::MaxValue.getTicksProperty());
    EXPECT_EQ(upper.getOffsetProperty(), TimeSpan::Zero);
    EXPECT_EQ(lower.getTicksProperty(), System::DateTimeOffset::MinValue.getTicksProperty());
    EXPECT_EQ(lower.getOffsetProperty(), TimeSpan::Zero);

    // Exactly reaching a boundary is representable and retains the destination offset; only a
    // value beyond the boundary saturates to the zero-offset constants above.
    const auto nearUpper = System::DateTimeOffset::MaxValue.AddHours(-14);
    const auto exactUpper = TimeZoneInfo::ConvertTime(nearUpper, *plus14);
    EXPECT_EQ(exactUpper.getTicksProperty(), DateTime::MaxTicks);
    EXPECT_EQ(exactUpper.getOffsetProperty(), TimeSpan::FromHours(14));
    EXPECT_EQ(exactUpper.getUtcTicksProperty(), nearUpper.getUtcTicksProperty());

    const auto nearLower = System::DateTimeOffset::MinValue.AddHours(14);
    const auto exactLower = TimeZoneInfo::ConvertTime(nearLower, *minus14);
    EXPECT_EQ(exactLower.getTicksProperty(), 0);
    EXPECT_EQ(exactLower.getOffsetProperty(), TimeSpan::FromHours(-14));
    EXPECT_EQ(exactLower.getUtcTicksProperty(), nearLower.getUtcTicksProperty());
}
