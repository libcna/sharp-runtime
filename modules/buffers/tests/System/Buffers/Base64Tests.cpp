// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/Text/Base64.hpp"

using System::Buffers::Text::Base64;
using System::Buffers::OperationStatus;
using System::ReadOnlySpan;
using System::Span;
using System::FormatException;

TEST(Base64Test, EncodeHello) {
    std::vector<uint8_t> input = {'H','e','l','l','o'};
    std::string encoded = Base64::EncodeToString(input);
    EXPECT_EQ(encoded, "SGVsbG8=");
}

TEST(Base64Test, EncodeEmpty) {
    std::vector<uint8_t> input;
    EXPECT_EQ(Base64::EncodeToString(input), "");
}

TEST(Base64Test, DecodeHello) {
    std::string b64 = "SGVsbG8=";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    auto status = Base64::DecodeFromUtf8(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(written, 5);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[4], 'o');
}

TEST(Base64Test, GetMaxDecodedLength) {
    EXPECT_EQ(Base64::GetMaxDecodedFromUtf8Length(4), 3);
    EXPECT_EQ(Base64::GetMaxDecodedFromUtf8Length(8), 6);
}

TEST(Base64Test, GetMaxEncodedLength) {
    EXPECT_EQ(Base64::GetMaxEncodedToUtf8Length(3), 4);
    EXPECT_EQ(Base64::GetMaxEncodedToUtf8Length(4), 8);
}

TEST(Base64Test, IsValidTrue) {
    std::string valid = "SGVsbG8=";
    ReadOnlySpan<uint8_t> span(reinterpret_cast<const uint8_t*>(valid.data()), static_cast<int>(valid.size()));
    EXPECT_TRUE(Base64::IsValid(span));
}

TEST(Base64Test, IsValidFalse) {
    std::string invalid = "SGVs!G8=";
    ReadOnlySpan<uint8_t> span(reinterpret_cast<const uint8_t*>(invalid.data()), static_cast<int>(invalid.size()));
    EXPECT_FALSE(Base64::IsValid(span));
}

TEST(Base64Test, DestinationTooSmall) {
    std::vector<uint8_t> input = {'A','B','C'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    uint8_t dstBuf[2] = {};
    Span<uint8_t> dst(dstBuf, 2);
    int consumed = 0, written = 0;
    auto status = Base64::EncodeToUtf8(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::DestinationTooSmall);
}

TEST(Base64Test, RoundTripBytes) {
    std::vector<uint8_t> original = {0x00, 0xFF, 0x42, 0xAB};
    std::string encoded = Base64::EncodeToString(original);
    ReadOnlySpan<uint8_t> srcSpan(reinterpret_cast<const uint8_t*>(encoded.data()),
                                   static_cast<int>(encoded.size()));
    std::vector<uint8_t> decoded(10);
    Span<uint8_t> dstSpan(decoded.data(), static_cast<int>(decoded.size()));
    int consumed = 0, written = 0;
    Base64::DecodeFromUtf8(srcSpan, dstSpan, consumed, written, true);
    ASSERT_EQ(written, 4);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(decoded[i], original[i]);
}

// ===========================================================================
// Whitespace handling / incomplete-group correctness (parity fixes)
// ===========================================================================

TEST(Base64Test, DecodeFromUtf8_WithEmbeddedWhitespace_Succeeds) {
    std::string b64 = "SGVs\r\nbG8=";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    auto status = Base64::DecodeFromUtf8(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    ASSERT_EQ(written, 5);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[4], 'o');
}

TEST(Base64Test, DecodeFromUtf8_TrailingDataAfterPadding_IsInvalid) {
    std::string b64 = "SGVsbG8=XX";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::InvalidData);
}

TEST(Base64Test, DecodeFromUtf8_IncompleteFinalGroup_IsInvalid) {
    std::string b64 = "QQQ"; // 3 symbols, not a multiple of 4, no padding
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::InvalidData);
}

// Regression tests for a code-audit finding (ticket 247): the isFinalBlock=false streaming
// path (NeedMoreData) had zero test coverage for either DecodeFromUtf8 or EncodeToChars, despite
// being explicit, documented OperationStatus outcomes used by the streaming (chunked) overloads.
TEST(Base64Test, DecodeFromUtf8_IncompleteFinalGroup_NotFinalBlock_NeedsMoreData) {
    std::string b64 = "QQQ"; // 3 symbols -- incomplete group, but more data may follow
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(10);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, false), OperationStatus::NeedMoreData);
    EXPECT_EQ(consumed, 0);
    EXPECT_EQ(written, 0);
}

