// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/BitArray.hpp"

using System::Collections::BitArray;

TEST(BitArrayTest, ConstructWithLength) {
    BitArray ba(8);
    EXPECT_EQ(ba.getLengthProperty(), 8);
    EXPECT_FALSE(ba.Get(0));
}

TEST(BitArrayTest, ConstructWithDefault) {
    BitArray ba(4, true);
    EXPECT_TRUE(ba.Get(0));
    EXPECT_TRUE(ba.HasAllSet());
}

TEST(BitArrayTest, SetAndGet) {
    BitArray ba(8);
    ba.Set(3, true);
    EXPECT_TRUE(ba.Get(3));
    EXPECT_FALSE(ba.Get(2));
}

TEST(BitArrayTest, SetAll) {
    BitArray ba(5);
    ba.SetAll(true);
    EXPECT_TRUE(ba.HasAllSet());
    ba.SetAll(false);
    EXPECT_FALSE(ba.HasAnySet());
}

TEST(BitArrayTest, And) {
    BitArray a(4, true), b(4);
    b.Set(1, true);
    a.And(b);
    EXPECT_FALSE(a.Get(0));
    EXPECT_TRUE(a.Get(1));
}

TEST(BitArrayTest, Or) {
    BitArray a(4), b(4);
    b.Set(2, true);
    a.Or(b);
    EXPECT_TRUE(a.Get(2));
}

TEST(BitArrayTest, Xor) {
    BitArray a(4, true), b(4, true);
    a.Xor(b);
    EXPECT_FALSE(a.HasAnySet());
}

TEST(BitArrayTest, Not) {
    BitArray ba(4);
    ba.Not();
    EXPECT_TRUE(ba.HasAllSet());
}

TEST(BitArrayTest, PopCount) {
    BitArray ba(8);
    ba.Set(0, true);
    ba.Set(3, true);
    ba.Set(7, true);
    EXPECT_EQ(ba.PopCount(), 3);
}

TEST(BitArrayTest, Clone) {
    BitArray a(4);
    a.Set(1, true);
    BitArray b = a.Clone();
    b.Set(2, true);
    EXPECT_FALSE(a.Get(2));
    EXPECT_TRUE(b.Get(2));
}

TEST(BitArrayTest, LeftShift) {
    BitArray ba(4);
    ba.Set(0, true);
    ba.LeftShift(1);
    EXPECT_FALSE(ba.Get(0));
    EXPECT_TRUE(ba.Get(1));
}

TEST(BitArrayTest, RightShift) {
    BitArray ba(4);
    ba.Set(3, true);
    ba.RightShift(1);
    EXPECT_TRUE(ba.Get(2));
    EXPECT_FALSE(ba.Get(3));
}
