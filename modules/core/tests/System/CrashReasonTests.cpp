// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <array>
#include <type_traits>

#include "System/CrashReason.hpp"

using System::CrashReason;

TEST(CrashReasonTests, Unknown_IsZero) {
    EXPECT_EQ(static_cast<int>(CrashReason::Unknown), 0);
}

TEST(CrashReasonTests, UnhandledException_IsOne) {
    EXPECT_EQ(static_cast<int>(CrashReason::UnhandledException), 1);
}

TEST(CrashReasonTests, EnvironmentFailFast_IsTwo) {
    EXPECT_EQ(static_cast<int>(CrashReason::EnvironmentFailFast), 2);
}

TEST(CrashReasonTests, InternalFailFast_IsThree) {
    EXPECT_EQ(static_cast<int>(CrashReason::InternalFailFast), 3);
}

TEST(CrashReasonTests, ValuesAreDistinct) {
    EXPECT_NE(CrashReason::Unknown, CrashReason::UnhandledException);
    EXPECT_NE(CrashReason::EnvironmentFailFast, CrashReason::InternalFailFast);
}

// --- SR-AUD-127 / #2280 -------------------------------------------------------------------
// The audit recorded that no full pairwise-distinctness vector and no invalid-value
// vector existed: the four value tests above cover each enumerator once and the
// distinctness test compares only two of the six pairs, so a duplicated value across
// the two untested pairs would have gone unnoticed.

namespace {
    constexpr std::array<CrashReason, 4> AllCrashReasons{CrashReason::Unknown, CrashReason::UnhandledException,
                                                         CrashReason::EnvironmentFailFast,
                                                         CrashReason::InternalFailFast};
} // namespace

TEST(CrashReasonTests, EveryPairOfEnumeratorsIsDistinct) {
    for (std::size_t i = 0; i < AllCrashReasons.size(); ++i) {
        for (std::size_t j = i + 1; j < AllCrashReasons.size(); ++j) {
            EXPECT_NE(AllCrashReasons[i], AllCrashReasons[j])
                << "enumerators at positions " << i << " and " << j << " share a value";
        }
    }
}

TEST(CrashReasonTests, TheEnumeratorsAreContiguousFromZero) {
    for (std::size_t i = 0; i < AllCrashReasons.size(); ++i) {
        EXPECT_EQ(static_cast<int>(AllCrashReasons[i]), static_cast<int>(i));
    }
}

TEST(CrashReasonTests, TheUnderlyingTypeIsIntAndTheEnumerationIsScoped) {
    static_assert(std::is_same_v<std::underlying_type_t<CrashReason>, int>);
    static_assert(std::is_enum_v<CrashReason>);
    // Scoped: no implicit conversion to the underlying type.
    static_assert(!std::is_convertible_v<CrashReason, int>);
    SUCCEED();
}

TEST(CrashReasonTests, AValueOutsideTheEnumeratorsIsRepresentableAndMatchesNoEnumerator) {
    // This enumeration is not closed over its underlying type, and nothing in this
    // header rejects an unlisted value. Pinned so that the doc-comment's statement
    // stays true if enumerators are ever added.
    const auto unlisted = static_cast<CrashReason>(4);
    for (const CrashReason known : AllCrashReasons) {
        EXPECT_NE(unlisted, known);
    }
    EXPECT_EQ(static_cast<int>(unlisted), 4);

    const auto negative = static_cast<CrashReason>(-1);
    for (const CrashReason known : AllCrashReasons) {
        EXPECT_NE(negative, known);
    }
    EXPECT_EQ(static_cast<int>(negative), -1);
}