TEST(Base64Test, EncodeToChars_NotFinalBlock_RemainderBytes_NeedsMoreData) {
    std::vector<uint8_t> input = {'H', 'e'}; // 2 bytes -- not a full 3-byte group
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    std::vector<char> out(10);
    Span<char> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64::EncodeToChars(src, dst, consumed, written, false), OperationStatus::NeedMoreData);
    EXPECT_EQ(consumed, 0);
    EXPECT_EQ(written, 0);
}

TEST(Base64Test, IsValid_AllowsEmbeddedWhitespace) {
    std::string valid = "SGVs\r\nbG8=";
    ReadOnlySpan<uint8_t> span(reinterpret_cast<const uint8_t*>(valid.data()), static_cast<int>(valid.size()));
    EXPECT_TRUE(Base64::IsValid(span));
}

TEST(Base64Test, IsValid_WithDecodedLength_CountsWhitespaceFree) {
    std::string valid = "SGVs\r\nbG8=";
    ReadOnlySpan<uint8_t> span(reinterpret_cast<const uint8_t*>(valid.data()), static_cast<int>(valid.size()));
    int decodedLength = 0;
    EXPECT_TRUE(Base64::IsValid(span, decodedLength));
    EXPECT_EQ(decodedLength, 5);
}

// ===========================================================================
// New convenience overloads
// ===========================================================================

TEST(Base64Test, GetEncodedLength_MatchesGetMaxEncodedToUtf8Length) {
    EXPECT_EQ(Base64::GetEncodedLength(3), Base64::GetMaxEncodedToUtf8Length(3));
}

TEST(Base64Test, GetMaxDecodedLength_MatchesGetMaxDecodedFromUtf8Length) {
    EXPECT_EQ(Base64::GetMaxDecodedLength(8), Base64::GetMaxDecodedFromUtf8Length(8));
}

TEST(Base64Test, EncodeToUtf8_TwoArg_ReturnsBytesWritten) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = Base64::EncodeToUtf8(src, dst);
    EXPECT_EQ(written, 4);
}

TEST(Base64Test, EncodeToUtf8_TwoArg_TooSmall_Throws) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    uint8_t buf[2] = {};
    Span<uint8_t> dst(buf, 2);
    EXPECT_THROW(Base64::EncodeToUtf8(src, dst), System::ArgumentException);
}

TEST(Base64Test, EncodeToUtf8_ReturnsVector) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    std::vector<uint8_t> encoded = Base64::EncodeToUtf8(src);
    std::string s(encoded.begin(), encoded.end());
    EXPECT_EQ(s, "SGk=");
}

TEST(Base64Test, TryEncodeToUtf8_Success) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = 0;
    EXPECT_TRUE(Base64::TryEncodeToUtf8(src, dst, written));
    EXPECT_EQ(written, 4);
}

TEST(Base64Test, TryEncodeToUtf8_TooSmall_ReturnsFalse) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    uint8_t buf[2] = {};
    Span<uint8_t> dst(buf, 2);
    int written = 0;
    EXPECT_FALSE(Base64::TryEncodeToUtf8(src, dst, written));
}

TEST(Base64Test, EncodeToUtf8InPlace_RoundTrip) {
    uint8_t buf[8] = {'M','a','n', 0,0,0,0,0};
    Span<uint8_t> span(buf, 8);
    int written = 0;
    auto status = Base64::EncodeToUtf8InPlace(span, 3, written);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(written, 4);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 4), "TWFu");
}

TEST(Base64Test, EncodeToUtf8InPlace_TooSmall_ReturnsDestinationTooSmall) {
    uint8_t buf[3] = {'M','a','n'};
    Span<uint8_t> span(buf, 3);
    int written = 0;
    EXPECT_EQ(Base64::EncodeToUtf8InPlace(span, 3, written), OperationStatus::DestinationTooSmall);
}

