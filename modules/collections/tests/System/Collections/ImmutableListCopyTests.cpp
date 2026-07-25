// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

using System::Collections::Immutable::ImmutableList;

TEST(ImmutableListCopyTests, CopyToCopiesEntireListWithoutResizingDestination) {
    const auto source = ImmutableList<int>::Create({10, 20, 30});
    std::vector<int> destination{0, 0, 0};

    source.CopyTo(destination);

    EXPECT_EQ(destination, (std::vector<int>{10, 20, 30}));
    EXPECT_EQ(source.getCountProperty(), 3);
    EXPECT_EQ(source[0], 10);
}

TEST(ImmutableListCopyTests, CopyToWithDestinationOffsetPreservesSurroundingElements) {
    const auto source = ImmutableList<int>::Create({10, 20, 30});
    std::vector<int> destination{-1, -1, -1, -1, -1};

    source.CopyTo(destination, 1);

    EXPECT_EQ(destination, (std::vector<int>{-1, 10, 20, 30, -1}));
}

TEST(ImmutableListCopyTests, CopyToRangeCopiesOnlyRequestedSourceRange) {
    const auto source = ImmutableList<int>::Create({10, 20, 30, 40, 50});
    std::vector<int> destination{-1, -1, -1, -1, -1, -1};

    source.CopyTo(1, destination, 2, 3);

    EXPECT_EQ(destination, (std::vector<int>{-1, -1, 20, 30, 40, -1}));
}

TEST(ImmutableListCopyTests, CopyToRangeAllowsEmptyRangeAtEndOfSourceAndDestination) {
    const auto source = ImmutableList<int>::Create({10, 20});
    std::vector<int> destination{7, 8};

    source.CopyTo(2, destination, 2, 0);

    EXPECT_EQ(destination, (std::vector<int>{7, 8}));
}

TEST(ImmutableListCopyTests, CopyToValidatesSourceRangeAndDestinationCapacity) {
    const auto source = ImmutableList<int>::Create({10, 20, 30});
    std::vector<int> destination(3);
    std::vector<int> tooSmall(2);

    EXPECT_THROW(source.CopyTo(destination, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.CopyTo(-1, destination, 0, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.CopyTo(1, destination, 0, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.CopyTo(2, destination, 0, 2), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.CopyTo(0, destination, -1, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.CopyTo(0, tooSmall, 0, 3), System::ArgumentException);
    EXPECT_THROW(source.CopyTo(destination, 1), System::ArgumentException);
}
