// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/IComparer.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

using System::Collections::Generic::IComparer;
using System::Collections::Immutable::ImmutableList;
using SharpRuntime::intcs;

namespace {

class DescendingIntComparer final : public IComparer<int> {
public:
    [[nodiscard]] intcs Compare(const int& left, const int& right) const override {
        if (left == right) {
            return 0;
        }
        return left > right ? -1 : 1;
    }
};

const DescendingIntComparer descendingIntComparer;

} // namespace

TEST(ImmutableListBinarySearchTest, FullList_UsesCustomComparerForFoundAndMissingValues) {
    auto source = ImmutableList<int>::Create({9, 7, 5, 3});

    EXPECT_EQ(source.BinarySearch(5, descendingIntComparer), 2);
    EXPECT_EQ(source.BinarySearch(6, descendingIntComparer), ~2);
    EXPECT_EQ(source.BinarySearch(10, descendingIntComparer), ~0);
    EXPECT_EQ(source.getCountProperty(), 4);
}

TEST(ImmutableListBinarySearchTest, Range_UsesAbsoluteInsertionPoint) {
    auto source = ImmutableList<int>::Create({11, 9, 7, 5, 3, 1});

    EXPECT_EQ(source.BinarySearch(1, 4, 5, descendingIntComparer), 3);
    EXPECT_EQ(source.BinarySearch(1, 4, 6, descendingIntComparer), ~3);
    EXPECT_EQ(source.BinarySearch(6, 0, 0, descendingIntComparer), ~6);
}

TEST(ImmutableListBinarySearchTest, Range_RejectsInvalidBounds) {
    auto source = ImmutableList<int>::Create({9, 7, 5});

    EXPECT_THROW(source.BinarySearch(-1, 1, 7, descendingIntComparer), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.BinarySearch(2, 2, 7, descendingIntComparer), System::ArgumentOutOfRangeException);
}
