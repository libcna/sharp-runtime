// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/Text/Base64Url.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"

using System::Buffers::Text::Base64Url;
using System::Buffers::OperationStatus;
using System::ReadOnlySpan;
using System::Span;
using System::FormatException;

TEST(Base64UrlTest, EncodeHello) {
    std::vector<uint8_t> input = {'H','e','l','l','o'};
    std::string encoded = Base64Url::EncodeToString(input);
    EXPECT_EQ(encoded, "SGVsbG8");
}

TEST(Base64UrlTest, EncodeEmpty) {
    std::vector<uint8_t> input;
    EXPECT_EQ(Base64Url::EncodeToString(input), "");
}

TEST(Base64UrlTest, DecodeHello) {
    std::string b64url = "SGVsbG8";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64url.data()), static_cast<int>(b64url.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    auto status = Base64Url::DecodeFromUtf8(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(written, 5);
    EXPECT_EQ(out[0], 'H');
}

TEST(Base64UrlTest, GetEncodedLength) {
    EXPECT_EQ(Base64Url::GetEncodedLength(3), 4);
    EXPECT_EQ(Base64Url::GetEncodedLength(4), 6);
    EXPECT_EQ(Base64Url::GetEncodedLength(5), 7);
}

TEST(Base64UrlTest, GetMaxDecodedLength) {
    EXPECT_EQ(Base64Url::GetMaxDecodedLength(4), 3);
    // 1 full 4-char group (-> 3 bytes) + a remainder of 3 chars (-> 2 bytes, per .NET's
    // exact whole*3 + (remainder-1) formula) = 5, not a loose 6-byte upper bound.
    EXPECT_EQ(Base64Url::GetMaxDecodedLength(7), 5);
}

TEST(Base64UrlTest, IsValidTrue) {
    std::string valid = "SGVsbG8";
    ReadOnlySpan<uint8_t> span(reinterpret_cast<const uint8_t*>(valid.data()), static_cast<int>(valid.size()));
    EXPECT_TRUE(Base64Url::IsValid(span));
}

TEST(Base64UrlTest, IsValidFalseOddLength) {
    std::string bad = "A";
    ReadOnlySpan<uint8_t> span(reinterpret_cast<const uint8_t*>(bad.data()), static_cast<int>(bad.size()));
    EXPECT_FALSE(Base64Url::IsValid(span));
}

TEST(Base64UrlTest, UsesUrlAlphabet) {
    std::vector<uint8_t> input(3, 0xFF);
    std::string encoded = Base64Url::EncodeToString(input);
    EXPECT_EQ(encoded.find('+'), std::string::npos);
    EXPECT_EQ(encoded.find('/'), std::string::npos);
}

TEST(Base64UrlTest, RoundTripBytes) {
    std::vector<uint8_t> original = {0xFB, 0xFF, 0x3E};
    std::string encoded = Base64Url::EncodeToString(original);
    ReadOnlySpan<uint8_t> srcSpan(reinterpret_cast<const uint8_t*>(encoded.data()),
                                   static_cast<int>(encoded.size()));
    std::vector<uint8_t> decoded(10);
    Span<uint8_t> dstSpan(decoded.data(), static_cast<int>(decoded.size()));
    int consumed = 0, written = 0;
    Base64Url::DecodeFromUtf8(srcSpan, dstSpan, consumed, written, true);
    ASSERT_EQ(written, 3);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(decoded[i], original[i]);
}

// ===========================================================================
// Whitespace / remainder-of-1 correctness
// ===========================================================================

TEST(Base64UrlTest, DecodeFromUtf8_WithEmbeddedWhitespace_Succeeds) {
    std::string b64url = "SGVs\r\nbG8";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64url.data()), static_cast<int>(b64url.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    auto status = Base64Url::DecodeFromUtf8(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    ASSERT_EQ(written, 5);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[4], 'o');
}

TEST(Base64UrlTest, DecodeFromUtf8_RemainderOfOne_IsInvalid) {
    std::string bad = "SGVsbG9X"; // 9 chars total once combined below: 2 full groups + 1 remainder
    bad += "X";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(bad.data()), static_cast<int>(bad.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64Url::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::InvalidData);
}

// ===========================================================================
// New convenience overloads
// ===========================================================================

TEST(Base64UrlTest, EncodeToUtf8_TwoArg_ReturnsBytesWritten) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = Base64Url::EncodeToUtf8(src, dst);
    std::string s(reinterpret_cast<char*>(out.data()), written);
    EXPECT_EQ(s, "SGk");
}

TEST(Base64UrlTest, EncodeToUtf8_TwoArg_TooSmall_Throws) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    uint8_t buf[1] = {};
    Span<uint8_t> dst(buf, 1);
    EXPECT_THROW(Base64Url::EncodeToUtf8(src, dst), System::ArgumentException);
}