TEST(Base64Test, TryEncodeToUtf8InPlace_Success) {
    uint8_t buf[8] = {'M','a', 0,0,0,0,0,0};
    Span<uint8_t> span(buf, 8);
    int written = 0;
    EXPECT_TRUE(Base64::TryEncodeToUtf8InPlace(span, 2, written));
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), written), "TWE=");
}

// ===========================================================================
// In-place encode write order (ticket #1816 / SR-AUD-078 / CCF-013)
// ===========================================================================
//
// The encoder wrote the full 3-byte packs before reading the trailing remainder,
// so the first full pack's fourth output byte overwrote the unread remainder.
// The suite used to cover only dataLength 2 and 3 -- the two lengths that have no
// full pack AND a remainder together -- so every affected length was untested.
// The remainder is now encoded first; these cases pin the boundary.

// The audit's exact reproduction: 'A','B','C',0 must encode the zero byte, not 'D'.
TEST(Base64Test, EncodeToUtf8InPlace_FourBytes_DoesNotOverwriteTrailingByte) {
    uint8_t buf[16] = {'A','B','C', 0};
    Span<uint8_t> span(buf, 8);
    int written = 0;
    EXPECT_EQ(Base64::EncodeToUtf8InPlace(span, 4, written), OperationStatus::Done);
    EXPECT_EQ(written, 8);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 8), "QUJDAA==");
}

TEST(Base64Test, EncodeToUtf8InPlace_FiveBytes_DoesNotOverwriteTrailingBytes) {
    uint8_t buf[16] = {'A','B','C', 0, 'E'};
    Span<uint8_t> span(buf, 8);
    int written = 0;
    EXPECT_EQ(Base64::EncodeToUtf8InPlace(span, 5, written), OperationStatus::Done);
    EXPECT_EQ(written, 8);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 8), "QUJDAEU=");
}

// Two full packs plus a remainder: the defect was never limited to lengths 4 and 5.
TEST(Base64Test, EncodeToUtf8InPlace_SevenBytes_DoesNotOverwriteTrailingByte) {
    uint8_t buf[16] = {'A','B','C', 0, 'E','F', 0};
    Span<uint8_t> span(buf, 12);
    int written = 0;
    EXPECT_EQ(Base64::EncodeToUtf8InPlace(span, 7, written), OperationStatus::Done);
    EXPECT_EQ(written, 12);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 12), "QUJDAEVGAA==");
}

// Every length 0..24 must agree with this type's own out-of-place encoder, and the
// byte just past the encoded output must be untouched. Before the fix this failed
// for all 14 lengths that have both a full pack and a remainder.
TEST(Base64Test, EncodeToUtf8InPlace_MatchesOutOfPlaceEncoderForEveryLength) {
    for (int n = 0; n <= 24; ++n) {
        std::vector<uint8_t> data;
        for (int i = 0; i < n; ++i)
            data.push_back(static_cast<uint8_t>((i % 4 == 3) ? 0 : ('A' + (i % 26))));

        const int encodedLen = Base64::GetEncodedLength(n);
        std::vector<uint8_t> reference(static_cast<size_t>(encodedLen) + 4, 0);
        int consumed = 0, refWritten = 0;
        ASSERT_EQ(Base64::EncodeToUtf8(
                      ReadOnlySpan<uint8_t>(data.data(), n),
                      Span<uint8_t>(reference.data(), static_cast<int>(reference.size())),
                      consumed, refWritten, true),
                  OperationStatus::Done) << "length " << n;
        const std::string want(reinterpret_cast<const char*>(reference.data()),
                               static_cast<size_t>(refWritten));

        std::vector<uint8_t> buf(static_cast<size_t>(encodedLen) + 4, 0xEE);
        for (int i = 0; i < n; ++i) buf[static_cast<size_t>(i)] = data[static_cast<size_t>(i)];
        int written = 0;
        ASSERT_EQ(Base64::EncodeToUtf8InPlace(Span<uint8_t>(buf.data(), encodedLen), n, written),
                  OperationStatus::Done) << "length " << n;
        EXPECT_EQ(written, encodedLen) << "length " << n;
        EXPECT_EQ(std::string(reinterpret_cast<char*>(buf.data()), static_cast<size_t>(written)), want)
            << "length " << n;
        EXPECT_EQ(buf[static_cast<size_t>(encodedLen)], 0xEE)
            << "in-place encode wrote past the encoded length at " << n;
    }
}

