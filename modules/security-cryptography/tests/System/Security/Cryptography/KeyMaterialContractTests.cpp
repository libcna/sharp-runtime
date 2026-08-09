// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2161. The other half of the key-material contract: the things this component
// deliberately does NOT guarantee, pinned so that a future change cannot quietly start claiming
// more than it delivers, and the things it does guarantee outside the two findings' own types.
//
// docs/SystemSecurityCryptographyNamespaceReviewPlan.md section 6 states the boundary in prose and
// section 12 lists the exclusions. Prose is not a regression; these are.
#include <gtest/gtest.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "System/ObjectDisposedException.hpp"
#include "System/Security/Cryptography/HKDF.hpp"
#include "System/Security/Cryptography/HMACSHA256.hpp"
#include "System/Security/Cryptography/Rfc2898DeriveBytes.hpp"
#include "System/Security/Cryptography/SHA256.hpp"
#include "System/Security/Cryptography/SHA3_256.hpp"
#include "System/Security/Cryptography/Shake128.hpp"

using namespace System::Security::Cryptography;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

using Bytes = std::vector<bytecs>;

Bytes filled(size_t n, unsigned v) { return Bytes(n, static_cast<bytecs>(v)); }

std::string toHex(const Bytes& bytes) {
    std::ostringstream oss;
    for (auto b : bytes) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

} // namespace

// --- exclusion: a copy handed out is the caller's, and erasure never reaches it ----------------

TEST(KeyMaterialContractTests, GetKeyHandsOutACopyThatDisposeDoesNotReach) {
    HMACSHA256 hmac(filled(32, 0xA7));
    Bytes handedOut = hmac.getKeyProperty();
    ASSERT_EQ(handedOut, filled(32, 0xA7));

    hmac.Dispose();

    // Deliberate: erasing the caller's own storage is not this component's to do, and a future
    // change that tried would be reaching into memory it does not own.
    EXPECT_EQ(handedOut, filled(32, 0xA7));
    EXPECT_TRUE(hmac.getKeyProperty().empty());
}

TEST(KeyMaterialContractTests, GetSaltHandsOutACopyThatIsIndependentOfTheInstance) {
    Rfc2898DeriveBytes pbkdf(filled(16, 0xC3), filled(8, 0x11), 4, HashAlgorithmName::SHA256);
    Bytes handedOut = pbkdf.getSaltProperty();
    ASSERT_EQ(handedOut, filled(8, 0x11));

    pbkdf.setSaltProperty(filled(8, 0x22));
    EXPECT_EQ(handedOut, filled(8, 0x11)) << "the copy must not track the instance";
    EXPECT_EQ(pbkdf.getSaltProperty(), filled(8, 0x22));
}

TEST(KeyMaterialContractTests, TheCallersOwnKeyVectorIsNeverTouched) {
    // The constructor takes its key by value, so an lvalue argument is copied and the caller keeps
    // its own. Disposing the HMAC erases what the HMAC owns, and nothing else.
    Bytes callerKey = filled(32, 0xA7);
    HMACSHA256 hmac(callerKey);
    hmac.Dispose();
    EXPECT_EQ(callerKey, filled(32, 0xA7));
}

// --- HKDF: static-only, so there is nothing to erase and nothing to dispose --------------------

TEST(KeyMaterialContractTests, HkdfCannotBeInstantiatedAndHoldsNoStateBetweenCalls) {
    static_assert(!std::is_default_constructible_v<HKDF>, "HKDF is a static-only type");

    const Bytes ikm = filled(22, 0x0b);
    const Bytes salt = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};
    const Bytes info = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9};

    const std::string first = toHex(HKDF::DeriveKey(HashAlgorithmName::SHA256, ikm, 42, salt, info));
    const std::string again = toHex(HKDF::DeriveKey(HashAlgorithmName::SHA256, ikm, 42, salt, info));
    EXPECT_EQ(first, again);

    // RFC 5869 test case 1 — unchanged by the erasure work in the HMACs it drives.
    EXPECT_EQ(first,
              "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
}

// --- unkeyed digests: disposal, reuse and the block-buffer erasure ----------------------------

TEST(KeyMaterialContractTests, ADisposedDigestRejectsFurtherUse) {
    SHA256 sha;
    (void)sha.ComputeHash(filled(8, 0x01));
    sha.Dispose();
    EXPECT_THROW((void)sha.ComputeHash(filled(8, 0x01)), System::ObjectDisposedException);
    EXPECT_THROW((void)sha.getHashProperty(), System::ObjectDisposedException);
    EXPECT_NO_THROW(sha.Dispose());
}

TEST(KeyMaterialContractTests, TheBlockBufferErasureDoesNotDisturbAReusedDigest) {
    // Initialize() now erases the block buffer, and ComputeHash calls Initialize() at the end of
    // every operation. A misplaced erasure would corrupt the second and later digests, not the
    // first, so a single-shot vector would not catch it.
    SHA256 sha;
    const std::string once = toHex(sha.ComputeHash(filled(200, 0x5A)));
    for (int i = 0; i < 4; ++i) {
        SHA256 fresh;
        EXPECT_EQ(toHex(sha.ComputeHash(filled(200, 0x5A))), once);
        EXPECT_EQ(toHex(fresh.ComputeHash(filled(200, 0x5A))), once);
    }
}

TEST(KeyMaterialContractTests, TheSpongeResetErasureLeavesAFreshSponge) {
    // Keccak::Sponge::Reset now goes through the same primitive. SHA3 and SHAKE both drive it.
    SHA3_256 sha3;
    const std::string once = toHex(sha3.ComputeHash(filled(300, 0x5A)));
    EXPECT_EQ(toHex(sha3.ComputeHash(filled(300, 0x5A))), once);
    EXPECT_EQ(toHex(SHA3_256().ComputeHash(filled(300, 0x5A))), once);

    Shake128 shake;
    const std::string xof = toHex(shake.GetHashAndReset(64));
    shake.AppendData(filled(300, 0x5A));
    shake.Reset();
    EXPECT_EQ(toHex(shake.GetHashAndReset(64)), xof) << "Reset must discard absorbed data entirely";
}

// --- the exclusions this component does not claim to cover ------------------------------------

TEST(KeyMaterialContractTests, NothingClaimsToEraseStorageTheAllocatorHasAlreadyReused) {
    // There is no assertion to make here, and that is the point: this component erases bytes in
    // storage it owns, before releasing it. It cannot reach a block the allocator has since handed
    // to someone else, a page the kernel swapped out, a core dump, or a register spill. This test
    // exists so that the boundary is stated in the suite and not only in a document; if a future
    // change starts claiming OS-wide erasure, it has to delete this test to do so.
    SUCCEED() << "see docs/SystemSecurityCryptographyNamespaceReviewPlan.md section 6";
}

TEST(KeyMaterialContractTests, NoThreadSafetyContractIsClaimedForASharedInstance) {
    // Likewise: no type in this component documents or implements thread safety for a shared
    // instance, so TSan is not a discriminating tool here and none of this batch's evidence claims
    // it is. Recorded as a decision rather than left as an absence.
    SUCCEED() << "see docs/SystemSecurityCryptographyNamespaceReviewPlan.md section 9";
}
