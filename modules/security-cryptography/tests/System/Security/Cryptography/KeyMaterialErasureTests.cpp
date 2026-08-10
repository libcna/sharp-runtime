// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2159, SR-AUD-332 (cause SC-A). Permanent regressions for the key-material erasure
// contract of System::Security::Cryptography's keyed-hash family.
//
// WHAT THESE ASSERT, and what they deliberately do not. Erasure is not observable through any
// public API by construction -- a type that let a caller read its own key-derived pads would be
// the defect these tests exist to prevent -- so the live-object assertions go through the
// test-only seam SharpRuntime::Testing::KeyMaterialAccess (support/KeyMaterialSeam.hpp, the one
// place in the repository that may define it). What they do NOT assert is the state of storage
// already handed back to the allocator: observing that needs a replacement global
// operator new/delete, and installing one in this executable would displace AddressSanitizer's
// own replacement and silently weaken every other suite in the binary. That half of the evidence
// is measured by build-probe/2158_probe2_residue.cpp and transcribed, with exact figures, into
// docs/SystemSecurityCryptographyNamespaceReviewPlan.md sections 4.2 and 14.
#include <gtest/gtest.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Security/Cryptography/MD5.hpp"
#include "System/Security/Cryptography/SHA256.hpp"
#include "System/Security/Cryptography/SHA3_256.hpp"
#include "System/Security/Cryptography/detail/SecureMemory.hpp"
#include "../../../support/KeyMaterialSeam.hpp"

using namespace System::Security::Cryptography;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

using Bytes = std::vector<bytecs>;

template <typename T>
using Access = SharpRuntime::Testing::KeyMaterialAccess<T>;

Bytes toBytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

std::string toHex(const Bytes& bytes) {
    std::ostringstream oss;
    for (auto b : bytes) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

bool allZero(const Bytes& v) {
    return std::all_of(v.begin(), v.end(), [](bytecs b) { return b == 0; });
}

/** @return The longest run of @p value anywhere in @p v. A retained pad is a run of blockSize. */
size_t longestRun(const Bytes& v, bytecs value) {
    size_t best = 0, run = 0;
    for (auto b : v) {
        if (b == value) { if (++run > best) best = run; } else { run = 0; }
    }
    return best;
}

/** @brief Every key-material buffer the seam can reach, for one keyed-hash object. */
template <typename T>
bool everyKeyBufferIsErased(const T& hmac) {
    return Access<T>::key(hmac).empty() && allZero(Access<T>::key(hmac)) &&
           Access<T>::innerPad(hmac).empty() && allZero(Access<T>::innerPad(hmac)) &&
           Access<T>::outerPad(hmac).empty() && allZero(Access<T>::outerPad(hmac)) &&
           Access<T>::pendingMessage(hmac).empty() && allZero(Access<T>::pendingMessage(hmac));
}

Bytes repeatedByte(size_t n, bytecs v) { return Bytes(n, v); }

/** The only way to leave a message buffered: HashCore is protected, ComputeHash finalises. */
class FeedableHmac : public HMACSHA256 {
public:
    using HMACSHA256::HMACSHA256;
    void feed(const Bytes& b) { HashCore(b, 0, static_cast<intcs>(b.size())); }
};

Bytes ascendingWithEmbeddedZeros(size_t n) {
    Bytes v(n);
    for (size_t i = 0; i < n; ++i) v[i] = (i % 5 == 0) ? bytecs{0} : static_cast<bytecs>(i * 7 + 1);
    return v;
}

} // namespace

// --- the seam's own control -------------------------------------------------------------------
//
// Every "the buffer is erased" assertion below is only worth something if the same instrument can
// see key material when it is there. This is that control: before Dispose, the pads are present,
// full length, and one XOR recovers the key.