TEST(Base64Test, DecodeFromUtf8_TwoArg_ReturnsBytesWritten) {
    std::string b64 = "SGk=";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = Base64::DecodeFromUtf8(src, dst);
    EXPECT_EQ(written, 2);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[1], 'i');
}

TEST(Base64Test, DecodeFromUtf8_TwoArg_InvalidData_ThrowsFormatException) {
    std::string b64 = "SG!=";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    EXPECT_THROW(Base64::DecodeFromUtf8(src, dst), FormatException);
}

TEST(Base64Test, DecodeFromUtf8_ReturnsVector) {
    std::string b64 = "SGk=";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> decoded = Base64::DecodeFromUtf8(src);
    ASSERT_EQ(decoded.size(), 2u);
    EXPECT_EQ(decoded[0], 'H');
    EXPECT_EQ(decoded[1], 'i');
}

TEST(Base64Test, TryDecodeFromUtf8_Success) {
    std::string b64 = "SGk=";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = 0;
    EXPECT_TRUE(Base64::TryDecodeFromUtf8(src, dst, written));
    EXPECT_EQ(written, 2);
}

TEST(Base64Test, TryDecodeFromUtf8_InvalidData_Throws) {
    std::string b64 = "SG!=";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = 0;
    EXPECT_THROW(Base64::TryDecodeFromUtf8(src, dst, written), FormatException);
}

TEST(Base64Test, DecodeFromUtf8InPlace_RoundTrip) {
    // The entire span is treated as base64 text to decode in place (matches .NET semantics).
    uint8_t buf[4] = {'T','W','F','u'};
    Span<uint8_t> span(buf, 4);
    int written = 0;
    auto status = Base64::DecodeFromUtf8InPlace(span, written);
    EXPECT_EQ(status, OperationStatus::Done);
    ASSERT_EQ(written, 3);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), 3), "Man");
}

TEST(Base64Test, EncodeToChars_RoundTrip) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    char buf[8] = {};
    Span<char> dst(buf, 8);
    int consumed = 0, written = 0;
    auto status = Base64::EncodeToChars(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(std::string(buf, written), "SGk=");
}

TEST(Base64Test, EncodeToChars_TwoArg_ThrowsWhenTooSmall) {
    std::vector<uint8_t> input = {'H','i'};
    ReadOnlySpan<uint8_t> src(input.data(), static_cast<int>(input.size()));
    char buf[2] = {};
    Span<char> dst(buf, 2);
    EXPECT_THROW(Base64::EncodeToChars(src, dst), System::ArgumentException);
}

TEST(Base64Test, DecodeFromChars_RoundTrip) {
    std::string b64 = "SGk=";
    ReadOnlySpan<char> src(b64.data(), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int consumed = 0, written = 0;
    auto status = Base64::DecodeFromChars(src, dst, consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    ASSERT_EQ(written, 2);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[1], 'i');
}

TEST(Base64Test, TryDecodeFromChars_Success) {
    std::string b64 = "SGk=";
    ReadOnlySpan<char> src(b64.data(), static_cast<int>(b64.size()));
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), static_cast<int>(out.size()));
    int written = 0;
    EXPECT_TRUE(Base64::TryDecodeFromChars(src, dst, written));
    EXPECT_EQ(written, 2);
}

TEST(Base64Test, IsValid_Char_WithDecodedLength) {
    std::string valid = "SGk=";
    ReadOnlySpan<char> span(valid.data(), static_cast<int>(valid.size()));
    int decodedLength = 0;
    EXPECT_TRUE(Base64::IsValid(span, decodedLength));
    EXPECT_EQ(decodedLength, 2);
}

TEST(Base64Test, GetMaxEncodedLength_Negative_Throws) {
    EXPECT_THROW(Base64::GetMaxEncodedToUtf8Length(-1), System::ArgumentOutOfRangeException);
}

