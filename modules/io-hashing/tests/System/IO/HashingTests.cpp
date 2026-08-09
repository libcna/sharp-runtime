// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cstdint>
#include <random>
#include <string>

#include "System/IO/Hashing/Adler32.hpp"
#include "System/IO/Hashing/Crc32.hpp"
#include "System/IO/Hashing/Crc32ParameterSet.hpp"
#include "System/IO/Hashing/Crc64.hpp"
#include "System/IO/Hashing/Crc64ParameterSet.hpp"
#include "System/IO/Hashing/XxHash32.hpp"
#include "System/IO/Hashing/XxHash64.hpp"
#include "System/IO/Hashing/XxHash3.hpp"
#include "System/IO/Hashing/XxHash128.hpp"
#include "System/IO/Hashing/HashingByteOrder.hpp"
#include "System/IO/Hashing/XxHash3Shared.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ArgumentException.hpp"
#include "System/Exception.hpp"

using System::IO::Hashing::Adler32;
using System::IO::Hashing::Crc32;
using System::IO::Hashing::Crc32ParameterSet;
using System::IO::Hashing::Crc64;
using System::IO::Hashing::Crc64ParameterSet;
using System::IO::Hashing::XxHash32;
using System::IO::Hashing::XxHash64;
using System::IO::Hashing::XxHash3;
using System::IO::Hashing::XxHash128;

static std::vector<uint8_t> bytes(const char* s) {
    return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(s),
                                reinterpret_cast<const uint8_t*>(s) + __builtin_strlen(s));
}

static uint32_t crc32Hash(const std::vector<uint8_t>& v) {
    return Crc32::HashToUInt32(v.data(), static_cast<int32_t>(v.size()));
}
static uint32_t xxHash32Hash(const std::vector<uint8_t>& v) {
    return XxHash32::HashToUInt32(v.data(), static_cast<int32_t>(v.size()));
}
static uint64_t xxHash64Hash(const std::vector<uint8_t>& v) {
    return XxHash64::HashToUInt64(v.data(), static_cast<int32_t>(v.size()));
}

// ---------------------------------------------------------------------------
// CRC32
// ---------------------------------------------------------------------------

TEST(HashingTests, Crc32EmptyInput) {
    // Empty: CRC starts at 0xFFFFFFFF XOR'd with 0xFFFFFFFF = 0
    EXPECT_EQ(crc32Hash({}), 0x00000000u);
}

TEST(HashingTests, Crc32StandardVector) {
    // ISO 3309 / ITU-T V.42 standard CRC-32 check value for "123456789"
    EXPECT_EQ(crc32Hash(bytes("123456789")), 0xCBF43926u);
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
    uint32_t oneShot = crc32Hash(full);

    Crc32 h;
    h.Append(full.data(), 4);
    h.Append(full.data() + 4, full.size() - 4);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), oneShot);
}

TEST(HashingTests, Crc32DifferentInputsDifferentHashes) {
    EXPECT_NE(crc32Hash(bytes("abc")),
              crc32Hash(bytes("abd")));
}

// Official .NET runtime test vectors (little-endian uint32 of the byte output)
TEST(HashingTests, Crc32OfficialVectors) {
    // Single byte 0x01 → 0xA505DF1B  (bytes {0x1B,0xDF,0x05,0xA5} little-endian)
    EXPECT_EQ(crc32Hash({0x01}), 0xA505DF1Bu);
    // "The quick brown fox jumps over the lazy dog"
    EXPECT_EQ(crc32Hash(bytes("The quick brown fox jumps over the lazy dog")),
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
    EXPECT_EQ(xxHash32Hash({}), 0x02CC5D05u);
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
    uint32_t oneShot = xxHash32Hash(full);

    XxHash32 h;
    // Feed in three chunks to exercise the partial-block buffering
    h.Append(full.data(), 10);
    h.Append(full.data() + 10, 20);
    h.Append(full.data() + 30, full.size() - 30);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), oneShot);
}

TEST(HashingTests, XxHash32SingleByteStreamingMatchesOneShot) {
    std::vector<uint8_t> data = bytes("abc");
    uint32_t oneShot = xxHash32Hash(data);

    XxHash32 h;
    for (auto b : data) h.Append(&b, 1);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), oneShot);
}

TEST(HashingTests, XxHash32LargeInputStreamingMatchesOneShot) {
    // 100 bytes — exercises the 16-byte block processing path
    std::vector<uint8_t> data(100);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i);
    uint32_t oneShot = xxHash32Hash(data);

    XxHash32 h;
    h.Append(data.data(), 50);
    h.Append(data.data() + 50, 50);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), oneShot);
}

TEST(HashingTests, XxHash32DifferentInputsDifferentHashes) {
    EXPECT_NE(xxHash32Hash(bytes("abc")),
              xxHash32Hash(bytes("abd")));
}

// Official .NET runtime test vectors (seed=0)
TEST(HashingTests, XxHash32OfficialVectors) {
    EXPECT_EQ(xxHash32Hash(bytes("abc")),                                          0x32D153FFu);
    EXPECT_EQ(xxHash32Hash(bytes("Nobody inspects the spammish repetition")),      0xE2293B2Fu);
    EXPECT_EQ(xxHash32Hash(bytes("The quick brown fox jumps over the lazy dog")),  0xE85EA4DEu);
    EXPECT_EQ(xxHash32Hash(bytes("The quick brown fox jumps over the lazy dog.")), 0x68D039C8u);
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
    EXPECT_EQ(xxHash64Hash({}), 0xEF46DB3751D8E999ULL);
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
    uint64_t oneShot = xxHash64Hash(full);

    XxHash64 h;
    h.Append(full.data(), 10);
    h.Append(full.data() + 10, 20);
    h.Append(full.data() + 30, full.size() - 30);
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), oneShot);
}

TEST(HashingTests, XxHash64SingleByteStreamingMatchesOneShot) {
    std::vector<uint8_t> data = bytes("abc");
    uint64_t oneShot = xxHash64Hash(data);

    XxHash64 h;
    for (auto b : data) h.Append(&b, 1);
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), oneShot);
}

TEST(HashingTests, XxHash64LargeInputStreamingMatchesOneShot) {
    // 100 bytes — exercises the 32-byte block processing path
    std::vector<uint8_t> data(100);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i);
    uint64_t oneShot = xxHash64Hash(data);

    XxHash64 h;
    h.Append(data.data(), 50);
    h.Append(data.data() + 50, 50);
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), oneShot);
}

TEST(HashingTests, XxHash64DifferentInputsDifferentHashes) {
    EXPECT_NE(xxHash64Hash(bytes("abc")),
              xxHash64Hash(bytes("abd")));
}

// Official .NET runtime test vectors (seed=0)
TEST(HashingTests, XxHash64OfficialVectors) {
    EXPECT_EQ(xxHash64Hash(bytes("abc")),                                          0x44BC2CF5AD770999ULL);
    EXPECT_EQ(xxHash64Hash(bytes("Nobody inspects the spammish repetition")),      0xFBCEA83C8A378BF1ULL);
    EXPECT_EQ(xxHash64Hash(bytes("The quick brown fox jumps over the lazy dog")),  0x0B242D361FDA71BCULL);
    EXPECT_EQ(xxHash64Hash(bytes("The quick brown fox jumps over the lazy dog.")), 0x44AD33705751AD73ULL);
}

TEST(HashingTests, XxHash64GetHashLengthInBytes) {
    XxHash64 h;
    EXPECT_EQ(h.getHashLengthInBytesProperty(), 8);
}

TEST(HashingTests, XxHash64HashesAreDistinctFromXxHash32) {
    // Sanity: 64-bit hash of same input != lower 32 bits of XxHash32 (generally)
    std::vector<uint8_t> data = bytes("hello");
    uint64_t h64 = xxHash64Hash(data);
    uint32_t h32 = xxHash32Hash(data);
    // They come from different algorithms — the 32-bit hash must not equal the lower word of the 64-bit hash
    EXPECT_NE(static_cast<uint32_t>(h64 & 0xFFFFFFFF), h32);
}

TEST(HashingTests, XxHash32_GetCurrentHash_IsBigEndianBytesOfHashValue) {
    // Regression: GetCurrentHashCore previously wrote the raw uint32_t via memcpy (host byte
    // order), but .NET's XxHash32 always writes big-endian, regardless of host endianness.
    XxHash32 h;
    auto bytesOut = h.GetCurrentHash();
    ASSERT_EQ(bytesOut.size(), 4u);
    uint32_t expected = h.GetCurrentHashAsUInt32();
    uint32_t fromBytes = (static_cast<uint32_t>(bytesOut[0]) << 24) |
                         (static_cast<uint32_t>(bytesOut[1]) << 16) |
                         (static_cast<uint32_t>(bytesOut[2]) << 8)  |
                          static_cast<uint32_t>(bytesOut[3]);
    EXPECT_EQ(fromBytes, expected);
}

TEST(HashingTests, XxHash64_GetCurrentHash_IsBigEndianBytesOfHashValue) {
    XxHash64 h;
    auto bytesOut = h.GetCurrentHash();
    ASSERT_EQ(bytesOut.size(), 8u);
    uint64_t expected = h.GetCurrentHashAsUInt64();
    uint64_t fromBytes = 0;
    for (int i = 0; i < 8; ++i) fromBytes = (fromBytes << 8) | bytesOut[static_cast<size_t>(i)];
    EXPECT_EQ(fromBytes, expected);
}

TEST(HashingTests, XxHash32_Clone_IndependentState) {
    XxHash32 h;
    auto hello = bytes("hello");
    h.Append(hello.data(), hello.size());
    XxHash32 clone = h.Clone();
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), clone.GetCurrentHashAsUInt32());
    auto world = bytes("world");
    clone.Append(world.data(), world.size());
    EXPECT_NE(h.GetCurrentHashAsUInt32(), clone.GetCurrentHashAsUInt32());
}

