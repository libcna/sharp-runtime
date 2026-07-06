// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <vector>
#include "System/Text/DecoderFallback.hpp"
#include "System/Text/Encoding.hpp"
#include "System/Text/EncoderFallback.hpp"
#include "System/Text/RunePosition.hpp"
#include "System/Text/StringBuilder.hpp"
#include "System/Text/StringBuilderRuneEnumerator.hpp"
#include "System/Text/StringRuneEnumerator.hpp"
#include "System/Text/UTF32Encoding.hpp"
#include "System/Text/UnicodeEncoding.hpp"

using namespace System::Text;

// --- DecoderFallbackBuffer / EncoderFallbackBuffer -------------------------------------------

TEST(DecoderFallbackBufferTests, ReplacementBuffer_YieldsReplacementChars) {
    DecoderReplacementFallback fallback("??");
    auto buffer = fallback.CreateFallbackBuffer();
    std::vector<SharpRuntime::bytecs> bad{0xFF};
    EXPECT_TRUE(buffer->Fallback(bad, 0));
    EXPECT_EQ(buffer->GetNextChar(), '?');
    EXPECT_EQ(buffer->GetNextChar(), '?');
    EXPECT_EQ(buffer->GetNextChar(), '\0');
}

TEST(DecoderFallbackBufferTests, ReplacementBuffer_MovePrevious) {
    DecoderReplacementFallback fallback("AB");
    auto buffer = fallback.CreateFallbackBuffer();
    std::vector<SharpRuntime::bytecs> bad{0xFF};
    buffer->Fallback(bad, 0);
    EXPECT_EQ(buffer->GetNextChar(), 'A');
    EXPECT_TRUE(buffer->MovePrevious());
    EXPECT_EQ(buffer->GetNextChar(), 'A');
}

TEST(DecoderFallbackBufferTests, ExceptionBuffer_Throws) {
    DecoderExceptionFallback fallback;
    auto buffer = fallback.CreateFallbackBuffer();
    std::vector<SharpRuntime::bytecs> bad{0xFF, 0xFE};
    EXPECT_THROW(buffer->Fallback(bad, 3), DecoderFallbackException);
}

TEST(DecoderFallbackExceptionTests, PropertiesSet) {
    std::vector<SharpRuntime::bytecs> bytes{0x01, 0x02};
    DecoderFallbackException ex("bad bytes", bytes, 5);
    EXPECT_EQ(ex.getBytesUnknownProperty(), bytes);
    EXPECT_EQ(ex.getIndexProperty(), 5);
}

TEST(EncoderFallbackBufferTests, ReplacementBuffer_YieldsReplacementChars) {
    EncoderReplacementFallback fallback("XY");
    auto buffer = fallback.CreateFallbackBuffer();
    EXPECT_TRUE(buffer->Fallback('\x01', 0));
    EXPECT_EQ(buffer->GetNextChar(), 'X');
    EXPECT_EQ(buffer->GetNextChar(), 'Y');
    EXPECT_EQ(buffer->GetNextChar(), '\0');
}

TEST(EncoderFallbackBufferTests, ExceptionBuffer_Throws) {
    EncoderExceptionFallback fallback;
    auto buffer = fallback.CreateFallbackBuffer();
    EXPECT_THROW(buffer->Fallback('\x01', 2), EncoderFallbackException);
}

TEST(EncoderFallbackExceptionTests, PropertiesSet) {
    EncoderFallbackException ex("bad char", 'z', 7);
    EXPECT_EQ(ex.getCharUnknownProperty(), 'z');
    EXPECT_EQ(ex.getIndexProperty(), 7);
}

// --- Encoding (expanded surface) --------------------------------------------------------------

TEST(EncodingTests, GetStringVectorOverload) {
    auto utf8 = Encoding::UTF8();
    std::vector<SharpRuntime::bytecs> bytes{'h', 'i'};
    EXPECT_EQ(utf8->GetString(bytes), "hi");
}