// Regression test for a code-audit finding (ticket 247): the negative-input bound of
// GetMaxEncodedToUtf8Length was tested above, but the upper bound (bytesLength >
// kMaximumEncodeLength, i.e. > 1610612733, matching .NET's Base64Helper.MaximumEncodeLength)
// had no test coverage at all.
TEST(Base64Test, GetMaxEncodedLength_AboveMaximum_Throws) {
    EXPECT_THROW(Base64::GetMaxEncodedToUtf8Length(1610612734), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(Base64::GetMaxEncodedToUtf8Length(1610612733));
}

TEST(Base64Test, GetMaxDecodedLength_Negative_Throws) {
    EXPECT_THROW(Base64::GetMaxDecodedFromUtf8Length(-1), System::ArgumentOutOfRangeException);
}

// ===========================================================================
// Canonical final quantum (ticket #1817 / SR-AUD-079)
// ===========================================================================
//
// The decoder derived the final one/two bytes from the leading sextets but never
// required the unused low bits to be zero: the low 4 bits of the second sextet
// before "==", the low 2 bits of the third before "=". validateCore repeated the
// omission, so IsValid agreed with the bad decode instead of screening it.
// .NET's Base64DecoderHelper.cs and Base64ValidatorHelper.cs both test those bits.
//
// This narrows the accepted input set: "AB==" and "AAB=" used to decode to Done.

TEST(Base64Test, Decode_NoncanonicalTwoPad_IsInvalid) {
    std::string b64 = "AB==";                    // 'B' == sextet 1: low nibble nonzero
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), 4);
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), 8);
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::InvalidData);
    EXPECT_EQ(written, 0);
}

TEST(Base64Test, Decode_NoncanonicalOnePad_IsInvalid) {
    std::string b64 = "AAB=";                    // low two bits of the third sextet nonzero
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(b64.data()), 4);
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), 8);
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::InvalidData);
    EXPECT_EQ(written, 0);
}

// The validator must not be more permissive than its own decoder: that combination
// tells a caller an input is safe to decode when it is not.
TEST(Base64Test, IsValid_AgreesWithDecoderOnNoncanonicalFinalQuantum) {
    for (const char* text : {"AB==", "AAB="}) {
        const auto len = static_cast<int>(std::string(text).size());
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text), len);
        EXPECT_FALSE(Base64::IsValid(src)) << text;
        int decodedLength = -1;
        EXPECT_FALSE(Base64::IsValid(src, decodedLength)) << text;
    }
}

// The char overloads share decodeCore/validateCore, so they inherit the rule.
TEST(Base64Test, DecodeFromChars_NoncanonicalFinalQuantum_IsInvalid) {
    const char* b64 = "AB==";
    ReadOnlySpan<char> src(b64, 4);
    std::vector<uint8_t> out(8);
    Span<uint8_t> dst(out.data(), 8);
    int consumed = 0, written = 0;
    EXPECT_EQ(Base64::DecodeFromChars(src, dst, consumed, written, true), OperationStatus::InvalidData);
    EXPECT_FALSE(Base64::IsValid(ReadOnlySpan<char>(b64, 4)));
}

// Canonical spellings of the same payload lengths must be untouched.
TEST(Base64Test, Decode_CanonicalFinalQuantum_StillDecodes) {
    struct { const char* text; int expectedBytes; } cases[] = {
        {"AA==", 1}, {"AAA=", 2}, {"QQ==", 1}, {"SGk=", 2}, {"SGVsbG8=", 5}, {"TWFu", 3},
    };
    for (const auto& c : cases) {
        const auto len = static_cast<int>(std::string(c.text).size());
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(c.text), len);
        std::vector<uint8_t> out(16);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = 0, written = 0;
        EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::Done) << c.text;
        EXPECT_EQ(written, c.expectedBytes) << c.text;
        EXPECT_TRUE(Base64::IsValid(src)) << c.text;
    }
}

// Everything this repository's own encoder produces must stay decodable: an encoder
// never emits nonzero unused bits, so the new rule must not touch a round trip.
TEST(Base64Test, Decode_AcceptsEveryEncodedLength_AfterCanonicalRule) {
    for (int n = 0; n <= 24; ++n) {
        std::vector<uint8_t> data;
        for (int i = 0; i < n; ++i) data.push_back(static_cast<uint8_t>((i * 37 + 11) & 0xFF));
        const std::string encoded = Base64::EncodeToString(data);
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(encoded.data()),
                                  static_cast<int>(encoded.size()));
        EXPECT_TRUE(Base64::IsValid(src)) << "length " << n;
        std::vector<uint8_t> out(64);
        Span<uint8_t> dst(out.data(), 64);
        int consumed = 0, written = 0;
        ASSERT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::Done)
            << "length " << n;
        ASSERT_EQ(written, n) << "length " << n;
        for (int i = 0; i < n; ++i) EXPECT_EQ(out[static_cast<size_t>(i)], data[static_cast<size_t>(i)]);
    }
}

