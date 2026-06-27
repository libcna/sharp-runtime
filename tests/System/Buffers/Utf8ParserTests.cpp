// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/Text/Utf8Parser.hpp"

using System::Buffers::Text::Utf8Parser;
using System::ReadOnlySpan;

static ReadOnlySpan<uint8_t> span(const char* s) {
    return ReadOnlySpan<uint8_t>(reinterpret_cast<const uint8_t*>(s), static_cast<int>(strlen(s)));
}

TEST(Utf8ParserTest, ParseBoolTrue) {
    bool v = false; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("True"), v, n));
    EXPECT_TRUE(v);
    EXPECT_EQ(n, 4);
}

TEST(Utf8ParserTest, ParseBoolFalse) {
    bool v = true; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("False"), v, n));
    EXPECT_FALSE(v);
    EXPECT_EQ(n, 5);
}

TEST(Utf8ParserTest, ParseBoolCaseInsensitive) {
    bool v = false; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("true"), v, n));
    EXPECT_TRUE(v);
}

TEST(Utf8ParserTest, ParseBoolInvalid) {
    bool v = false; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("yes"), v, n));
    EXPECT_EQ(n, 0);
}

TEST(Utf8ParserTest, ParseInt32Positive) {
    int32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("12345"), v, n));
    EXPECT_EQ(v, 12345);
    EXPECT_EQ(n, 5);
}

TEST(Utf8ParserTest, ParseInt32Negative) {
    int32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("-42"), v, n));
    EXPECT_EQ(v, -42);
    EXPECT_EQ(n, 3);
}

TEST(Utf8ParserTest, ParseInt32Zero) {
    int32_t v = 1; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("0"), v, n));
    EXPECT_EQ(v, 0);
}

TEST(Utf8ParserTest, ParseUInt64Large) {
    uint64_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("18446744073709551615"), v, n));
    EXPECT_EQ(v, UINT64_MAX);
}

TEST(Utf8ParserTest, ParseUInt8) {
    uint8_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("255"), v, n));
    EXPECT_EQ(v, 255);
}

TEST(Utf8ParserTest, ParseInvalidNotDigit) {
    int32_t v = 0; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("abc"), v, n));
}
