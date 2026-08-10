// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2160, SR-AUD-331 (causes SC-B and SC-C). Permanent regressions for
// Rfc2898DeriveBytes' disposal contract.
//
// The finding: DeriveBytes::Dispose() is an empty virtual, Rfc2898DeriveBytes overrode nothing, and
// a disposed instance kept its password, its salt and its derived-byte buffer and went on producing
// real PBKDF2 output -- byte-identical to a fresh instance's continuation, so the post-disposal path
// was the live derivation and not residue.
//
// The inherited handoff predicted that repairing this needed an object-layout change. It did not:
// the flag lives in a four-byte padding hole that already existed between blockSize_ and buffer_,
// so sizeof stays 160, alignof stays 8, every pre-existing member keeps its offset, and Dispose was
// already virtual on DeriveBytes so the override fills an existing vtable slot. Measured in
// build-probe/2160_layout_real.log and recorded in
// docs/SystemSecurityCryptographyNamespaceReviewPlan.md sections 4.1 and 15.
#include <gtest/gtest.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Security/Cryptography/CryptographicException.hpp"
#include "../../../support/KeyMaterialSeam.hpp"

using namespace System::Security::Cryptography;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

using Bytes = std::vector<bytecs>;
using Access = SharpRuntime::Testing::KeyMaterialAccess<Rfc2898DeriveBytes>;

Bytes filled(size_t n, unsigned v) { return Bytes(n, static_cast<bytecs>(v)); }

std::string toHex(const Bytes& bytes) {
    std::ostringstream oss;
    for (auto b : bytes) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

bool allZero(const Bytes& v) { return std::all_of(v.begin(), v.end(), [](bytecs b) { return b == 0; }); }

Rfc2898DeriveBytes make(intcs iterations = 10, HashAlgorithmName alg = HashAlgorithmName::SHA256) {
    return Rfc2898DeriveBytes(filled(16, 0xC3), filled(8, 0x11), iterations, std::move(alg));
}

} // namespace

// --- the control ------------------------------------------------------------------------------
//
// Before Dispose, the password really is resident and really is the caller's. Without this, a green
// "it is erased" assertion could equally mean the instrument sees nothing at all.

TEST(Rfc2898DisposalControlTests, BeforeDisposeTheSecretsAreResident) {
    Rfc2898DeriveBytes pbkdf = make();
    (void)pbkdf.GetBytes(8);

    EXPECT_EQ(Access::password(pbkdf), filled(16, 0xC3));
    EXPECT_EQ(Access::saltAndCounter(pbkdf).size(), 12u); // 8 salt bytes + the 4-byte counter
    EXPECT_EQ(Access::buffer(pbkdf).size(), 32u);
    EXPECT_FALSE(allZero(Access::buffer(pbkdf)));
    EXPECT_FALSE(Access::disposed(pbkdf));
}

// --- the finding ------------------------------------------------------------------------------

TEST(Rfc2898DisposalTests, DisposeErasesThePasswordSaltAndDerivedBuffer) {
    Rfc2898DeriveBytes pbkdf = make();
    (void)pbkdf.GetBytes(8);

    pbkdf.Dispose();

    EXPECT_TRUE(Access::password(pbkdf).empty());
    EXPECT_TRUE(allZero(Access::password(pbkdf)));
    EXPECT_TRUE(Access::saltAndCounter(pbkdf).empty());
    EXPECT_TRUE(allZero(Access::saltAndCounter(pbkdf)));
    EXPECT_TRUE(Access::buffer(pbkdf).empty());
    EXPECT_TRUE(allZero(Access::buffer(pbkdf)));
    EXPECT_TRUE(Access::disposed(pbkdf));
}

TEST(Rfc2898DisposalTests, GetBytesAfterDisposeThrowsInsteadOfDeriving) {
    Rfc2898DeriveBytes pbkdf = make();
    (void)pbkdf.GetBytes(8);
    pbkdf.Dispose();
    EXPECT_THROW((void)pbkdf.GetBytes(4), System::ObjectDisposedException);
}

TEST(Rfc2898DisposalTests, EveryDoorRejectsAfterDispose) {
    Rfc2898DeriveBytes pbkdf = make();
    pbkdf.Dispose();

    EXPECT_THROW((void)pbkdf.GetBytes(4), System::ObjectDisposedException);
    EXPECT_THROW(pbkdf.Reset(), System::ObjectDisposedException);
    EXPECT_THROW(pbkdf.setIterationCountProperty(50), System::ObjectDisposedException);
    EXPECT_THROW(pbkdf.setSaltProperty(filled(8, 0x22)), System::ObjectDisposedException);
    // getSaltProperty is not merely a lifecycle nicety: it computes salt_.end() - 4, which on the
    // emptied vector Dispose leaves behind would be undefined behaviour rather than an empty result.
    EXPECT_THROW((void)pbkdf.getSaltProperty(), System::ObjectDisposedException);
}