// ---------------------------------------------------------------------------
// Ticket #1818 / SR-AUD-080: '=' is invalid while isFinalBlock is false.
//
// decodeCore used to consult isFinalBlock only after an INCOMPLETE unpadded group,
// so a COMPLETE padded group decoded to Done regardless of the flag and a chunked
// caller was told a terminal quantum was ordinary intermediate data. .NET never
// reaches its padding handler with the flag clear (Base64DecoderHelper's
// `skipLastChunk = isFinalBlock ? 4 : 0`, and DecodeWithWhiteSpaceBlockwise forcing
// its per-block localIsFinalBlock back to false), so '=' is simply an unmapped
// character there. Measured: build-probe/1818_prefix_defects.log (before) and
// build-probe/1818_postfix_defects.log (after).
// ---------------------------------------------------------------------------

TEST(Base64Test, DecodeFromUtf8_PaddedQuantum_NotFinalBlock_IsInvalidData) {
    struct { const char* text; int expectedConsumed; int expectedWritten; } cases[] = {
        {"QQ==",      0, 0},  // two '=' -- the finding's own example
        {"QUJDQQ==",  4, 3},  // a padded quantum after a complete one: the prefix stays decoded
        {"QUJDQUI=",  4, 3},  // a single '='
        {"QQ==QUJD",  0, 0},  // padding in a non-terminal position
        {"QQ =  = ",  0, 0},  // padding split by whitespace
    };
    for (const auto& c : cases) {
        const auto len = static_cast<int>(std::string(c.text).size());
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(c.text), len);
        std::vector<uint8_t> out(16, 0xCC);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = -1, written = -1;
        EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, false),
                  OperationStatus::InvalidData) << c.text;
        EXPECT_EQ(consumed, c.expectedConsumed) << c.text;
        EXPECT_EQ(written, c.expectedWritten) << c.text;
    }
}

// The char overload shares decodeCore, so it inherits the rule rather than needing its own.
TEST(Base64Test, DecodeFromChars_PaddedQuantum_NotFinalBlock_IsInvalidData) {
    for (const char* text : {"QQ==", "QUJDQQ==", "QUJDQUI=", "QQ==QUJD", "QQ =  = "}) {
        const auto len = static_cast<int>(std::string(text).size());
        ReadOnlySpan<char> src(text, len);
        std::vector<uint8_t> out(16, 0xCC);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = -1, written = -1;
        EXPECT_EQ(Base64::DecodeFromChars(src, dst, consumed, written, false),
                  OperationStatus::InvalidData) << text;
    }
}

// The narrowing must be confined to isFinalBlock == false: every one of those inputs
// keeps exactly the status, cursor and payload it had before the ticket when the flag
// is set. "QQ==QUJD" is InvalidData either way -- non-whitespace after padding.
TEST(Base64Test, DecodeFromUtf8_PaddedQuantum_FinalBlock_Unchanged) {
    struct { const char* text; OperationStatus status; int consumed; int written; const char* bytes; } cases[] = {
        {"QQ==",     OperationStatus::Done,        4, 1, "A"},
        {"QUJDQQ==", OperationStatus::Done,        8, 4, "ABCA"},
        {"QUJDQUI=", OperationStatus::Done,        8, 5, "ABCAB"},
        {"QQ==QUJD", OperationStatus::InvalidData, 4, 1, "A"},
        {"QQ =  = ", OperationStatus::Done,        8, 1, "A"},
        {"QUJD QQ==",OperationStatus::Done,        9, 4, "ABCA"},
    };
    for (const auto& c : cases) {
        const auto len = static_cast<int>(std::string(c.text).size());
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(c.text), len);
        std::vector<uint8_t> out(16, 0xCC);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = -1, written = -1;
        EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), c.status) << c.text;
        EXPECT_EQ(consumed, c.consumed) << c.text;
        EXPECT_EQ(written, c.written) << c.text;
        const std::string expected(c.bytes);
        for (int i = 0; i < written; ++i)
            EXPECT_EQ(out[static_cast<size_t>(i)], static_cast<uint8_t>(expected[static_cast<size_t>(i)])) << c.text;
    }
}

