// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <functional>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

using System::Collections::Immutable::ImmutableList;

TEST(ImmutableListPredicateTests, FindsElementsAndIndexesWithoutMutatingSource) {
    const auto source = ImmutableList<int>::Create({1, 2, 3, 2, 4});

    EXPECT_TRUE(source.Exists([](const int& value) { return value % 2 == 0; }));
    EXPECT_FALSE(source.Exists([](const int& value) { return value > 10; }));
    EXPECT_EQ(source.Find([](const int& value) { return value % 2 == 0; }), 2);
    EXPECT_EQ(source.Find([](const int& value) { return value > 10; }), 0);
    EXPECT_EQ(source.FindIndex([](const int& value) { return value == 2; }), 1);
    EXPECT_EQ(source.FindIndex([](const int& value) { return value > 10; }), -1);
    EXPECT_EQ(source.FindLast([](const int& value) { return value % 2 == 0; }), 4);
    EXPECT_EQ(source.FindLast([](const int& value) { return value > 10; }), 0);
    EXPECT_EQ(source.FindLastIndex([](const int& value) { return value == 2; }), 3);
    EXPECT_EQ(source.FindLastIndex([](const int& value) { return value > 10; }), -1);

    const auto filtered = source.FindAll([](const int& value) { return value % 2 == 0; });
    ASSERT_EQ(filtered.getCountProperty(), 3);
    EXPECT_EQ(filtered[0], 2);
    EXPECT_EQ(filtered[1], 2);
    EXPECT_EQ(filtered[2], 4);
    EXPECT_EQ(source.getCountProperty(), 5);
    EXPECT_EQ(source[0], 1);
    EXPECT_EQ(source[4], 4);
}

TEST(ImmutableListPredicateTests, ForEachPreservesOrderAndTrueForAllUsesVacuousTruth) {
    const auto source = ImmutableList<int>::Create({3, 1, 4});
    std::vector<int> visited;

    source.ForEach([&visited](const int& value) { visited.push_back(value); });

    EXPECT_EQ(visited, (std::vector<int>{3, 1, 4}));
    EXPECT_TRUE(source.TrueForAll([](const int& value) { return value > 0; }));
    EXPECT_FALSE(source.TrueForAll([](const int& value) { return value < 4; }));
    EXPECT_TRUE(ImmutableList<int>::Empty().TrueForAll(
        [](const int&) { return false; }));
}

TEST(ImmutableListPredicateTests, EmptyListHasDotNetDefaultAndIndexSemantics) {
    const auto empty = ImmutableList<int>::Empty();
    int calls = 0;

    empty.ForEach([&calls](const int&) { ++calls; });

    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(empty.Exists([](const int&) { return true; }));
    EXPECT_EQ(empty.Find([](const int&) { return true; }), 0);
    EXPECT_TRUE(empty.FindAll([](const int&) { return true; }).getIsEmptyProperty());
    EXPECT_EQ(empty.FindIndex([](const int&) { return true; }), -1);
    EXPECT_EQ(empty.FindLast([](const int&) { return true; }), 0);
    EXPECT_EQ(empty.FindLastIndex([](const int&) { return true; }), -1);
}

TEST(ImmutableListPredicateTests, EmptyDelegatesThrowArgumentNullException) {
    const auto source = ImmutableList<int>::Create({1});
    const std::function<bool(const int&)> noMatch;
    const std::function<void(const int&)> noAction;

    EXPECT_THROW(source.ForEach(noAction), System::ArgumentNullException);
    EXPECT_THROW(source.Exists(noMatch), System::ArgumentNullException);
    EXPECT_THROW(source.Find(noMatch), System::ArgumentNullException);
    EXPECT_THROW(source.FindAll(noMatch), System::ArgumentNullException);
    EXPECT_THROW(source.FindIndex(noMatch), System::ArgumentNullException);
    EXPECT_THROW(source.FindLast(noMatch), System::ArgumentNullException);
    EXPECT_THROW(source.FindLastIndex(noMatch), System::ArgumentNullException);
    EXPECT_THROW(source.TrueForAll(noMatch), System::ArgumentNullException);
}