TEST(HmacKeyMaterialControlTests, BeforeDisposeThePadsArePresentAndInvertToTheKey) {
    const Bytes key = repeatedByte(32, 0xA7);
    HMACSHA256 hmac(key);

    const Bytes& inner = Access<HMACSHA256>::innerPad(hmac);
    const Bytes& outer = Access<HMACSHA256>::outerPad(hmac);

    ASSERT_EQ(inner.size(), 64u);
    ASSERT_EQ(outer.size(), 64u);
    EXPECT_EQ(longestRun(inner, static_cast<bytecs>(0xA7 ^ 0x36)), 32u);
    EXPECT_EQ(longestRun(outer, static_cast<bytecs>(0xA7 ^ 0x5C)), 32u);

    // SR-AUD-332's exact claim: either pad recovers the padded key with a fixed XOR.
    size_t recovered = 0;
    for (size_t i = 0; i < key.size(); ++i) {
        if (static_cast<bytecs>(inner[i] ^ 0x36) == key[i]) ++recovered;
    }
    EXPECT_EQ(recovered, key.size());
    EXPECT_EQ(Access<HMACSHA256>::key(hmac), key);
}

// --- Dispose ----------------------------------------------------------------------------------

TEST(HmacKeyMaterialErasureTests, DisposeErasesTheKeyAndBothPads) {
    HMACSHA256 hmac(repeatedByte(32, 0xA7));
    (void)hmac.ComputeHash(toBytes("payload"));

    hmac.Dispose();

    EXPECT_TRUE(everyKeyBufferIsErased(hmac));
}

TEST(HmacKeyMaterialErasureTests, DisposeErasesABufferedMessageThatWasNeverFinalised) {
    // HashCore is protected and ComputeHash always finalises, so a message can only be left
    // buffered by a subclass. That is exactly the shape a future TransformBlock port would have,
    // and it is the only way to observe the erasure the audit asked for.
    FeedableHmac hmac(repeatedByte(32, 0xA7));
    hmac.feed(repeatedByte(40, 0x5A));
    ASSERT_EQ(Access<HMACSHA256>::pendingMessage(hmac).size(), 40u);

    hmac.Dispose();
    EXPECT_TRUE(Access<HMACSHA256>::pendingMessage(hmac).empty());
    EXPECT_TRUE(allZero(Access<HMACSHA256>::pendingMessage(hmac)));
}

TEST(HmacKeyMaterialErasureTests, DisposeIsIdempotent) {
    HMACSHA256 hmac(repeatedByte(32, 0xA7));
    hmac.Dispose();
    EXPECT_NO_THROW(hmac.Dispose());
    EXPECT_NO_THROW(hmac.Dispose());
    EXPECT_TRUE(everyKeyBufferIsErased(hmac));
}

TEST(HmacKeyMaterialErasureTests, ComputeHashAfterDisposeStillThrows) {
    HMACSHA256 hmac(repeatedByte(32, 0xA7));
    hmac.Dispose();
    EXPECT_THROW((void)hmac.ComputeHash(toBytes("payload")), System::ObjectDisposedException);
}

TEST(HmacKeyMaterialErasureTests, GetKeyAfterDisposeIsEmpty) {
    HMACSHA256 hmac(repeatedByte(32, 0xA7));
    hmac.Dispose();
    EXPECT_TRUE(hmac.getKeyProperty().empty());
}

// --- key replacement --------------------------------------------------------------------------

TEST(HmacKeyMaterialErasureTests, ReplacingTheKeyLeavesNoTraceOfThePreviousPads) {
    HMACSHA256 hmac(repeatedByte(32, 0xA7));
    ASSERT_EQ(longestRun(Access<HMACSHA256>::innerPad(hmac), static_cast<bytecs>(0xA7 ^ 0x36)), 32u);

    hmac.setKeyProperty(repeatedByte(32, 0x22));

    EXPECT_EQ(longestRun(Access<HMACSHA256>::innerPad(hmac), static_cast<bytecs>(0xA7 ^ 0x36)), 0u);
    EXPECT_EQ(longestRun(Access<HMACSHA256>::outerPad(hmac), static_cast<bytecs>(0xA7 ^ 0x5C)), 0u);
    EXPECT_EQ(longestRun(Access<HMACSHA256>::innerPad(hmac), static_cast<bytecs>(0x22 ^ 0x36)), 32u);
    EXPECT_EQ(Access<HMACSHA256>::key(hmac), repeatedByte(32, 0x22));
}

