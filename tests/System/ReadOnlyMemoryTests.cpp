// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ReadOnlyMemory.hpp"

using System::ReadOnlyMemory;

TEST(ReadOnlyMemoryTest, DefaultCtor_IsEmpty) {
    ReadOnlyMemory<int> m;
    EXPECT_TRUE(m.getIsEmptyProperty());
    EXPECT_EQ(m.getLengthProperty(), 0);
}

TEST(ReadOnlyMemoryTest, VectorCtor) {
    std::vector<int> v = {1, 2, 3};
    ReadOnlyMemory<int> m(v);
    EXPECT_EQ(m.getLengthProperty(), 3);
    EXPECT_FALSE(m.getIsEmptyProperty());
}

TEST(ReadOnlyMemoryTest, PtrLengthCtor) {
    int data[] = {10, 20, 30};
    ReadOnlyMemory<int> m(data, 3);
    EXPECT_EQ(m.getLengthProperty(), 3);
    EXPECT_EQ(m[0], 10);
    EXPECT_EQ(m[2], 30);
}

TEST(ReadOnlyMemoryTest, IndexAccess) {
    std::vector<int> v = {5, 6, 7};
    ReadOnlyMemory<int> m(v);
    EXPECT_EQ(m[0], 5);
    EXPECT_EQ(m[1], 6);
    EXPECT_EQ(m[2], 7);
}

TEST(ReadOnlyMemoryTest, IndexOutOfRange_Throws) {
    std::vector<int> v = {1, 2};
    ReadOnlyMemory<int> m(v);
    EXPECT_THROW(m[2], std::out_of_range);
    EXPECT_THROW(m[-1], std::out_of_range);
}

TEST(ReadOnlyMemoryTest, SliceStartLength) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    ReadOnlyMemory<int> m(v);
    auto s = m.Slice(1, 3);
    EXPECT_EQ(s.getLengthProperty(), 3);
    EXPECT_EQ(s[0], 2);
    EXPECT_EQ(s[2], 4);
}

TEST(ReadOnlyMemoryTest, SliceStart) {
    std::vector<int> v = {1, 2, 3, 4};
    ReadOnlyMemory<int> m(v);
    auto s = m.Slice(2);
    EXPECT_EQ(s.getLengthProperty(), 2);
    EXPECT_EQ(s[0], 3);
}

TEST(ReadOnlyMemoryTest, SliceOutOfRange_Throws) {
    std::vector<int> v = {1, 2, 3};
    ReadOnlyMemory<int> m(v);
    EXPECT_THROW(m.Slice(1, 5), std::out_of_range);
}

TEST(ReadOnlyMemoryTest, ToArray) {
    std::vector<int> v = {10, 20, 30};
    ReadOnlyMemory<int> m(v);
    auto arr = m.ToArray();
    EXPECT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr[1], 20);
}

TEST(ReadOnlyMemoryTest, GetEmptyProperty) {
    auto e = ReadOnlyMemory<int>::getEmptyProperty();
    EXPECT_TRUE(e.getIsEmptyProperty());
    EXPECT_EQ(e.getLengthProperty(), 0);
}

TEST(ReadOnlyMemoryTest, GetSpanProperty) {
    std::vector<int> v = {1, 2, 3};
    ReadOnlyMemory<int> m(v);
    auto sp = m.getSpanProperty();
    EXPECT_EQ(sp.getLengthProperty(), 3);
    EXPECT_EQ(sp[0], 1);
}

TEST(ReadOnlyMemoryTest, Equals_SameRegion) {
    std::vector<int> v = {1, 2, 3};
    ReadOnlyMemory<int> a(v);
    ReadOnlyMemory<int> b(v);
    EXPECT_TRUE(a.Equals(b));
    EXPECT_TRUE(a == b);
}

TEST(ReadOnlyMemoryTest, Equals_DifferentRegion) {
    std::vector<int> v1 = {1, 2, 3};
    std::vector<int> v2 = {1, 2, 3};
    ReadOnlyMemory<int> a(v1);
    ReadOnlyMemory<int> b(v2);
    EXPECT_FALSE(a.Equals(b));
    EXPECT_TRUE(a != b);
}

TEST(ReadOnlyMemoryTest, ToString_ContainsLength) {
    std::vector<int> v = {1, 2, 3};
    ReadOnlyMemory<int> m(v);
    EXPECT_NE(m.ToString().find("3"), std::string::npos);
}
