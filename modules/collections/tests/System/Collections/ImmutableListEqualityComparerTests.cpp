// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/IEqualityComparer.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"
#include <vector>

using System::Collections::Generic::IEqualityComparer;
using System::Collections::Immutable::ImmutableList;
using SharpRuntime::intcs;

namespace {

class ParityComparer final : public IEqualityComparer<int> {
public:
    [[nodiscard]] bool Equals(const int& left, const int& right) const override {
        return (left & 1) == (right & 1);
    }

    [[nodiscard]] intcs GetHashCode(const int& value) const override {
        return value & 1;
    }
};

const ParityComparer parityComparer;

} // namespace

TEST(ImmutableListEqualityComparerTest, Remove_UsesCustomEqualityAndPreservesSource) {
    auto source = ImmutableList<int>::Create({1, 2, 4, 3});

    auto result = source.Remove(6, parityComparer);

    EXPECT_EQ(result.getCountProperty(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(source.getCountProperty(), 4);
    EXPECT_EQ(source[1], 2);
}

TEST(ImmutableListEqualityComparerTest, RemoveRange_DefaultEqualityRemovesOnePerInputValue) {
    auto source = ImmutableList<int>::Create({1, 2, 2, 3});

    auto result = source.RemoveRange(std::vector<int>{2, 2, 9});

    EXPECT_EQ(result.getCountProperty(), 2);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 3);
    EXPECT_EQ(source.getCountProperty(), 4);
}

TEST(ImmutableListEqualityComparerTest, RemoveRange_CustomEqualityProcessesDuplicateInputsSequentially) {
    auto source = ImmutableList<int>::Create({1, 2, 4, 3});

    auto result = source.RemoveRange(std::vector<int>{0, 2}, parityComparer);

    EXPECT_EQ(result.getCountProperty(), 2);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 3);
    EXPECT_EQ(source.getCountProperty(), 4);
}

TEST(ImmutableListEqualityComparerTest, Replace_UsesCustomEqualityAndThrowsWhenValueIsAbsent) {
    auto source = ImmutableList<int>::Create({1, 2, 4, 3});

    auto result = source.Replace(6, 8, parityComparer);

    EXPECT_EQ(result[1], 8);
    EXPECT_EQ(result[2], 4);
    EXPECT_EQ(source[1], 2);
    EXPECT_THROW(ImmutableList<int>::Create({1}).Replace(0, 8, parityComparer), System::ArgumentException);
    EXPECT_THROW(source.Replace(9, 8), System::ArgumentException);
}

TEST(ImmutableListEqualityComparerTest, IndexOf_RangeUsesDefaultEqualityAndHonorsBounds) {
    auto source = ImmutableList<int>::Create({1, 4, 2, 3, 2});

    EXPECT_EQ(source.IndexOf(2, 1, 2), 2);
    EXPECT_EQ(source.IndexOf(2, 0, 2), -1);
    EXPECT_EQ(ImmutableList<int>::Empty().IndexOf(1, 0, 0), -1);
    EXPECT_THROW(source.IndexOf(2, -1, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(source.IndexOf(2, 4, 2), System::ArgumentOutOfRangeException);
}

TEST(ImmutableListEqualityComparerTest, IndexOf_RangeUsesCustomEquality) {
    auto source = ImmutableList<int>::Create({1, 4, 2, 3, 6});

    EXPECT_EQ(source.IndexOf(0, 1, 3, parityComparer), 1);
    EXPECT_EQ(source.IndexOf(0, 0, 1, parityComparer), -1);
}

TEST(ImmutableListEqualityComparerTest, LastIndexOf_RangeUsesDefaultEqualityAndHonorsBounds) {
    auto source = ImmutableList<int>::Create({1, 2, 3, 2, 4});

    EXPECT_EQ(source.LastIndexOf(2, 3, 3), 3);
    EXPECT_EQ(source.LastIndexOf(2, 2, 2), 1);
    EXPECT_EQ(source.LastIndexOf(2, 0, 0), -1);
    EXPECT_THROW(source.LastIndexOf(2, 1, 3), System::ArgumentOutOfRangeException);
    EXPECT_THROW(ImmutableList<int>::Empty().LastIndexOf(2, 1, 0), System::ArgumentOutOfRangeException);
}

TEST(ImmutableListEqualityComparerTest, LastIndexOf_RangeUsesCustomEqualityAndEmptyContract) {
    auto source = ImmutableList<int>::Create({1, 2, 4, 3});

    EXPECT_EQ(source.LastIndexOf(6, 3, 3, parityComparer), 2);
    EXPECT_EQ(source.LastIndexOf(6, 0, 1, parityComparer), -1);
    EXPECT_EQ(ImmutableList<int>::Empty().LastIndexOf(0, 0, 0, parityComparer), -1);
}
