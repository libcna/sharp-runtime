// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include "System/Text/StringBuilder.hpp"

using System::Text::StringBuilder;

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

TEST(StringBuilderTests, DefaultCtorIsEmpty) {
    StringBuilder sb;
    EXPECT_EQ(sb.ToString(), "");
    EXPECT_EQ(sb.getLengthProperty(), 0);
    EXPECT_TRUE(sb.Empty());
}

TEST(StringBuilderTests, CtorWithInitialValue) {
    StringBuilder sb(std::string("hello"));
    EXPECT_EQ(sb.ToString(), "hello");
    EXPECT_EQ(sb.getLengthProperty(), 5);
    EXPECT_FALSE(sb.Empty());
}

// ---------------------------------------------------------------------------
// Append (string overloads)
// ---------------------------------------------------------------------------

TEST(StringBuilderTests, AppendString) {
    StringBuilder sb;
    sb.Append(std::string("foo"));
    EXPECT_EQ(sb.ToString(), "foo");
}

TEST(StringBuilderTests, AppendCString) {
    StringBuilder sb;
    sb.Append("bar");
    EXPECT_EQ(sb.ToString(), "bar");
}

TEST(StringBuilderTests, AppendNullptrIsNoop) {
    StringBuilder sb;
    sb.Append("x");
    sb.Append(nullptr);
    EXPECT_EQ(sb.ToString(), "x");
}

TEST(StringBuilderTests, AppendChar) {
    StringBuilder sb;
    sb.Append('A').Append('B').Append('C');
    EXPECT_EQ(sb.ToString(), "ABC");
}

TEST(StringBuilderTests, AppendInt) {
    StringBuilder sb;
    sb.Append(42);
    EXPECT_EQ(sb.ToString(), "42");
}

TEST(StringBuilderTests, AppendNegativeInt) {
    StringBuilder sb;
    sb.Append(-7);
    EXPECT_EQ(sb.ToString(), "-7");
}

TEST(StringBuilderTests, AppendDouble) {
    StringBuilder sb;
    sb.Append(3.14);
    // Just check it starts with "3.14" (implementation uses std::to_string)
    EXPECT_EQ(sb.ToString().substr(0, 4), "3.14");
}

TEST(StringBuilderTests, AppendBoolTrue) {
    StringBuilder sb;
    sb.Append(true);
    EXPECT_EQ(sb.ToString(), "True");
}

TEST(StringBuilderTests, AppendBoolFalse) {
    StringBuilder sb;
    sb.Append(false);
    EXPECT_EQ(sb.ToString(), "False");
}

// ---------------------------------------------------------------------------
// AppendLine
// ---------------------------------------------------------------------------

TEST(StringBuilderTests, AppendLineNoArg) {
    StringBuilder sb;
    sb.AppendLine();
    EXPECT_EQ(sb.ToString(), "\n");
    EXPECT_EQ(sb.getLengthProperty(), 1);
}

TEST(StringBuilderTests, AppendLineWithString) {
    StringBuilder sb;
    sb.AppendLine(std::string("hello"));
    EXPECT_EQ(sb.ToString(), "hello\n");
}

TEST(StringBuilderTests, AppendLineTwice) {
    StringBuilder sb;
    sb.AppendLine(std::string("line1"));
    sb.AppendLine(std::string("line2"));
    EXPECT_EQ(sb.ToString(), "line1\nline2\n");
}

// ---------------------------------------------------------------------------
// Chaining (fluent API)
// ---------------------------------------------------------------------------

TEST(StringBuilderTests, ChainingAppend) {
    StringBuilder sb;
    sb.Append(std::string("Hello")).Append(", ").Append(std::string("world")).Append('!');
    EXPECT_EQ(sb.ToString(), "Hello, world!");
}

TEST(StringBuilderTests, ChainingMixedTypes) {
    StringBuilder sb;
    sb.Append("n=").Append(42).Append(" ok=").Append(true);
    EXPECT_EQ(sb.ToString(), "n=42 ok=True");
}

// ---------------------------------------------------------------------------
// Length and Empty
// ---------------------------------------------------------------------------

TEST(StringBuilderTests, LengthAfterAppend) {
    StringBuilder sb;
    sb.Append("abc");
    EXPECT_EQ(sb.getLengthProperty(), 3);
}

TEST(StringBuilderTests, LengthAccumulates) {
    StringBuilder sb;
    sb.Append("ab").Append("cde");
    EXPECT_EQ(sb.getLengthProperty(), 5);
}

TEST(StringBuilderTests, EmptyAfterDefault) {
    EXPECT_TRUE(StringBuilder().Empty());
}

TEST(StringBuilderTests, NotEmptyAfterAppend) {
    StringBuilder sb;
    sb.Append("x");
    EXPECT_FALSE(sb.Empty());
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST(StringBuilderTests, ClearResetsBuffer) {
    StringBuilder sb(std::string("hello"));
    sb.Clear();
    EXPECT_EQ(sb.ToString(), "");
    EXPECT_EQ(sb.getLengthProperty(), 0);
    EXPECT_TRUE(sb.Empty());
}

TEST(StringBuilderTests, AppendAfterClear) {
    StringBuilder sb(std::string("old"));
    sb.Clear();
    sb.Append("new");
    EXPECT_EQ(sb.ToString(), "new");
}

// ---------------------------------------------------------------------------
// ToString
// ---------------------------------------------------------------------------

TEST(StringBuilderTests, ToStringEmpty) {
    EXPECT_EQ(StringBuilder().ToString(), "");
}

TEST(StringBuilderTests, ToStringMultipleAppends) {
    StringBuilder sb;
    for (int i = 0; i < 5; ++i) sb.Append(std::to_string(i));
    EXPECT_EQ(sb.ToString(), "01234");
}

TEST(StringBuilderTests, ToStringDoesNotMutate) {
    StringBuilder sb(std::string("immutable"));
    std::string s1 = sb.ToString();
    std::string s2 = sb.ToString();
    EXPECT_EQ(s1, s2);
    EXPECT_EQ(sb.getLengthProperty(), 9);
}

// ---------------------------------------------------------------------------
// Large input stress
// ---------------------------------------------------------------------------

TEST(StringBuilderTests, AppendManyTimes) {
    StringBuilder sb;
    for (int i = 0; i < 1000; ++i) sb.Append('x');
    EXPECT_EQ(sb.getLengthProperty(), 1000);
    EXPECT_EQ(sb.ToString(), std::string(1000, 'x'));
}

TEST(StringBuilderTests, AppendLargeString) {
    std::string big(10000, 'a');
    StringBuilder sb;
    sb.Append(big);
    EXPECT_EQ(sb.getLengthProperty(), 10000);
    EXPECT_EQ(sb.ToString(), big);
}
