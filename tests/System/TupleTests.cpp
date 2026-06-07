// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <string>
#include "System/Tuple.hpp"

using System::Tuple2;
using System::Tuple3;
using System::Tuple4;

// ---------------------------------------------------------------------------
// Tuple2
// ---------------------------------------------------------------------------

TEST(TupleTests, Tuple2_ConstructAndAccessItems) {
    Tuple2<int, std::string> t(42, "hello");
    EXPECT_EQ(t.Item1, 42);
    EXPECT_EQ(t.Item2, "hello");
}

TEST(TupleTests, Tuple2_IntInt_Items) {
    Tuple2<int, int> t(10, 20);
    EXPECT_EQ(t.Item1, 10);
    EXPECT_EQ(t.Item2, 20);
}

TEST(TupleTests, Tuple2_EqualityTrue) {
    Tuple2<int, int> a(1, 2);
    Tuple2<int, int> b(1, 2);
    EXPECT_TRUE(a == b);
}

TEST(TupleTests, Tuple2_EqualityFalse_DifferentItem1) {
    Tuple2<int, int> a(1, 2);
    Tuple2<int, int> b(9, 2);
    EXPECT_FALSE(a == b);
}

TEST(TupleTests, Tuple2_EqualityFalse_DifferentItem2) {
    Tuple2<int, int> a(1, 2);
    Tuple2<int, int> b(1, 9);
    EXPECT_FALSE(a == b);
}

TEST(TupleTests, Tuple2_InequalityTrue) {
    Tuple2<int, int> a(1, 2);
    Tuple2<int, int> b(3, 4);
    EXPECT_TRUE(a != b);
}

TEST(TupleTests, Tuple2_InequalityFalse_SameValues) {
    Tuple2<int, int> a(5, 6);
    Tuple2<int, int> b(5, 6);
    EXPECT_FALSE(a != b);
}

TEST(TupleTests, Tuple2_StringString) {
    Tuple2<std::string, std::string> t("key", "value");
    EXPECT_EQ(t.Item1, "key");
    EXPECT_EQ(t.Item2, "value");
}

TEST(TupleTests, Tuple2_DoubleDouble_Equality) {
    Tuple2<double, double> a(1.5, 2.5);
    Tuple2<double, double> b(1.5, 2.5);
    EXPECT_TRUE(a == b);
}

TEST(TupleTests, Tuple2_BoolInt) {
    Tuple2<bool, int> t(true, 99);
    EXPECT_TRUE(t.Item1);
    EXPECT_EQ(t.Item2, 99);
}

// ---------------------------------------------------------------------------
// Tuple3
// ---------------------------------------------------------------------------

TEST(TupleTests, Tuple3_ConstructAndAccessItems) {
    Tuple3<int, std::string, double> t(1, "abc", 3.14);
    EXPECT_EQ(t.Item1, 1);
    EXPECT_EQ(t.Item2, "abc");
    EXPECT_DOUBLE_EQ(t.Item3, 3.14);
}

TEST(TupleTests, Tuple3_IntIntInt_Items) {
    Tuple3<int, int, int> t(10, 20, 30);
    EXPECT_EQ(t.Item1, 10);
    EXPECT_EQ(t.Item2, 20);
    EXPECT_EQ(t.Item3, 30);
}

TEST(TupleTests, Tuple3_EqualityTrue) {
    Tuple3<int, int, int> a(1, 2, 3);
    Tuple3<int, int, int> b(1, 2, 3);
    EXPECT_TRUE(a == b);
}

TEST(TupleTests, Tuple3_EqualityFalse_DifferentItem3) {
    Tuple3<int, int, int> a(1, 2, 3);
    Tuple3<int, int, int> b(1, 2, 9);
    EXPECT_FALSE(a == b);
}

TEST(TupleTests, Tuple3_InequalityTrue) {
    Tuple3<int, int, int> a(1, 2, 3);
    Tuple3<int, int, int> b(4, 5, 6);
    EXPECT_TRUE(a != b);
}

TEST(TupleTests, Tuple3_InequalityFalse_SameValues) {
    Tuple3<int, int, int> a(7, 8, 9);
    Tuple3<int, int, int> b(7, 8, 9);
    EXPECT_FALSE(a != b);
}

TEST(TupleTests, Tuple3_StringIntBool) {
    Tuple3<std::string, int, bool> t("x", 42, false);
    EXPECT_EQ(t.Item1, "x");
    EXPECT_EQ(t.Item2, 42);
    EXPECT_FALSE(t.Item3);
}

// ---------------------------------------------------------------------------
// Tuple4
// ---------------------------------------------------------------------------

TEST(TupleTests, Tuple4_ConstructAndAccessItems) {
    Tuple4<int, int, int, int> t(1, 2, 3, 4);
    EXPECT_EQ(t.Item1, 1);
    EXPECT_EQ(t.Item2, 2);
    EXPECT_EQ(t.Item3, 3);
    EXPECT_EQ(t.Item4, 4);
}

TEST(TupleTests, Tuple4_MixedTypes) {
    Tuple4<std::string, int, double, bool> t("hello", 7, 2.5, true);
    EXPECT_EQ(t.Item1, "hello");
    EXPECT_EQ(t.Item2, 7);
    EXPECT_DOUBLE_EQ(t.Item3, 2.5);
    EXPECT_TRUE(t.Item4);
}

TEST(TupleTests, Tuple4_AllZero) {
    Tuple4<int, int, int, int> t(0, 0, 0, 0);
    EXPECT_EQ(t.Item1, 0);
    EXPECT_EQ(t.Item2, 0);
    EXPECT_EQ(t.Item3, 0);
    EXPECT_EQ(t.Item4, 0);
}
