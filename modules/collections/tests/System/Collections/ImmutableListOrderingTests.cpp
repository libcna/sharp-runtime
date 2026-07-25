// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <functional>

#include "System/ArgumentNullException.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

using System::Collections::Immutable::ImmutableList;

TEST(ImmutableListOrderingTests, SortReturnsOrderedCopyWithoutMutatingSource) {
    const auto source = ImmutableList<int>::Create({4, 1, 3, 2});
    const auto sorted = source.Sort();

    EXPECT_EQ(source[0], 4);
    EXPECT_EQ(source[1], 1);
    ASSERT_EQ(sorted.getCountProperty(), 4);
    EXPECT_EQ(sorted[0], 1);
    EXPECT_EQ(sorted[1], 2);
    EXPECT_EQ(sorted[2], 3);
    EXPECT_EQ(sorted[3], 4);
}

TEST(ImmutableListOrderingTests, ReverseReturnsReorderedCopyWithoutMutatingSource) {
    const auto source = ImmutableList<int>::Create({1, 2, 3, 4});
    const auto reversed = source.Reverse();

    EXPECT_EQ(source[0], 1);
    EXPECT_EQ(source[3], 4);
    ASSERT_EQ(reversed.getCountProperty(), 4);
    EXPECT_EQ(reversed[0], 4);
    EXPECT_EQ(reversed[1], 3);
    EXPECT_EQ(reversed[2], 2);
    EXPECT_EQ(reversed[3], 1);
}

TEST(ImmutableListOrderingTests, SortAndReverseHandleEmptyAndSingletonLists) {
    const auto empty = ImmutableList<int>::Empty();
    const auto single = ImmutableList<int>::Create({42});

    EXPECT_TRUE(empty.Sort().getIsEmptyProperty());
    EXPECT_TRUE(empty.Reverse().getIsEmptyProperty());
    ASSERT_EQ(single.Sort().getCountProperty(), 1);
    EXPECT_EQ(single.Sort()[0], 42);
    ASSERT_EQ(single.Reverse().getCountProperty(), 1);
    EXPECT_EQ(single.Reverse()[0], 42);
}

TEST(ImmutableListOrderingTests, SortWithCustomComparisonReturnsIndependentOrder) {
    const auto source = ImmutableList<int>::Create({4, 1, 3, 2});
    const auto sorted = source.Sort([](const int& left, const int& right) {
        return right - left;
    });

    EXPECT_EQ(source[0], 4);
    EXPECT_EQ(source[1], 1);
    EXPECT_EQ(sorted[0], 4);
    EXPECT_EQ(sorted[1], 3);
    EXPECT_EQ(sorted[2], 2);
    EXPECT_EQ(sorted[3], 1);
}

TEST(ImmutableListOrderingTests, SortWithEmptyComparisonThrowsArgumentNullException) {
    const auto source = ImmutableList<int>::Create({1});
    const std::function<SharpRuntime::intcs(const int&, const int&)> comparison;

    EXPECT_THROW(source.Sort(comparison), System::ArgumentNullException);
}
