// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ValueTuple.hpp"

using System::ValueTuple1;
using System::ValueTuple2;
using System::ValueTuple3;
using System::ValueTuple4;

TEST(ValueTuple1Test, BasicConstruct) {
    ValueTuple1<int> t(42);
    EXPECT_EQ(t.Item1, 42);
}

TEST(ValueTuple1Test, Equality) {
    ValueTuple1<int> a(1), b(1), c(2);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}

TEST(ValueTuple1Test, CompareTo) {
    ValueTuple1<int> a(1), b(2);
    EXPECT_LT(a.CompareTo(b), 0);
    EXPECT_EQ(a.CompareTo(a), 0);
    EXPECT_GT(b.CompareTo(a), 0);
}

TEST(ValueTuple2Test, BasicConstruct) {
    ValueTuple2<int, std::string> t(1, "hello");
    EXPECT_EQ(t.Item1, 1);
    EXPECT_EQ(t.Item2, "hello");
}

TEST(ValueTuple2Test, Equality) {
    ValueTuple2<int, int> a(1, 2), b(1, 2), c(1, 3);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}

TEST(ValueTuple2Test, GetHashCode) {
    ValueTuple2<int, int> a(1, 2), b(1, 2);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(ValueTuple3Test, BasicConstruct) {
    ValueTuple3<int, int, int> t(1, 2, 3);
    EXPECT_EQ(t.Item1, 1);
    EXPECT_EQ(t.Item2, 2);
    EXPECT_EQ(t.Item3, 3);
}

TEST(ValueTuple4Test, BasicConstruct) {
    ValueTuple4<int, int, int, int> t(10, 20, 30, 40);
    EXPECT_EQ(t.Item4, 40);
}

TEST(ValueTuple2Test, ToString) {
    ValueTuple2<int, int> t(1, 2);
    std::string s = t.ToString();
    EXPECT_NE(s.find("1"), std::string::npos);
}
