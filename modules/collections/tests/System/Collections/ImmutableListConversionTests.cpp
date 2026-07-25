// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <functional>
#include <string>

#include "System/ArgumentNullException.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

using System::Collections::Immutable::ImmutableList;

TEST(ImmutableListConversionTests, ConvertAllReturnsOrderedConvertedCopy) {
    const auto source = ImmutableList<int>::Create({4, 1, 9});
    const auto converted = source.ConvertAll<std::string>(
        [](const int& value) { return "value=" + std::to_string(value); });

    ASSERT_EQ(converted.getCountProperty(), 3);
    EXPECT_EQ(converted[0], "value=4");
    EXPECT_EQ(converted[1], "value=1");
    EXPECT_EQ(converted[2], "value=9");
    EXPECT_EQ(source.getCountProperty(), 3);
    EXPECT_EQ(source[0], 4);
    EXPECT_EQ(source[2], 9);
}

TEST(ImmutableListConversionTests, ConvertAllHandlesEmptySource) {
    const auto source = ImmutableList<int>::Empty();
    const auto converted = source.ConvertAll<std::string>(
        [](const int& value) { return std::to_string(value); });

    EXPECT_TRUE(converted.getIsEmptyProperty());
}

TEST(ImmutableListConversionTests, ConvertAllEmptyConverterThrowsArgumentNullException) {
    const auto source = ImmutableList<int>::Create({1});
    const std::function<std::string(const int&)> converter;

    EXPECT_THROW(source.ConvertAll<std::string>(converter), System::ArgumentNullException);
}