TEST(Rfc2898DisposalTests, TheDisposedObjectNamesItselfInTheDiagnostic) {
    Rfc2898DeriveBytes pbkdf = make();
    pbkdf.Dispose();
    try {
        (void)pbkdf.GetBytes(4);
        FAIL() << "expected ObjectDisposedException";
    } catch (const System::ObjectDisposedException& e) {
        EXPECT_NE(std::string(e.what()).find("Rfc2898DeriveBytes"), std::string::npos) << e.what();
    }
}

TEST(Rfc2898DisposalTests, DisposeIsIdempotent) {
    Rfc2898DeriveBytes pbkdf = make();
    pbkdf.Dispose();
    EXPECT_NO_THROW(pbkdf.Dispose());
    EXPECT_NO_THROW(pbkdf.Dispose());
    EXPECT_TRUE(Access::password(pbkdf).empty());
}

TEST(Rfc2898DisposalTests, DisposeAfterPartialConsumptionStillErasesTheRemainder) {
    Rfc2898DeriveBytes pbkdf = make();
    // 20 of a 32-byte SHA-256 block: the remaining 12 stay buffered for the next call.
    (void)pbkdf.GetBytes(20);
    ASSERT_FALSE(allZero(Access::buffer(pbkdf)));

    pbkdf.Dispose();
    EXPECT_TRUE(Access::buffer(pbkdf).empty());
    EXPECT_TRUE(allZero(Access::buffer(pbkdf)));
}

TEST(Rfc2898DisposalTests, DisposeBeforeAnyDerivationIsSafe) {
    Rfc2898DeriveBytes pbkdf = make();
    EXPECT_NO_THROW(pbkdf.Dispose());
    EXPECT_TRUE(Access::password(pbkdf).empty());
}

// --- cause SC-C: a rejected argument had already consumed the password -------------------------

