// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <string>
#include "System/ArgumentException.hpp"
#include "System/TimeZoneInfo.hpp"

using System::TimeZoneInfo;
using System::DateTime;
using System::TimeSpan;
using System::DayOfWeek;

// Helpers
static TimeZoneInfo::TransitionTime fixedTT(int month = 3, int day = 14) {
    return TimeZoneInfo::TransitionTime::CreateFixedDateRule(DateTime{}, month, day);
}
static TimeZoneInfo::TransitionTime floatTT(int month = 10, int week = 4) {
    return TimeZoneInfo::TransitionTime::CreateFloatingDateRule(
        DateTime{}, month, week, DayOfWeek::Sunday);
}

// ---------------------------------------------------------------------------
// 5-param CreateAdjustmentRule — properties
// ---------------------------------------------------------------------------

TEST(AdjustmentRuleTests, Create5_DateStartStored) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT());
    EXPECT_EQ(r->getDateStartProperty().getYearProperty(), 2020);
    EXPECT_EQ(r->getDateStartProperty().getMonthProperty(), 1);
}

TEST(AdjustmentRuleTests, Create5_DateEndStored) {
    DateTime start(2020, 1, 1), end(2025, 6, 30);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT());
    EXPECT_EQ(r->getDateEndProperty().getYearProperty(), 2025);
}

TEST(AdjustmentRuleTests, Create5_DaylightDeltaStored) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    TimeSpan delta = TimeSpan::FromHours(1);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, delta, fixedTT(), floatTT());
    EXPECT_EQ(r->getDaylightDeltaProperty().getTotalHoursProperty(), 1.0);
}

TEST(AdjustmentRuleTests, Create5_TransitionStartStored) {
    auto tt = fixedTT(4, 7);
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, tt, floatTT());
    EXPECT_EQ(r->getDaylightTransitionStartProperty().getMonthProperty(), 4);
    EXPECT_EQ(r->getDaylightTransitionStartProperty().getDayProperty(), 7);
}

TEST(AdjustmentRuleTests, Create5_TransitionEndStored) {
    auto tt = floatTT(11, 3);
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), tt);
    EXPECT_EQ(r->getDaylightTransitionEndProperty().getMonthProperty(), 11);
    EXPECT_EQ(r->getDaylightTransitionEndProperty().getWeekProperty(), 3);
}

TEST(AdjustmentRuleTests, Create5_BaseUtcOffsetDelta_IsZero) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT());
    EXPECT_EQ(r->getBaseUtcOffsetDeltaProperty().getTotalSecondsProperty(), 0.0);
}

TEST(AdjustmentRuleTests, Create5_NoDaylightTransitions_IsFalse) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT());
    EXPECT_FALSE(r->getNoDaylightTransitionsProperty());
}

// ---------------------------------------------------------------------------
// 6-param CreateAdjustmentRule
// ---------------------------------------------------------------------------

TEST(AdjustmentRuleTests, Create6_BaseUtcOffsetDeltaStored) {
    DateTime start(2021, 1, 1), end(2021, 12, 31);
    TimeSpan baseDelta = TimeSpan::FromHours(2);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT(), baseDelta);
    EXPECT_EQ(r->getBaseUtcOffsetDeltaProperty().getTotalHoursProperty(), 2.0);
}

TEST(AdjustmentRuleTests, Create6_OtherFieldsPreserved) {
    DateTime start(2021, 3, 1), end(2021, 11, 1);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::FromMinutes(30), fixedTT(5, 1), floatTT(9, 2),
        TimeSpan::FromHours(1));
    EXPECT_EQ(r->getDateStartProperty().getMonthProperty(), 3);
    EXPECT_EQ(r->getDateEndProperty().getMonthProperty(), 11);
    EXPECT_EQ(r->getDaylightDeltaProperty().getTotalMinutesProperty(), 30.0);
    EXPECT_EQ(r->getDaylightTransitionStartProperty().getMonthProperty(), 5);
    EXPECT_EQ(r->getDaylightTransitionEndProperty().getMonthProperty(), 9);
}

// ---------------------------------------------------------------------------
// getNoDaylightTransitionsProperty
// ---------------------------------------------------------------------------

