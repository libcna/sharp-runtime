// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentException.hpp"
#include "System/Collections/Generic/IEqualityComparer.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

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
