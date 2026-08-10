// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <vector>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

using System::Collections::Immutable::ImmutableList;

TEST(ImmutableListBuilderTest, ToBuilderMutatesWorkingCopyWithoutChangingSource) {
    const auto source = ImmutableList<int>::Create({1, 2, 4});
    auto builder = source.ToBuilder();

    builder.Insert(2, 3);
    builder[0] = 10;
    builder.Add(5);

    const auto snapshot = builder.ToImmutable();
    EXPECT_EQ(source.getCountProperty(), 3);
    EXPECT_EQ(source[0], 1);
    EXPECT_EQ(source[2], 4);
    EXPECT_EQ(snapshot.getCountProperty(), 5);
    EXPECT_EQ(snapshot[0], 10);
    EXPECT_EQ(snapshot[1], 2);
    EXPECT_EQ(snapshot[2], 3);
    EXPECT_EQ(snapshot[4], 5);
}

TEST(ImmutableListBuilderTest, SnapshotsRemainImmutableAfterFurtherBuilderMutations) {
    auto builder = ImmutableList<int>::CreateBuilder();
    builder.AddRange({1, 2});

    const auto first = builder.ToImmutable();
    builder.SetItem(0, 10);
    builder.Add(3);
    const auto second = builder.ToImmutable();

    EXPECT_EQ(first.getCountProperty(), 2);
    EXPECT_EQ(first[0], 1);
    EXPECT_EQ(first[1], 2);
    EXPECT_EQ(second.getCountProperty(), 3);
    EXPECT_EQ(second[0], 10);
    EXPECT_EQ(second[2], 3);
}

TEST(ImmutableListBuilderTest, RangeMutationsAndQueriesFollowMutableListSemantics) {
    auto builder = ImmutableList<int>::CreateBuilder();
    builder.AddRange({1, 4});
    builder.InsertRange(1, {2, 3});
    EXPECT_TRUE(builder.Contains(3));
    EXPECT_EQ(builder.IndexOf(3), 2);
    EXPECT_TRUE(builder.Remove(2));
    EXPECT_FALSE(builder.Remove(99));
    builder.RemoveRange(1, 2);

    const auto result = builder.ToImmutable();
    ASSERT_EQ(result.getCountProperty(), 1);
    EXPECT_EQ(result[0], 1);
}

TEST(ImmutableListBuilderTest, ValidatesIndexesAndRangesAndClearCreatesEmptySnapshot) {
    auto builder = ImmutableList<int>::CreateBuilder();
    builder.Add(1);

    EXPECT_THROW(builder[-1], System::ArgumentOutOfRangeException);
    EXPECT_THROW(builder.Insert(2, 3), System::ArgumentOutOfRangeException);
    EXPECT_THROW(builder.RemoveAt(1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(builder.RemoveRange(1, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(builder.RemoveRange(0, -1), System::ArgumentOutOfRangeException);

    builder.Clear();
    EXPECT_TRUE(builder.ToImmutable().getIsEmptyProperty());
}
