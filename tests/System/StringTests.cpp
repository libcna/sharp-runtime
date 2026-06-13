// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for System::String static helpers.
#include <gtest/gtest.h>
#include "System/String.hpp"

using System::String;

// --- IsNullOrWhiteSpace ---

TEST(StringTests, IsNullOrWhiteSpace_Empty) {
    EXPECT_TRUE(String::IsNullOrWhiteSpace(""));
}
TEST(StringTests, IsNullOrWhiteSpace_Spaces) {
    EXPECT_TRUE(String::IsNullOrWhiteSpace("   "));
}
TEST(StringTests, IsNullOrWhiteSpace_Tabs) {
    EXPECT_TRUE(String::IsNullOrWhiteSpace("\t\n\r"));
}
TEST(StringTests, IsNullOrWhiteSpace_NotWhitespace) {
    EXPECT_FALSE(String::IsNullOrWhiteSpace("hello"));
}
TEST(StringTests, IsNullOrWhiteSpace_LeadingSpace) {
    EXPECT_FALSE(String::IsNullOrWhiteSpace("  x"));
}

// --- EndsWith ---

TEST(StringTests, EndsWith_True) {
    EXPECT_TRUE(String::EndsWith("hello world", "world"));
}
TEST(StringTests, EndsWith_False) {
    EXPECT_FALSE(String::EndsWith("hello world", "hello"));
}
TEST(StringTests, EndsWith_EmptySuffix) {
    EXPECT_TRUE(String::EndsWith("hello", ""));
}
TEST(StringTests, EndsWith_SuffixLonger) {
    EXPECT_FALSE(String::EndsWith("hi", "hello"));
}
TEST(StringTests, EndsWith_ExactMatch) {
    EXPECT_TRUE(String::EndsWith("abc", "abc"));
}

// --- Contains ---

TEST(StringTests, Contains_True) {
    EXPECT_TRUE(String::Contains("hello world", "world"));
}
TEST(StringTests, Contains_False) {
    EXPECT_FALSE(String::Contains("hello", "xyz"));
}
TEST(StringTests, Contains_EmptySubstr) {
    EXPECT_TRUE(String::Contains("hello", ""));
}

// --- Replace (string) ---

TEST(StringTests, Replace_String_Single) {
    EXPECT_EQ(String::Replace("hello world", "world", "there"), "hello there");
}
TEST(StringTests, Replace_String_Multiple) {
    EXPECT_EQ(String::Replace("aaa", "a", "b"), "bbb");
}
TEST(StringTests, Replace_String_NoMatch) {
    EXPECT_EQ(String::Replace("hello", "xyz", "abc"), "hello");
}
TEST(StringTests, Replace_String_EmptyOld) {
    EXPECT_EQ(String::Replace("hello", "", "x"), "hello");
}

// --- Replace (char) ---

TEST(StringTests, Replace_Char) {
    EXPECT_EQ(String::Replace("hello", 'l', 'r'), "herro");
}
TEST(StringTests, Replace_Char_NoMatch) {
    EXPECT_EQ(String::Replace("hello", 'z', 'x'), "hello");
}

// --- Substring ---

TEST(StringTests, Substring_FromIndex) {
    EXPECT_EQ(String::Substring("hello world", 6), "world");
}
TEST(StringTests, Substring_FromZero) {
    EXPECT_EQ(String::Substring("hello", 0), "hello");
}
TEST(StringTests, Substring_WithLength) {
    EXPECT_EQ(String::Substring("hello world", 0, 5), "hello");
}
TEST(StringTests, Substring_MiddleWithLength) {
    EXPECT_EQ(String::Substring("hello world", 6, 5), "world");
}

// --- Trim ---

TEST(StringTests, Trim_Both) {
    EXPECT_EQ(String::Trim("  hello  "), "hello");
}
TEST(StringTests, Trim_None) {
    EXPECT_EQ(String::Trim("hello"), "hello");
}
TEST(StringTests, Trim_AllWhitespace) {
    EXPECT_EQ(String::Trim("   "), "");
}
TEST(StringTests, TrimStart) {
    EXPECT_EQ(String::TrimStart("  hello  "), "hello  ");
}
TEST(StringTests, TrimEnd) {
    EXPECT_EQ(String::TrimEnd("  hello  "), "  hello");
}

// --- Concat ---

TEST(StringTests, Concat_Two) {
    EXPECT_EQ(String::Concat("foo", "bar"), "foobar");
}
TEST(StringTests, Concat_Three) {
    EXPECT_EQ(String::Concat("a", "b", "c"), "abc");
}
TEST(StringTests, Concat_Four) {
    EXPECT_EQ(String::Concat("a", "b", "c", "d"), "abcd");
}
TEST(StringTests, Concat_Vector) {
    EXPECT_EQ(String::Concat(std::vector<std::string>{"x", "y", "z"}), "xyz");
}
TEST(StringTests, Concat_VectorEmpty) {
    EXPECT_EQ(String::Concat(std::vector<std::string>{}), "");
}

// --- Join ---

TEST(StringTests, Join_Comma) {
    EXPECT_EQ(String::Join(", ", {"a", "b", "c"}), "a, b, c");
}
TEST(StringTests, Join_Empty) {
    EXPECT_EQ(String::Join(",", {}), "");
}
TEST(StringTests, Join_Single) {
    EXPECT_EQ(String::Join(",", {"only"}), "only");
}
TEST(StringTests, Join_EmptySeparator) {
    EXPECT_EQ(String::Join("", {"a", "b", "c"}), "abc");
}