TEST(Rfc2898ConstructionTests, ARejectedIterationCountStillRejects) {
    EXPECT_THROW(Rfc2898DeriveBytes(filled(16, 0xC3), filled(8, 0x11), 0, HashAlgorithmName::SHA256),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(Rfc2898DeriveBytes(filled(16, 0xC3), filled(8, 0x11), -1, HashAlgorithmName::SHA256),
                 System::ArgumentOutOfRangeException);
}

TEST(Rfc2898ConstructionTests, AnUnknownHashAlgorithmIsRejectedBeforeThePasswordIsStored) {
    // Validation moved into the member-initialiser list so that the password can be erased on the
    // way out. The rejection itself, and its exception type, must be unchanged.
    EXPECT_THROW(Rfc2898DeriveBytes(filled(16, 0xC3), filled(8, 0x11), 10, HashAlgorithmName::MD5),
                 CryptographicException);
    EXPECT_THROW(Rfc2898DeriveBytes(filled(16, 0xC3), filled(8, 0x11), 10, HashAlgorithmName::SHA3_256),
                 CryptographicException);
    EXPECT_THROW(Rfc2898DeriveBytes(filled(16, 0xC3), filled(8, 0x11), 10, HashAlgorithmName()),
                 CryptographicException);
}

TEST(Rfc2898ConstructionTests, TheStringOverloadRejectsTheSameWay) {
    EXPECT_THROW(Rfc2898DeriveBytes(std::string("password"), filled(8, 0x11), 0, HashAlgorithmName::SHA256),
                 System::ArgumentOutOfRangeException);
}

// --- copy and move must keep exactly their previous semantics ---------------------------------

TEST(Rfc2898SpecialMemberTests, AllFourCopyAndMoveOperationsRemainAvailable) {
    static_assert(std::is_copy_constructible_v<Rfc2898DeriveBytes>);
    static_assert(std::is_move_constructible_v<Rfc2898DeriveBytes>);
    static_assert(std::is_copy_assignable_v<Rfc2898DeriveBytes>);
    static_assert(std::is_move_assignable_v<Rfc2898DeriveBytes>);
    SUCCEED();
}

TEST(Rfc2898SpecialMemberTests, MoveConstructionTransfersTheSecretAndLeavesTheSourceEmpty) {
    Rfc2898DeriveBytes source = make();
    const std::string expected = toHex(make().GetBytes(16));

    Rfc2898DeriveBytes moved(std::move(source));
    EXPECT_EQ(toHex(moved.GetBytes(16)), expected);
    EXPECT_TRUE(Access::password(source).empty()) << "a suppressed move would have copied the password";
}

TEST(Rfc2898SpecialMemberTests, CopyingKeepsBothInstancesUsableAndIndependent) {
    Rfc2898DeriveBytes source = make();
    const std::string expected = toHex(make().GetBytes(16));

    Rfc2898DeriveBytes copy(source);
    EXPECT_EQ(toHex(copy.GetBytes(16)), expected);
    EXPECT_EQ(toHex(source.GetBytes(16)), expected);

    copy.Dispose();
    EXPECT_THROW((void)copy.GetBytes(4), System::ObjectDisposedException);
    EXPECT_FALSE(Access::disposed(source)) << "disposing a copy must not dispose the original";
    EXPECT_NO_THROW((void)source.GetBytes(4));
}

// --- derivation output is untouched -----------------------------------------------------------
//
// The repair reshaped func() and initialize() so their intermediates are erasable. Not one output
// byte may move. Published PBKDF2-HMAC-SHA1 vectors from RFC 6070, plus the SHA-256 vector the
// audit's own probe recorded for this exact type.

TEST(Rfc2898VectorTests, Rfc6070Sha1Vectors) {
    Rfc2898DeriveBytes a(std::string("password"), Bytes{'s', 'a', 'l', 't'}, 1, HashAlgorithmName::SHA1);
    EXPECT_EQ(toHex(a.GetBytes(20)), "0c60c80f961f0e71f3a9b524af6012062fe037a6");

    Rfc2898DeriveBytes b(std::string("password"), Bytes{'s', 'a', 'l', 't'}, 2, HashAlgorithmName::SHA1);
    EXPECT_EQ(toHex(b.GetBytes(20)), "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957");

    Rfc2898DeriveBytes c(std::string("password"), Bytes{'s', 'a', 'l', 't'}, 4096, HashAlgorithmName::SHA1);
    EXPECT_EQ(toHex(c.GetBytes(20)), "4b007901b765489abead49d926f721d065a429c1");
}

TEST(Rfc2898VectorTests, TheAuditProbesSha256VectorIsUnchanged) {
    Rfc2898DeriveBytes pbkdf(std::string("password"), Bytes{'s', 'a', 'l', 't'}, 1, HashAlgorithmName::SHA256);
    EXPECT_EQ(toHex(pbkdf.GetBytes(32)),
              "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
}

TEST(Rfc2898VectorTests, SplitCallsContinueExactlyWhereASingleCallWouldHave) {
    const std::string whole = toHex(make().GetBytes(80));

    Rfc2898DeriveBytes split = make();
    std::string pieces;
    for (intcs n : {intcs{7}, intcs{1}, intcs{24}, intcs{32}, intcs{16}}) pieces += toHex(split.GetBytes(n));
    EXPECT_EQ(pieces, whole);
}

TEST(Rfc2898VectorTests, ResetRestartsTheSequenceAndSettersStillDoToo) {
    Rfc2898DeriveBytes pbkdf = make();
    const std::string first = toHex(pbkdf.GetBytes(40));

    pbkdf.Reset();
    EXPECT_EQ(toHex(pbkdf.GetBytes(40)), first);

    pbkdf.setIterationCountProperty(10);
    EXPECT_EQ(toHex(pbkdf.GetBytes(40)), first);

    pbkdf.setSaltProperty(filled(8, 0x11));
    EXPECT_EQ(toHex(pbkdf.GetBytes(40)), first);
    EXPECT_EQ(pbkdf.getSaltProperty(), filled(8, 0x11));
}

TEST(Rfc2898VectorTests, EverySupportedHashAlgorithmStillDerives) {
    for (const HashAlgorithmName& alg : {HashAlgorithmName::SHA1, HashAlgorithmName::SHA256,
                                          HashAlgorithmName::SHA384, HashAlgorithmName::SHA512}) {
        Rfc2898DeriveBytes pbkdf(filled(16, 0xC3), filled(8, 0x11), 5, alg);
        const Bytes out = pbkdf.GetBytes(64);
        EXPECT_EQ(out.size(), 64u) << alg.ToString();
        EXPECT_FALSE(allZero(out)) << alg.ToString();
    }
}

TEST(Rfc2898VectorTests, ARejectedGetBytesArgumentLeavesTheInstanceUsable) {
    Rfc2898DeriveBytes pbkdf = make();
    EXPECT_THROW((void)pbkdf.GetBytes(0), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)pbkdf.GetBytes(-1), System::ArgumentOutOfRangeException);
    EXPECT_EQ(toHex(pbkdf.GetBytes(16)), toHex(make().GetBytes(16)));
}

// --- the layout claim, pinned in the build rather than only in the plan ------------------------

TEST(Rfc2898LayoutTests, TheDisposedFlagCostNothing) {
    // The repair's whole premise. If a future change appends a member instead of using the hole,
    // or reorders these, this fails and the plan's section 4.1 has to be revisited deliberately.
    EXPECT_EQ(sizeof(Rfc2898DeriveBytes), 160u);
    EXPECT_EQ(alignof(Rfc2898DeriveBytes), 8u);
}