// ---------------------------------------------------------------------------
// NonCryptographicHashAlgorithm base class API
// ---------------------------------------------------------------------------

TEST(HashingTests, TryGetCurrentHash_TooShort_ReturnsFalse) {
    Crc32 h;
    uint8_t buf[2];
    int32_t written = -1;
    EXPECT_FALSE(h.TryGetCurrentHash(buf, 2, written));
    EXPECT_EQ(written, 0);
}

TEST(HashingTests, GetCurrentHash_TooShort_Throws) {
    Crc32 h;
    uint8_t buf[2];
    EXPECT_THROW(h.GetCurrentHash(buf, 2), System::ArgumentException);
}

TEST(HashingTests, TryGetHashAndReset_Succeeds_ResetsState) {
    Crc32 h;
    auto data = bytes("hello");
    h.Append(data.data(), static_cast<int32_t>(data.size()));
    uint8_t buf[4];
    int32_t written = 0;
    ASSERT_TRUE(h.TryGetHashAndReset(buf, 4, written));
    EXPECT_EQ(written, 4);
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), Crc32().GetCurrentHashAsUInt32());
}

// ---------------------------------------------------------------------------
// Adler32 (RFC 1950)
// ---------------------------------------------------------------------------

TEST(HashingTests, Adler32_EmptyInput_IsOne) {
    EXPECT_EQ(Adler32::HashToUInt32(nullptr, 0), 0x00000001u);
}

TEST(HashingTests, Adler32_WikipediaExample) {
    // Well-known Adler-32 check value for the ASCII string "Wikipedia".
    auto data = bytes("Wikipedia");
    EXPECT_EQ(Adler32::HashToUInt32(data.data(), static_cast<int32_t>(data.size())), 0x11E60398u);
}

TEST(HashingTests, Adler32_Reset) {
    Adler32 h;
    auto data = bytes("hello");
    h.Append(data.data(), static_cast<int32_t>(data.size()));
    h.Reset();
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), 0x00000001u);
}

TEST(HashingTests, Adler32_StreamingMatchesOneShot) {
    auto data = bytes("Wikipedia");
    Adler32 h;
    h.Append(data.data(), 4);
    h.Append(data.data() + 4, static_cast<int32_t>(data.size() - 4));
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), 0x11E60398u);
}

TEST(HashingTests, Adler32_Clone_IndependentState) {
    Adler32 h;
    auto hello = bytes("hello");
    h.Append(hello.data(), static_cast<int32_t>(hello.size()));
    Adler32 clone = h.Clone();
    auto world = bytes("world");
    clone.Append(world.data(), static_cast<int32_t>(world.size()));
    EXPECT_NE(h.GetCurrentHashAsUInt32(), clone.GetCurrentHashAsUInt32());
}

// ---------------------------------------------------------------------------
// Crc32ParameterSet / Crc32C
// ---------------------------------------------------------------------------

TEST(HashingTests, Crc32C_OfficialCheckValue) {
    // .NET official test vector for CRC-32C, "123456789" -> byte output "839206E3"
    // (little-endian reflected output) => UInt32 value 0xE3069283.
    auto data = bytes("123456789");
    auto crc32c = Crc32ParameterSet::getCrc32CProperty();
    Crc32 h(crc32c);
    h.Append(data.data(), static_cast<int32_t>(data.size()));
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), 0xE3069283u);
}

TEST(HashingTests, Crc32_DefaultParameterSet_MatchesWellKnown) {
    EXPECT_EQ(Crc32ParameterSet::getCrc32Property()->getPolynomialProperty(), 0x04c11db7u);
    EXPECT_EQ(Crc32ParameterSet::getCrc32Property()->getInitialValueProperty(), 0xffffffffu);
    EXPECT_TRUE(Crc32ParameterSet::getCrc32Property()->getReflectValuesProperty());
}

TEST(HashingTests, Crc32ParameterSet_Create_CustomParameters) {
    auto custom = Crc32ParameterSet::Create(0x04c11db7u, 0u, 0u, true);
    EXPECT_EQ(custom->getPolynomialProperty(), 0x04c11db7u);
    EXPECT_EQ(custom->getInitialValueProperty(), 0u);
    EXPECT_EQ(custom->getFinalXorValueProperty(), 0u);
}

TEST(HashingTests, Crc32_NullParameterSet_Throws) {
    EXPECT_THROW(Crc32{std::shared_ptr<Crc32ParameterSet>(nullptr)}, System::ArgumentNullException);
}

// ---------------------------------------------------------------------------
// Crc64
// ---------------------------------------------------------------------------

TEST(HashingTests, Crc64_Ecma182_OfficialCheckValue) {
    // .NET official test vector: CRC-64/ECMA-182, "123456789" -> 0x6C40DF5F0B497347
    auto data = bytes("123456789");
    Crc64 h;
    h.Append(data.data(), static_cast<int32_t>(data.size()));
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), 0x6C40DF5F0B497347ull);
}

TEST(HashingTests, Crc64_Nvme_OfficialCheckValue) {
    // .NET official test vector: CRC-64/NVMe, "123456789" -> byte output "8898790A86148BAE"
    // (little-endian reflected output) => UInt64 value 0xAE8B14860A799888.
    auto data = bytes("123456789");
    Crc64 h(Crc64ParameterSet::getNvmeProperty());
    h.Append(data.data(), static_cast<int32_t>(data.size()));
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), 0xAE8B14860A799888ull);
}

TEST(HashingTests, Crc64_Reset) {
    Crc64 h;
    auto data = bytes("hello");
    h.Append(data.data(), static_cast<int32_t>(data.size()));
    h.Reset();
    Crc64 fresh;
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), fresh.GetCurrentHashAsUInt64());
}

TEST(HashingTests, Crc64_Clone_IndependentState) {
    Crc64 h;
    auto hello = bytes("hello");
    h.Append(hello.data(), static_cast<int32_t>(hello.size()));
    Crc64 clone = h.Clone();
    auto world = bytes("world");
    clone.Append(world.data(), static_cast<int32_t>(world.size()));
    EXPECT_NE(h.GetCurrentHashAsUInt64(), clone.GetCurrentHashAsUInt64());
}

TEST(HashingTests, Crc64_NullParameterSet_Throws) {
    EXPECT_THROW(Crc64{std::shared_ptr<Crc64ParameterSet>(nullptr)}, System::ArgumentNullException);
}

// ---------------------------------------------------------------------------
// XxHash3 — official .NET test vectors (github.com/dotnet/runtime XxHash3Tests.cs)
// ---------------------------------------------------------------------------

static uint64_t xxHash3Hash(const std::vector<uint8_t>& v, int64_t seed = 0) {
    return XxHash3::HashToUInt64(v.data(), static_cast<int32_t>(v.size()), seed);
}

TEST(HashingTests, XxHash3_OfficialVector_EmptySeed0) {
    EXPECT_EQ(xxHash3Hash(bytes("")), 0x2d06800538d394c2ULL);
}

TEST(HashingTests, XxHash3_OfficialVector_EmptySeeded) {
    EXPECT_EQ(xxHash3Hash(bytes(""), 0x13f0), 0x0f99783e68de7322ULL);
}

TEST(HashingTests, XxHash3_OfficialVector_Length1) {
    EXPECT_EQ(xxHash3Hash(bytes("Z")), 0x2753d05a8f320003ULL);
}

TEST(HashingTests, XxHash3_OfficialVector_Length4Seeded) {
    EXPECT_EQ(xxHash3Hash(bytes("KdjE"), 0x499), 0x17ac41dfec94111fULL);
}

TEST(HashingTests, XxHash3_OfficialVector_Length9Seeded) {
    EXPECT_EQ(xxHash3Hash(bytes("EGM4yxHgk"), 0x6d1), 0x04661f74a752afb2ULL);
}

TEST(HashingTests, XxHash3_OfficialVector_Length17Seeded) {
    EXPECT_EQ(xxHash3Hash(bytes("4pgR1N0LgL2QpoaNc"), 0x19c9), 0x34722478aff47051ULL);
}

TEST(HashingTests, XxHash3_OfficialVector_Length129Seeded) {
    EXPECT_EQ(xxHash3Hash(bytes("uqMPHc9Ks7bAPG6zdpkbSjcqkVXzOGBSyBXTvbzQlpdQ6Zv362rdkRlzBIbV7nPyLOGutx6Q4YD2wksGcsC6GapRBMT5QOQq3Vt6NkobpxENMG3anX1cChirvogNtnej8"),
                            0x1b1d), 0xe822ad7ac402f592ULL);
}

TEST(HashingTests, XxHash3_OfficialVector_Length512Seeded) {
    // Two concatenated segments totaling 512 bytes (well over the 240-byte streaming threshold).
    std::string s =
        "a2WBB5GKMkHnAySB7uXFtoT8z1GgX0MPgzX4Zu4QWjZ5sfUPXZq3UXFUbG74Fw8Jk55geWbT9PB35YSVEbtIizEgLcpOll09vmDXlLcR6MxmGl6apvXEAodsve1dgw4eq9Lsx8LLdd5kJY1HlJL7Sd4fckloMiVNR1n8UdOzUdyZa0T0iKRc9wYvpG6py0QWwevVqXlZjwEyYmSQsHoXEtwzjUaRL5Fx15E21ANuyugJVKk7S"
        "gmi8CVrQYFQZbeF6e2POv5ZFzf1OgjLnYHp0xpfWDD8uOKH4zE882uBMshXjVSjFRY9Yv5rKdymxkR4VMTZTNPNgiuPJSKQlt0XU2NdzVdH7iTNZ3Hxt1vIyFEtqCqHjFGxXPNaPGJbioubL3hGCs5lqEOVPHHNQNxjhmzmyS3O2DsZrvXkFbD1HpLisgz6T4S3mEJ3tDhzWpAkC3UnitHpgaoJnyBa0EchtNbtekxnSBQ6yShQN3CktaPjfTfGWMmUzkYOZFoG2dYZ";
    ASSERT_EQ(s.size(), 512u);
    EXPECT_EQ(xxHash3Hash(bytes(s.c_str()), 0x21f), 0x2f37d7c5257055c5ULL);
}

