// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include "System/IO/Hashing/Crc32.hpp"
#include "System/IO/Hashing/XxHash32.hpp"
#include "System/IO/Hashing/XxHash64.hpp"

using System::IO::Hashing::Crc32;
using System::IO::Hashing::XxHash32;
using System::IO::Hashing::XxHash64;

static std::vector<uint8_t> bytes(const char* s) {
    return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(s),
                                reinterpret_cast<const uint8_t*>(s) + __builtin_strlen(s));
}

// ---------------------------------------------------------------------------
// CRC32
// ---------------------------------------------------------------------------

TEST(HashingTests, Crc32EmptyInput) {
    // Empty: CRC starts at 0xFFFFFFFF XOR'd with 0xFFFFFFFF = 0
    EXPECT_EQ(Crc32::HashToUInt32({}), 0x00000000u);
}

TEST(HashingTests, Crc32StandardVector) {
    // ISO 3309 / ITU-T V.42 standard CRC-32 check value for "123456789"
    EXPECT_EQ(Crc32::HashToUInt32(bytes("123456789")), 0xCBF43926u);
}

TEST(HashingTests, Crc32Reset) {
    Crc32 h;
    auto hello = bytes("hello");
    h.Append(hello.data(), hello.size());
    uint32_t afterHello = h.GetCurrentHashAsUInt32();
    h.Reset();
    uint32_t afterReset = h.GetCurrentHashAsUInt32();
    // After reset the state is identical to a fresh instance
    Crc32 fresh;
    EXPECT_EQ(afterReset, fresh.GetCurrentHashAsUInt32());
    EXPECT_EQ(afterReset, 0x00000000u);
    // Confirm the hash before reset was different from empty
    EXPECT_NE(afterHello, 0x00000000u);
}

TEST(HashingTests, Crc32StreamingMatchesOneShot) {
    // Append in two chunks must match a single one-shot call
    std::vector<uint8_t> full = bytes("123456789");
    uint32_t oneShot = Crc32::HashToUInt32(full);

    Crc32 h;
    h.Append(full.data(), 4);
    h.Append(full.data() + 4, full.size() - 4);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), oneShot);
}

TEST(HashingTests, Crc32DifferentInputsDifferentHashes) {
    EXPECT_NE(Crc32::HashToUInt32(bytes("abc")),
              Crc32::HashToUInt32(bytes("abd")));
}

// Official .NET runtime test vectors (little-endian uint32 of the byte output)
TEST(HashingTests, Crc32OfficialVectors) {
    // Single byte 0x01 → 0xA505DF1B  (bytes {0x1B,0xDF,0x05,0xA5} little-endian)
    EXPECT_EQ(Crc32::HashToUInt32({0x01}), 0xA505DF1Bu);
    // "The quick brown fox jumps over the lazy dog"
    EXPECT_EQ(Crc32::HashToUInt32(bytes("The quick brown fox jumps over the lazy dog")),
              0x414FA339u);
}

// ---------------------------------------------------------------------------
// XxHash32
// ---------------------------------------------------------------------------

TEST(HashingTests, XxHash32EmptyInput) {
    // Canonical spec test vector: seed=0, empty input
    XxHash32 h;
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), 0x02CC5D05u);
}

TEST(HashingTests, XxHash32EmptyInputStaticHelper) {
    EXPECT_EQ(XxHash32::HashToUInt32({}), 0x02CC5D05u);
}

TEST(HashingTests, XxHash32NonZeroSeedDiffers) {
    XxHash32 h0(0);
    XxHash32 h1(1);
    EXPECT_NE(h0.GetCurrentHashAsUInt32(), h1.GetCurrentHashAsUInt32());
}

TEST(HashingTests, XxHash32Reset) {
    XxHash32 h;
    auto hello = bytes("hello");
    h.Append(hello.data(), hello.size());
    h.Reset();
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), 0x02CC5D05u);
}

TEST(HashingTests, XxHash32StreamingMatchesOneShot) {
    std::vector<uint8_t> full = bytes("The quick brown fox jumps over the lazy dog");
    uint32_t oneShot = XxHash32::HashToUInt32(full);

    XxHash32 h;
    // Feed in three chunks to exercise the partial-block buffering
    h.Append(full.data(), 10);
    h.Append(full.data() + 10, 20);
    h.Append(full.data() + 30, full.size() - 30);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), oneShot);
}

TEST(HashingTests, XxHash32SingleByteStreamingMatchesOneShot) {
    std::vector<uint8_t> data = bytes("abc");
    uint32_t oneShot = XxHash32::HashToUInt32(data);

    XxHash32 h;
    for (auto b : data) h.Append(&b, 1);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), oneShot);
}

TEST(HashingTests, XxHash32LargeInputStreamingMatchesOneShot) {
    // 100 bytes — exercises the 16-byte block processing path
    std::vector<uint8_t> data(100);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i);
    uint32_t oneShot = XxHash32::HashToUInt32(data);

    XxHash32 h;
    h.Append(data.data(), 50);
    h.Append(data.data() + 50, 50);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), oneShot);
}