TEST(HmacKeyMaterialErasureTests, ReplacingAKeyWithAShorterOneLeavesNoTailOfTheOldPad) {
    HMACSHA256 hmac(repeatedByte(64, 0xA7)); // exactly the block size: the pad is fully populated
    ASSERT_EQ(longestRun(Access<HMACSHA256>::innerPad(hmac), static_cast<bytecs>(0xA7 ^ 0x36)), 64u);

    hmac.setKeyProperty(repeatedByte(4, 0x22));

    // The zero-padded tail must be the ipad constant, not the previous key's pad bytes.
    const Bytes& inner = Access<HMACSHA256>::innerPad(hmac);
    ASSERT_EQ(inner.size(), 64u);
    EXPECT_EQ(longestRun(inner, static_cast<bytecs>(0xA7 ^ 0x36)), 0u);
    for (size_t i = 4; i < inner.size(); ++i) EXPECT_EQ(inner[i], bytecs{0x36}) << "at " << i;
}

TEST(HmacKeyMaterialErasureTests, ComputeHashClearsTheBufferedMessage) {
    HMACSHA256 hmac(repeatedByte(32, 0xA7));
    (void)hmac.ComputeHash(repeatedByte(200, 0x5A));
    EXPECT_TRUE(Access<HMACSHA256>::pendingMessage(hmac).empty());
}

// --- every variant, every key shape -----------------------------------------------------------

template <typename THmac>
void expectDisposeErasesEverything(const Bytes& key) {
    THmac hmac(key);
    (void)hmac.ComputeHash(toBytes("payload"));
    hmac.Dispose();
    EXPECT_TRUE(everyKeyBufferIsErased(hmac));
}

TEST(HmacKeyMaterialErasureTests, EveryHmacVariantErasesOnDispose) {
    const Bytes key = repeatedByte(32, 0xA7);
    expectDisposeErasesEverything<HMACMD5>(key);
    expectDisposeErasesEverything<HMACSHA1>(key);
    expectDisposeErasesEverything<HMACSHA256>(key);
    expectDisposeErasesEverything<HMACSHA384>(key);
    expectDisposeErasesEverything<HMACSHA512>(key);
    expectDisposeErasesEverything<HMACSHA3_256>(key);
    expectDisposeErasesEverything<HMACSHA3_384>(key);
    expectDisposeErasesEverything<HMACSHA3_512>(key);
}

TEST(HmacKeyMaterialErasureTests, EveryKeyShapeErasesOnDispose) {
    expectDisposeErasesEverything<HMACSHA256>(Bytes{});                          // empty
    expectDisposeErasesEverything<HMACSHA256>(repeatedByte(1, 0x01));            // minimum
    expectDisposeErasesEverything<HMACSHA256>(repeatedByte(32, 0x00));           // all zero
    expectDisposeErasesEverything<HMACSHA256>(repeatedByte(63, 0xFF));           // block size - 1
    expectDisposeErasesEverything<HMACSHA256>(repeatedByte(64, 0xFF));           // exactly block
    expectDisposeErasesEverything<HMACSHA256>(repeatedByte(65, 0xFF));           // block + 1
    expectDisposeErasesEverything<HMACSHA256>(repeatedByte(131, 0xAA));          // RFC 4231 case 6
    expectDisposeErasesEverything<HMACSHA256>(ascendingWithEmbeddedZeros(96));   // embedded zeros
    expectDisposeErasesEverything<HMACSHA256>(repeatedByte(4096, 0x5A));         // large
    expectDisposeErasesEverything<HMACSHA512>(repeatedByte(200, 0xAA));          // > 128-byte block
}

// --- copy and move must keep exactly their previous semantics ---------------------------------
//
// The erasing destructors added by this ticket suppress the implicit move operations unless they
// are explicitly defaulted. A suppressed move silently becomes a COPY, which would leave the
// source object holding a live key -- a security regression introduced by the security fix. These
// pin all four operations.

TEST(HmacSpecialMemberTests, AllFourCopyAndMoveOperationsRemainAvailable) {
    static_assert(std::is_copy_constructible_v<HMACSHA256>);
    static_assert(std::is_move_constructible_v<HMACSHA256>);
    static_assert(std::is_copy_assignable_v<HMACSHA256>);
    static_assert(std::is_move_assignable_v<HMACSHA256>);
    SUCCEED();
}