TEST(HashingTests, XxHash3_EmptyInput_MatchesDefaultCtor) {
    XxHash3 h;
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), 0x2d06800538d394c2ULL);
}

TEST(HashingTests, XxHash3_Reset) {
    XxHash3 h;
    auto data = bytes("hello");
    h.Append(data.data(), static_cast<int32_t>(data.size()));
    h.Reset();
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), 0x2d06800538d394c2ULL);
}

TEST(HashingTests, XxHash3_Clone_IndependentState) {
    XxHash3 h;
    auto hello = bytes("hello");
    h.Append(hello.data(), static_cast<int32_t>(hello.size()));
    XxHash3 clone = h.Clone();
    auto world = bytes("world");
    clone.Append(world.data(), static_cast<int32_t>(world.size()));
    EXPECT_NE(h.GetCurrentHashAsUInt64(), clone.GetCurrentHashAsUInt64());
}

TEST(HashingTests, XxHash3_GetHashLengthInBytes) {
    XxHash3 h;
    EXPECT_EQ(h.getHashLengthInBytesProperty(), 8);
}

TEST(HashingTests, XxHash3_GetCurrentHash_IsBigEndianBytesOfHashValue) {
    XxHash3 h;
    auto hello = bytes("hello world, this is a somewhat longer test string for hashing");
    h.Append(hello.data(), static_cast<int32_t>(hello.size()));
    auto bytesOut = h.GetCurrentHash();
    ASSERT_EQ(bytesOut.size(), 8u);
    uint64_t expected = h.GetCurrentHashAsUInt64();
    uint64_t fromBytes = 0;
    for (int i = 0; i < 8; ++i) fromBytes = (fromBytes << 8) | bytesOut[static_cast<size_t>(i)];
    EXPECT_EQ(fromBytes, expected);
}

TEST(HashingTests, XxHash3_StreamingMatchesOneShot_Length512) {
    std::string s(512, 'x');
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<char>('a' + (i % 26));
    auto data = bytes(s.c_str());
    uint64_t oneShot = xxHash3Hash(data, 0x123);

    XxHash3 h(0x123);
    h.Append(data.data(), 37);
    h.Append(data.data() + 37, 200);
    h.Append(data.data() + 237, static_cast<int32_t>(data.size() - 237));
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), oneShot);
}

TEST(HashingTests, XxHash3_StreamingMatchesOneShot_ManySmallChunks) {
    std::string s(600, 'x');
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<char>('a' + (i % 26));
    auto data = bytes(s.c_str());
    uint64_t oneShot = xxHash3Hash(data);

    XxHash3 h;
    for (size_t i = 0; i < data.size(); ++i) h.Append(&data[i], 1);
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), oneShot);
}

TEST(HashingTests, XxHash3_DifferentInputsDifferentHashes) {
    EXPECT_NE(xxHash3Hash(bytes("abc")), xxHash3Hash(bytes("abd")));
}

TEST(HashingTests, XxHash3_TryHash_DestinationTooShort_ReturnsFalse) {
    auto data = bytes("abc");
    uint8_t dest[7];
    int32_t written = -1;
    EXPECT_FALSE(XxHash3::TryHash(data.data(), 3, dest, 7, written));
    EXPECT_EQ(written, 0);
}

TEST(HashingTests, XxHash3_Hash_DestinationTooShort_Throws) {
    auto data = bytes("abc");
    uint8_t dest[7];
    EXPECT_THROW(XxHash3::Hash(data.data(), 3, dest, 7), System::ArgumentException);
}