TEST(Base64UrlTest, EncodeToUtf8_ReturnsVector) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    std::vector<uint8_t> encoded = Base64Url::EncodeToUtf8(src);
    std::string s(encoded.begin(), encoded.end());
    EXPECT_EQ(s, "SGk");
}

TEST(Base64UrlTest, TryEncodeToUtf8_Success) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = 0;
    EXPECT_TRUE(Base64Url::TryEncodeToUtf8(src, dst, written));
    EXPECT_EQ(written, 3);
}

TEST(Base64UrlTest, TryEncodeToUtf8InPlace_RoundTrip) {
    uint8_t buf[8] = {'M','a','n', 0,0,0,0,0};
    Span<uint8_t> span(buf, 8);
    int written = 0;
    EXPECT_TRUE(Base64Url::TryEncodeToUtf8InPlace(span, 3, written));
    EXPECT_EQ(written, 4);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 4), "TWFu");
}

TEST(Base64UrlTest, TryEncodeToUtf8InPlace_TooSmall_ReturnsFalse) {
    uint8_t buf[3] = {'M','a','n'};
    Span<uint8_t> span(buf, 3);
    int written = 0;
    EXPECT_FALSE(Base64Url::TryEncodeToUtf8InPlace(span, 3, written));
}

// ===========================================================================
// In-place encode write order (ticket #1816 / SR-AUD-078 / CCF-013)
// ===========================================================================
//
// The same defect as Base64::EncodeToUtf8InPlace, in the same shape: the full
// 3-byte packs were written before the trailing remainder was read, so the first
// full pack's fourth output byte overwrote it. SR-AUD-078 spans both headers,
// which is why CCF-013 requires the repair and its tests to cover both.

// The audit's exact reproduction: 'A','B','C',0 must encode the zero byte, not 'D'.
TEST(Base64UrlTest, TryEncodeToUtf8InPlace_FourBytes_DoesNotOverwriteTrailingByte) {
    uint8_t buf[16] = {'A','B','C', 0};
    Span<uint8_t> span(buf, 6);
    int written = 0;
    EXPECT_TRUE(Base64Url::TryEncodeToUtf8InPlace(span, 4, written));
    EXPECT_EQ(written, 6);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 6), "QUJDAA");
}

TEST(Base64UrlTest, TryEncodeToUtf8InPlace_FiveBytes_DoesNotOverwriteTrailingBytes) {
    uint8_t buf[16] = {'A','B','C', 0, 'E'};
    Span<uint8_t> span(buf, 7);
    int written = 0;
    EXPECT_TRUE(Base64Url::TryEncodeToUtf8InPlace(span, 5, written));
    EXPECT_EQ(written, 7);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 7), "QUJDAEU");
}

TEST(Base64UrlTest, TryEncodeToUtf8InPlace_SevenBytes_DoesNotOverwriteTrailingByte) {
    uint8_t buf[16] = {'A','B','C', 0, 'E','F', 0};
    Span<uint8_t> span(buf, 10);
    int written = 0;
    EXPECT_TRUE(Base64Url::TryEncodeToUtf8InPlace(span, 7, written));
    EXPECT_EQ(written, 10);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 10), "QUJDAEVGAA");
}

TEST(Base64UrlTest, TryEncodeToUtf8InPlace_MatchesOutOfPlaceEncoderForEveryLength) {
    for (int n = 0; n <= 24; ++n) {
        std::vector<uint8_t> data;
        for (int i = 0; i < n; ++i)
            data.push_back(static_cast<uint8_t>((i % 4 == 3) ? 0 : ('A' + (i % 26))));

        const int encodedLen = Base64Url::GetEncodedLength(n);
        std::vector<uint8_t> reference(static_cast<size_t>(encodedLen) + 4, 0);
        int consumed = 0, refWritten = 0;
        ASSERT_EQ(Base64Url::EncodeToUtf8(
                      ReadOnlySpan<uint8_t>(data.data(), n),
                      Span<uint8_t>(reference.data(), static_cast<int>(reference.size())),
                      consumed, refWritten, true),
                  OperationStatus::Done) << "length " << n;
        const std::string want(reinterpret_cast<const char*>(reference.data()),
                               static_cast<size_t>(refWritten));

        std::vector<uint8_t> buf(static_cast<size_t>(encodedLen) + 4, 0xEE);
        for (int i = 0; i < n; ++i) buf[static_cast<size_t>(i)] = data[static_cast<size_t>(i)];
        int written = 0;
        ASSERT_TRUE(Base64Url::TryEncodeToUtf8InPlace(Span<uint8_t>(buf.data(), encodedLen), n, written))
            << "length " << n;
        EXPECT_EQ(written, encodedLen) << "length " << n;
        EXPECT_EQ(std::string(reinterpret_cast<char*>(buf.data()), static_cast<size_t>(written)), want)
            << "length " << n;
        EXPECT_EQ(buf[static_cast<size_t>(encodedLen)], 0xEE)
            << "in-place encode wrote past the encoded length at " << n;
    }
}

