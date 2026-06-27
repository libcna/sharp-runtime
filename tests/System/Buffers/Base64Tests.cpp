// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/Text/Base64.hpp"

using System::Buffers::Text::Base64;
using System::Buffers::OperationStatus;
using System::ReadOnlySpan;
using System::Span;

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