// Regression tests for ticket 355: a negative length previously wrapped to a huge value once
// cast to unsigned (confirmed via a standalone repro), turning the internal memcpy call into an
// immediate crash / massive out-of-bounds access -- a severe memory-safety bug, not just abstract
// UB. Fixed at both the one-shot hashing entry points (HashToUInt64Impl) and the shared streaming
// Append() (used by both XxHash3 and XxHash128).
TEST(HashingTests, XxHash3_HashToUInt64_NegativeLength_Throws) {
    auto data = bytes("abc");
    EXPECT_THROW(XxHash3::HashToUInt64(data.data(), -1), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash3_Hash_NegativeLength_Throws) {
    auto data = bytes("abc");
    EXPECT_THROW(XxHash3::Hash(data.data(), -1), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash3_Append_NegativeLength_Throws) {
    XxHash3 h;
    auto data = bytes("abc");
    EXPECT_THROW(h.Append(data.data(), -1), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash128_Append_NegativeLength_Throws) {
    XxHash128 h;
    auto data = bytes("abc");
    EXPECT_THROW(h.Append(data.data(), -1), System::ArgumentOutOfRangeException);
}

// XxHash128's one-shot entry points (Hash x3, TryHash, HashToHash128) all funnel through
// HashToHash128Impl, a SEPARATE function from XxHash3's HashToUInt64Impl -- ticket 355's fix
// did not cover it, and the streaming-path fix above (Append) does not protect these one-shot
// callers either, since they never touch Detail::XxHash3Shared::Append at all. Same bug shape,
// confirmed via a standalone ASan repro (negative length -> unsigned wraparound -> out-of-bounds
// read), found via a code-audit pass on this file.
TEST(HashingTests, XxHash128_Hash_NegativeLength_Throws) {
    auto data = bytes("abc");
    EXPECT_THROW(XxHash128::Hash(data.data(), -1), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash128_HashWithSeed_NegativeLength_Throws) {
    auto data = bytes("abc");
    EXPECT_THROW(XxHash128::Hash(data.data(), -1, 42), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash128_HashToDestination_NegativeLength_Throws) {
    auto data = bytes("abc");
    uint8_t dest[16];
    EXPECT_THROW(XxHash128::Hash(data.data(), -1, dest, 16, 0), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash128_TryHash_NegativeLength_Throws) {
    auto data = bytes("abc");
    uint8_t dest[16];
    SharpRuntime::intcs written = 0;
    EXPECT_THROW(XxHash128::TryHash(data.data(), -1, dest, 16, written, 0), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash128_HashToHash128_NegativeLength_Throws) {
    auto data = bytes("abc");
    EXPECT_THROW(XxHash128::HashToHash128(data.data(), -1, 0), System::ArgumentOutOfRangeException);
}

// XxHash32/XxHash64 have their own independent Append() buffering (not routed through
// Detail::XxHash3Shared), so ticket 355's fix did not cover them. Same bug, same fix, found via
// a ported-type-audit pass on these two types: a negative length reaches
// `static_cast<size_t>(copy)` in the mid-buffer memcpy once bufLen_ > 0 from a prior partial
// Append(), turning it into a massive out-of-bounds write. The two-call sequence below (5 bytes
// to populate bufLen_, then a negative-length call) is the exact reachable crash shape -- a
// single negative-length call on a fresh instance doesn't reach the vulnerable memcpy, so this
// regression test intentionally covers the two-call case, not just the single-call case.
TEST(HashingTests, XxHash32_Append_NegativeLength_Throws) {
    XxHash32 h;
    auto data = bytes("abc");
    EXPECT_THROW(h.Append(data.data(), -1), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash32_Append_NegativeLength_AfterPartialBuffer_Throws) {
    XxHash32 h;
    auto data = bytes("abcde");
    h.Append(data.data(), 5); // bufLen_ == 5, primes the vulnerable mid-buffer memcpy path
    EXPECT_THROW(h.Append(data.data(), -1), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash64_Append_NegativeLength_Throws) {
    XxHash64 h;
    auto data = bytes("abc");
    EXPECT_THROW(h.Append(data.data(), -1), System::ArgumentOutOfRangeException);
}

TEST(HashingTests, XxHash64_Append_NegativeLength_AfterPartialBuffer_Throws) {
    XxHash64 h;
    auto data = bytes("abcde");
    h.Append(data.data(), 5); // bufLen_ == 5, primes the vulnerable mid-buffer memcpy path
    EXPECT_THROW(h.Append(data.data(), -1), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// XxHash128 — official .NET test vectors (github.com/dotnet/runtime XxHash128Tests.cs)
// ---------------------------------------------------------------------------

static System::IO::Hashing::Hash128 xxHash128Hash(const std::vector<uint8_t>& v, int64_t seed = 0) {
    return XxHash128::HashToHash128(v.data(), static_cast<int32_t>(v.size()), seed);
}

TEST(HashingTests, XxHash128_OfficialVector_EmptySeed0) {
    auto h = xxHash128Hash(bytes(""));
    EXPECT_EQ(h.High64, 0x99aa06d3014798d8ULL);
    EXPECT_EQ(h.Low64, 0x6001c324468d497fULL);
}

TEST(HashingTests, XxHash128_OfficialVector_EmptySeeded) {
    auto h = xxHash128Hash(bytes(""), 0x13f0);
    EXPECT_EQ(h.High64, 0x4a807558806f6b31ULL);
    EXPECT_EQ(h.Low64, 0xeca8475b2cc08feeULL);
}

TEST(HashingTests, XxHash128_OfficialVector_Length1) {
    auto h = xxHash128Hash(bytes("Z"));
    EXPECT_EQ(h.High64, 0xa26f5ff5290b016cULL);
    EXPECT_EQ(h.Low64, 0x2753d05a8f320003ULL);
}

TEST(HashingTests, XxHash128_OfficialVector_Length4Seeded) {
    auto h = xxHash128Hash(bytes("KdjE"), 0x499);
    EXPECT_EQ(h.High64, 0xf29da7d603a9409fULL);
    EXPECT_EQ(h.Low64, 0x90373b3f6da10e37ULL);
}

TEST(HashingTests, XxHash128_OfficialVector_Length9Seeded) {
    auto h = xxHash128Hash(bytes("EGM4yxHgk"), 0x6d1);
    EXPECT_EQ(h.High64, 0xa98564b5fb852a84ULL);
    EXPECT_EQ(h.Low64, 0xdef8b6b70a62024cULL);
}

TEST(HashingTests, XxHash128_OfficialVector_Length17Seeded) {
    auto h = xxHash128Hash(bytes("4pgR1N0LgL2QpoaNc"), 0x19c9);
    EXPECT_EQ(h.High64, 0x1f3ba7ee629b72e0ULL);
    EXPECT_EQ(h.Low64, 0x8e26be7440f5fc90ULL);
}

TEST(HashingTests, XxHash128_EmptyInput_MatchesDefaultCtor) {
    XxHash128 h;
    auto current = h.GetCurrentHashAsHash128();
    EXPECT_EQ(current.High64, 0x99aa06d3014798d8ULL);
    EXPECT_EQ(current.Low64, 0x6001c324468d497fULL);
}

TEST(HashingTests, XxHash128_Reset) {
    XxHash128 h;
    auto data = bytes("hello");
    h.Append(data.data(), static_cast<int32_t>(data.size()));
    h.Reset();
    auto current = h.GetCurrentHashAsHash128();
    EXPECT_EQ(current.High64, 0x99aa06d3014798d8ULL);
    EXPECT_EQ(current.Low64, 0x6001c324468d497fULL);
}

TEST(HashingTests, XxHash128_Clone_IndependentState) {
    XxHash128 h;
    auto hello = bytes("hello");
    h.Append(hello.data(), static_cast<int32_t>(hello.size()));
    XxHash128 clone = h.Clone();
    auto world = bytes("world");
    clone.Append(world.data(), static_cast<int32_t>(world.size()));
    EXPECT_NE(h.GetCurrentHashAsHash128(), clone.GetCurrentHashAsHash128());
}

TEST(HashingTests, XxHash128_GetHashLengthInBytes) {
    XxHash128 h;
    EXPECT_EQ(h.getHashLengthInBytesProperty(), 16);
}

TEST(HashingTests, XxHash128_GetCurrentHash_IsBigEndianHighThenLow) {
    XxHash128 h;
    auto hello = bytes("hello world, this is a somewhat longer test string for hashing");
    h.Append(hello.data(), static_cast<int32_t>(hello.size()));
    auto bytesOut = h.GetCurrentHash();
    ASSERT_EQ(bytesOut.size(), 16u);

    auto current = h.GetCurrentHashAsHash128();
    uint64_t highFromBytes = 0, lowFromBytes = 0;
    for (int i = 0; i < 8; ++i) highFromBytes = (highFromBytes << 8) | bytesOut[static_cast<size_t>(i)];
    for (int i = 0; i < 8; ++i) lowFromBytes = (lowFromBytes << 8) | bytesOut[static_cast<size_t>(8 + i)];
    EXPECT_EQ(highFromBytes, current.High64);
    EXPECT_EQ(lowFromBytes, current.Low64);
}

TEST(HashingTests, XxHash128_StreamingMatchesOneShot_Length512) {
    std::string s(512, 'x');
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<char>('a' + (i % 26));
    auto data = bytes(s.c_str());
    auto oneShot = xxHash128Hash(data, 0x123);

    XxHash128 h(0x123);
    h.Append(data.data(), 37);
    h.Append(data.data() + 37, 200);
    h.Append(data.data() + 237, static_cast<int32_t>(data.size() - 237));
    EXPECT_EQ(h.GetCurrentHashAsHash128(), oneShot);
}

TEST(HashingTests, XxHash128_StreamingMatchesOneShot_ManySmallChunks) {
    std::string s(600, 'x');
    for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<char>('a' + (i % 26));
    auto data = bytes(s.c_str());
    auto oneShot = xxHash128Hash(data);

    XxHash128 h;
    for (size_t i = 0; i < data.size(); ++i) h.Append(&data[i], 1);
    EXPECT_EQ(h.GetCurrentHashAsHash128(), oneShot);
}

TEST(HashingTests, XxHash128_DifferentInputsDifferentHashes) {
    EXPECT_NE(xxHash128Hash(bytes("abc")), xxHash128Hash(bytes("abd")));
}

// XxHash3Shared.cpp's streaming Append() path (buffer completion, per-block consumption,
// multi-block loops, tail-stripe buffering) is complex state-machine logic that the existing
// fixed-pattern streaming tests above only exercise for a handful of hand-picked chunk splits.
// Randomized differential testing (many lengths x many chunk-split patterns, comparing
// streaming digest against the one-shot digest of the same bytes) gives much stronger
// correctness assurance for this kind of bit-twiddling code than manual line-by-line reading
// -- the same "verify via testing" approach used elsewhere this session for other
// algorithm-critical files (Utf8Parser's overflow idiom, Random's seeded PRNG). A fixed PRNG
// seed keeps this deterministic and reproducible across runs.
TEST(HashingTests, XxHash3_StreamingMatchesOneShot_RandomizedChunkSplits) {
    std::mt19937 rng(0xC0FFEE);
    // Lengths chosen to straddle every structural boundary in Append()/DigestLong(): the
    // internal buffer size (~192 bytes for XxHash3's default secret), a single stripe (64
    // bytes), a full block, and multi-block inputs, plus small/edge lengths (0, 1, and just
    // above/below each boundary).
    // Includes 159-162/192-193/224-226/239-242, straddling HashLength129To240's own internal
    // `bound` sub-branch transitions (at length 161/193/225, where an extra Mix32Bytes call
    // kicks in) and the HashLength129To240 -> HashLengthOver240 boundary at MidSizeMaxBytes=240
    // -- none of which the original length list (which jumped from 193 straight to 200 then
    // 255) actually exercised. Manually verified via a standalone repro before adding these:
    // all of these lengths already produced correct (streaming == one-shot) results, so this is
    // permanent regression coverage for previously-untested branches, not a bug fix.
    std::vector<int> lengths = {0, 1, 2, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65,
                                 127, 128, 129, 159, 160, 161, 162, 191, 192, 193,
                                 200, 224, 225, 226, 239, 240, 241, 242, 255, 256, 257,
                                 511, 512, 513, 1000, 2000, 4096, 8192};
    for (int length : lengths) {
        std::vector<uint8_t> data(static_cast<size_t>(length));
        for (auto& b : data) b = static_cast<uint8_t>(rng());
        uint64_t oneShot = XxHash3::HashToUInt64(data.data(), length, 0);
        uint64_t oneShotSeeded = XxHash3::HashToUInt64(data.data(), length, 0x1234);

        for (int trial = 0; trial < 5; ++trial) {
            XxHash3 h;
            XxHash3 hSeeded(0x1234);
            int pos = 0;
            while (pos < length) {
                int remaining = length - pos;
                int chunk = remaining == 0 ? 0 : 1 + static_cast<int>(rng() % static_cast<unsigned>(remaining));
                h.Append(data.data() + pos, chunk);
                hSeeded.Append(data.data() + pos, chunk);
                pos += chunk;
            }
            ASSERT_EQ(h.GetCurrentHashAsUInt64(), oneShot)
                << "length=" << length << " trial=" << trial;
            ASSERT_EQ(hSeeded.GetCurrentHashAsUInt64(), oneShotSeeded)
                << "length=" << length << " trial=" << trial << " (seeded)";
        }
    }
}

TEST(HashingTests, XxHash128_StreamingMatchesOneShot_RandomizedChunkSplits) {
    std::mt19937 rng(0xC0FFEE);
    // Includes 159-162/192-193/224-226/239-242, straddling HashLength129To240's own internal
    // `bound` sub-branch transitions (at length 161/193/225, where an extra Mix32Bytes call
    // kicks in) and the HashLength129To240 -> HashLengthOver240 boundary at MidSizeMaxBytes=240
    // -- none of which the original length list (which jumped from 193 straight to 200 then
    // 255) actually exercised. Manually verified via a standalone repro before adding these:
    // all of these lengths already produced correct (streaming == one-shot) results, so this is
    // permanent regression coverage for previously-untested branches, not a bug fix.
    std::vector<int> lengths = {0, 1, 2, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65,
                                 127, 128, 129, 159, 160, 161, 162, 191, 192, 193,
                                 200, 224, 225, 226, 239, 240, 241, 242, 255, 256, 257,
                                 511, 512, 513, 1000, 2000, 4096, 8192};
    for (int length : lengths) {
        std::vector<uint8_t> data(static_cast<size_t>(length));
        for (auto& b : data) b = static_cast<uint8_t>(rng());
        auto oneShot = XxHash128::HashToHash128(data.data(), length, 0);
        auto oneShotSeeded = XxHash128::HashToHash128(data.data(), length, 0x1234);

        for (int trial = 0; trial < 5; ++trial) {
            XxHash128 h;
            XxHash128 hSeeded(0x1234);
            int pos = 0;
            while (pos < length) {
                int remaining = length - pos;
                int chunk = remaining == 0 ? 0 : 1 + static_cast<int>(rng() % static_cast<unsigned>(remaining));
                h.Append(data.data() + pos, chunk);
                hSeeded.Append(data.data() + pos, chunk);
                pos += chunk;
            }
            ASSERT_EQ(h.GetCurrentHashAsHash128(), oneShot)
                << "length=" << length << " trial=" << trial;
            ASSERT_EQ(hSeeded.GetCurrentHashAsHash128(), oneShotSeeded)
                << "length=" << length << " trial=" << trial << " (seeded)";
        }
    }
}

TEST(HashingTests, XxHash128_TryHash_DestinationTooShort_ReturnsFalse) {
    auto data = bytes("abc");
    uint8_t dest[15];
    int32_t written = -1;
    EXPECT_FALSE(XxHash128::TryHash(data.data(), 3, dest, 15, written));
    EXPECT_EQ(written, 0);
}

TEST(HashingTests, XxHash128_Hash_DestinationTooShort_Throws) {
    auto data = bytes("abc");
    uint8_t dest[15];
    EXPECT_THROW(XxHash128::Hash(data.data(), 3, dest, 15), System::ArgumentException);
}

// ===========================================================================
// Ticket #2142 (SR-AUD-261, cause IH-B) -- a negative length was a successful
// EMPTY operation in Adler32, Crc32, Crc64 and both CRC parameter sets.
//
// The defect was not "an error that goes unreported": every one of those loops
// is `for (intcs i = 0; i < length; ++i)`, which is false at once for a
// negative length, so HashToUInt32(data, -1) returned the hash of NOTHING and
// the caller could not tell. The four XXH types in the same module already
// rejected it, so this is a divergence being deleted rather than a rule being
// invented -- the exception type, the parameter name and the message below are
// verbatim the ones XxHash32/64/3/128 have always used.
// ===========================================================================

namespace {

    // The exact contract: ArgumentOutOfRangeException, paramName "length", and the module's
    // one message. Checking all three is what makes a mutation that changes only the message
    // or only the parameter name fail.
    template <typename Fn>
    ::testing::AssertionResult ThrowsNegativeLength(Fn&& fn) {
        try {
            fn();
        } catch (const System::ArgumentOutOfRangeException& e) {
            if (e.getParamNameProperty() != "length") {
                return ::testing::AssertionFailure()
                       << "paramName was '" << e.getParamNameProperty() << "', expected 'length'";
            }
            const std::string message = e.what();
            if (message.find("Non-negative number required.") == std::string::npos) {
                return ::testing::AssertionFailure()
                       << "message was '" << message << "', expected it to contain "
                       << "\"Non-negative number required.\"";
            }
            return ::testing::AssertionSuccess();
        } catch (const System::Exception& e) {
            return ::testing::AssertionFailure()
                   << "threw a different System:: exception: " << e.what();
        } catch (...) {
            return ::testing::AssertionFailure() << "threw a non-System:: exception";
        }
        return ::testing::AssertionFailure() << "returned normally";
    }

    constexpr int32_t kNegativeLengths[] = {-1, -2, INT32_MIN};

    // EXPECT_THROW(..., System::ArgumentException) is NOT strict enough for a capacity check:
    // ArgumentNullException AND ArgumentOutOfRangeException both DERIVE from ArgumentException,
    // so a door that reported the wrong one of the three would still satisfy it. Every pin whose
    // point is "the capacity claim is what was decided" uses this instead, and rejects the two
    // derived types explicitly.
    template <typename Fn>
    ::testing::AssertionResult ThrowsDestinationTooShort(Fn&& fn) {
        try {
            fn();
        } catch (const System::ArgumentNullException& e) {
            return ::testing::AssertionFailure()
                   << "threw ArgumentNullException('" << e.getParamNameProperty()
                   << "') instead of reporting the capacity";
        } catch (const System::ArgumentOutOfRangeException& e) {
            return ::testing::AssertionFailure()
                   << "threw ArgumentOutOfRangeException('" << e.getParamNameProperty()
                   << "') instead of reporting the capacity";
        } catch (const System::ArgumentException& e) {
            if (e.getParamNameProperty() != "destination") {
                return ::testing::AssertionFailure()
                       << "paramName was '" << e.getParamNameProperty() << "'";
            }
            const std::string message = e.what();
            if (message.find("Destination is too short.") == std::string::npos) {
                return ::testing::AssertionFailure() << "message was '" << message << "'";
            }
            return ::testing::AssertionSuccess();
        } catch (const System::Exception& e) {
            return ::testing::AssertionFailure()
                   << "threw a different System:: exception: " << e.what();
        } catch (...) {
            return ::testing::AssertionFailure() << "threw a non-System:: exception";
        }
        return ::testing::AssertionFailure() << "returned normally";
    }

} // namespace

TEST(HashingTests, Adler32_NegativeLength_EveryDoor_Throws) {
    auto data = bytes("abcdefgh");
    uint8_t dest[8] = {};
    int32_t written = -1;

    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Adler32::HashToUInt32(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Adler32::Hash(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Adler32::Hash(data.data(), n, dest, 8);
        })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Adler32::TryHash(data.data(), n, dest, 8, written);
        })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { Adler32 h; h.Append(data.data(), n); })) << n;
    }
}

TEST(HashingTests, Crc32_NegativeLength_EveryDoor_Throws) {
    auto data = bytes("abcdefgh");
    const auto& set = Crc32ParameterSet::getCrc32CProperty();
    uint8_t dest[8] = {};
    int32_t written = -1;

    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc32::HashToUInt32(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc32::HashToUInt32(set, data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc32::Hash(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc32::Hash(set, data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc32::Hash(data.data(), n, dest, 8); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc32::Hash(set, data.data(), n, dest, 8); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Crc32::TryHash(data.data(), n, dest, 8, written);
        })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Crc32::TryHash(set, data.data(), n, dest, 8, written);
        })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { Crc32 h; h.Append(data.data(), n); })) << n;
    }
}

TEST(HashingTests, Crc64_NegativeLength_EveryDoor_Throws) {
    auto data = bytes("abcdefgh");
    const auto& set = Crc64ParameterSet::getNvmeProperty();
    uint8_t dest[16] = {};
    int32_t written = -1;

    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc64::HashToUInt64(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc64::HashToUInt64(set, data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc64::Hash(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc64::Hash(set, data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc64::Hash(data.data(), n, dest, 16); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)Crc64::Hash(set, data.data(), n, dest, 16); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Crc64::TryHash(data.data(), n, dest, 16, written);
        })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Crc64::TryHash(set, data.data(), n, dest, 16, written);
        })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { Crc64 h; h.Append(data.data(), n); })) << n;
    }
}