TEST(AdjustmentRuleTests, NoDaylightTransitions_DefaultIsFalse) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT());
    EXPECT_FALSE(r->getNoDaylightTransitionsProperty());
}

// ---------------------------------------------------------------------------
// getHasDaylightSavingProperty
// ---------------------------------------------------------------------------

TEST(AdjustmentRuleTests, HasDaylightSaving_ZeroDelta_DefaultTransitions_False) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    // Zero delta, default transitions → no DST
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero,
        TimeZoneInfo::TransitionTime{}, TimeZoneInfo::TransitionTime{});
    EXPECT_FALSE(r->getHasDaylightSavingProperty());
}

TEST(AdjustmentRuleTests, HasDaylightSaving_NonZeroDelta_True) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::FromHours(1), fixedTT(), floatTT());
    EXPECT_TRUE(r->getHasDaylightSavingProperty());
}

TEST(AdjustmentRuleTests, HasDaylightSaving_NonDefaultTransitionStart_True) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), TimeZoneInfo::TransitionTime{});
    EXPECT_TRUE(r->getHasDaylightSavingProperty());
}

// ---------------------------------------------------------------------------
// Equals — now includes BaseUtcOffsetDelta
// ---------------------------------------------------------------------------

TEST(AdjustmentRuleTests, Equals_SameBaseUtcOffsetDelta_True) {
    DateTime start(2022, 1, 1), end(2022, 12, 31);
    TimeSpan baseDelta = TimeSpan::FromHours(1);
    auto a = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT(), baseDelta);
    auto b = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT(), baseDelta);
    EXPECT_TRUE(a->Equals(*b));
    EXPECT_TRUE(*a == *b);
}

TEST(AdjustmentRuleTests, Equals_DiffBaseUtcOffsetDelta_False) {
    DateTime start(2022, 1, 1), end(2022, 12, 31);
    auto a = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT(), TimeSpan::FromHours(1));
    auto b = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT(), TimeSpan::FromHours(2));
    EXPECT_FALSE(a->Equals(*b));
    EXPECT_TRUE(*a != *b);
}

TEST(AdjustmentRuleTests, Equals_DiffDaylightDelta_False) {
    DateTime start(2022, 1, 1), end(2022, 12, 31);
    auto a = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::FromHours(1), fixedTT(), floatTT());
    auto b = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::FromHours(2), fixedTT(), floatTT());
    EXPECT_FALSE(a->Equals(*b));
}

// ---------------------------------------------------------------------------
// GetHashCode
// ---------------------------------------------------------------------------

TEST(AdjustmentRuleTests, GetHashCode_SameStart_SameHash) {
    DateTime start(2023, 3, 1), end(2023, 11, 1);
    auto a = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT());
    auto b = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::FromHours(1), fixedTT(), floatTT());
    // Hash is based only on DateStart ticks
    EXPECT_EQ(a->GetHashCode(), b->GetHashCode());
}

// Replaces GetHashCode_DiffStart_LikelyDiffHash, whose own name conceded the claim was
// probabilistic. The hash folds 64 tick bits into 32, so two rules with different starts may
// legally collide (docs/HashAssertionContractRule.md R2); what they must not do is compare
// equal. GetHashCode_SameStartDifferentDelta_SameHash above pins that the hash reads DateStart
// and nothing else.
TEST(AdjustmentRuleTests, DiffStart_RulesAreUnequal) {
    DateTime start1(2020, 1, 1), start2(2021, 1, 1);
    DateTime end(2025, 12, 31);
    auto a = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start1, end, TimeSpan::Zero, fixedTT(), floatTT());
    auto b = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start2, end, TimeSpan::Zero, fixedTT(), floatTT());
    EXPECT_FALSE(a->Equals(*b));
    EXPECT_NE(*a, *b);
}

TEST(AdjustmentRuleTests, GetHashCode_IsConsistent) {
    DateTime start(2020, 1, 1), end(2020, 12, 31);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        start, end, TimeSpan::Zero, fixedTT(), floatTT());
    EXPECT_EQ(r->GetHashCode(), r->GetHashCode());
}