TEST(EncodingTests, GetByteCount_MatchesGetBytesSize) {
    auto utf8 = Encoding::UTF8();
    EXPECT_EQ(utf8->GetByteCount("hello"), 5);
}

TEST(EncodingTests, Equals_SameCodePage) {
    auto a = Encoding::UTF8();
    auto b = Encoding::UTF8();
    EXPECT_TRUE(a->Equals(*b));
}

TEST(EncodingTests, Equals_DifferentCodePage) {
    auto utf8 = Encoding::UTF8();
    auto ascii = Encoding::ASCII();
    EXPECT_FALSE(utf8->Equals(*ascii));
}

TEST(EncodingTests, DecoderFallback_DefaultIsReplacement) {
    auto utf8 = Encoding::UTF8();
    EXPECT_NE(utf8->getDecoderFallbackProperty(), nullptr);
}

TEST(EncodingTests, SetDecoderFallback_Roundtrips) {
    auto utf8 = Encoding::UTF8();
    auto originalFallback = utf8->getDecoderFallbackProperty();
    auto exceptionFallback = DecoderFallback::ExceptionFallback();
    utf8->setDecoderFallbackProperty(exceptionFallback);
    EXPECT_EQ(utf8->getDecoderFallbackProperty(), exceptionFallback);
    utf8->setDecoderFallbackProperty(originalFallback);
}

TEST(EncodingTests, StaticFactories_ReturnNonNull) {
    EXPECT_NE(Encoding::Latin1(), nullptr);
    EXPECT_NE(Encoding::UTF32(), nullptr);
    EXPECT_NE(Encoding::UTF7(), nullptr);
    EXPECT_NE(Encoding::BigEndianUnicode(), nullptr);
    EXPECT_NE(Encoding::Default(), nullptr);
}

TEST(EncodingTests, Default_IsUtf8) {
    EXPECT_EQ(Encoding::Default()->getCodePageProperty(), Encoding::UTF8()->getCodePageProperty());
}

// --- UnicodeEncoding (now full Unicode range, not just ASCII) --------------------------------

TEST(UnicodeEncodingTests, RoundTrip_Ascii) {
    UnicodeEncoding enc;
    auto bytes = enc.GetBytes("Hi!");
    EXPECT_EQ(enc.GetString(bytes.data(), 0, static_cast<SharpRuntime::intcs>(bytes.size())), "Hi!");
}

TEST(UnicodeEncodingTests, RoundTrip_NonAsciiBmp) {
    UnicodeEncoding enc;
    std::string original = "caf\xC3\xA9"; // "café" in UTF-8 (é = U+00E9)
    auto bytes = enc.GetBytes(original);
    EXPECT_EQ(enc.GetString(bytes.data(), 0, static_cast<SharpRuntime::intcs>(bytes.size())), original);
}

TEST(UnicodeEncodingTests, RoundTrip_SupplementaryPlane) {
    UnicodeEncoding enc;
    // U+1F600 (😀) UTF-8: F0 9F 98 80 — requires surrogate pair handling in UTF-16.
    std::string original = "\xF0\x9F\x98\x80";
    auto bytes = enc.GetBytes(original);
    EXPECT_EQ(bytes.size(), 4u); // one surrogate pair = 4 bytes
    EXPECT_EQ(enc.GetString(bytes.data(), 0, static_cast<SharpRuntime::intcs>(bytes.size())), original);
}

TEST(UnicodeEncodingTests, BigEndianRoundTrip) {
    UnicodeEncoding enc(true, false);
    std::string original = "test";
    auto bytes = enc.GetBytes(original);
    EXPECT_EQ(bytes[0], 0);
    EXPECT_EQ(bytes[1], 't');
    EXPECT_EQ(enc.GetString(bytes.data(), 0, static_cast<SharpRuntime::intcs>(bytes.size())), original);
}