TEST(Base64UrlTest, DecodeFromUtf8_TwoArg_ReturnsBytesWritten) {
    std::string b64url = "SGk";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64url.data()), static_cast<int>(b64url.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = Base64Url::DecodeFromUtf8(src, dst);
    EXPECT_EQ(written, 2);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[1], 'i');
}

TEST(Base64UrlTest, DecodeFromUtf8_TwoArg_InvalidData_Throws) {
    std::string b64url = "SG!";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64url.data()), static_cast<int>(b64url.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    EXPECT_THROW(Base64Url::DecodeFromUtf8(src, dst), FormatException);
}

TEST(Base64UrlTest, DecodeFromUtf8_ReturnsVector) {
    std::string b64url = "SGk";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64url.data()), static_cast<int>(b64url.size()));
    std::vector<uint8_t> decoded = Base64Url::DecodeFromUtf8(src);
    ASSERT_EQ(decoded.size(), 2u);
    EXPECT_EQ(decoded[0], 'H');
    EXPECT_EQ(decoded[1], 'i');
}

TEST(Base64UrlTest, TryDecodeFromUtf8_Success) {
    std::string b64url = "SGk";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64url.data()), static_cast<int>(b64url.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = 0;
    EXPECT_TRUE(Base64Url::TryDecodeFromUtf8(src, dst, written));
    EXPECT_EQ(written, 2);
}

TEST(Base64UrlTest, DecodeFromUtf8InPlace_ReturnsBytesWritten) {
    uint8_t buf[3] = {'S','G','k'};
    Span<uint8_t> span(buf, 3);
    int written = Base64Url::DecodeFromUtf8InPlace(span);
    ASSERT_EQ(written, 2);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 2), "Hi");
}

TEST(Base64UrlTest, DecodeFromUtf8InPlace_Invalid_Throws) {
    uint8_t buf[3] = {'S','G','!'};
    Span<uint8_t> span(buf, 3);
    EXPECT_THROW(Base64Url::DecodeFromUtf8InPlace(span), FormatException);
}

TEST(Base64UrlTest, EncodeToChars_RoundTrip) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    char buf[8] = {};
    Span<char> dst(buf, 8);
    int consumed = 0, written = 0;
    auto status = Base64Url::EncodeToChars(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(std::string(buf, written), "SGk");
}

TEST(Base64UrlTest, DecodeFromChars_RoundTrip) {
    std::string b64url = "SGk";
    ReadOnlySpan<char> src(b64url.data(), static_cast<int>(b64url.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    auto status = Base64Url::DecodeFromChars(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    ASSERT_EQ(written, 2);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[1], 'i');
}

TEST(Base64UrlTest, IsValid_Char_WithDecodedLength) {
    std::string valid = "SGk";
    ReadOnlySpan<char> span(valid.data(), static_cast<int>(valid.size()));
    int decodedLength = 0;
    EXPECT_TRUE(Base64Url::IsValid(span, decodedLength));
    EXPECT_EQ(decodedLength, 2);
}

TEST(Base64UrlTest, GetEncodedLength_Negative_Throws) {
    EXPECT_THROW(Base64Url::GetEncodedLength(-1), System::ArgumentOutOfRangeException);
}

TEST(Base64UrlTest, GetMaxDecodedLength_Negative_Throws) {
    EXPECT_THROW(Base64Url::GetMaxDecodedLength(-1), System::ArgumentOutOfRangeException);
}

// Regression tests for a code-audit finding (ticket 253, same gaps found in sibling Base64.hpp
// under ticket 247): the isFinalBlock=false streaming path (NeedMoreData) and
// GetEncodedLength's upper-bound throw had zero test coverage. The core algorithm itself was
// fully verified against real .NET's Base64UrlEncoder.cs/Base64UrlDecoder.cs (GetEncodedLength/
// GetMaxDecodedLength formulas, the encode/decode alphabet table, and the API shape of
// TryEncodeToUtf8InPlace/DecodeFromUtf8InPlace returning bool/int rather than OperationStatus,
// which matches .NET's actual, deliberately simpler Base64Url signatures) and found correct.
TEST(Base64UrlTest, DecodeFromUtf8_IncompleteFinalGroup_NotFinalBlock_NeedsMoreData) {
    std::string b64url = "SGVs"; // 4 symbols consumed as a full group, "bG8" left incomplete
    std::string partial = "SGVsbG8"; // 7 symbols -- remainder of 3 is valid only when final
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(partial.data()), static_cast<int>(partial.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    // With isFinalBlock=false, the trailing 3-symbol remainder must wait for more data.
    EXPECT_EQ(Base64Url::DecodeFromUtf8(src, dst, consumed, written, false), OperationStatus::NeedMoreData);
    EXPECT_EQ(consumed, 4);
    EXPECT_EQ(written, 3);
}

TEST(Base64UrlTest, EncodeToChars_NotFinalBlock_RemainderBytes_NeedsMoreData) {
    std::vector<uint8_t> input = {'H', 'e'}; // 2 bytes -- not a full 3-byte group
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    std::vector<char> out(10);
    Span<char> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64Url::EncodeToChars(src, dst, consumed, written, false), OperationStatus::NeedMoreData);
    EXPECT_EQ(consumed, 0);
    EXPECT_EQ(written, 0);
}

TEST(Base64UrlTest, GetEncodedLength_AboveMaximum_Throws) {
    EXPECT_THROW(Base64Url::GetEncodedLength(1610612734), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(Base64Url::GetEncodedLength(1610612733));
}

// ===========================================================================
// Canonical final quantum (ticket #1817 / SR-AUD-079, extended)
// ===========================================================================
//
// The unpadded instance of the same boundary the padded sibling has: a two-symbol
// tail carries one byte, so the low 4 bits of the second sextet must be zero, and
// a three-symbol tail carries two bytes, so the low 2 bits of the third must be.
// Neither decodeCore nor validateCore required it, so "AB" and "AAB" decoded to
// Done and IsValid agreed.

TEST(Base64UrlTest, Decode_NoncanonicalTwoSymbolTail_IsInvalid) {
    const char* text = "AB";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text), 2);
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), 8);
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64Url::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::InvalidData);
    EXPECT_EQ(written, 0);
}

