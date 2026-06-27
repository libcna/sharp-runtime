// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/Text/Utf8Formatter.hpp"

using System::Buffers::Text::Utf8Formatter;
using System::Span;

static std::string spanToString(const uint8_t* p, int len) {
    return std::string(reinterpret_cast<const char*>(p), static_cast<size_t>(len));
}

TEST(Utf8FormatterTest, FormatBoolTrue) {
    uint8_t buf[10] = {};
    Span<uint8_t> span(buf, 10);
    int written = 0;
    EXPECT_TRUE(Utf8Formatter::TryFormat(true, span, written));
    EXPECT_EQ(spanToString(buf, written), "True");
}

TEST(Utf8FormatterTest, FormatBoolFalse) {
    uint8_t buf[10] = {};
    Span<uint8_t> span(buf, 10);
    int written = 0;
    EXPECT_TRUE(Utf8Formatter::TryFormat(false, span, written));
    EXPECT_EQ(spanToString(buf, written), "False");
}

TEST(Utf8FormatterTest, FormatBoolLowercase) {
    uint8_t buf[10] = {};
    Span<uint8_t> span(buf, 10);
    int written = 0;
    System::Buffers::StandardFormat fmt('l');
    EXPECT_TRUE(Utf8Formatter::TryFormat(true, span, written, fmt));
    EXPECT_EQ(spanToString(buf, written), "true");
}

TEST(Utf8FormatterTest, FormatBoolDestinationTooSmall) {
    uint8_t buf[2] = {};
    Span<uint8_t> span(buf, 2);
    int written = 0;
    EXPECT_FALSE(Utf8Formatter::TryFormat(true, span, written));
    EXPECT_EQ(written, 0);
}

TEST(Utf8FormatterTest, FormatInt32Zero) {
    uint8_t buf[10] = {};
    Span<uint8_t> span(buf, 10);
    int written = 0;
    EXPECT_TRUE(Utf8Formatter::TryFormat(int32_t(0), span, written));
    EXPECT_EQ(spanToString(buf, written), "0");
}

TEST(Utf8FormatterTest, FormatInt32Positive) {
    uint8_t buf[20] = {};
    Span<uint8_t> span(buf, 20);
    int written = 0;
    EXPECT_TRUE(Utf8Formatter::TryFormat(int32_t(12345), span, written));
    EXPECT_EQ(spanToString(buf, written), "12345");
}

TEST(Utf8FormatterTest, FormatInt32Negative) {
    uint8_t buf[20] = {};
    Span<uint8_t> span(buf, 20);
    int written = 0;
    EXPECT_TRUE(Utf8Formatter::TryFormat(int32_t(-42), span, written));
    EXPECT_EQ(spanToString(buf, written), "-42");
}

TEST(Utf8FormatterTest, FormatUInt64Large) {
    uint8_t buf[25] = {};
    Span<uint8_t> span(buf, 25);
    int written = 0;
    EXPECT_TRUE(Utf8Formatter::TryFormat(uint64_t(18446744073709551615ULL), span, written));
    EXPECT_EQ(spanToString(buf, written), "18446744073709551615");
}

TEST(Utf8FormatterTest, FormatUInt8) {
    uint8_t buf[10] = {};
    Span<uint8_t> span(buf, 10);
    int written = 0;
    EXPECT_TRUE(Utf8Formatter::TryFormat(uint8_t(255), span, written));
    EXPECT_EQ(spanToString(buf, written), "255");
}

TEST(Utf8FormatterTest, FormatDestinationTooSmallInt) {
    uint8_t buf[2] = {};
    Span<uint8_t> span(buf, 2);
    int written = 0;
    EXPECT_FALSE(Utf8Formatter::TryFormat(int32_t(12345), span, written));
    EXPECT_EQ(written, 0);
}
