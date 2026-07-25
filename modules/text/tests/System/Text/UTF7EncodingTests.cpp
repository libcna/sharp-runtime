// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "System/Text/UTF7Encoding.hpp"

using SharpRuntime::bytecs;
using System::Text::UTF7Encoding;

namespace {
std::string toString(const std::vector<bytecs>& bytes) {
    return {bytes.begin(), bytes.end()};
}

std::vector<bytecs> toBytes(const std::string& text) {
    return {text.begin(), text.end()};
}
} // namespace

TEST(UTF7EncodingTest, EncodesAndDecodesRfc2152ReferenceExamples) {
    const UTF7Encoding encoding;
    const std::string notIdenticalAlpha = std::string("A") + "\xE2\x89\xA2" + "\xCE\x91.";
    const std::string nihongo = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";

    EXPECT_EQ(toString(encoding.GetBytes(notIdenticalAlpha)), "A+ImIDkQ.");
    EXPECT_EQ(encoding.GetString(toBytes("A+ImIDkQ.")), notIdenticalAlpha);
    EXPECT_EQ(toString(encoding.GetBytes(nihongo)), "+ZeVnLIqe-");
    EXPECT_EQ(encoding.GetString(toBytes("+ZeVnLIqe-")), nihongo);
}

TEST(UTF7EncodingTest, HandlesLiteralPlusAndOptionalDirectCharacterSetting) {
    const UTF7Encoding defaultEncoding;
    const UTF7Encoding optionalEncoding(true);
    const std::string value = "plus + and optionals ![]";

    EXPECT_EQ(toString(defaultEncoding.GetBytes(value)), "plus +- and optionals +ACEAWwBd-");
    EXPECT_EQ(toString(optionalEncoding.GetBytes(value)), "plus +- and optionals ![]");
    EXPECT_EQ(defaultEncoding.GetString(defaultEncoding.GetBytes(value)), value);
    EXPECT_EQ(optionalEncoding.GetString(optionalEncoding.GetBytes(value)), value);
}

TEST(UTF7EncodingTest, RoundTripsAstralUnicodeThroughUtf16SurrogatePair) {
    const UTF7Encoding encoding;
    const std::string value = "x\xF0\x9F\x98\x80" "y";

    EXPECT_EQ(toString(encoding.GetBytes(value)), "x+2D3eAA-y");
    EXPECT_EQ(encoding.GetString(encoding.GetBytes(value)), value);
}

TEST(UTF7EncodingTest, TerminatesShiftBeforeLiteralHyphen) {
    const UTF7Encoding encoding(true);
    const std::string value = std::string("Hi Mom -") + "\xE2\x98\xBA" + "-!";

    EXPECT_EQ(toString(encoding.GetBytes(value)), "Hi Mom -+Jjo--!");
    EXPECT_EQ(encoding.GetString(encoding.GetBytes(value)), value);
}

TEST(UTF7EncodingTest, DecoderAcceptsOptionalShiftTerminatorAndPermissiveDirectInput) {
    const UTF7Encoding encoding;

    EXPECT_EQ(encoding.GetString(toBytes("+AKM")), "\xC2\xA3");
    EXPECT_EQ(encoding.GetString(toBytes("Hi Mom -+Jjo--!")),
              std::string("Hi Mom -") + "\xE2\x98\xBA" + "-!");
}

TEST(UTF7EncodingTest, DecoderReplacesMalformedShiftedDataAndNonAsciiBytes) {
    const UTF7Encoding encoding;
    const std::string replacement = "\xEF\xBF\xBD";

    EXPECT_EQ(encoding.GetString(toBytes("+A-")), replacement);
    EXPECT_EQ(encoding.GetString(toBytes("+2AA-")), replacement);
    EXPECT_EQ(encoding.GetString(std::vector<bytecs>{0x80, static_cast<bytecs>('A')}), replacement + "A");
}
