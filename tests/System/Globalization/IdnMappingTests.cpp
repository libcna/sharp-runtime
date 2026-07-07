// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include <gtest/gtest.h>
#include "System/ArgumentException.hpp"
#include "System/Globalization/IdnMapping.hpp"

using namespace System::Globalization;

// RFC 3492 test vectors + common examples

TEST(IdnMappingTests, AsciiPassthrough) {
    IdnMapping idn;
    EXPECT_EQ(idn.GetAscii("example.com"), "example.com");
}

TEST(IdnMappingTests, AsciiPassthroughUpperLowered) {
    IdnMapping idn;
    EXPECT_EQ(idn.GetAscii("EXAMPLE.COM"), "example.com");
}

TEST(IdnMappingTests, PunycodeEncodeGerman) {
    // München → xn--mnchen-3ya
    IdnMapping idn;
    std::string result = idn.GetAscii("m\xC3\xBCnchen"); // "münchen" in UTF-8
    EXPECT_EQ(result, "xn--mnchen-3ya");
}

TEST(IdnMappingTests, PunycodeEncodeFullDomain) {
    // münchen.de → xn--mnchen-3ya.de
    IdnMapping idn;
    std::string result = idn.GetAscii("m\xC3\xBCnchen.de");
    EXPECT_EQ(result, "xn--mnchen-3ya.de");
}

TEST(IdnMappingTests, PunycodeDecodeMunchen) {
    IdnMapping idn;
    std::string result = idn.GetUnicode("xn--mnchen-3ya");
    EXPECT_EQ(result, "m\xC3\xBCnchen"); // "münchen" in UTF-8
}

TEST(IdnMappingTests, PunycodeDecodeFullDomain) {
    IdnMapping idn;
    std::string result = idn.GetUnicode("xn--mnchen-3ya.de");
    EXPECT_EQ(result, "m\xC3\xBCnchen.de");
}

TEST(IdnMappingTests, PunycodeEncodeChinese) {
    // "中文.com" (Chinese characters)
    IdnMapping idn;
    std::string input = "\xE4\xB8\xAD\xE6\x96\x87.com"; // 中文.com
    std::string result = idn.GetAscii(input);
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.substr(0, 4), "xn--");
}

TEST(IdnMappingTests, PunycodeDecodeChinese) {
    IdnMapping idn;
    // Encode then decode — round trip
    std::string original = "\xE4\xB8\xAD\xE6\x96\x87.com";
    std::string ascii = idn.GetAscii(original);
    std::string decoded = idn.GetUnicode(ascii);
    EXPECT_EQ(decoded, original);
}

TEST(IdnMappingTests, RoundTripArabic) {
    IdnMapping idn;
    // مثال.إختبار — example.test in Arabic
    std::string original = "\xD9\x85\xD8\xAB\xD8\xA7\xD9\x84.\xD8\xA5\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7\xD8\xB1";
    std::string ascii = idn.GetAscii(original);
    std::string decoded = idn.GetUnicode(ascii);
    EXPECT_EQ(decoded, original);
}

TEST(IdnMappingTests, MultipleLabels) {
    IdnMapping idn;
    std::string original = "m\xC3\xBCnchen.m\xC3\xBCnchen.de";
    std::string ascii = idn.GetAscii(original);
    EXPECT_EQ(ascii, "xn--mnchen-3ya.xn--mnchen-3ya.de");
}

TEST(IdnMappingTests, TrailingDot) {
    IdnMapping idn;
    std::string result = idn.GetAscii("example.com.");
    EXPECT_EQ(result, "example.com.");
}

TEST(IdnMappingTests, EmptyThrows) {
    IdnMapping idn;
    EXPECT_THROW(idn.GetAscii(""), System::ArgumentException);
}

TEST(IdnMappingTests, GetHashCode_MatchesForEqualInstances) {
    IdnMapping a, b;
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
    b.setAllowUnassignedProperty(true);
    EXPECT_NE(a.GetHashCode(), b.GetHashCode());
}

TEST(IdnMappingTests, Equality) {
    IdnMapping a, b;
    EXPECT_TRUE(a == b);
    b.setAllowUnassignedProperty(true);
    EXPECT_FALSE(a == b);
}