TEST(HashingTests, Crc32ParameterSet_Update_NegativeLength_Throws) {
    auto data = bytes("abcdefgh");
    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Crc32ParameterSet::getCrc32Property()->Update(0, data.data(), n);
        })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Crc32ParameterSet::getCrc32CProperty()->Update(0, data.data(), n);
        })) << n;
    }
}

TEST(HashingTests, Crc64ParameterSet_Update_NegativeLength_Throws) {
    auto data = bytes("abcdefgh");
    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Crc64ParameterSet::getCrc64Property()->Update(0, data.data(), n);
        })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] {
            (void)Crc64ParameterSet::getNvmeProperty()->Update(0, data.data(), n);
        })) << n;
    }
}

// A guard placed AFTER the state update would pass a naive "it throws" test. These three pin
// that the rejected Append is a no-op on the accumulated hash, which is why the validation sits
// inside Update() -- before the caller's `state_ = Update(...)` assignment can run.
TEST(HashingTests, Adler32_RejectedAppend_LeavesHashStateUntouched) {
    auto data = bytes("abcdefgh");
    Adler32 h;
    h.Append(data.data(), 8);
    const uint32_t before = h.GetCurrentHashAsUInt32();

    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] { h.Append(data.data(), n); })) << n;
        EXPECT_EQ(h.GetCurrentHashAsUInt32(), before) << n;
    }

    // And the hasher is still usable afterwards: appending the same bytes twice in total must
    // equal the one-shot hash of the doubled input, i.e. the rejections changed nothing at all.
    h.Append(data.data(), 8);
    auto doubled = bytes("abcdefghabcdefgh");
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), Adler32::HashToUInt32(doubled.data(), 16));
}

TEST(HashingTests, Crc32_RejectedAppend_LeavesHashStateUntouched) {
    auto data = bytes("abcdefgh");
    Crc32 h;
    h.Append(data.data(), 8);
    const uint32_t before = h.GetCurrentHashAsUInt32();

    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] { h.Append(data.data(), n); })) << n;
        EXPECT_EQ(h.GetCurrentHashAsUInt32(), before) << n;
    }

    h.Append(data.data(), 8);
    auto doubled = bytes("abcdefghabcdefgh");
    EXPECT_EQ(h.GetCurrentHashAsUInt32(), Crc32::HashToUInt32(doubled.data(), 16));
}

TEST(HashingTests, Crc64_RejectedAppend_LeavesHashStateUntouched) {
    auto data = bytes("abcdefgh");
    Crc64 h;
    h.Append(data.data(), 8);
    const uint64_t before = h.GetCurrentHashAsUInt64();

    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] { h.Append(data.data(), n); })) << n;
        EXPECT_EQ(h.GetCurrentHashAsUInt64(), before) << n;
    }

    h.Append(data.data(), 8);
    auto doubled = bytes("abcdefghabcdefgh");
    EXPECT_EQ(h.GetCurrentHashAsUInt64(), Crc64::HashToUInt64(doubled.data(), 16));
}