TEST(Base64UrlTest, Decode_NoncanonicalThreeSymbolTail_IsInvalid) {
    const char* text = "AAB";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text), 3);
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), 8);
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64Url::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::InvalidData);
    EXPECT_EQ(written, 0);
}

TEST(Base64UrlTest, IsValid_AgreesWithDecoderOnNoncanonicalFinalQuantum) {
    for (const char* text : {"AB", "AAB"}) {
        const auto len = static_cast<int>(std::string(text).size());
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text), len);
        EXPECT_FALSE(Base64Url::IsValid(src)) << text;
        int decodedLength = -1;
        EXPECT_FALSE(Base64Url::IsValid(src, decodedLength)) << text;
    }
}

TEST(Base64UrlTest, DecodeFromChars_NoncanonicalFinalQuantum_IsInvalid) {
    const char* text = "AB";
    ReadOnlySpan<char> src(text, 2);
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), 8);
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64Url::DecodeFromChars(src, dst, consumed, written, true), OperationStatus::InvalidData);
    EXPECT_FALSE(Base64Url::IsValid(ReadOnlySpan<char>(text, 2)));
}

TEST(Base64UrlTest, Decode_CanonicalFinalQuantum_StillDecodes) {
    struct { const char* text; int expectedBytes; } cases[] = {
        {"AA", 1}, {"AAA", 2}, {"QQ", 1}, {"SGk", 2}, {"SGVsbG8", 5}, {"TWFu", 3},
    };
    for (const auto& c : cases) {
        const auto len = static_cast<int>(std::string(c.text).size());
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(c.text), len);
        std::vector<uint8_t> out(16);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = 0, written = 0;
        EXPECT_EQ(Base64Url::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::Done) << c.text;
        EXPECT_EQ(written, c.expectedBytes) << c.text;
        EXPECT_TRUE(Base64Url::IsValid(src)) << c.text;
    }
}

TEST(Base64UrlTest, Decode_AcceptsEveryEncodedLength_AfterCanonicalRule) {
    for (int n = 0; n <= 24; ++n) {
        std::vector<uint8_t> data;
        for (int i = 0; i < n; ++i) data.push_back(static_cast<uint8_t>((i * 37 + 11) & 0xFF));
        const std::string encoded = Base64Url::EncodeToString(data);
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(encoded.data()),
                                  static_cast<int>(encoded.size()));
        EXPECT_TRUE(Base64Url::IsValid(src)) << "length " << n;
        std::vector<uint8_t> out(64);
        Span<uint8_t> dst(out.data(), 64);
        int consumed = 0, written = 0;
        ASSERT_EQ(Base64Url::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::Done)
            << "length " << n;
        ASSERT_EQ(written, n) << "length " << n;
        for (int i = 0; i < n; ++i) EXPECT_EQ(out[static_cast<size_t>(i)], data[static_cast<size_t>(i)]);
    }
}