// --- ToUpper / ToLower ---

TEST(StringTests, ToUpper_Basic) {
    EXPECT_EQ(String::ToUpper("hello"), "HELLO");
}
TEST(StringTests, ToUpper_AlreadyUpper) {
    EXPECT_EQ(String::ToUpper("WORLD"), "WORLD");
}
TEST(StringTests, ToLower_Basic) {
    EXPECT_EQ(String::ToLower("HELLO"), "hello");
}
TEST(StringTests, ToLower_Mixed) {
    EXPECT_EQ(String::ToLower("HeLLo"), "hello");
}

// --- IndexOf ---

TEST(StringTests, IndexOf_String_Found) {
    EXPECT_EQ(String::IndexOf("hello world", "world"), 6);
}
TEST(StringTests, IndexOf_String_NotFound) {
    EXPECT_EQ(String::IndexOf("hello", "xyz"), -1);
}
TEST(StringTests, IndexOf_Char_Found) {
    EXPECT_EQ(String::IndexOf("hello", 'l'), 2);
}
TEST(StringTests, IndexOf_Char_NotFound) {
    EXPECT_EQ(String::IndexOf("hello", 'z'), -1);
}

// --- LastIndexOf ---

TEST(StringTests, LastIndexOf_String_Found) {
    EXPECT_EQ(String::LastIndexOf("abcabc", "bc"), 4);
}
TEST(StringTests, LastIndexOf_String_NotFound) {
    EXPECT_EQ(String::LastIndexOf("hello", "xyz"), -1);
}
TEST(StringTests, LastIndexOf_Char_Found) {
    EXPECT_EQ(String::LastIndexOf("hello", 'l'), 3);
}
TEST(StringTests, LastIndexOf_Char_NotFound) {
    EXPECT_EQ(String::LastIndexOf("hello", 'z'), -1);
}

// --- PadLeft ---

TEST(StringTests, PadLeft_WithSpaces) {
    EXPECT_EQ(String::PadLeft("42", 5), "   42");
}
TEST(StringTests, PadLeft_WithChar) {
    EXPECT_EQ(String::PadLeft("42", 5, '0'), "00042");
}
TEST(StringTests, PadLeft_AlreadyWide) {
    EXPECT_EQ(String::PadLeft("hello", 3), "hello");
}

// --- PadRight ---

TEST(StringTests, PadRight_WithSpaces) {
    EXPECT_EQ(String::PadRight("42", 5), "42   ");
}
TEST(StringTests, PadRight_WithChar) {
    EXPECT_EQ(String::PadRight("hi", 5, '-'), "hi---");
}
TEST(StringTests, PadRight_AlreadyWide) {
    EXPECT_EQ(String::PadRight("hello", 3), "hello");
}

// --- Format specifiers ---

TEST(StringTests, Format_Int_Plain) {
    EXPECT_EQ(String::Format("{0}", 42), "42");
}
TEST(StringTests, Format_Int_HexUpper) {
    EXPECT_EQ(String::Format("{0:X}", 255), "FF");
}
TEST(StringTests, Format_Int_HexLower) {
    EXPECT_EQ(String::Format("{0:x}", 255), "ff");
}
TEST(StringTests, Format_Int_HexPadded) {
    EXPECT_EQ(String::Format("{0:X4}", 255), "00FF");
}
TEST(StringTests, Format_Int_DecimalPadded) {
    EXPECT_EQ(String::Format("{0:D3}", 7), "007");
}
TEST(StringTests, Format_Int_DecimalPadded_Negative) {
    EXPECT_EQ(String::Format("{0:D3}", -7), "-007");
}
TEST(StringTests, Format_Double_Fixed) {
    EXPECT_EQ(String::Format("{0:F2}", 3.14159), "3.14");
}
TEST(StringTests, Format_Double_Fixed_Zero) {
    EXPECT_EQ(String::Format("{0:F0}", 3.7), "4");
}
TEST(StringTests, Format_Double_Plain) {
    EXPECT_EQ(String::Format("{0}", 1.5), "1.5");
}

// --- Format multi-arg ---

TEST(StringTests, Format_TwoInts) {
    EXPECT_EQ(String::Format("{0} / {1}", 10, 20), "10 / 20");
}
TEST(StringTests, Format_TwoInts_WithSpecs) {
    EXPECT_EQ(String::Format("{0:D2}:{1:D2}", 3, 5), "03:05");
}
TEST(StringTests, Format_IntAndString) {
    EXPECT_EQ(String::Format("HP: {0}/{1}", 80, std::string("100")), "HP: 80/100");
}
TEST(StringTests, Format_StringAndInt) {
    EXPECT_EQ(String::Format("{0}={1}", std::string("x"), 7), "x=7");
}
TEST(StringTests, Format_TwoStrings) {
    EXPECT_EQ(String::Format("{0} {1}", std::string("hello"), std::string("world")), "hello world");
}
TEST(StringTests, Format_TwoDoubles) {
    EXPECT_EQ(String::Format("({0:F1},{1:F1})", 1.25, 3.75), "(1.2,3.8)");
}