// CONTROL: the repair must not over-reach. Length 0 -- including the zero-length form of every
// destination door -- and ordinary positive lengths are unchanged.
TEST(HashingTests, ZeroAndPositiveLengths_AreUnaffectedByTheNegativeLengthGuard) {
    auto data = bytes("abcdefgh");

    EXPECT_EQ(Adler32::HashToUInt32(data.data(), 0), 1u);
    EXPECT_EQ(Crc32::HashToUInt32(data.data(), 0), 0u);
    EXPECT_NO_THROW({ Adler32 h; h.Append(data.data(), 0); });
    EXPECT_NO_THROW({ Crc32 h; h.Append(data.data(), 0); });
    EXPECT_NO_THROW({ Crc64 h; h.Append(data.data(), 0); });
    EXPECT_NO_THROW((void)Crc32ParameterSet::getCrc32Property()->Update(0, data.data(), 0));
    EXPECT_NO_THROW((void)Crc64ParameterSet::getCrc64Property()->Update(0, data.data(), 0));

    // Positive lengths still agree with the streaming path byte for byte.
    for (int32_t n = 1; n <= 8; ++n) {
        Adler32 a; a.Append(data.data(), n);
        EXPECT_EQ(a.GetCurrentHashAsUInt32(), Adler32::HashToUInt32(data.data(), n)) << n;
        Crc32 c; c.Append(data.data(), n);
        EXPECT_EQ(c.GetCurrentHashAsUInt32(), Crc32::HashToUInt32(data.data(), n)) << n;
        Crc64 d; d.Append(data.data(), n);
        EXPECT_EQ(d.GetCurrentHashAsUInt64(), Crc64::HashToUInt64(data.data(), n)) << n;
    }
}

