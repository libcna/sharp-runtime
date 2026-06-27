// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/Text/Base64Url.hpp"

using System::Buffers::Text::Base64Url;
using System::Buffers::OperationStatus;
using System::ReadOnlySpan;
using System::Span;

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
    EXPECT_EQ(Base64Url::GetMaxDecodedLength(7), 6);
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