TEST(HashingTests, XxHash32DifferentInputsDifferentHashes) {
    EXPECT_NE(XxHash32::HashToUInt32(bytes("abc")),
              XxHash32::HashToUInt32(bytes("abd")));
}

// Official .NET runtime test vectors (seed=0)
TEST(HashingTests, XxHash32OfficialVectors) {
    EXPECT_EQ(XxHash32::HashToUInt32(bytes("abc")),                                          0x32D153FFu);
    EXPECT_EQ(XxHash32::HashToUInt32(bytes("Nobody inspects the spammish repetition")),      0xE2293B2Fu);
    EXPECT_EQ(XxHash32::HashToUInt32(bytes("The quick brown fox jumps over the lazy dog")),  0xE85EA4DEu);
    EXPECT_EQ(XxHash32::HashToUInt32(bytes("The quick brown fox jumps over the lazy dog.")), 0x68D039C8u);
}

TEST(HashingTests, XxHash32GetHashLengthInBytes) {
    XxHash32 h;
    EXPECT_EQ(h.getHashLengthInBytesProperty(), 4);
}

// ---------------------------------------------------------------------------
// XxHash64
// ---------------------------------------------------------------------------

TEST(HashingTests, XxHash64EmptyInput) {
    // Canonical spec test vector: seed=0, empty input
    XxHash64 h;
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), 0xEF46DB3751D8E999ULL);
}

TEST(HashingTests, XxHash64EmptyInputStaticHelper) {
    EXPECT_EQ(XxHash64::HashToUInt64({}), 0xEF46DB3751D8E999ULL);
}

TEST(HashingTests, XxHash64NonZeroSeedDiffers) {
    XxHash64 h0(0);
    XxHash64 h1(1);
    EXPECT_NE(h0.GetCurrentHashAsUInt64(), h1.GetCurrentHashAsUInt64());
}

TEST(HashingTests, XxHash64Reset) {
    XxHash64 h;
    auto hello = bytes("hello");
    h.Append(hello.data(), hello.size());
    h.Reset();
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), 0xEF46DB3751D8E999ULL);
}

TEST(HashingTests, XxHash64StreamingMatchesOneShot) {
    std::vector<uint8_t> full = bytes("The quick brown fox jumps over the lazy dog");
    uint64_t oneShot = XxHash64::HashToUInt64(full);

    XxHash64 h;
    h.Append(full.data(), 10);
    h.Append(full.data() + 10, 20);
    h.Append(full.data() + 30, full.size() - 30);
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), oneShot);
}

TEST(HashingTests, XxHash64SingleByteStreamingMatchesOneShot) {
    std::vector<uint8_t> data = bytes("abc");
    uint64_t oneShot = XxHash64::HashToUInt64(data);

    XxHash64 h;
    for (auto b : data) h.Append(&b, 1);
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), oneShot);
}

TEST(HashingTests, XxHash64LargeInputStreamingMatchesOneShot) {
    // 100 bytes — exercises the 32-byte block processing path
    std::vector<uint8_t> data(100);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i);
    uint64_t oneShot = XxHash64::HashToUInt64(data);

    XxHash64 h;
    h.Append(data.data(), 50);
    h.Append(data.data() + 50, 50);
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), oneShot);
}

TEST(HashingTests, XxHash64DifferentInputsDifferentHashes) {
    EXPECT_NE(XxHash64::HashToUInt64(bytes("abc")),
              XxHash64::HashToUInt64(bytes("abd")));
}

// Official .NET runtime test vectors (seed=0)
TEST(HashingTests, XxHash64OfficialVectors) {
    EXPECT_EQ(XxHash64::HashToUInt64(bytes("abc")),                                          0x44BC2CF5AD770999ULL);
    EXPECT_EQ(XxHash64::HashToUInt64(bytes("Nobody inspects the spammish repetition")),      0xFBCEA83C8A378BF1ULL);
    EXPECT_EQ(XxHash64::HashToUInt64(bytes("The quick brown fox jumps over the lazy dog")),  0x0B242D361FDA71BCULL);
    EXPECT_EQ(XxHash64::HashToUInt64(bytes("The quick brown fox jumps over the lazy dog.")), 0x44AD33705751AD73ULL);
}

TEST(HashingTests, XxHash64GetHashLengthInBytes) {
    XxHash64 h;
    EXPECT_EQ(h.getHashLengthInBytesProperty(), 8);
}

TEST(HashingTests, XxHash64HashesAreDistinctFromXxHash32) {
    // Sanity: 64-bit hash of same input != lower 32 bits of XxHash32 (generally)
    std::vector<uint8_t> data = bytes("hello");
    uint64_t h64 = XxHash64::HashToUInt64(data);
    uint32_t h32 = XxHash32::HashToUInt32(data);
    // They come from different algorithms — the 32-bit hash must not equal the lower word of the 64-bit hash
    EXPECT_NE(static_cast<uint32_t>(h64 & 0xFFFFFFFF), h32);
}