// --- UTF32Encoding (now full Unicode range) ---------------------------------------------------

TEST(UTF32EncodingTests, RoundTrip_NonAscii) {
    UTF32Encoding enc(false, false);
    std::string original = "caf\xC3\xA9"; // "café"
    auto bytes = enc.GetBytes(original);
    EXPECT_EQ(enc.GetString(bytes.data(), 0, static_cast<SharpRuntime::intcs>(bytes.size())), original);
}

TEST(UTF32EncodingTests, RoundTrip_SupplementaryPlane) {
    UTF32Encoding enc(false, false);
    std::string original = "\xF0\x9F\x98\x80"; // U+1F600
    auto bytes = enc.GetBytes(original);
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(enc.GetString(bytes.data(), 0, static_cast<SharpRuntime::intcs>(bytes.size())), original);
}

// --- StringRuneEnumerator / RunePosition / StringBuilderRuneEnumerator ------------------------

TEST(StringRuneEnumeratorTests, EnumeratesAsciiRunes) {
    StringRuneEnumerator e("abc");
    std::vector<uint32_t> values;
    for (Rune r : e) values.push_back(r.getValueProperty());
    EXPECT_EQ(values, (std::vector<uint32_t>{'a', 'b', 'c'}));
}

TEST(StringRuneEnumeratorTests, EnumeratesMultiByteRune) {
    StringRuneEnumerator e("a\xC3\xA9z"); // 'a', 'é' (U+00E9), 'z'
    std::vector<uint32_t> values;
    for (Rune r : e) values.push_back(r.getValueProperty());
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], static_cast<uint32_t>('a'));
    EXPECT_EQ(values[1], 0xE9u);
    EXPECT_EQ(values[2], static_cast<uint32_t>('z'));
}

TEST(RunePositionTests, EnumerateReportsStartIndexAndLength) {
    std::string s = "a\xC3\xA9"; // 'a' (1 byte) + 'é' (2 bytes)
    std::vector<RunePosition> positions;
    for (RunePosition p : RunePosition::Enumerate(s)) positions.push_back(p);
    ASSERT_EQ(positions.size(), 2u);
    EXPECT_EQ(positions[0].getStartIndexProperty(), 0);
    EXPECT_EQ(positions[0].getLengthProperty(), 1);
    EXPECT_FALSE(positions[0].getWasReplacedProperty());
    EXPECT_EQ(positions[1].getStartIndexProperty(), 1);
    EXPECT_EQ(positions[1].getLengthProperty(), 2);
}

TEST(RunePositionTests, Equality) {
    RunePosition a(Rune('x'), 0, 1, false);
    RunePosition b(Rune('x'), 0, 1, false);
    RunePosition c(Rune('y'), 0, 1, false);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(StringBuilderRuneEnumeratorTests, EnumeratesBuilderContents) {
    StringBuilder sb;
    sb.Append("hi");
    std::vector<uint32_t> values;
    for (Rune r : sb.EnumerateRunes()) values.push_back(r.getValueProperty());
    EXPECT_EQ(values, (std::vector<uint32_t>{'h', 'i'}));
}

// --- StringBuilder::ChunkEnumerator / indexer -------------------------------------------------

TEST(StringBuilderTests, GetChunks_YieldsSingleChunkWithFullContent) {
    StringBuilder sb;
    sb.Append("hello world");
    int chunkCount = 0;
    for (const std::string& chunk : sb.GetChunks()) {
        EXPECT_EQ(chunk, "hello world");
        ++chunkCount;
    }
    EXPECT_EQ(chunkCount, 1);
}

TEST(StringBuilderTests, Indexer_ReadAndWrite) {
    StringBuilder sb("hello");
    EXPECT_EQ(sb[0], 'h');
    sb[0] = 'H';
    EXPECT_EQ(sb.ToString(), "Hello");
}