// An unpadded incomplete quantum with the flag clear must still be NeedMoreData: the
// ticket narrows padding only, never the streaming contract the flag exists for.
TEST(Base64Test, DecodeFromUtf8_UnpaddedIncompleteQuantum_NotFinalBlock_StillNeedsMoreData) {
    struct { const char* text; OperationStatus status; int consumed; int written; } cases[] = {
        {"Q",        OperationStatus::NeedMoreData, 0, 0},
        {"QQ",       OperationStatus::NeedMoreData, 0, 0},
        {"QQQ",      OperationStatus::NeedMoreData, 0, 0},
        {"QUJDQQ",   OperationStatus::NeedMoreData, 4, 3},
        {"QUJD",     OperationStatus::Done,         4, 3},
        {"",         OperationStatus::Done,         0, 0},
    };
    for (const auto& c : cases) {
        const auto len = static_cast<int>(std::string(c.text).size());
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(c.text), len);
        std::vector<uint8_t> out(16, 0xCC);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = -1, written = -1;
        EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, false), c.status) << c.text;
        EXPECT_EQ(consumed, c.consumed) << c.text;
        EXPECT_EQ(written, c.written) << c.text;
    }
}

// A destination too small to hold even the first quantum must not let the padding
// rejection be skipped: the flag is checked at the '=', before any size arithmetic.
TEST(Base64Test, DecodeFromUtf8_PaddedQuantum_NotFinalBlock_BeatsDestinationTooSmall) {
    const char* text = "QQ==";
    ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text), 4);
    std::vector<uint8_t> out(1, 0xCC);
    Span<uint8_t> dst(out.data(), 0);
    int consumed = -1, written = -1;
    EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, false), OperationStatus::InvalidData);
    EXPECT_EQ(out[0], 0xCC);
}

// IsValid has no isFinalBlock parameter, so it is the isFinalBlock == true decoder's
// validator and must be unaffected by this ticket.
TEST(Base64Test, IsValid_UnaffectedByNonFinalPaddingRule) {
    for (const char* text : {"QQ==", "QUJDQQ==", "QUJDQUI=", "QQ =  = "}) {
        const auto len = static_cast<int>(std::string(text).size());
        EXPECT_TRUE(Base64::IsValid(ReadOnlySpan<uint8_t>(
            reinterpret_cast<const uint8_t*>(text), len))) << text;
    }
    EXPECT_FALSE(Base64::IsValid(ReadOnlySpan<uint8_t>(
        reinterpret_cast<const uint8_t*>("QQ==QUJD"), 8)));
}

// A two-chunk stream is the shape the flag exists for: the non-final chunk must not
// carry padding, and the final chunk decodes normally.
TEST(Base64Test, DecodeFromUtf8_TwoChunkStream_PaddingOnlyInTheFinalChunk) {
    std::vector<uint8_t> out(16, 0xCC);
    Span<uint8_t> dst(out.data(), 16);
    int consumed = -1, written = -1;

    ReadOnlySpan<uint8_t> chunk1(reinterpret_cast<const uint8_t*>("QUJD"), 4);
    EXPECT_EQ(Base64::DecodeFromUtf8(chunk1, dst, consumed, written, false), OperationStatus::Done);
    EXPECT_EQ(consumed, 4);
    EXPECT_EQ(written, 3);

    Span<uint8_t> rest(out.data() + written, 16 - written);
    ReadOnlySpan<uint8_t> chunk2(reinterpret_cast<const uint8_t*>("QQ=="), 4);
    int consumed2 = -1, written2 = -1;
    EXPECT_EQ(Base64::DecodeFromUtf8(chunk2, rest, consumed2, written2, true), OperationStatus::Done);
    EXPECT_EQ(consumed2, 4);
    EXPECT_EQ(written2, 1);
    EXPECT_EQ(out[0], 'A'); EXPECT_EQ(out[1], 'B'); EXPECT_EQ(out[2], 'C'); EXPECT_EQ(out[3], 'A');
}

