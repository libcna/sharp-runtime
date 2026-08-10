// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

using System::Collections::Immutable::ImmutableList;

TEST(ImmutableListRangeTests, GetRangeReturnsOrderedIndependentSlice) {
    const auto source = ImmutableList<int>::Create({10, 20, 30, 40, 50});
    const auto range = source.GetRange(1, 3);

    ASSERT_EQ(range.getCountProperty(), 3);
    EXPECT_EQ(range[0], 20);
    EXPECT_EQ(range[1], 30);
    EXPECT_EQ(range[2], 40);
    EXPECT_EQ(source.getCountProperty(), 5);
    EXPECT_EQ(source[1], 20);
    EXPECT_EQ(source[3], 40);
}

TEST(ImmutableListRangeTests, GetRangeAllowsEmptyRangeAtBothBoundaries) {
    const auto source = ImmutableList<int>::Create({10, 20});

    EXPECT_TRUE(source.GetRange(0, 0).getIsEmptyProperty());
    EXPECT_TRUE(source.GetRange(2, 0).getIsEmptyProperty());
}

TEST(ImmutableListRangeTests, GetRangeInvalidRangesThrowArgumentOutOfRangeException) {
    const auto source = ImmutableList<int>::Create({10, 20});

    EXPECT_THROW(source.GetRange(-1, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.GetRange(3, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.GetRange(1, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.GetRange(1, 2), System::ArgumentOutOfRangeException);
}
