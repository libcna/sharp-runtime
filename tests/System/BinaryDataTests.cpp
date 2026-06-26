// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/BinaryData.hpp"

using System::BinaryData;

TEST(BinaryDataTests, Equals_SameContent_True) {
    auto a = BinaryData::FromString("abc");
    auto b = BinaryData::FromString("abc");
    EXPECT_TRUE(a.Equals(b));
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(BinaryDataTests, Equals_DifferentContent_False) {
    auto a = BinaryData::FromString("abc");
    auto b = BinaryData::FromString("xyz");
    EXPECT_FALSE(a.Equals(b));
    EXPECT_TRUE(a != b);
}

TEST(BinaryDataTests, Equals_DifferentMediaType_False) {
    std::vector<uint8_t> v{1, 2, 3};
    auto a = BinaryData::FromBytes(v, "text/plain");
    auto b = BinaryData::FromBytes(v, "application/octet-stream");
    EXPECT_FALSE(a.Equals(b));
}

TEST(BinaryDataTests, GetHashCode_SameContent_SameHash) {
    auto a = BinaryData::FromString("hello");
    auto b = BinaryData::FromString("hello");
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(BinaryDataTests, GetHashCode_NonNegative) {
    auto bd = BinaryData::FromString("test");
    EXPECT_GE(bd.GetHashCode(), 0);
}

TEST(BinaryDataTests, FromBytes_VectorWithMediaType_Stored) {
    std::vector<uint8_t> v{1, 2, 3};
    auto bd = BinaryData::FromBytes(v, "image/png");
    EXPECT_EQ(bd.getMediaTypeProperty(), "image/png");
    EXPECT_EQ(bd.getLengthProperty(), 3);
}