// ---------------------------------------------------------------------------
// Ticket #1819 / SR-AUD-081: NOT a defect -- these tests pin the .NET-verified
// behaviour so the finding's premise cannot be re-applied by mistake.
//
// SR-AUD-081 claims that whitespace following a padded quantum must be left in the
// input for the enclosing parser, citing "the current .NET Base64 test base". That
// test base says the opposite. Its member data is literally named
// BasicDecodingWithExtraWhitespaceShouldBeCountedInConsumedBytes_MemberData and
// yields { "AQ==" + whitespace(i), 4 + i, 1 }; a second test,
// DecodingWithWhiteSpaceSplitFinalQuantumAndIsFinalBlockFalse, decodes "AQ\r\nQ=\r\n"
// and asserts bytesConsumed == the whole input length. Measured against this port:
// build-probe/1819_defects.cpp, log build-probe/1819_defects.log -- 27 of 27
// whitespace-consumption vectors already match current .NET.
// ---------------------------------------------------------------------------

TEST(Base64Test, DecodeFromUtf8_TrailingWhitespaceAfterPadding_IsConsumed) {
    // .NET's own vector shape: "AQ==" plus i whitespace characters, expecting 4 + i.
    const std::string whitespace = " \n\t\r";
    for (int i = 0; i <= 4; ++i) {
        const std::string text = "AQ==" + whitespace.substr(0, static_cast<size_t>(i));
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text.data()),
                                  static_cast<int>(text.size()));
        std::vector<uint8_t> out(16, 0xCC);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = -1, written = -1;
        EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::Done) << i;
        EXPECT_EQ(consumed, 4 + i) << i;
        EXPECT_EQ(written, 1) << i;
        EXPECT_EQ(out[0], 0x01) << i;
    }
}

// The other half of the same .NET member data: four repeats of one whitespace
// placement, expecting the whole input consumed and 12 bytes written.
TEST(Base64Test, DecodeFromUtf8_WhitespacePlacements_ConsumeTheWholeInput) {
    for (const char* piece : {"MTIz", "M TIz", "MT Iz", "MTI z", "MTIz ", "M    TI   z", "M T I Z "}) {
        const std::string one(piece);
        const std::string text = one + one + one + one;
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text.data()),
                                  static_cast<int>(text.size()));
        std::vector<uint8_t> out(16, 0xCC);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = -1, written = -1;
        EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::Done) << piece;
        EXPECT_EQ(consumed, static_cast<int>(text.size())) << piece;
        EXPECT_EQ(written, 12) << piece;
    }
}

// .NET DecodingWithWhiteSpaceSplitFinalQuantumAndIsFinalBlockFalse, the isFinalBlock
// == true half: a final quantum split by whitespace, with or without whitespace after
// the padding, consumes the entire input.
TEST(Base64Test, DecodeFromUtf8_WhitespaceSplitFinalQuantum_ConsumesTheWholeInput) {
    for (const char* text : {"AQ\r\nQ=", "AQ\r\nQ=\r\n", "AQ Q=", "AQ\tQ="}) {
        const auto len = static_cast<int>(std::string(text).size());
        ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text), len);
        std::vector<uint8_t> out(16, 0xCC);
        Span<uint8_t> dst(out.data(), 16);
        int consumed = -1, written = -1;
        EXPECT_EQ(Base64::DecodeFromUtf8(src, dst, consumed, written, true), OperationStatus::Done) << text;
        EXPECT_EQ(consumed, len) << text;
        EXPECT_EQ(written, 2) << text;
        EXPECT_EQ(out[0], 0x01) << text;
        EXPECT_EQ(out[1], 0x04) << text;
    }
}

// The char overload shares decodeCore, so the cursor rule is one rule, not two.
TEST(Base64Test, DecodeFromChars_TrailingWhitespaceAfterPadding_IsConsumed) {
    const char* text = "QQ== \n";
    ReadOnlySpan<char> src(text, 6);
    std::vector<uint8_t> out(16, 0xCC);
    Span<uint8_t> dst(out.data(), 16);
    int consumed = -1, written = -1;
    EXPECT_EQ(Base64::DecodeFromChars(src, dst, consumed, written, true), OperationStatus::Done);
    EXPECT_EQ(consumed, 6);
    EXPECT_EQ(written, 1);
}
