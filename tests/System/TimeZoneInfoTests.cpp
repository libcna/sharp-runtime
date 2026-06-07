// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <stdexcept>
#include "System/TimeZoneInfo.hpp"

using System::TimeZoneInfo;
using System::TimeSpan;
using System::DateTime;

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

TEST(TimeZoneInfoTests, Local_BaseUtcOffset_IsZero) {
    EXPECT_TRUE(TimeZoneInfo::Local().getBaseUtcOffsetProperty() == TimeSpan::Zero);
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
    // UTC zone has zero offset, so ConvertTimeToUtc == Add(Zero) == identity on ticks
    DateTime dt;
    DateTime result = TimeZoneInfo::Utc().ConvertTimeToUtc(dt);
    EXPECT_EQ(result.getTicksProperty(), dt.getTicksProperty());
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
                 std::invalid_argument);
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
                 std::invalid_argument);
}