TEST(HmacSpecialMemberTests, MoveConstructionTransfersTheKeyAndLeavesTheSourceEmpty) {
    HMACSHA256 source(repeatedByte(32, 0xA7));
    const std::string expected = toHex(HMACSHA256(repeatedByte(32, 0xA7)).ComputeHash(toBytes("Hi There")));

    HMACSHA256 moved(std::move(source));
    EXPECT_EQ(toHex(moved.ComputeHash(toBytes("Hi There"))), expected);

    // A real move, not a copy: the source's own key storage is empty afterwards.
    EXPECT_TRUE(Access<HMACSHA256>::key(source).empty());
    EXPECT_TRUE(Access<HMACSHA256>::innerPad(source).empty());
}

TEST(HmacSpecialMemberTests, MoveAssignmentTransfersTheKeyAndLeavesTheSourceEmpty) {
    HMACSHA256 source(repeatedByte(32, 0xA7));
    HMACSHA256 target(repeatedByte(32, 0x11));
    const std::string expected = toHex(HMACSHA256(repeatedByte(32, 0xA7)).ComputeHash(toBytes("Hi There")));

    target = std::move(source);
    EXPECT_EQ(toHex(target.ComputeHash(toBytes("Hi There"))), expected);
    EXPECT_TRUE(Access<HMACSHA256>::key(source).empty());
}

TEST(HmacSpecialMemberTests, CopyConstructionAndAssignmentKeepBothObjectsUsable) {
    HMACSHA256 source(repeatedByte(32, 0xA7));
    const std::string expected = toHex(HMACSHA256(repeatedByte(32, 0xA7)).ComputeHash(toBytes("Hi There")));

    HMACSHA256 copy(source);
    EXPECT_EQ(toHex(copy.ComputeHash(toBytes("Hi There"))), expected);
    EXPECT_EQ(toHex(source.ComputeHash(toBytes("Hi There"))), expected);

    HMACSHA256 assigned(repeatedByte(32, 0x11));
    assigned = source;
    EXPECT_EQ(toHex(assigned.ComputeHash(toBytes("Hi There"))), expected);
    EXPECT_EQ(toHex(source.ComputeHash(toBytes("Hi There"))), expected);
}

TEST(HmacSpecialMemberTests, DisposingACopyDoesNotDisturbTheOriginal) {
    HMACSHA256 source(repeatedByte(32, 0xA7));
    HMACSHA256 copy(source);
    copy.Dispose();

    EXPECT_TRUE(everyKeyBufferIsErased(copy));
    EXPECT_EQ(Access<HMACSHA256>::key(source), repeatedByte(32, 0xA7));
    EXPECT_EQ(toHex(source.ComputeHash(toBytes("Hi There"))),
              "890678db9a6a93cb8b94e4128976b0afac364c70b51132c5f21e4b7097720e81");
}

// --- failure paths ----------------------------------------------------------------------------

TEST(HmacKeyMaterialErasureTests, AFailedComputeHashLeavesTheObjectIntactAndStillCorrect) {
    HMACSHA256 hmac(repeatedByte(20, 0x0b));
    Bytes buffer(8, 0x01);

    EXPECT_THROW((void)hmac.ComputeHash(buffer, -1, 4), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)hmac.ComputeHash(buffer, 0, 99), System::ArgumentException);
    EXPECT_THROW((void)hmac.ComputeHash(buffer, 6, 4), System::ArgumentException);

    // The rejected calls must not have consumed the key or corrupted the pads.
    EXPECT_EQ(Access<HMACSHA256>::key(hmac), repeatedByte(20, 0x0b));
    EXPECT_EQ(toHex(hmac.ComputeHash(toBytes("Hi There"))),
              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
}

// --- cryptographic invariance -----------------------------------------------------------------
//
// The repair reshaped derivePads() and HashFinal() to make their working buffers erasable. Not a
// single output byte may move. RFC 4231 case 6 is the one that exercises the restructured
// long-key path, and repeated cycles pin the Initialize() erasure against state leaking forward.

