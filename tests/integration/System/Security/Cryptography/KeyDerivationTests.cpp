// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include "System/Security/Cryptography/RNGCryptoServiceProvider.hpp"
#include "System/Security/Cryptography/RandomNumberGenerator.hpp"
#include "System/Security/Cryptography/Rfc2898DeriveBytes.hpp"

using namespace System::Security::Cryptography;

namespace {

std::vector<SharpRuntime::bytecs> toBytes(const std::string& s) { return std::vector<SharpRuntime::bytecs>(s.begin(), s.end()); }

std::string toHex(const std::vector<SharpRuntime::bytecs>& bytes) {
    std::ostringstream oss;
    for (auto b : bytes) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

} // namespace

// --- RandomNumberGenerator -------------------------------------------------------------------

TEST(RandomNumberGeneratorTests, GetBytes_ProducesRequestedLength) {
    auto bytes = RandomNumberGenerator::GetBytes(32);
    EXPECT_EQ(bytes.size(), 32u);
}

TEST(RandomNumberGeneratorTests, GetBytes_ProducesDifferentValuesEachCall) {
    auto a = RandomNumberGenerator::GetBytes(32);
    auto b = RandomNumberGenerator::GetBytes(32);
    EXPECT_NE(a, b);
}

TEST(RandomNumberGeneratorTests, Fill_FillsEntireBuffer) {
    std::vector<SharpRuntime::bytecs> buffer(16, 0);
    RandomNumberGenerator::Fill(buffer);
    bool allZero = std::all_of(buffer.begin(), buffer.end(), [](auto b) { return b == 0; });
    EXPECT_FALSE(allZero);
}

TEST(RandomNumberGeneratorTests, GetInt32_Range_StaysWithinBounds) {
    for (int i = 0; i < 200; ++i) {
        SharpRuntime::intcs v = RandomNumberGenerator::GetInt32(10, 20);
        EXPECT_GE(v, 10);
        EXPECT_LT(v, 20);
    }
}

TEST(RandomNumberGeneratorTests, GetInt32_SingleValueRange_AlwaysReturnsThatValue) {
    EXPECT_EQ(RandomNumberGenerator::GetInt32(5, 6), 5);
}

TEST(RandomNumberGeneratorTests, Create_ReturnsWorkingInstance) {
    auto rng = RandomNumberGenerator::Create();
    std::vector<SharpRuntime::bytecs> buffer(8);
    rng->GetBytes(buffer);
    EXPECT_EQ(buffer.size(), 8u);
}

// #2399 made this type `final` and `[[deprecated]]`, transcribing .NET's `public sealed class` and
// its [Obsolete] with diagnostic id SYSLIB0023 (RNGCryptoServiceProvider.cs:8-10, message at
// Obsoletions.cs:82). THE SUPPRESSION BELOW IS THE EVIDENCE: it is REQUIRED, and deleting it fails
// this build with "error: ... is deprecated ... [-Werror=deprecated-declarations]" -- the diagnostic
// demonstrated rather than asserted, which is the idiom #2289 established.
//
// The assertion also changed. It used to be EXPECT_EQ(buffer.size(), 24u) on a buffer whose size was
// fixed BEFORE the call, so it passed against a generator that wrote nothing at all.
TEST(RNGCryptoServiceProviderTests, GetBytes_DelegatesToARealGenerator) {
    constexpr SharpRuntime::bytecs sentinel = 0x5A;
    std::vector<SharpRuntime::bytecs> buffer(24, sentinel);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    RNGCryptoServiceProvider rng;
#pragma GCC diagnostic pop
    rng.GetBytes(buffer);
    ASSERT_EQ(buffer.size(), 24u);
    EXPECT_NE(buffer, std::vector<SharpRuntime::bytecs>(24, sentinel))
        << "the buffer is untouched, so this type is not reaching a generator at all";
}

// .NET's (string) and (byte[]) constructors ignore their argument entirely -- both chain to
// `this((CspParameters?)null)` (RNGCryptoServiceProvider.cs:14-16). Asserted by building through
// each and checking the result still writes, because "ignores its argument" and "does not
// construct a generator" look identical to a compile-only test.
TEST(RNGCryptoServiceProviderTests, TheTwoIgnoringConstructorsStillBuildAWorkingGenerator) {
    constexpr SharpRuntime::bytecs sentinel = 0x5A;
    const std::vector<SharpRuntime::bytecs> seed = {1, 2, 3, 4};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    RNGCryptoServiceProvider fromString(std::string("ignored"));
    RNGCryptoServiceProvider fromBytes(seed);
#pragma GCC diagnostic pop
    std::vector<SharpRuntime::bytecs> a(16, sentinel), b(16, sentinel);
    fromString.GetBytes(a);
    fromBytes.GetBytes(b);
    EXPECT_NE(a, std::vector<SharpRuntime::bytecs>(16, sentinel));
    EXPECT_NE(b, std::vector<SharpRuntime::bytecs>(16, sentinel));
    // The byte[] overload must not be a seed: two generators built from the same bytes must not
    // agree, or the argument is not being ignored at all.
    std::vector<SharpRuntime::bytecs> c(16, sentinel);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    RNGCryptoServiceProvider alsoFromBytes(seed);
#pragma GCC diagnostic pop
    alsoFromBytes.GetBytes(c);
    EXPECT_NE(b, c) << "two generators built from the same bytes agreed, so the argument is a seed";
}

// #2399 pins the SHAPE, not only the behaviour. A behavioural test cannot see a seal, and it
// cannot see an overload that was never added.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
TEST(RNGCryptoServiceProviderTests, TheShapeIsDotNets) {
    using Rng = RNGCryptoServiceProvider;
    // RNGCryptoServiceProvider.cs:10 -- `public sealed class`.
    static_assert(std::is_final_v<Rng>, "#2399: .NET declares this type sealed");
    // ...and still a RandomNumberGenerator, which the seal must not have cost.
    static_assert(std::is_base_of_v<RandomNumberGenerator, Rng>,
                  "#2399: sealing must not change what this type IS");

    // RNGCryptoServiceProvider.cs:13-15 -- the three constructors this port can transcribe.
    static_assert(std::is_constructible_v<Rng>);
    static_assert(std::is_constructible_v<Rng, std::string>);
    static_assert(std::is_constructible_v<Rng, std::vector<SharpRuntime::bytecs>>);

    // Both single-argument constructors are `explicit`, because a C# constructor never
    // participates in an implicit conversion -- so `explicit` is the faithful translation rather
    // than a narrowing this port invented.
    static_assert(!std::is_convertible_v<std::string, Rng>);
    static_assert(!std::is_convertible_v<std::vector<SharpRuntime::bytecs>, Rng>);

    // .NET'S FOURTH CONSTRUCTOR, `(CspParameters?)`, IS DELIBERATELY ABSENT, because
    // CspParameters does not exist in this port and inventing it to carry a type whose only
    // behaviour is `if (cspParams != null) throw new PlatformNotSupportedException()` would be
    // inventing public surface rather than porting it. There is no expression that names a type
    // which does not exist, so what is pinned is the SHAPE such an overload would introduce: a
    // constructor this type can reach with a null. A later ticket that adds CspParameters trips
    // this and has to justify the type first.
    static_assert(!std::is_constructible_v<Rng, std::nullptr_t>,
                  "#2399: adding a null-accepting constructor means CspParameters was invented");

    SUCCEED();
}
#pragma GCC diagnostic pop

// --- Rfc2898DeriveBytes (PBKDF2, RFC 6070 test vectors) ------------------------------------

TEST(Rfc2898DeriveBytesTests, RFC6070_SHA1_OneIteration) {
    Rfc2898DeriveBytes pbkdf2(std::string("password"), toBytes("salt"), 1, HashAlgorithmName::SHA1);
    EXPECT_EQ(toHex(pbkdf2.GetBytes(20)), "0c60c80f961f0e71f3a9b524af6012062fe037a6");
}

TEST(Rfc2898DeriveBytesTests, RFC6070_SHA1_TwoIterations) {
    Rfc2898DeriveBytes pbkdf2(std::string("password"), toBytes("salt"), 2, HashAlgorithmName::SHA1);
    EXPECT_EQ(toHex(pbkdf2.GetBytes(20)), "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957");
}

TEST(Rfc2898DeriveBytesTests, RFC6070_SHA1_4096Iterations) {
    Rfc2898DeriveBytes pbkdf2(std::string("password"), toBytes("salt"), 4096, HashAlgorithmName::SHA1);
    EXPECT_EQ(toHex(pbkdf2.GetBytes(20)), "4b007901b765489abead49d926f721d065a429c1");
}

TEST(Rfc2898DeriveBytesTests, GetBytes_MultipleCallsAccumulate_MatchesSingleCall) {
    Rfc2898DeriveBytes a(std::string("password"), toBytes("salt"), 1, HashAlgorithmName::SHA1);
    auto whole = a.GetBytes(40);

    Rfc2898DeriveBytes b(std::string("password"), toBytes("salt"), 1, HashAlgorithmName::SHA1);
    auto part1 = b.GetBytes(7);
    auto part2 = b.GetBytes(33);
    std::vector<SharpRuntime::bytecs> combined = part1;
    combined.insert(combined.end(), part2.begin(), part2.end());

    EXPECT_EQ(whole, combined);
}

TEST(Rfc2898DeriveBytesTests, Reset_RestartsFromBeginning) {
    Rfc2898DeriveBytes pbkdf2(std::string("password"), toBytes("salt"), 1, HashAlgorithmName::SHA1);
    auto first = pbkdf2.GetBytes(20);
    pbkdf2.Reset();
    auto second = pbkdf2.GetBytes(20);
    EXPECT_EQ(first, second);
}

TEST(Rfc2898DeriveBytesTests, SaltAndIterationCountAccessors) {
    Rfc2898DeriveBytes pbkdf2(std::string("password"), toBytes("salt"), 4096, HashAlgorithmName::SHA1);
    EXPECT_EQ(pbkdf2.getIterationCountProperty(), 4096);
    EXPECT_EQ(pbkdf2.getSaltProperty(), toBytes("salt"));
}