// ---------------------------------------------------------------------------
// Ticket #2179 (SR-AUD-226): both CreateAdjustmentRule overloads blindly retained
// a reversed effective date range. Measured before the fix
// (build-probe/2176_probe1_surface.log): CreateAdjustmentRule(2025-01-02,
// 2025-01-01, ...) returned normally on the 5-arg *and* the 6-arg form -- the
// audit records only that "the identical current-.NET call throws
// ArgumentException", so the second overload is an extension of the finding
// established by measurement here. A rule whose end precedes its start describes
// an empty period and can never apply to any instant.
//
// The three adjacent validations measured as also missing -- a dateStart carrying
// a time-of-day, a daylightDelta outside +/-14 hours, and a sub-minute
// daylightDelta -- are deliberately NOT repaired here: the audit's managed probe
// covers only the reversed range, and #2186 carries them for verification against
// the reference rather than acting on a recollection.
// ---------------------------------------------------------------------------

TEST(AdjustmentRuleTests, Create5_EndBeforeStart_ThrowsArgumentException) {
    EXPECT_THROW(TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 1, 2), DateTime(2025, 1, 1), TimeSpan::Zero,
                     fixedTT(), floatTT()),
                 System::ArgumentException);
}

TEST(AdjustmentRuleTests, Create6_EndBeforeStart_ThrowsArgumentException) {
    EXPECT_THROW(TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime(2025, 1, 2), DateTime(2025, 1, 1), TimeSpan::Zero,
                     fixedTT(), floatTT(), TimeSpan::FromHours(1)),
                 System::ArgumentException);
}

TEST(AdjustmentRuleTests, Create5_EndBeforeStartByOneTick_Throws) {
    DateTime start(2025, 6, 15);
    DateTime end(start.getTicksProperty() - 1);
    EXPECT_THROW(TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     start, end, TimeSpan::Zero, fixedTT(), floatTT()),
                 System::ArgumentException);
}

TEST(AdjustmentRuleTests, Create5_EqualStartAndEnd_IsStillAccepted) {
    // A single-day rule is legal; only a strictly earlier end is rejected.
    DateTime same(2025, 6, 15);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        same, same, TimeSpan::Zero, fixedTT(), floatTT());
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->getDateStartProperty().getTicksProperty(),
              r->getDateEndProperty().getTicksProperty());
}

TEST(AdjustmentRuleTests, Create6_EqualStartAndEnd_IsStillAccepted) {
    DateTime same(2025, 6, 15);
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        same, same, TimeSpan::Zero, fixedTT(), floatTT(), TimeSpan::FromHours(1));
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->getBaseUtcOffsetDeltaProperty().getTotalHoursProperty(), 1.0);
}

TEST(AdjustmentRuleTests, Create5_AscendingRange_IsUnaffected) {
    // The 19 pre-existing tests all use ascending ranges; this asserts the guard did not
    // narrow them, including the widest legal span.
    auto r = TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
        DateTime::MinValue, DateTime::MaxValue, TimeSpan::Zero, fixedTT(), floatTT());
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->getDateStartProperty().getTicksProperty(),
              DateTime::MinValue.getTicksProperty());
    EXPECT_EQ(r->getDateEndProperty().getTicksProperty(), DateTime::MaxValue.getTicksProperty());
}

TEST(AdjustmentRuleTests, Create5_ReversedExtremes_Throws) {
    EXPECT_THROW(TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
                     DateTime::MaxValue, DateTime::MinValue, TimeSpan::Zero,
                     fixedTT(), floatTT()),
                 System::ArgumentException);
}

TEST(AdjustmentRuleTests, Create5_EndBeforeStart_MessageNamesTheOrdering) {
    // The exception type is what the audit's managed probe establishes; the text is pinned
    // here so it cannot drift silently (#2186 carries the reference-text question).
    try {
        TimeZoneInfo::AdjustmentRule::CreateAdjustmentRule(
            DateTime(2025, 1, 2), DateTime(2025, 1, 1), TimeSpan::Zero, fixedTT(), floatTT());
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find("must come before"), std::string::npos);
    }
}