// PIN: the four XXH types already threw exactly this, and #2142 only moved the rule into a
// shared helper. If that move changed any of them, this fails.
TEST(HashingTests, XxHash_NegativeLength_ContractIsUnchanged) {
    auto data = bytes("abcdefgh");
    for (int32_t n : kNegativeLengths) {
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)XxHash32::HashToUInt32(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)XxHash64::HashToUInt64(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)XxHash3::HashToUInt64(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { (void)XxHash128::HashToHash128(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { XxHash32 h; h.Append(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { XxHash64 h; h.Append(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { XxHash3 h; h.Append(data.data(), n); })) << n;
        EXPECT_TRUE(ThrowsNegativeLength([&] { XxHash128 h; h.Append(data.data(), n); })) << n;
    }
}

// PIN: a short destination is still reported as a short destination, and it is decided FIRST --
// the destination-capacity check precedes the source-length check at every door that has both.
// Pinned so the ordering is a decision rather than an accident of where the guards were added.
TEST(HashingTests, ShortDestination_IsDecidedBeforeNegativeLength) {
    auto data = bytes("abcdefgh");
    uint8_t dest[1] = {};
    int32_t written = -1;

    EXPECT_TRUE(ThrowsDestinationTooShort([&] { (void)Adler32::Hash(data.data(), -1, dest, 1); }));
    EXPECT_TRUE(ThrowsDestinationTooShort([&] { (void)Crc32::Hash(data.data(), -1, dest, 1); }));
    EXPECT_TRUE(ThrowsDestinationTooShort([&] { (void)Crc64::Hash(data.data(), -1, dest, 1); }));
    EXPECT_TRUE(ThrowsDestinationTooShort([&] { (void)XxHash32::Hash(data.data(), -1, dest, 1); }));

    // ... and Try* still reports it by returning false rather than by throwing.
    EXPECT_FALSE(Adler32::TryHash(data.data(), -1, dest, 1, written));
    EXPECT_EQ(written, 0);
    EXPECT_FALSE(Crc64::TryHash(data.data(), -1, dest, 1, written));
    EXPECT_EQ(written, 0);
}

// PIN: the eight published check values of the review plan's section 6.2, in one place. These
// are the values published by the algorithms' own specifications, not values read out of this
// implementation, so they are the control every repair in this module must leave standing.
TEST(HashingTests, PublishedCheckValues_AllEight_AreUnchanged) {
    auto abc = bytes("abc");
    auto check = bytes("123456789");

    EXPECT_EQ(Adler32::HashToUInt32(abc.data(), 3), 0x024d0127u);
    EXPECT_EQ(Crc32::HashToUInt32(check.data(), 9), 0xcbf43926u);
    EXPECT_EQ(Crc64::HashToUInt64(check.data(), 9), 0x6c40df5f0b497347ull);
    EXPECT_EQ(XxHash32::HashToUInt32(abc.data(), 0), 0x02cc5d05u);
    EXPECT_EQ(XxHash32::HashToUInt32(abc.data(), 3), 0x32d153ffu);
    EXPECT_EQ(XxHash64::HashToUInt64(abc.data(), 3), 0x44bc2cf5ad770999ull);
    EXPECT_EQ(XxHash3::HashToUInt64(abc.data(), 3), 0x78af5f94892f3950ull);

    const auto h128 = XxHash128::HashToHash128(abc.data(), 3);
    EXPECT_EQ(h128.High64, 0x06b05ab6733a6185ull);
    EXPECT_EQ(h128.Low64, 0x78af5f94892f3950ull);
}

// ===========================================================================
// Ticket #2141 (SR-AUD-260, causes IH-A + IH-D) -- every raw-pointer door
// dereferenced a null buffer, SOURCE and DESTINATION.
//
// Measured before the repair: 58 of the review's 102 cases died with SIGSEGV,
// and a second probe found NINE MORE the 102-case matrix never reached --
// TryGetHashAndReset on all seven types (the fifth base-class destination
// door) and both parameter sets' WriteCrcToSpan, a public destination door
// that carries no capacity argument at all. 67 doors in total, now zero.
//
// The one thing the repair must NOT do is reject every null pointer:
// default(ReadOnlySpan<byte>) in .NET is an empty span whose reference is
// null, so a null pointer with length 0 is a legal empty operation and stays
// accepted. Only a null with a POSITIVE length -- the state a span cannot
// represent and this port's raw pointer can -- is an error.
// ===========================================================================

namespace {

    template <typename Fn>
    ::testing::AssertionResult ThrowsNullArgument(const char* expectedParam, Fn&& fn) {
        try {
            fn();
        } catch (const System::ArgumentNullException& e) {
            if (e.getParamNameProperty() != expectedParam) {
                return ::testing::AssertionFailure()
                       << "paramName was '" << e.getParamNameProperty() << "', expected '"
                       << expectedParam << "'";
            }
            return ::testing::AssertionSuccess();
        } catch (const System::Exception& e) {
            return ::testing::AssertionFailure()
                   << "threw a different System:: exception: " << e.what();
        } catch (...) {
            return ::testing::AssertionFailure() << "threw a non-System:: exception";
        }
        return ::testing::AssertionFailure() << "returned normally";
    }

    // Every door of one hasher type, driven uniformly. `oneShot` is the only per-type piece,
    // because HashToUInt32 / HashToUInt64 / HashToHash128 differ in return type only.
    template <typename Hasher, typename OneShot>
    void AssertEveryDoorRejectsNull(const char* name, int32_t size, OneShot oneShot) {
        SCOPED_TRACE(name);
        const auto data = bytes("abcdefgh");
        uint8_t dest[32] = {};
        int32_t written = -1;

        // --- null SOURCE with a positive length -------------------------------------------
        EXPECT_TRUE(ThrowsNullArgument("source", [&] { oneShot(nullptr, 1); }));
        EXPECT_TRUE(ThrowsNullArgument("source", [&] { (void)Hasher::Hash(nullptr, 1); }));
        EXPECT_TRUE(ThrowsNullArgument("source", [&] {
            (void)Hasher::Hash(nullptr, 1, dest, 32);
        }));
        EXPECT_TRUE(ThrowsNullArgument("source", [&] {
            (void)Hasher::TryHash(nullptr, 1, dest, 32, written);
        }));
        EXPECT_TRUE(ThrowsNullArgument("source", [&] { Hasher h; h.Append(nullptr, 1); }));

        // --- null DESTINATION whose claimed capacity is sufficient -------------------------
        EXPECT_TRUE(ThrowsNullArgument("destination", [&] {
            (void)Hasher::Hash(data.data(), 3, nullptr, 32);
        }));
        EXPECT_TRUE(ThrowsNullArgument("destination", [&] {
            (void)Hasher::TryHash(data.data(), 3, nullptr, 32, written);
        }));
        EXPECT_TRUE(ThrowsNullArgument("destination", [&] {
            Hasher h; (void)h.GetCurrentHash(nullptr, 32);
        }));
        EXPECT_TRUE(ThrowsNullArgument("destination", [&] {
            Hasher h; (void)h.TryGetCurrentHash(nullptr, 32, written);
        }));
        EXPECT_TRUE(ThrowsNullArgument("destination", [&] {
            Hasher h; (void)h.GetHashAndReset(nullptr, 32);
        }));
        // The fifth base-class destination door, absent from the review's 102-case matrix.
        EXPECT_TRUE(ThrowsNullArgument("destination", [&] {
            Hasher h; (void)h.TryGetHashAndReset(nullptr, 32, written);
        }));

        // --- CONTROL: a null pointer with length 0 is a legal empty operation --------------
        EXPECT_NO_THROW(oneShot(nullptr, 0));
        EXPECT_NO_THROW({ Hasher h; h.Append(nullptr, 0); });
        EXPECT_NO_THROW((void)Hasher::Hash(nullptr, 0));

        // --- CONTROL: capacity is still decided FIRST, and still reported the same way -----
        // A null destination whose claimed length is INSUFFICIENT is "too short", not "null":
        // the caller got the capacity claim wrong before the pointer mattered.
        EXPECT_TRUE(ThrowsDestinationTooShort([&] {
            Hasher h; (void)h.GetCurrentHash(nullptr, 0);
        }));
        EXPECT_TRUE(ThrowsDestinationTooShort([&] {
            Hasher h; (void)h.GetCurrentHash(dest, size - 1);
        }));
        {
            Hasher h;
            EXPECT_FALSE(h.TryGetCurrentHash(nullptr, 0, written));
            EXPECT_EQ(written, 0);
        }

        // --- CONTROL: the boundary sizes on a real buffer all still work -------------------
        EXPECT_NO_THROW({ Hasher h; (void)h.GetCurrentHash(dest, size); });      // exactly enough
        EXPECT_NO_THROW({ Hasher h; (void)h.GetCurrentHash(dest, 32); });        // oversized
        EXPECT_NO_THROW({ Hasher h; (void)h.TryGetHashAndReset(dest, size, written); });
        EXPECT_EQ(written, size);
    }

} // namespace

TEST(HashingTests, Adler32_EveryRawPointerDoor_RejectsNull) {
    AssertEveryDoorRejectsNull<Adler32>("Adler32", 4,
        [](const uint8_t* s, int32_t n) { (void)Adler32::HashToUInt32(s, n); });
}
TEST(HashingTests, Crc32_EveryRawPointerDoor_RejectsNull) {
    AssertEveryDoorRejectsNull<Crc32>("Crc32", 4,
        [](const uint8_t* s, int32_t n) { (void)Crc32::HashToUInt32(s, n); });
}
TEST(HashingTests, Crc64_EveryRawPointerDoor_RejectsNull) {
    AssertEveryDoorRejectsNull<Crc64>("Crc64", 8,
        [](const uint8_t* s, int32_t n) { (void)Crc64::HashToUInt64(s, n); });
}
TEST(HashingTests, XxHash32_EveryRawPointerDoor_RejectsNull) {
    AssertEveryDoorRejectsNull<XxHash32>("XxHash32", 4,
        [](const uint8_t* s, int32_t n) { (void)XxHash32::HashToUInt32(s, n); });
}
TEST(HashingTests, XxHash64_EveryRawPointerDoor_RejectsNull) {
    AssertEveryDoorRejectsNull<XxHash64>("XxHash64", 8,
        [](const uint8_t* s, int32_t n) { (void)XxHash64::HashToUInt64(s, n); });
}
TEST(HashingTests, XxHash3_EveryRawPointerDoor_RejectsNull) {
    AssertEveryDoorRejectsNull<XxHash3>("XxHash3", 8,
        [](const uint8_t* s, int32_t n) { (void)XxHash3::HashToUInt64(s, n); });
}
TEST(HashingTests, XxHash128_EveryRawPointerDoor_RejectsNull) {
    AssertEveryDoorRejectsNull<XxHash128>("XxHash128", 16,
        [](const uint8_t* s, int32_t n) { (void)XxHash128::HashToHash128(s, n); });
}

// The two CRC parameter sets: the public Update door on the source axis, and WriteCrcToSpan --
// a public destination door with NO capacity argument, so a null check is the only validation
// available to it. Neither appeared in the review's 102-case matrix.
TEST(HashingTests, Crc32ParameterSet_RawPointerDoors_RejectNull) {
    const auto& set = Crc32ParameterSet::getCrc32Property();
    uint8_t dest[4] = {};
    EXPECT_TRUE(ThrowsNullArgument("source", [&] { (void)set->Update(0, nullptr, 1); }));
    EXPECT_TRUE(ThrowsNullArgument("destination", [&] { set->WriteCrcToSpan(0, nullptr); }));
    EXPECT_NO_THROW((void)set->Update(0, nullptr, 0));   // control
    EXPECT_NO_THROW(set->WriteCrcToSpan(0, dest));       // control
}

TEST(HashingTests, Crc64ParameterSet_RawPointerDoors_RejectNull) {
    const auto& set = Crc64ParameterSet::getCrc64Property();
    uint8_t dest[8] = {};
    EXPECT_TRUE(ThrowsNullArgument("source", [&] { (void)set->Update(0, nullptr, 1); }));
    EXPECT_TRUE(ThrowsNullArgument("destination", [&] { set->WriteCrcToSpan(0, nullptr); }));
    EXPECT_NO_THROW((void)set->Update(0, nullptr, 0));   // control
    EXPECT_NO_THROW(set->WriteCrcToSpan(0, dest));       // control
}

// A guard placed after the state update, or after a partial write, would pass a naive
// "it throws" test. These two pin that a rejected call changes nothing observable.
TEST(HashingTests, RejectedNullAppend_LeavesHashStateUntouched) {
    const auto data = bytes("abcdefgh");

    Adler32 a; a.Append(data.data(), 8);
    const uint32_t aBefore = a.GetCurrentHashAsUInt32();
    EXPECT_TRUE(ThrowsNullArgument("source", [&] { a.Append(nullptr, 1); }));
    EXPECT_EQ(a.GetCurrentHashAsUInt32(), aBefore);

    Crc32 c; c.Append(data.data(), 8);
    const uint32_t cBefore = c.GetCurrentHashAsUInt32();
    EXPECT_TRUE(ThrowsNullArgument("source", [&] { c.Append(nullptr, 1); }));
    EXPECT_EQ(c.GetCurrentHashAsUInt32(), cBefore);

    Crc64 d; d.Append(data.data(), 8);
    const uint64_t dBefore = d.GetCurrentHashAsUInt64();
    EXPECT_TRUE(ThrowsNullArgument("source", [&] { d.Append(nullptr, 1); }));
    EXPECT_EQ(d.GetCurrentHashAsUInt64(), dBefore);

    XxHash3 x; x.Append(data.data(), 8);
    const uint64_t xBefore = x.GetCurrentHashAsUInt64();
    EXPECT_TRUE(ThrowsNullArgument("source", [&] { x.Append(nullptr, 1); }));
    EXPECT_EQ(x.GetCurrentHashAsUInt64(), xBefore);
}

TEST(HashingTests, RejectedDestinationWrite_LeavesTheBufferUntouched) {
    const auto data = bytes("abcdefgh");
    uint8_t dest[32];
    int32_t written = -1;

    // A rejection must not have written a partial hash first.
    for (auto& b : dest) b = 0xAB;
    EXPECT_TRUE(ThrowsDestinationTooShort([&] { (void)Crc64::Hash(data.data(), 8, dest, 7); }));
    for (auto b : dest) EXPECT_EQ(b, 0xAB);

    for (auto& b : dest) b = 0xCD;
    EXPECT_FALSE(Crc64::TryHash(data.data(), 8, dest, 7, written));
    EXPECT_EQ(written, 0);
    for (auto b : dest) EXPECT_EQ(b, 0xCD);

    for (auto& b : dest) b = 0xEF;
    EXPECT_TRUE(ThrowsNullArgument("source", [&] {
        (void)XxHash128::Hash(nullptr, 1, dest, 32);
    }));
    for (auto b : dest) EXPECT_EQ(b, 0xEF);
}

// A null source at the SOURCE axis must not be confused with a legal empty append coming in
// through the std::vector overload, whose data() is null for an empty vector.
TEST(HashingTests, EmptyVectorAppend_IsAcceptedOnEveryType) {
    const std::vector<uint8_t> empty;
    EXPECT_EQ(empty.data(), nullptr);
    EXPECT_NO_THROW({ Adler32 h;   h.Append(empty); });
    EXPECT_NO_THROW({ Crc32 h;     h.Append(empty); });
    EXPECT_NO_THROW({ Crc64 h;     h.Append(empty); });
    EXPECT_NO_THROW({ XxHash32 h;  h.Append(empty); });
    EXPECT_NO_THROW({ XxHash64 h;  h.Append(empty); });
    EXPECT_NO_THROW({ XxHash3 h;   h.Append(empty); });
    EXPECT_NO_THROW({ XxHash128 h; h.Append(empty); });
}

// A null + zero-length Append must be a true no-op, not merely "does not throw": the hash after
// it must equal the hash without it, on every type.
TEST(HashingTests, NullZeroLengthAppend_IsATrueNoOp) {
    const auto data = bytes("abcdefgh");

    Adler32 a1, a2;
    a1.Append(data.data(), 8);
    a2.Append(nullptr, 0); a2.Append(data.data(), 8); a2.Append(nullptr, 0);
    EXPECT_EQ(a1.GetCurrentHashAsUInt32(), a2.GetCurrentHashAsUInt32());

    XxHash32 b1, b2;
    b1.Append(data.data(), 8);
    b2.Append(nullptr, 0); b2.Append(data.data(), 8); b2.Append(nullptr, 0);
    EXPECT_EQ(b1.GetCurrentHashAsUInt32(), b2.GetCurrentHashAsUInt32());

    XxHash64 c1, c2;
    c1.Append(data.data(), 8);
    c2.Append(nullptr, 0); c2.Append(data.data(), 8); c2.Append(nullptr, 0);
    EXPECT_EQ(c1.GetCurrentHashAsUInt64(), c2.GetCurrentHashAsUInt64());

    XxHash3 d1, d2;
    d1.Append(data.data(), 8);
    d2.Append(nullptr, 0); d2.Append(data.data(), 8); d2.Append(nullptr, 0);
    EXPECT_EQ(d1.GetCurrentHashAsUInt64(), d2.GetCurrentHashAsUInt64());

    XxHash128 e1, e2;
    e1.Append(data.data(), 8);
    e2.Append(nullptr, 0); e2.Append(data.data(), 8); e2.Append(nullptr, 0);
    EXPECT_EQ(e1.GetCurrentHashAsHash128(), e2.GetCurrentHashAsHash128());
}

// PIN: the DESTINATION byte order of the three types that had no byte-order test.
//
// This gap was found by a surviving mutation: corrupting Crc64ParameterSet::WriteCrcToSpan's
// non-reflected byte order changed nothing any test could see, because every Crc32/Crc64/Adler32
// check-value test read the NUMERIC result and only the four XXH types pinned their bytes. The
// numeric value and the emitted bytes are two separate parts of each algorithm's published
// contract, and #2143 rewrites byte-order helpers, so both need pinning independently.
//
// Measured (build-probe/2141_probe3_byteorder.log), and each matches its algorithm's convention:
// Adler-32 is stored most-significant byte first; a CRC parameter set writes little-endian when
// it reflects its values and big-endian when it does not.
TEST(HashingTests, DestinationByteOrder_OfAdlerAndCrc_IsPinned) {
    auto abc = bytes("abc");
    auto check = bytes("123456789");

    // Adler-32: big-endian, per the zlib/RFC 1950 convention.
    EXPECT_EQ(Adler32::Hash(abc.data(), 3), (std::vector<uint8_t>{0x02, 0x4d, 0x01, 0x27}));

    // CRC-32 (reflected) -> little-endian.
    EXPECT_EQ(Crc32::Hash(check.data(), 9), (std::vector<uint8_t>{0x26, 0x39, 0xf4, 0xcb}));
    EXPECT_EQ(Crc32::Hash(Crc32ParameterSet::getCrc32CProperty(), check.data(), 9),
              (std::vector<uint8_t>{0x83, 0x92, 0x06, 0xe3}));

    // CRC-64/ECMA-182 does NOT reflect -> big-endian.
    EXPECT_EQ(Crc64::Hash(check.data(), 9),
              (std::vector<uint8_t>{0x6c, 0x40, 0xdf, 0x5f, 0x0b, 0x49, 0x73, 0x47}));
    // CRC-64/NVMe reflects -> little-endian.
    EXPECT_EQ(Crc64::Hash(Crc64ParameterSet::getNvmeProperty(), check.data(), 9),
              (std::vector<uint8_t>{0x88, 0x98, 0x79, 0x0a, 0x86, 0x14, 0x8b, 0xae}));

    // ... and the same bytes arrive through the streaming destination doors, not only the
    // one-shot vector form, so a divergence between the two paths cannot hide.
    uint8_t dest[8] = {};
    Crc64 c;
    c.Append(check.data(), 9);
    EXPECT_EQ(c.GetCurrentHash(dest, 8), 8);
    EXPECT_EQ(std::vector<uint8_t>(dest, dest + 8),
              (std::vector<uint8_t>{0x6c, 0x40, 0xdf, 0x5f, 0x0b, 0x49, 0x73, 0x47}));

    uint8_t dest4[4] = {};
    Adler32 a;
    a.Append(abc.data(), 3);
    EXPECT_EQ(a.GetCurrentHash(dest4, 4), 4);
    EXPECT_EQ(std::vector<uint8_t>(dest4, dest4 + 4),
              (std::vector<uint8_t>{0x02, 0x4d, 0x01, 0x27}));
}

// ===========================================================================
// Ticket #2143 (SR-AUD-262, cause IH-C) -- the "LE" helpers were native-order
// memcpy, so the published values were a property of the host.
//
// A function named ReadUInt32LE that performs a memcpy of a native integer is
// a little-endian load only on a little-endian machine. xxHash specifies
// little-endian lane loads, so on a big-endian target the module would have
// computed hashes that are not xxHash -- silently, and only there.
//
// WHAT THESE TESTS PROVE, stated plainly: that the helpers map a fixed byte
// pattern to one specific value, which is an assertion about the CODE and
// holds on any host; and that every published check value is unchanged here.
// WHAT THEY DO NOT PROVE: that the module is correct when EXECUTED on a
// big-endian machine. There is no such host in this environment and no
// cross-run in this repository's CI. That is an environment limit, not a
// deferred reference question -- no reference checkout would help.
// ===========================================================================

TEST(HashingTests, ByteOrderHelpers_AreLittleEndianByConstruction) {
    namespace Detail = System::IO::Hashing::Detail;
    namespace Shared = System::IO::Hashing::Detail::XxHash3Shared;

    const uint8_t pattern[9] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x5a};

    // Host-independent by construction: these say which BYTE lands in which bit position.
    EXPECT_EQ(Detail::ReadUInt32LittleEndian(pattern), 0x67452301u);
    EXPECT_EQ(Detail::ReadUInt64LittleEndian(pattern), 0xefcdab8967452301ULL);

    // Unaligned reads must behave identically -- the memcpy these replaced tolerated them.
    EXPECT_EQ(Detail::ReadUInt32LittleEndian(pattern + 1), 0x89674523u);
    EXPECT_EQ(Detail::ReadUInt64LittleEndian(pattern + 1), 0x5aefcdab89674523ULL);

    // The write side, and its round trip.
    uint8_t out[8] = {};
    Detail::WriteUInt64LittleEndian(out, 0xefcdab8967452301ULL);
    EXPECT_EQ(std::vector<uint8_t>(out, out + 8),
              (std::vector<uint8_t>{0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef}));
    EXPECT_EQ(Detail::ReadUInt64LittleEndian(out), 0xefcdab8967452301ULL);

    // The XxHash3Shared names the algorithm code still uses now delegate to the same definition.
    EXPECT_EQ(Shared::ReadUInt32LE(pattern), 0x67452301u);
    EXPECT_EQ(Shared::ReadUInt64LE(pattern), 0xefcdab8967452301ULL);

    // All-zero and all-ones, so a sign or shift error cannot hide behind a lucky pattern.
    const uint8_t zeros[8] = {};
    const uint8_t ones[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    EXPECT_EQ(Detail::ReadUInt64LittleEndian(zeros), 0ULL);
    EXPECT_EQ(Detail::ReadUInt64LittleEndian(ones), 0xffffffffffffffffULL);
    EXPECT_EQ(Detail::ReadUInt32LittleEndian(ones), 0xffffffffu);
}

// The correctness matrix a byte-order change has to survive, across every algorithm: empty, one
// byte, short ASCII, binary containing NUL, the internal block boundaries of each type, odd
// lengths, multi-block input, incremental versus one-shot, Reset-then-recompute, and independent
// instances. None of these can be satisfied by a lucky constant.
namespace {

    template <typename Hasher>
    void AssertHashingIsSelfConsistent(const char* name) {
        SCOPED_TRACE(name);

        // A deterministic buffer that CONTAINS EMBEDDED NULs and covers every byte value, so a
        // truncation-at-NUL or a sign-extension error shows up.
        std::vector<uint8_t> buf(600);
        for (size_t i = 0; i < buf.size(); ++i) buf[i] = static_cast<uint8_t>((i * 37) % 256);
        buf[0] = 0; buf[17] = 0; buf[64] = 0; buf[255] = 0; buf[256] = 0;

        // The internal block boundaries of every type in this module (16 for XxHash32, 32 for
        // XxHash64, 64/240/256 for XxHash3 and XxHash128), each with its neighbours, plus odd
        // lengths and sizes spanning several blocks.
        const int32_t lengths[] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65,
                                   127, 128, 129, 239, 240, 241, 255, 256, 257, 383, 512, 599};

        for (int32_t n : lengths) {
            const std::vector<uint8_t> want = Hasher::Hash(buf.data(), n);

            // Incremental versus one-shot, split at many interior points. A lane-order error
            // inside a block changes both paths identically, so this is a consistency check
            // rather than a value check -- the published vectors are what pin the value.
            for (int32_t split = 0; split <= n; split += (n > 24 ? n / 8 + 1 : 1)) {
                Hasher h;
                h.Append(buf.data(), split);
                h.Append(buf.data() + split, n - split);
                ASSERT_EQ(h.GetCurrentHash(), want) << "length=" << n << " split=" << split;
            }

            // Reset then recompute reaches the same value as a fresh instance.
            Hasher reused;
            reused.Append(buf.data(), 599);
            reused.Reset();
            reused.Append(buf.data(), n);
            ASSERT_EQ(reused.GetCurrentHash(), want) << "length=" << n;

            // Independent instances agree, and repeated reads of one instance are stable.
            Hasher a, b;
            a.Append(buf.data(), n);
            b.Append(buf.data(), n);
            ASSERT_EQ(a.GetCurrentHash(), b.GetCurrentHash()) << "length=" << n;
            ASSERT_EQ(a.GetCurrentHash(), a.GetCurrentHash()) << "length=" << n;

            // An exactly sized and an oversized destination receive the same bytes, and both
            // match the vector form; an undersized one is refused.
            const int32_t size = a.getHashLengthInBytesProperty();
            uint8_t exact[16] = {}, over[32] = {};
            ASSERT_EQ(a.GetCurrentHash(exact, size), size) << "length=" << n;
            ASSERT_EQ(a.GetCurrentHash(over, 32), size) << "length=" << n;
            ASSERT_EQ(std::vector<uint8_t>(exact, exact + size), want) << "length=" << n;
            ASSERT_EQ(std::vector<uint8_t>(over, over + size), want) << "length=" << n;
            ASSERT_TRUE(ThrowsDestinationTooShort([&] { (void)a.GetCurrentHash(over, size - 1); }))
                << "length=" << n;
        }
    }

} // namespace

TEST(HashingTests, Adler32_HashingIsSelfConsistentAcrossEveryBoundary) {
    AssertHashingIsSelfConsistent<Adler32>("Adler32");
}
TEST(HashingTests, Crc32_HashingIsSelfConsistentAcrossEveryBoundary) {
    AssertHashingIsSelfConsistent<Crc32>("Crc32");
}
TEST(HashingTests, Crc64_HashingIsSelfConsistentAcrossEveryBoundary) {
    AssertHashingIsSelfConsistent<Crc64>("Crc64");
}
TEST(HashingTests, XxHash32_HashingIsSelfConsistentAcrossEveryBoundary) {
    AssertHashingIsSelfConsistent<XxHash32>("XxHash32");
}
TEST(HashingTests, XxHash64_HashingIsSelfConsistentAcrossEveryBoundary) {
    AssertHashingIsSelfConsistent<XxHash64>("XxHash64");
}
TEST(HashingTests, XxHash3_HashingIsSelfConsistentAcrossEveryBoundary) {
    AssertHashingIsSelfConsistent<XxHash3>("XxHash3");
}
TEST(HashingTests, XxHash128_HashingIsSelfConsistentAcrossEveryBoundary) {
    AssertHashingIsSelfConsistent<XxHash128>("XxHash128");
}