TEST(HmacInvarianceTests, Rfc4231LongKeyPathIsUnchangedByTheErasureRestructure) {
    HMACSHA256 hmac(repeatedByte(131, 0xAA));
    EXPECT_EQ(toHex(hmac.ComputeHash(toBytes("Test Using Larger Than Block-Size Key - Hash Key First"))),
              "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

    HMACSHA512 wide(repeatedByte(131, 0xAA));
    EXPECT_EQ(toHex(wide.ComputeHash(toBytes("Test Using Larger Than Block-Size Key - Hash Key First"))),
              "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e52"
              "95e64f73f63f0aec8b915a985d786598");
}

TEST(HmacInvarianceTests, RepeatedComputeCyclesAgreeWithFreshInstances) {
    HMACSHA256 reused(repeatedByte(20, 0x0b));
    for (int i = 0; i < 5; ++i) {
        HMACSHA256 fresh(repeatedByte(20, 0x0b));
        EXPECT_EQ(toHex(reused.ComputeHash(toBytes("Hi There"))), toHex(fresh.ComputeHash(toBytes("Hi There"))));
    }
}

TEST(HmacInvarianceTests, DigestsAreUnchangedByTheHashBufferErasure) {
    // The five .cpp-backed digests now erase their block buffer in Initialize(); ComputeHash calls
    // Initialize() at the end of every operation, so a wrong placement would corrupt the SECOND
    // digest an instance produces, not the first.
    MD5 md5;
    EXPECT_EQ(toHex(md5.ComputeHash(toBytes("abc"))), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(toHex(md5.ComputeHash(toBytes("abc"))), "900150983cd24fb0d6963f7d28e17f72");

    SHA256 sha256;
    EXPECT_EQ(toHex(sha256.ComputeHash(toBytes("abc"))),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(toHex(sha256.ComputeHash(toBytes("abc"))),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    SHA3_256 sha3;
    EXPECT_EQ(toHex(sha3.ComputeHash(toBytes("abc"))),
              "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
    EXPECT_EQ(toHex(sha3.ComputeHash(toBytes("abc"))),
              "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
}

// --- the primitive itself ---------------------------------------------------------------------
//
// A control the compiler cannot fold away, alongside a deliberately UNCLEARED buffer, so a green
// result here means "the clear happened" and not "the observation was optimised out too".

TEST(SecureMemoryTests, ClearErasesAndTheUnclearedControlStaysIntact) {
    std::vector<bytecs> cleared(64, 0xA7);
    std::vector<bytecs> control(64, 0xA7);

    SharpRuntimeDetail::SecureMemory::Clear(cleared.data(), cleared.size());

    EXPECT_TRUE(allZero(cleared));
    EXPECT_EQ(longestRun(control, bytecs{0xA7}), 64u) << "the control must still be observable";
}

TEST(SecureMemoryTests, ClearContainerErasesThenEmptiesWithoutReleasingCapacity) {
    std::vector<bytecs> v(64, 0xA7);
    const auto* data = v.data();
    const size_t capacity = v.capacity();

    SharpRuntimeDetail::SecureMemory::ClearContainer(v);

    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.capacity(), capacity);
    EXPECT_EQ(v.data(), data) << "clear() must not release the block the erased bytes live in";
}

TEST(SecureMemoryTests, ScrubKeepsTheSizeAndTheBlock) {
    std::vector<bytecs> v(64, 0xA7);
    SharpRuntimeDetail::SecureMemory::Scrub(v);
    EXPECT_EQ(v.size(), 64u);
    EXPECT_TRUE(allZero(v));
}

TEST(SecureMemoryTests, ClearToleratesNullAndZeroLength) {
    EXPECT_NO_THROW(SharpRuntimeDetail::SecureMemory::Clear(nullptr, 0));
    EXPECT_NO_THROW(SharpRuntimeDetail::SecureMemory::Clear(nullptr, 64));
    std::vector<bytecs> empty;
    EXPECT_NO_THROW(SharpRuntimeDetail::SecureMemory::ClearContainer(empty));
    EXPECT_NO_THROW(SharpRuntimeDetail::SecureMemory::Scrub(empty));
}
